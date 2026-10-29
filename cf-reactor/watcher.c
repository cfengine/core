/*
  Copyright 2026 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; version 3.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <watcher.h>
#include <wakeup_channel.h>
#include <stoppable_thread.h>
#include <signal_lib.h>         // MaskTerminationSignalsInThread()
#include <signals.h>            // IsPendingTermination()
#include <logging.h>
#include <alloc.h>
#include <string_lib.h>         // StringEqual_untyped(), StringHash_untyped()
#include <map.h>
#include <threaded_queue.h>
#include <file_watcher.h>
#include <mutex.h>              // ThreadLock(), ThreadUnlock()
#include <locks.h>              // PromiseRuntimeHash()
#include <hash.h>               // HashPrintSafe()
#include <policy.h>             // Promise
#include <mod_reactor.h>        // REACTOR_WATCH_METHODS
#include <watch_backends.h>     // WatchBackendsAdd(), WatchBackendsRemove()
#include <json.h>
#include <file_lib.h>           // safe_fopen()
#include <writer.h>             // FileWriter()
#include <known_dirs.h>         // GetStateDir()

/* Upper bound on how long the watcher thread sleeps in one go while it has
 * watchers to check, regardless of what poll intervals they asked for. When
 * it has none it sleeps until woken up. Either way, shutdown doesn't depend
 * on it: EventWatcherFinalize() wakes the thread up to stop it. */
#define MAX_WATCHER_THREAD_SLEEP_SECS 1

#define WATCHER_THREAD_EXIT_TIMEOUT_SECS 5

/* How long the watcher thread waits after a watch method marked watchers
 * dirty, before checking them. A single operation is often several changes
 * in a row (touch creates a file then sets its times, a package manager
 * replaces a file then fixes its owner), and checking in between would
 * observe states polling never sees. The marks arriving meanwhile are
 * checked together. */
#define WATCH_SETTLE_MSECS 200

/* Limit on how often a watcher runs its 'then' bundles. Polling alone caps it
 * at once per interval, but the watch methods wake a watcher up on every
 * change, so anyone able to touch a watched file could otherwise make
 * cf-reactor run the bundles continuously. Beyond MAX_TRIGGERS_PER_WINDOW
 * runs in TRIGGER_WINDOW_SECS, the watcher waits before its next run, twice
 * as long each time (up to MAX_TRIGGER_BACKOFF_SECS), until a window passes
 * below the limit. Events in the meantime are coalesced into the next run. */
#define TRIGGER_WINDOW_SECS 60
#define MAX_TRIGGERS_PER_WINDOW 10
#define MIN_TRIGGER_BACKOFF_SECS 10
#define MAX_TRIGGER_BACKOFF_SECS 300

/**
 * Seconds on a clock that only moves forward, for scheduling checks. With the
 * wall clock, stepping it back would postpone every check by the size of the
 * step. Falls back to the wall clock where there is no monotonic clock.
 */
static time_t WatcherClockNow(void)
{
    struct timespec ts;
#ifdef CLOCK_MONOTONIC
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return ts.tv_sec;
}

typedef struct
{
    WatcherCheckFn check_callback;
    WatcherStateDestroyFn destroy_state;
    void *state;
    Promise *promise; // copy of the expanded events promise, see PromiseCopy()
    char *promise_hash; // hash of `promise`, see WatcherPromiseHash()
    time_t poll_interval_secs;
    time_t next_due; // on WatcherClockNow()'s clock
    WatcherCheckResult last_check; // only touched by the watcher thread

    uint64_t id;          // passed to the watch methods, see WatcherMarkDirty()
    const char *method;   // watch method used, static, see WatchBackendsAdd()

    /* Whether an event was queued and its bundles haven't finished running.
     * Events observed meanwhile are held, see EventHandled(). */
    bool event_pending;

    /* An observed event not run yet because of the trigger limit */
    bool event_held;

    /* A check skipped while an event was held, done once it can run, see
     * WatcherThreadMain() */
    bool check_deferred;
    time_t throttled_until;    // 0 when not throttled
    time_t window_start;
    unsigned int triggers_in_window;
    time_t backoff_secs;
} Watcher;

/* Item of the event queue */
typedef struct
{
    Watcher *watcher;
    WatcherCheckResult check; // see WatcherEventFn
} WatcherEvent;

static void WatcherDestroy(void *item);

/* Guards `watchers`, `old_watchers` and `paused` against the watcher thread
 * (WatcherThreadMain()) iterating over `watchers` concurrently with the main
 * thread re-registering watchers from a freshly (re-)read policy
 * (WatcherRegister(), EventWatcherPause()). */
static pthread_mutex_t watchers_mutex = PTHREAD_MUTEX_INITIALIZER;
static Map *watchers = NULL;
static Map *old_watchers = NULL;
static bool paused = false;         /* see EventWatcherPause() */

static WakeupChannel wakeup_channel = { .fds = { -1, -1 } };

/* Events of watchers (WatcherEvent), queued by the watcher thread for the
 * main thread to handle. The watchers are not owned: the watcher thread only
 * queues events of watchers of the registry while holding watchers_mutex, and
 * EventWatcherPause() empties the queue before setting the watchers aside, so the watchers of
 * the queued events are always registered. */
static ThreadedQueue *event_queue = NULL;
static StoppableThread *watcher_thread = NULL;

/* Ids of the watchers to check right away, marked by the watch methods (see
 * WatcherMarkDirty()). Only ever taken after watchers_mutex, never before:
 * the watch methods mark watchers while holding their own lock, which is
 * taken after watchers_mutex when (un)registering. */
static pthread_mutex_t dirty_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t *dirty_ids = NULL;
static size_t num_dirty_ids = 0;
static size_t max_dirty_ids = 0;
static bool all_dirty = false;

/* Ids of the watchers, never reused */
static uint64_t last_watcher_id = 0;

static const ReactorWatchHost WATCH_HOST =
{
    .abi_version = REACTOR_WATCH_BACKEND_ABI_VERSION,
    .mark_dirty = WatcherMarkDirty,
    .mark_all_dirty = WatcherMarkAllDirty,
};

/*****************************************************************************/

/* The hash covers the whole expanded promise, including what is watched
 * (the constraints of the 'when' body) and what is run ('then'), so that a
 * state is only taken over by a watcher of the same type, on the same
 * thing. It leaves out 'watch_methods', which only sets how the event is
 * watched for: changing it must not reset the state, or an event happening
 * while the policy is re-read would be missed. */
void WatcherPromiseHash(const Promise *pp, char *hash, size_t hash_size)
{
    assert(pp != NULL);
    assert(hash != NULL);

    const size_t length = SeqLength(pp->conlist);
    Seq *conlist = SeqNew(length, NULL);
    for (size_t i = 0; i < length; i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);
        if (!StringEqual(cp->lval, REACTOR_WATCH_METHODS))
        {
            SeqAppend(conlist, cp);
        }
    }
    Promise hashed = *pp;
    hashed.conlist = conlist;

    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    PromiseRuntimeHash(&hashed, "events_promise_hash", digest, CF_DEFAULT_DIGEST);
    HashPrintSafe(hash, hash_size, digest, CF_DEFAULT_DIGEST, true);
    SeqDestroy(conlist);
}

void WatcherRegistryInitialize(void)
{
    assert(watchers == NULL);

    watchers = MapNew(StringHash_untyped, StringEqual_untyped, NULL, WatcherDestroy);
}

void WatcherRegistryFinalize(void)
{
    MapDestroy(watchers);
    watchers = NULL;
    MapDestroy(old_watchers);
    old_watchers = NULL;
}

// Expects interval to be strictly greater than 0, otherwise the watcher thread will busy spin
bool WatcherRegister(const Promise *pp, WatcherOptions *opt)
{
    assert(pp != NULL);
    assert(pp->promiser != NULL);
    assert(opt != NULL);
    assert(opt->check_callback != NULL);
    assert(opt->destroy_state != NULL);
    assert(opt->interval > 0);
    assert(watchers != NULL);

    char promise_hash[CF_HOSTKEY_STRING_SIZE];
    WatcherPromiseHash(pp, promise_hash, sizeof(promise_hash));

    ThreadLock(&watchers_mutex);

    if (MapHasKey(watchers, promise_hash))
    {
        // ignore duplicates
        ThreadUnlock(&watchers_mutex);
        opt->destroy_state(opt->state);
        return true;
    }

    const uint64_t id = ++last_watcher_id;

    // Take over the state of an identical watcher
    Watcher *prev = (old_watchers != NULL) ? MapGet(old_watchers, promise_hash) : NULL;
    const bool taken_over = (prev != NULL);
    void *state = opt->state;
    if (taken_over)
    {
        state = prev->state;
        prev->state = NULL; /* so not destroyed with prev */
    }

    ThreadUnlock(&watchers_mutex);

    if (taken_over)
    {
        Log(LOG_LEVEL_DEBUG, "Reactor watcher '%s' takes over the state of an identical watcher", pp->promiser);
        opt->destroy_state(opt->state);
    }
    else
    {
        /* Without holding watchers_mutex, as the check may block (e.g.
         * lstat() on an unresponsive NFS mount). The watcher isn't registered
         * yet, so the watcher thread can't check it concurrently. A failure
         * (e.g. the file locked on Windows) is logged by the check, and the
         * watcher's first successful check becomes the reference instead. */
        opt->check_callback(state);
    }

    /* Before the watcher is in the registry, so a wakeup from the method in
     * between is for an unknown id, and ignored. The watcher is checked on the
     * watcher thread's first pass anyway. */
    const char *method = WatchBackendsAdd(id, &opt->target, opt->methods, pp->promiser);
    if (method == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Reactor events promise '%s': none of its watch methods is available on this host",
            pp->promiser);
        if (!taken_over)
        {
            opt->destroy_state(state);
        }
        else
        {
            /* Hand the state back, for a later registration to take it over */
            ThreadLock(&watchers_mutex);
            if (old_watchers != NULL && prev == MapGet(old_watchers, promise_hash))
            {
                prev->state = state;
            }
            else
            {
                opt->destroy_state(state);
            }
            ThreadUnlock(&watchers_mutex);
        }
        return false;
    }
    Log(LOG_LEVEL_VERBOSE, "Reactor watcher '%s' watches with '%s'", pp->promiser, method);

    Watcher *w = xcalloc(1, sizeof(Watcher));
    w->id = id;
    w->method = method;
    w->promise_hash = xstrdup(promise_hash);
    w->window_start = WatcherClockNow();
    w->promise = PromiseCopy(pp);
    w->poll_interval_secs = opt->interval;
    w->next_due = 0; /* due immediately on the watcher thread's first pass */
    w->last_check = WATCHER_CHECK_NO_EVENT;
    w->check_callback = opt->check_callback;
    w->destroy_state = opt->destroy_state;
    w->state = state;

    ThreadLock(&watchers_mutex);
    MapInsert(watchers, w->promise_hash, w);
    ThreadUnlock(&watchers_mutex);

    return true;
}

static void WatcherDestroy(void *item)
{
    Watcher *w = item;
    WatchBackendsRemove(w->id, w->method);
    // w->state is NULL when it was borrowed by a new watcher
    if (w->destroy_state != NULL && w->state != NULL)
    {
        w->destroy_state(w->state);
    }
    PromiseDestroy(w->promise);
    free(w->promise_hash);
    free(w);
}

static bool IdIsDirty(const uint64_t *ids, size_t num_ids, uint64_t id)
{
    for (size_t i = 0; i < num_ids; i++)
    {
        if (ids[i] == id)
        {
            return true;
        }
    }
    return false;
}

/* Called by the watcher thread, holding watchers_mutex */
static void QueueEvent(Watcher *w, WatcherCheckResult check)
{
    assert(w != NULL);

    WatcherEvent *event = xmalloc(sizeof(WatcherEvent));
    event->watcher = w;
    event->check = check;
    ThreadedQueuePush(event_queue, event);
}

/* Whether the watcher may run its bundles now, see TRIGGER_WINDOW_SECS.
 * Counts the run if so. */
static bool WatcherMayTrigger(Watcher *w, time_t now)
{
    assert(w != NULL);

    if (now - w->window_start >= TRIGGER_WINDOW_SECS)
    {
        if (w->triggers_in_window < MAX_TRIGGERS_PER_WINDOW)
        {
            /* A calm window ends the backoff */
            w->backoff_secs = 0;
        }
        w->window_start = now;
        w->triggers_in_window = 0;
    }

    if (w->throttled_until > now)
    {
        return false;
    }
    if (w->throttled_until != 0)
    {
        /* The delay is over: run once for the events held meanwhile */
        w->throttled_until = 0;
        w->triggers_in_window++;
        return true;
    }

    if (w->triggers_in_window >= MAX_TRIGGERS_PER_WINDOW)
    {
        w->backoff_secs = (w->backoff_secs == 0) ?
            MIN_TRIGGER_BACKOFF_SECS : MIN(w->backoff_secs * 2, MAX_TRIGGER_BACKOFF_SECS);
        w->throttled_until = now + w->backoff_secs;
        Log(LOG_LEVEL_WARNING,
            "Reactor watcher '%s' fired more than %d times in %d seconds, delaying its next run by %jd seconds",
            w->promise->promiser, MAX_TRIGGERS_PER_WINDOW, TRIGGER_WINDOW_SECS,
            (intmax_t) w->backoff_secs);
        return false;
    }

    w->triggers_in_window++;
    return true;
}

static void WatcherThreadMain(StoppableThread *thread, ARG_UNUSED void *unused)
{
    /* Keep termination signals landing on the main thread (which owns the
     * daemon's HandleSignalsForDaemon()-based shutdown), never on this one.
     * No-op on Windows -- see signal_lib.h. */
#ifndef __MINGW32__
    MaskTerminationSignalsInThread();
#endif

    uint64_t *dirty = NULL;
    size_t num_dirty = 0;

    while (!IsPendingTermination() && !StoppableThreadShouldStop(thread))
    {
        time_t now = WatcherClockNow();
        time_t sleep_for = MAX_WATCHER_THREAD_SLEEP_SECS;
        bool any_event = false;

        ThreadLock(&dirty_mutex);
        const bool marked = (num_dirty_ids > 0 || all_dirty);
        ThreadUnlock(&dirty_mutex);
        if (marked)
        {
            const struct timespec settle = { 0, WATCH_SETTLE_MSECS * 1000000L };
            nanosleep(&settle, NULL);
            now = WatcherClockNow();
        }

        ThreadLock(&watchers_mutex);

        /* Take the dirty marks, see WatcherMarkDirty() */
        ThreadLock(&dirty_mutex);
        free(dirty);
        dirty = dirty_ids;
        num_dirty = num_dirty_ids;
        const bool check_all = all_dirty;
        dirty_ids = NULL;
        num_dirty_ids = 0;
        max_dirty_ids = 0;
        all_dirty = false;
        ThreadUnlock(&dirty_mutex);

        /* Nothing to check until EventWatcherWakeUp() or EventWatcherResume()
         * (EventWatcherPause() empties `watchers`) wakes the thread up */
        const bool idle = (MapSize(watchers) == 0);

        MapIterator it = MapIteratorInit(watchers);
        MapKeyValue *item;
        while (!paused && (item = MapIteratorNext(&it)) != NULL)
        {
            Watcher *w = item->value;

            // check_callback cannot be null if it is correctly assigned during policy evaluation
            assert(w->check_callback != NULL);

            const bool due = (now >= w->next_due || check_all ||
                              IdIsDirty(dirty, num_dirty, w->id));

            /* While an event is held, checking again can't change what runs
             * next, and a check may be costly (file_content_modified hashes
             * the file). Skip it and check once when the event can run, so
             * a busy file is checked about as often as the bundles run. */
            const bool can_run = !w->event_held ||
                (!w->event_pending && w->throttled_until <= now);
            if (due && !can_run)
            {
                w->check_deferred = true;
                w->next_due = now + w->poll_interval_secs;
            }
            else if (can_run && (due || w->check_deferred))
            {
                w->check_deferred = false;
                WatcherCheckResult check = w->check_callback(w->state);
                w->next_due = now + w->poll_interval_secs;

                if (check == WATCHER_CHECK_EVENT)
                {
                    w->event_held = true;
                }
                /* Only report a failing watcher once, not on every check,
                 * until it recovers */
                else if (check == WATCHER_CHECK_ERROR && w->last_check != WATCHER_CHECK_ERROR)
                {
                    QueueEvent(w, WATCHER_CHECK_ERROR);
                    any_event = true;
                }
                w->last_check = check;
            }

            if (w->event_held && !w->event_pending && WatcherMayTrigger(w, now))
            {
                w->event_held = false;
                w->event_pending = true;
                QueueEvent(w, WATCHER_CHECK_EVENT);
                any_event = true;
            }

            time_t until_due = (w->next_due > now) ? (w->next_due - now) : 0;
            if (w->event_held && w->throttled_until > now)
            {
                until_due = MIN(until_due, w->throttled_until - now);
            }
            sleep_for = MIN(sleep_for, until_due);
        }

        ThreadUnlock(&watchers_mutex);

        if (any_event)
        {
            WakeupChannelNotify(&wakeup_channel);
        }

        if (idle)
        {
            StoppableThreadSleepUntilWoken(thread);
        }
        else
        {
            StoppableThreadSleep(thread, sleep_for);
        }
    }
    free(dirty);
}

bool EventWatcherInitialize(int *fd)
{
    assert(fd != NULL);
    assert(watchers != NULL); /* WatcherRegistryInitialize() first */

    if (!WakeupChannelOpen(&wakeup_channel))
    {
        Log(LOG_LEVEL_ERR, "Failed to open wakeup channel for cf-reactor event watcher");
        return false;
    }

    event_queue = ThreadedQueueNew(16, free);
    WatchBackendsInitialize(&WATCH_HOST);

    watcher_thread = StoppableThreadStart(WatcherThreadMain, NULL);
    if (watcher_thread == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to start cf-reactor watcher thread");
        WatchBackendsFinalize();
        ThreadedQueueDestroy(event_queue);
        event_queue = NULL;
        WakeupChannelClose(&wakeup_channel);
        return false;
    }

    *fd = WakeupChannelReadFd(&wakeup_channel);

    /* No watchers yet, they are registered from the policy afterwards and
     * counted by EventWatcherWakeUp() */
    Log(LOG_LEVEL_VERBOSE, "Started cf-reactor event watcher");
    return true;
}

/* The event's bundles have run: the watcher may queue events again. Whatever
 * it observed meanwhile, including what the bundles changed themselves, was
 * kept aside (event_held) and fires now. A converging bundle runs once more
 * and changes nothing, so this ends. Taking the state after the run as the
 * new reference instead would avoid that run, but lose any change made by
 * something else while the bundles ran. */
static void EventHandled(Watcher *w)
{
    assert(w != NULL);

    ThreadLock(&watchers_mutex);
    w->event_pending = false;
    const bool held = w->event_held;
    ThreadUnlock(&watchers_mutex);

    if (held && watcher_thread != NULL)
    {
        StoppableThreadWake(watcher_thread);
    }
}

/* Watchers are only destroyed by the main thread (EventWatcherResume()),
 * which is also the one handling events, and never while events are queued,
 * so the watchers of queued events can be used without holding
 * watchers_mutex. Their promise is never modified by the watcher thread. */
static void HandleQueuedEvents(EvalContext *ctx, WatcherEventFn on_event)
{
    assert(on_event != NULL);

    void *item;
    while (ThreadedQueuePop(event_queue, &item, 0))
    {
        WatcherEvent *event = item;
        Watcher *w = event->watcher;
        if (event->check == WATCHER_CHECK_ERROR)
        {
            Log(LOG_LEVEL_ERR, "Reactor watcher '%s' failed to check for its event", w->promise->promiser);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Reactor watcher '%s' fired", w->promise->promiser);
        }
        on_event(ctx, w->promise, event->check);
        if (event->check == WATCHER_CHECK_EVENT)
        {
            EventHandled(w);
        }
        free(event);
    }
}

void EventWatcherHandleEvents(EvalContext *ctx, WatcherEventFn on_event, int fd, fd_set *readfds)
{
    assert(on_event != NULL);
    assert(readfds != NULL);

    if (!FD_ISSET(fd, readfds))
    {
        return;
    }

    /* Drain before popping, so that a wakeup for an event queued after the
     * last pop stays in the channel for the next select() */
    WakeupChannelDrain(&wakeup_channel);

    HandleQueuedEvents(ctx, on_event);
}

void EventWatcherPause(EvalContext *ctx, WatcherEventFn on_event)
{
    assert(on_event != NULL);

    /* A pass of the watcher thread holds watchers_mutex, so once `paused` is
     * set no further events are queued */
    ThreadLock(&watchers_mutex);
    paused = true;
    ThreadUnlock(&watchers_mutex);

    if (event_queue != NULL)
    {
        HandleQueuedEvents(ctx, on_event);
    }

    ThreadLock(&watchers_mutex);

    /* Paused, so the watcher thread can't have queued any event since */
    if (event_queue != NULL && !ThreadedQueueIsEmpty(event_queue))
    {
        ProgrammingError("Events of reactor watchers still queued after handling them while pausing");
    }

    /* Set the watchers aside before the policy is re-read, for the watchers
     * registered from it to take over the state of the identical ones, see
     * WatcherRegister(). If paused twice in a row, the watchers of the last
     * policy are the ones registered since the first pause. */
    MapDestroy(old_watchers);
    old_watchers = watchers;
    watchers = MapNew(StringHash_untyped, StringEqual_untyped, NULL, WatcherDestroy);

    ThreadUnlock(&watchers_mutex);
}

void EventWatcherResume(void)
{
    ThreadLock(&watchers_mutex);
    /* The watchers of the previous policy not taken over */
    MapDestroy(old_watchers);
    old_watchers = NULL;
    paused = false;
    ThreadUnlock(&watchers_mutex);

    EventWatcherWakeUp();
}

/* Writes which method each watcher watches with to REACTOR_WATCHERS_REPORT_FILE,
 * for Enterprise reporting to show which hosts get real-time detection. */
static void WriteWatchersReport(void)
{
    JsonElement *report = JsonArrayCreate(8);

    ThreadLock(&watchers_mutex);
    MapIterator it = MapIteratorInit(watchers);
    MapKeyValue *item;
    while ((item = MapIteratorNext(&it)) != NULL)
    {
        const Watcher *w = item->value;
        JsonElement *entry = JsonObjectCreate(4);
        JsonObjectAppendString(entry, "promiser", w->promise->promiser);
        JsonObjectAppendString(entry, "bundle", PromiseGetBundle(w->promise)->name);
        JsonObjectAppendString(entry, "method", w->method);
        JsonObjectAppendInteger(entry, "interval", (int) w->poll_interval_secs);
        JsonArrayAppendObject(report, entry);
    }
    ThreadUnlock(&watchers_mutex);

    /* Written aside and renamed over, so that a reader never sees half of it */
    char path[PATH_MAX];
    NDEBUG_UNUSED int ret = snprintf(path, sizeof(path), "%s%c%s",
                                     GetStateDir(), FILE_SEPARATOR, REACTOR_WATCHERS_REPORT_FILE);
    assert(ret >= 0 && ret < (int) sizeof(path));
    char tmp_path[PATH_MAX];
    ret = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    assert(ret >= 0 && ret < (int) sizeof(tmp_path));

    FILE *file = safe_fopen(tmp_path, "w");
    if (file == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Failed to write the reactor watchers report '%s': %s",
            tmp_path, GetErrorStr());
    }
    else
    {
        Writer *writer = FileWriter(file);
        JsonWrite(writer, report, 0);
        WriterClose(writer);
        if (rename(tmp_path, path) != 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to rename '%s' to '%s': %s",
                tmp_path, path, GetErrorStr());
        }
    }
    JsonDestroy(report);
}

void EventWatcherWakeUp(void)
{
    /* Called once the watchers of the (re-)read policy are registered */
    ThreadLock(&watchers_mutex);
    const size_t count = MapSize(watchers);
    ThreadUnlock(&watchers_mutex);
    WriteWatchersReport();
    Log(LOG_LEVEL_VERBOSE, "cf-reactor event watcher has %zu watcher(s)", count);

    if (watcher_thread != NULL)
    {
        StoppableThreadWake(watcher_thread);
    }
}

void EventWatcherFinalize(void)
{
    /* Before stopping the watcher thread, which they wake up */
    WatchBackendsFinalize();

    bool joined = StoppableThreadStop(watcher_thread, WATCHER_THREAD_EXIT_TIMEOUT_SECS);
    watcher_thread = NULL;
    if (!joined)
    {
        /* The thread may still be running and using the event queue, the
         * wakeup channel and the watcher registry, so leak them rather than
         * risk a use-after-free. We're shutting down anyway. */
        return;
    }

    ThreadedQueueDestroy(event_queue);
    event_queue = NULL;
    WakeupChannelClose(&wakeup_channel);

    WatcherRegistryFinalize();
}

void WatcherMarkDirty(uint64_t watcher_id)
{
    ThreadLock(&dirty_mutex);
    if (!IdIsDirty(dirty_ids, num_dirty_ids, watcher_id))
    {
        if (num_dirty_ids == max_dirty_ids)
        {
            max_dirty_ids = (max_dirty_ids == 0) ? 16 : max_dirty_ids * 2;
            dirty_ids = xrealloc(dirty_ids, max_dirty_ids * sizeof(uint64_t));
        }
        dirty_ids[num_dirty_ids++] = watcher_id;
    }
    ThreadUnlock(&dirty_mutex);

    if (watcher_thread != NULL)
    {
        StoppableThreadWake(watcher_thread);
    }
}

void WatcherMarkAllDirty(void)
{
    ThreadLock(&dirty_mutex);
    all_dirty = true;
    ThreadUnlock(&dirty_mutex);

    if (watcher_thread != NULL)
    {
        StoppableThreadWake(watcher_thread);
    }
}
