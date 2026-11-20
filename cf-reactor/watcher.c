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
#include <mod_reactor.h>        // REACTOR_WAKEUP_SOURCES
#include <watch_backends.h>     // WatchBackendsAdd(), WatchBackendsRemove()
#include <json.h>
#include <sequence.h>             // SeqSort()
#include <watcher_report.h>    // WatchersReportWrite()
#include <watch_location.h>    // WatchTargetIsRemote(), WatchLocationForgetMountTable()

/* Longest a lane sleeps before its watchers are due, as a safety net against
 * a lost wakeup or a clock step. With no watchers it sleeps until woken up.
 * The intervals are on MonotonicClockNow()'s clock, CLOCK_MONOTONIC where
 * available, which on Linux stops while the system is suspended: after a
 * resume, a watcher is due up to its interval later than by the wall clock
 * (the wakeup sources still wake it up as soon as something changes). */
#define MAX_WATCHER_THREAD_SLEEP_SECS 30

#define WATCHER_THREAD_EXIT_TIMEOUT_SECS 5

/* Wait after a wakeup source marked watchers dirty, before checking them. One
 * operation is often several changes (a file created, then its times set), and
 * checking in between would see states polling never sees. */
#define WATCH_SETTLE_MSECS 200

/* A watcher whose check failed (e.g. its file locked on Windows) is retried
 * after this long, not after its interval: until a check succeeds it has no
 * reference, so a change before that would become the reference, not fire. */
#define CHECK_ERROR_RETRY_SECS 1

/* Limit on how often a watcher runs its 'then' bundles: wakeup sources wake a
 * watcher on every change, so anyone able to touch a watched file could make
 * the bundles run continuously. Beyond MAX_TRIGGERS_PER_WINDOW runs in
 * TRIGGER_WINDOW_SECS the watcher waits before its next run, twice as long
 * each time (up to MAX_TRIGGER_BACKOFF_SECS), until a window stays below the
 * limit. Events meanwhile are coalesced into the next run. */
#define TRIGGER_WINDOW_SECS 60
#define MAX_TRIGGERS_PER_WINDOW 10
#define MIN_TRIGGER_BACKOFF_SECS 10
#define MAX_TRIGGER_BACKOFF_SECS 300

/* State of the limit on how often a watcher runs its 'then' bundles, see
 * ThrottleMayRun() */
typedef struct
{
    time_t window_start;
    unsigned int triggers_in_window;
    time_t backoff_secs;
    time_t throttled_until;    // 0 when not throttled
} Throttle;

static void ThrottleInit(Throttle *t, time_t now)
{
    assert(t != NULL);

    memset(t, 0, sizeof(*t));
    t->window_start = now;
}

/** Seconds left of the delay after the limit was hit, 0 if not delayed */
static time_t ThrottleDelayLeft(const Throttle *t, time_t now)
{
    assert(t != NULL);

    return (t->throttled_until > now) ? (t->throttled_until - now) : 0;
}

/** Whether the watcher may run its bundles now, see TRIGGER_WINDOW_SECS.
 * Counts the run if so. `promiser` is for the log. */
static bool ThrottleMayRun(Throttle *t, const char *promiser, time_t now)
{
    assert(t != NULL);
    assert(promiser != NULL);

    if (now - t->window_start >= TRIGGER_WINDOW_SECS)
    {
        if (t->triggers_in_window < MAX_TRIGGERS_PER_WINDOW)
        {
            /* A calm window ends the backoff */
            t->backoff_secs = 0;
        }
        t->window_start = now;
        t->triggers_in_window = 0;
    }

    if (t->throttled_until > now)
    {
        return false;
    }
    if (t->throttled_until != 0)
    {
        /* The delay is over: run once for the events held meanwhile */
        t->throttled_until = 0;
        t->triggers_in_window++;
        return true;
    }

    if (t->triggers_in_window >= MAX_TRIGGERS_PER_WINDOW)
    {
        t->backoff_secs = (t->backoff_secs == 0) ?
            MIN_TRIGGER_BACKOFF_SECS : MIN(t->backoff_secs * 2, MAX_TRIGGER_BACKOFF_SECS);
        t->throttled_until = now + t->backoff_secs;
        Log(LOG_LEVEL_WARNING,
            "Reactor watcher '%s' fired more than %d times in %d seconds, delaying its next run by %jd seconds",
            promiser, MAX_TRIGGERS_PER_WINDOW, TRIGGER_WINDOW_SECS,
            (intmax_t) t->backoff_secs);
        return false;
    }

    t->triggers_in_window++;
    return true;
}

typedef struct WatcherLane_ WatcherLane;

/* What the identical watchers (same promise hash) of successive policies
 * share, so that re-reading the policy keeps what the watcher observed. See
 * the invariants above watchers_mutex. */
typedef struct
{
    char *hash;                 // promise hash, see WatcherPromiseHash()
    WatcherLane *lane;          // the lane checking it, never changes
    WatcherCheckFn check_callback;
    WatcherStateDestroyFn destroy_state;
    WatcherBundlesRanFn bundles_ran_callback; // NULL if none
    void *state;
    time_t interval;            // seconds between two checks
    unsigned int refs;          // watchers using it, plus one while its lane checks it
    time_t next_due;            // on MonotonicClockNow()'s clock
    WatcherCheckResult last_check;

    /* An event was queued and its bundles haven't finished running. Events
     * observed meanwhile are held, see EventHandled(). */
    bool event_pending;

    /* Its bundles ran since its lane last checked it, see WatcherBundlesRanFn */
    bool bundles_ran;

    /* An observed event not queued yet: one is pending, the trigger limit
     * delays it, or no registered watcher uses the slot (paused, or the
     * re-read policy dropped the watcher while its check ran) */
    bool event_held;

    /* The wakeup source whose wakeup led to the check that observed the held
     * event, NULL if it was the interval, for the log */
    const char *event_woken_by;

    /* To check as soon as it is of use, whether due or not: a check was
     * skipped while an event was held, or its wakeups were dropped while
     * paused. See WatcherWantsCheck(). */
    bool recheck;
    Throttle throttle;
} StateSlot;

typedef struct
{
    StateSlot *slot;      // holds a reference to it
    Promise *promise;     // copy of the expanded events promise, see PromiseCopy()
    uint64_t id;          // passed to the wakeup sources, see LaneOfId()
    const char *method;   // wakeup source used, static, see WatchBackendsAdd()
} Watcher;

/* Item of the event queue */
typedef struct
{
    Watcher *watcher;
    WatcherCheckResult check; // see WatcherEventFn
    const char *woken_by;     // copy of StateSlot.event_woken_by
} WatcherEvent;

static void WatcherDestroy(void *item);

/* Guards `watchers`, `old_watchers`, `paused` and the state slots, except
 * the `state` of a pinned slot. The invariants:
 *
 * - Only the main thread creates and frees watchers (WatcherRegister(),
 *   EventWatcherPause(), EventWatcherResume(), finalizing), so only it calls
 *   the wakeup sources' add() and remove(). `watchers` maps the promise hash
 *   to the registered watcher, `old_watchers` holds those set aside.
 * - A watcher holds a reference to a state slot. An identical watcher of the
 *   re-read policy takes a reference to the same slot, whatever the slot is
 *   doing, so the state, a held event and the trigger limit carry over. Not
 *   if it is on the other lane (the mount table changed, so the path is on
 *   another file system): it gets a fresh slot, as the old state describes
 *   the other file system, and it mustn't wait for a check stuck on it. So
 *   every watcher of a slot is on the slot's lane.
 * - Only its lane checks a slot. It takes a reference to it under the lock
 *   (pins it), runs the check without the lock, as checks may block (an
 *   unresponsive NFS server, a big file to hash), then applies the result
 *   and drops the reference under the lock. While pinned, the `state`
 *   belongs to the check alone.
 * - A slot is freed by whoever drops its last reference: the main thread
 *   freeing a watcher, or the lane after its check. Never while pinned.
 * - Events are queued for the registered watcher of the slot
 *   (MapGet(watchers, hash)), and never while paused. Otherwise the slot
 *   holds the event, for a watcher registered later to queue.
 * - Lock order: watchers_mutex, then the wakeup sources' locks, then
 *   dirty_mutex, then the lanes' StoppableThread locks. */
static pthread_mutex_t watchers_mutex = PTHREAD_MUTEX_INITIALIZER;
static Map *watchers = NULL;
static Map *old_watchers = NULL;
static bool paused = false;         /* see EventWatcherPause() */

static WakeupChannel wakeup_channel = { .fds = { -1, -1 } };

/* Events (WatcherEvent) queued by the lanes for the main thread. The
 * watchers aren't owned by the events: they are queued while holding
 * watchers_mutex, and EventWatcherPause() empties the queue before setting
 * the watchers aside, so the watchers of queued events are always registered. */
static ThreadedQueue *event_queue = NULL;

/* Set of watcher ids: open addressing, linear probing, 0 is an empty cell
 * (ids are never 0). Add and lookup are O(1), so a burst of marks is linear. */
typedef struct
{
    uint64_t *cells;
    size_t capacity;    // 0 or a power of two
    size_t count;
} IdSet;

static size_t IdSetHome(uint64_t id, size_t capacity)
{
    return (size_t) ((id * UINT64_C(0x9E3779B97F4A7C15)) >> 32) & (capacity - 1);
}

static bool IdSetContains(const IdSet *set, uint64_t id)
{
    assert(set != NULL);

    if (set->count == 0)
    {
        return false;
    }
    for (size_t i = IdSetHome(id, set->capacity); set->cells[i] != 0;
         i = (i + 1) & (set->capacity - 1))
    {
        if (set->cells[i] == id)
        {
            return true;
        }
    }
    return false;
}

static void IdSetAdd(IdSet *set, uint64_t id)
{
    assert(set != NULL);
    assert(id != 0);

    if (IdSetContains(set, id))
    {
        return;
    }
    if ((set->count + 1) * 2 > set->capacity)
    {
        const IdSet old = *set;
        set->capacity = (old.capacity == 0) ? 16 : old.capacity * 2;
        set->cells = xcalloc(set->capacity, sizeof(uint64_t));
        for (size_t i = 0; i < old.capacity; i++)
        {
            if (old.cells[i] != 0)
            {
                size_t j = IdSetHome(old.cells[i], set->capacity);
                while (set->cells[j] != 0)
                {
                    j = (j + 1) & (set->capacity - 1);
                }
                set->cells[j] = old.cells[i];
            }
        }
        free(old.cells);
    }
    size_t i = IdSetHome(id, set->capacity);
    while (set->cells[i] != 0)
    {
        i = (i + 1) & (set->capacity - 1);
    }
    set->cells[i] = id;
    set->count++;
}

static void IdSetDestroy(IdSet *set)
{
    assert(set != NULL);

    free(set->cells);
    *set = (IdSet) { 0 };
}

/** Keeps the capacity, to not reallocate on every burst */
static void IdSetClear(IdSet *set)
{
    assert(set != NULL);

    if (set->count > 0)
    {
        memset(set->cells, 0, set->capacity * sizeof(uint64_t));
        set->count = 0;
    }
}

/* A slot pinned for a check in a pass of its lane */
typedef struct
{
    StateSlot *slot;
    bool bundles_ran;           // copy of StateSlot.bundles_ran, taken when pinned
    WatcherCheckResult check;
} PinnedCheck;

/* A thread checking its own watchers, see WatcherLaneMain() */
struct WatcherLane_
{
    const char *name;           // for the log
    StoppableThread *thread;    // NULL while not running

    /* Left behind by EventWatcherFinalize(), stuck in a check, see LaneStop() */
    StoppableThread *stuck_thread;

    /* Ids of its watchers to check right away, marked by the wakeup sources,
     * see WatcherMarkDirty() */
    IdSet dirty_ids;
    bool all_dirty;

    /* Only used by its thread, not on its stack so they stay reachable if it
     * is left behind (and cancelled), see LaneStop() */
    IdSet taken_ids;
    PinnedCheck *pinned;
    size_t pinned_capacity;
};

/* Guards the lanes' `thread`, `dirty_ids` and `all_dirty`. Only ever taken
 * after watchers_mutex, never before: the wakeup sources mark watchers while
 * holding their own lock, which is taken after watchers_mutex when
 * (un)registering. The main thread alone starts and stops the lanes, so it
 * may read `thread` without it. */
static pthread_mutex_t dirty_mutex = PTHREAD_MUTEX_INITIALIZER;

/* The watchers whose checks may block on a remote file system have their
 * own lane, started with the first of them, so that an unresponsive server
 * doesn't hold the checks of the local ones up. */
static WatcherLane local_lane = { .name = "local" };
static WatcherLane remote_lane = { .name = "remote" };

/* Watcher ids are (n << 1) | 1 on the remote lane, (n << 1) on the local
 * one, with n never reused (and never 0), so a mark is routed to its lane
 * without a lookup */
static uint64_t last_watcher_number = 0;
#define WATCHER_ID_REMOTE_BIT UINT64_C(1)

static WatcherLane *LaneOfId(uint64_t id)
{
    return ((id & WATCHER_ID_REMOTE_BIT) != 0) ? &remote_lane : &local_lane;
}

static const ReactorWatchHost WATCH_HOST =
{
    .mark_dirty = WatcherMarkDirty,
    .mark_all_dirty = WatcherMarkAllDirty,
};

/*****************************************************************************/

/* The hash covers the whole expanded promise, including what is watched
 * (the constraints of the 'when' body) and what is run ('then'), so that a
 * state is only taken over by a watcher of the same type, on the same
 * thing. It leaves out 'wakeup_sources', which only sets how the event is
 * watched for: changing it must not reset the state, or an event happening
 * while the policy is re-read would be missed. */
void WatcherPromiseHash(const Promise *pp, char *hash, size_t hash_size)
{
    assert(pp != NULL);
    assert(hash != NULL);

    const size_t length = (pp->conlist == NULL) ? 0 : SeqLength(pp->conlist);
    Seq *conlist = SeqNew(length, NULL);
    for (size_t i = 0; i < length; i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);
        if (!StringEqual(cp->lval, REACTOR_WAKEUP_SOURCES))
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
    paused = false; /* if finalized while paused */
}

void WatcherRegistryFinalize(void)
{
    MapDestroy(watchers);
    watchers = NULL;
    MapDestroy(old_watchers);
    old_watchers = NULL;
}

static void WatcherLaneMain(StoppableThread *thread, void *arg);

/** Wakes the lane up to check its watchers, holding dirty_mutex */
static void LaneWakeLocked(WatcherLane *lane)
{
    assert(lane != NULL);

    if (lane->thread != NULL)
    {
        StoppableThreadWake(lane->thread);
    }
}

/** Wakes the lane up to check its watchers. Thread-safe. */
static void LaneWake(WatcherLane *lane)
{
    /* Under the lock, as EventWatcherFinalize() may be stopping the thread */
    ThreadLock(&dirty_mutex);
    LaneWakeLocked(lane);
    ThreadUnlock(&dirty_mutex);
}

/** Starts the lane's thread unless running. Called by the main thread. */
static bool LaneStart(WatcherLane *lane)
{
    assert(lane != NULL);

    if (lane->thread != NULL)
    {
        return true;
    }
    StoppableThread *thread = StoppableThreadStart(WatcherLaneMain, lane);
    if (thread == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to start the cf-reactor watcher thread of the %s watchers",
            lane->name);
        return false;
    }
    ThreadLock(&dirty_mutex);
    lane->thread = thread;
    ThreadUnlock(&dirty_mutex);
    Log(LOG_LEVEL_VERBOSE, "Started the cf-reactor watcher thread of the %s watchers", lane->name);
    return true;
}

/** Stops the lane's thread, if running. Returns false if it didn't exit in
 * time, and may still use the registry. Called by the main thread. */
static bool LaneStop(WatcherLane *lane)
{
    assert(lane != NULL);

    ThreadLock(&dirty_mutex);
    StoppableThread *thread = lane->thread;
    lane->thread = NULL;
    ThreadUnlock(&dirty_mutex);

    if (thread == NULL)
    {
        return true;
    }
    if (!StoppableThreadStop(thread, WATCHER_THREAD_EXIT_TIMEOUT_SECS))
    {
        Log(LOG_LEVEL_ERR,
            "The cf-reactor watcher thread of the %s watchers is stuck in a check, leaving it behind",
            lane->name);
        /* Its handle is leaked on purpose, see StoppableThreadStop(): kept
         * reachable, so a leak checker doesn't report it */
        lane->stuck_thread = thread;
        return false;
    }
    return true;
}

static StateSlot *SlotNew(const char *hash, WatcherLane *lane, const WatcherOptions *opt)
{
    assert(hash != NULL);
    assert(lane != NULL);
    assert(opt != NULL);

    StateSlot *slot = xcalloc(1, sizeof(StateSlot));
    slot->hash = xstrdup(hash);
    slot->lane = lane;
    slot->check_callback = opt->check_callback;
    slot->destroy_state = opt->destroy_state;
    slot->bundles_ran_callback = opt->bundles_ran;
    slot->state = opt->state;
    slot->interval = opt->interval;
    slot->refs = 1;
    slot->next_due = 0; /* due immediately on its lane's first pass */
    slot->last_check = WATCHER_CHECK_NO_EVENT;
    ThrottleInit(&slot->throttle, MonotonicClockNow());
    return slot;
}

/** Drops a reference to the slot, freeing it with its state if it was the
 * last. Holding watchers_mutex (or with no lanes). */
static void SlotRelease(StateSlot *slot)
{
    assert(slot != NULL);
    assert(slot->refs > 0);

    slot->refs--;
    if (slot->refs == 0)
    {
        slot->destroy_state(slot->state);
        free(slot->hash);
        free(slot);
    }
}

/** The lane of a new watcher: the remote one if `remote`, started with the
 * first remote watcher once the lanes run (else by EventWatcherInitialize()),
 * falling back to the local one. Called by the main thread. */
static WatcherLane *LaneForNewWatcher(bool remote)
{
    if (remote && (local_lane.thread == NULL || LaneStart(&remote_lane)))
    {
        return &remote_lane;
    }
    return &local_lane;
}

// Expects interval to be strictly greater than 0, otherwise its lane will busy spin
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

    /* Only the main thread changes the maps, so they don't change until the
     * watcher is inserted below */
    /* From the path and the mount table alone, as anything touching the file
     * system of the path could block on it. For a duplicate, the lane is the
     * one already started for its identical watcher. */
    const bool remote = WatchTargetIsRemote(&opt->target);
    WatcherLane *lane = LaneForNewWatcher(remote);

    /* The slot of an identical watcher of the previous policy, whatever it
     * is doing, see the invariants above watchers_mutex */
    StateSlot *slot = NULL;
    ThreadLock(&watchers_mutex);
    const bool duplicate = MapHasKey(watchers, promise_hash);
    const Watcher *prev = (old_watchers == NULL) ? NULL : MapGet(old_watchers, promise_hash);
    if (!duplicate && prev != NULL && prev->slot->lane == lane)
    {
        slot = prev->slot;
        slot->refs++;
        /* Its wakeups were dropped while paused */
        slot->recheck = true;
    }
    ThreadUnlock(&watchers_mutex);

    if (duplicate)
    {
        opt->destroy_state(opt->state);
        return true;
    }

    const uint64_t id = (++last_watcher_number << 1) |
        ((lane == &remote_lane) ? WATCHER_ID_REMOTE_BIT : 0);

    if (slot != NULL)
    {
        Log(LOG_LEVEL_DEBUG, "Reactor watcher '%s' keeps the state of the identical watcher it replaces",
            pp->promiser);
        opt->destroy_state(opt->state);
    }
    else
    {
        if (!remote)
        {
            /* Without holding watchers_mutex, as the check may block (e.g. on
             * a big file to hash). Not registered yet, so its lane can't check
             * it concurrently. If it fails, the watcher's first successful
             * check becomes the reference instead. That is how a remote
             * watcher gets its reference, on its lane: here, an unresponsive
             * server would block the policy (re-)read. A change before that
             * first check is missed. */
            opt->check_callback(opt->state);
        }
        slot = SlotNew(promise_hash, lane, opt);
    }

    /* Before the watcher is in the registry: a wakeup from the method in
     * between is for an unknown id, and ignored, but the watcher is checked on
     * its lane's first pass anyway. */
    const char *method = WatchBackendsAdd(id, &opt->target, remote, opt->methods, pp->promiser);
    if (method == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Reactor events promise '%s': none of its wakeup sources is available on this host",
            pp->promiser);
        ThreadLock(&watchers_mutex);
        SlotRelease(slot);
        ThreadUnlock(&watchers_mutex);
        return false;
    }
    Log(LOG_LEVEL_VERBOSE, "Reactor watcher '%s' uses wakeup source '%s'%s", pp->promiser, method,
        (lane == &remote_lane) ? ", on a remote file system" : "");

    Watcher *w = xcalloc(1, sizeof(Watcher));
    w->slot = slot;
    w->id = id;
    w->method = method;
    w->promise = PromiseCopy(pp);

    ThreadLock(&watchers_mutex);
    MapInsert(watchers, slot->hash, w);
    ThreadUnlock(&watchers_mutex);

    return true;
}

/* Destroy function of the registry's maps. Called by the main thread,
 * holding watchers_mutex (or with no lanes). */
static void WatcherDestroy(void *item)
{
    Watcher *w = item;
    assert(w != NULL);

    WatchBackendsRemove(w->id, w->method);
    PromiseDestroy(w->promise);
    SlotRelease(w->slot);
    free(w);
}

/** The registered watcher using the slot, NULL if none or paused. Holding
 * watchers_mutex. */
static Watcher *SlotRegisteredWatcher(const StateSlot *slot)
{
    assert(slot != NULL);

    if (paused)
    {
        return NULL;
    }
    Watcher *w = MapGet(watchers, slot->hash);
    return (w != NULL && w->slot == slot) ? w : NULL;
}

/** Called by a lane, holding watchers_mutex */
static void QueueEvent(Watcher *w, WatcherCheckResult check)
{
    assert(w != NULL);

    WatcherEvent *event = xmalloc(sizeof(WatcherEvent));
    event->watcher = w;
    event->check = check;
    event->woken_by = (check == WATCHER_CHECK_EVENT) ? w->slot->event_woken_by : NULL;
    ThreadedQueuePush(event_queue, event);
}

/** Waits for the burst of changes that marked watchers of the lane to settle,
 * see WATCH_SETTLE_MSECS. Returns the time after the wait. */
static time_t SettleIfMarked(const WatcherLane *lane, time_t now)
{
    assert(lane != NULL);

    ThreadLock(&dirty_mutex);
    const bool marked = (lane->dirty_ids.count > 0 || lane->all_dirty);
    ThreadUnlock(&dirty_mutex);
    if (!marked)
    {
        return now;
    }

    const struct timespec settle = { 0, WATCH_SETTLE_MSECS * 1000000L };
    nanosleep(&settle, NULL);
    return MonotonicClockNow();
}

/** Takes the dirty marks of the lane into `taken`, see WatcherMarkDirty(),
 * replacing the ids taken last time. Returns whether all its watchers are
 * marked. */
static bool TakeDirtyMarks(WatcherLane *lane, IdSet *taken)
{
    assert(lane != NULL);
    assert(taken != NULL);

    ThreadLock(&dirty_mutex);
    const IdSet previous = *taken;
    *taken = lane->dirty_ids;
    lane->dirty_ids = previous;
    IdSetClear(&lane->dirty_ids);
    const bool check_all = lane->all_dirty;
    lane->all_dirty = false;
    ThreadUnlock(&dirty_mutex);
    return check_all;
}

/** Whether a check of the slot is of use now, see WatcherWantsCheck().
 * While an event is held, checking again can't change what runs next, and a
 * check may be costly (file_content_modified hashes the file). */
static bool SlotCanCheck(const StateSlot *slot, time_t now)
{
    assert(slot != NULL);

    return !slot->event_held ||
        (!slot->event_pending && ThrottleDelayLeft(&slot->throttle, now) == 0);
}

/** Whether to check the slot in this pass: it is due (or its watcher was woken
 * up by its wakeup source) and the check is of use, see below. Called by its
 * lane, holding watchers_mutex. */
static bool WatcherWantsCheck(StateSlot *slot, time_t now, bool woken)
{
    assert(slot != NULL);

    const bool due = (now >= slot->next_due || woken);

    /* Skip the check while an event is held and check once when it can run,
     * so a busy file is checked about as often as the bundles run. */
    const bool can_run = SlotCanCheck(slot, now);
    if (due && !can_run)
    {
        slot->recheck = true;
        slot->next_due = now + slot->interval;
        return false;
    }
    return can_run && (due || slot->recheck);
}

/** Applies the result of a check of the slot, which ran without holding
 * watchers_mutex. An event is held, to be queued for the slot's registered
 * watcher `w` (see WatcherQueueHeldEvent()), and an error queued for it right
 * away, unless `w` is NULL: then it is reported by a later check. Returns
 * whether an error was queued. Called by its lane, holding watchers_mutex. */
static bool SlotApplyCheck(StateSlot *slot, Watcher *w, WatcherCheckResult check, time_t now)
{
    assert(slot != NULL);

    slot->next_due = now + ((check == WATCHER_CHECK_ERROR) ?
                            MIN(slot->interval, CHECK_ERROR_RETRY_SECS) :
                            slot->interval);

    if (check == WATCHER_CHECK_EVENT)
    {
        slot->event_held = true;
    }
    /* Only report a failing watcher once, not on every check,
     * until it recovers */
    else if (check == WATCHER_CHECK_ERROR && slot->last_check != WATCHER_CHECK_ERROR)
    {
        if (w == NULL)
        {
            return false;
        }
        QueueEvent(w, WATCHER_CHECK_ERROR);
        slot->last_check = check;
        return true;
    }
    slot->last_check = check;
    return false;
}

/** Queues the event held by the slot of the registered watcher if the trigger
 * limit allows. Returns whether it did. Called by its lane, holding
 * watchers_mutex. */
static bool WatcherQueueHeldEvent(Watcher *w, time_t now)
{
    assert(w != NULL);

    StateSlot *slot = w->slot;
    if (slot->event_held && !slot->event_pending &&
        ThrottleMayRun(&slot->throttle, w->promise->promiser, now))
    {
        slot->event_held = false;
        slot->event_pending = true;
        QueueEvent(w, WATCHER_CHECK_EVENT);
        return true;
    }
    return false;
}

/** How long until the slot needs its lane again. A slot waiting for its
 * event to be handled is woken by EventHandled(). */
static time_t SlotSecsUntilDue(const StateSlot *slot, time_t now)
{
    assert(slot != NULL);

    if (slot->recheck && SlotCanCheck(slot, now))
    {
        return 0;
    }

    time_t until_due = (slot->next_due > now) ? (slot->next_due - now) : 0;
    const time_t delay_left = ThrottleDelayLeft(&slot->throttle, now);
    if (slot->event_held && delay_left > 0)
    {
        until_due = MIN(until_due, delay_left);
    }
    return until_due;
}

/** Pins the slots of the lane's watchers to check in this pass, see
 * WatcherWantsCheck(), into lane->pinned. Returns how many. Holding
 * watchers_mutex. */
static size_t LanePinChecks(WatcherLane *lane, bool check_all, time_t now)
{
    assert(lane != NULL);

    size_t count = 0;
    MapIterator it = MapIteratorInit(watchers);
    MapKeyValue *item;
    while ((item = MapIteratorNext(&it)) != NULL)
    {
        Watcher *w = item->value;
        StateSlot *slot = w->slot;
        const bool woken = (check_all || IdSetContains(&lane->taken_ids, w->id));
        if (slot->lane != lane || !WatcherWantsCheck(slot, now, woken))
        {
            continue;
        }
        if (count == lane->pinned_capacity)
        {
            lane->pinned_capacity = (lane->pinned_capacity == 0) ? 16 : lane->pinned_capacity * 2;
            lane->pinned = xrealloc(lane->pinned, lane->pinned_capacity * sizeof(PinnedCheck));
        }
        slot->recheck = false;
        slot->refs++;
        lane->pinned[count].slot = slot;
        lane->pinned[count].bundles_ran = slot->bundles_ran;
        slot->bundles_ran = false;
        /* A held event keeps the wakeup that led to it */
        if (!slot->event_held)
        {
            slot->event_woken_by = woken ? w->method : NULL;
        }
        count++;
    }
    return count;
}

/** The loop of a lane: checks the lane's watchers when due or marked, and
 * queues their events. Every lane runs it, each on its own watchers only. */
static void WatcherLaneMain(StoppableThread *thread, void *arg)
{
    WatcherLane *lane = arg;
    assert(lane != NULL);

    /* Keep termination signals landing on the main thread (which owns the
     * daemon's HandleSignalsForDaemon()-based shutdown), never on this one.
     * No-op on Windows -- see signal_lib.h. */
#ifndef __MINGW32__
    MaskTerminationSignalsInThread();
#endif

    while (!IsPendingTermination() && !StoppableThreadShouldStop(thread))
    {
        time_t sleep_for = MAX_WATCHER_THREAD_SLEEP_SECS;
        bool any_event = false;
        size_t lane_watchers = 0;

        const time_t now = SettleIfMarked(lane, MonotonicClockNow());

        ThreadLock(&watchers_mutex);
        /* Marks taken while paused are dropped, see `recheck` */
        const bool check_all = TakeDirtyMarks(lane, &lane->taken_ids);
        const size_t pinned_count = paused ? 0 : LanePinChecks(lane, check_all, now);
        ThreadUnlock(&watchers_mutex);

        /* Check them, without the lock: the checks may block */
        for (size_t i = 0; i < pinned_count; i++)
        {
            StateSlot *slot = lane->pinned[i].slot;
            if (lane->pinned[i].bundles_ran && slot->bundles_ran_callback != NULL)
            {
                slot->bundles_ran_callback(slot->state);
            }
            lane->pinned[i].check = slot->check_callback(slot->state);
        }

        ThreadLock(&watchers_mutex);

        /* Apply the results and unpin. The watchers may have been replaced
         * meanwhile, by a policy re-read. */
        for (size_t i = 0; i < pinned_count; i++)
        {
            StateSlot *slot = lane->pinned[i].slot;
            any_event |= SlotApplyCheck(slot, SlotRegisteredWatcher(slot), lane->pinned[i].check,
                                        now);
            SlotRelease(slot);
        }

        /* Queue the held events */
        MapIterator it = MapIteratorInit(watchers);
        MapKeyValue *item;
        while ((item = MapIteratorNext(&it)) != NULL)
        {
            Watcher *w = item->value;
            if (w->slot->lane != lane)
            {
                continue;
            }
            lane_watchers++;
            if (!paused)
            {
                any_event |= WatcherQueueHeldEvent(w, now);
                sleep_for = MIN(sleep_for, SlotSecsUntilDue(w->slot, now));
            }
        }

        ThreadUnlock(&watchers_mutex);

        if (any_event)
        {
            WakeupChannelNotify(&wakeup_channel);
        }

        /* Wakeup sources, new watchers and shutdown wake the thread up, and
         * the time of everything else is in sleep_for. Without watchers,
         * nothing to check until EventWatcherWakeUp() or EventWatcherResume()
         * (EventWatcherPause() empties `watchers`). */
        if (lane_watchers == 0)
        {
            StoppableThreadSleepUntilWoken(thread);
        }
        else
        {
            StoppableThreadSleep(thread, sleep_for);
        }
    }
    free(lane->pinned);
    lane->pinned = NULL;
    lane->pinned_capacity = 0;
    IdSetDestroy(&lane->taken_ids);
}

/** Whether a registered watcher is on the remote lane. Called by the main
 * thread before the lanes run. */
static bool AnyRemoteWatcher(void)
{
    MapIterator it = MapIteratorInit(watchers);
    MapKeyValue *item;
    while ((item = MapIteratorNext(&it)) != NULL)
    {
        const Watcher *w = item->value;
        if (w->slot->lane == &remote_lane)
        {
            return true;
        }
    }
    return false;
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

    /* The remote lane too if remote watchers were registered before */
    if (!LaneStart(&local_lane) || (AnyRemoteWatcher() && !LaneStart(&remote_lane)))
    {
        LaneStop(&local_lane);
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

/** The event's bundles have run: the watcher may queue events again, and what
 * it observed meanwhile (event_held), including the bundles' own changes,
 * fires now. A converging bundle changes nothing the second time, so this
 * ends. Taking the state after the run as the reference would avoid that run,
 * but lose changes made by anything else meanwhile. The watcher is checked
 * right away, as a wakeup source may not report the bundles' own changes, and
 * that check is told the bundles ran (see WatcherBundlesRanFn). */
static void EventHandled(Watcher *w)
{
    assert(w != NULL);

    ThreadLock(&watchers_mutex);
    w->slot->event_pending = false;
    w->slot->bundles_ran = true;
    w->slot->recheck = true;
    ThreadUnlock(&watchers_mutex);

    LaneWake(w->slot->lane);
}

/* Watchers are only removed by the main thread (EventWatcherPause(),
 * EventWatcherResume()), which is also the one handling events, and never
 * while events are queued, so the watchers of queued events can be used
 * without holding watchers_mutex. Their promise is never modified by the
 * lanes. */
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
            if (event->woken_by != NULL)
            {
                Log(LOG_LEVEL_VERBOSE, "Reactor watcher '%s' fired, woken by its wakeup source '%s'",
                    w->promise->promiser, event->woken_by);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Reactor watcher '%s' fired, at its interval",
                    w->promise->promiser);
            }
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

    /* A pass of a lane holds watchers_mutex, so once `paused` is
     * set no further events are queued */
    ThreadLock(&watchers_mutex);
    paused = true;
    ThreadUnlock(&watchers_mutex);

    if (event_queue != NULL)
    {
        HandleQueuedEvents(ctx, on_event);
    }

    ThreadLock(&watchers_mutex);

    /* Paused, so the lanes can't have queued any event since */
    if (event_queue != NULL && !ThreadedQueueIsEmpty(event_queue))
    {
        ProgrammingError("Events of reactor watchers still queued after handling them while pausing");
    }

    /* Set the watchers aside before the policy is re-read, for the watchers
     * registered from it to share the state slots of the identical ones, see
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
    /* The watchers of the previous policy, their slots go with the last of
     * their references */
    MapDestroy(old_watchers);
    old_watchers = NULL;
    paused = false;
    ThreadUnlock(&watchers_mutex);

    EventWatcherWakeUp();
}

static int CompareReportEntries(const void *a, const void *b, ARG_UNUSED void *user_data)
{
    const JsonElement *entry_a = a;
    const JsonElement *entry_b = b;
    const int by_bundle = StringSafeCompare(JsonObjectGetAsString(entry_a, REACTOR_WATCHERS_KEY_BUNDLE),
                                 JsonObjectGetAsString(entry_b, REACTOR_WATCHERS_KEY_BUNDLE));
    if (by_bundle != 0)
    {
        return by_bundle;
    }
    return StringSafeCompare(JsonObjectGetAsString(entry_a, REACTOR_WATCHERS_KEY_PROMISER),
                  JsonObjectGetAsString(entry_b, REACTOR_WATCHERS_KEY_PROMISER));
}

/** Writes the report of the method each watcher uses, for Enterprise
 * reporting, sorted by bundle then promiser, so that the same watchers give
 * the same report (see WatchersReportWrite()) whatever the map's order */
static void WriteWatchersReport(void)
{
    Seq *entries = SeqNew(16, NULL);

    ThreadLock(&watchers_mutex);
    MapIterator it = MapIteratorInit(watchers);
    MapKeyValue *item;
    while ((item = MapIteratorNext(&it)) != NULL)
    {
        const Watcher *w = item->value;
        JsonElement *entry = JsonObjectCreate(4);
        JsonObjectAppendString(entry, REACTOR_WATCHERS_KEY_PROMISER, w->promise->promiser);
        const Bundle *bundle = PromiseGetBundle(w->promise);
        if (StringEqual(bundle->ns, NamespaceDefault()))
        {
            JsonObjectAppendString(entry, REACTOR_WATCHERS_KEY_BUNDLE, bundle->name);
        }
        else
        {
            /* So same-named bundles of different namespaces don't collide */
            char *qualified = StringFormat("%s:%s", bundle->ns, bundle->name);
            JsonObjectAppendString(entry, REACTOR_WATCHERS_KEY_BUNDLE, qualified);
            free(qualified);
        }
        JsonObjectAppendString(entry, REACTOR_WATCHERS_KEY_WAKEUP_SOURCE, w->method);
        JsonObjectAppendInteger64(entry, REACTOR_WATCHERS_KEY_INTERVAL,
                                  (int64_t) w->slot->interval);
        SeqAppend(entries, entry);
    }
    ThreadUnlock(&watchers_mutex);

    SeqSort(entries, CompareReportEntries, NULL);
    const size_t count = SeqLength(entries);
    JsonElement *report = JsonArrayCreate(count);
    for (size_t i = 0; i < count; i++)
    {
        JsonArrayAppendObject(report, SeqAt(entries, i));
    }
    SeqDestroy(entries);

    WatchersReportWrite(report);
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

    /* Until the next re-read */
    WatchLocationForgetMountTable();

    LaneWake(&local_lane);
    LaneWake(&remote_lane);
}

void EventWatcherFinalize(void)
{
    /* Clean shutdown: the watchers are gone, so is their report */
    WatchersReportRemove();

    /* Both, even if the first is stuck. The local lane first: the remote one
     * is the likely one to be stuck, on a server that doesn't respond. */
    const bool local_joined = LaneStop(&local_lane);
    const bool remote_joined = LaneStop(&remote_lane);

    /* Once the lanes are stopped, so nothing wakes them up any more. A lane
     * left behind doesn't call the methods (only the main thread does). */
    WatchBackendsFinalize();

    WatchLocationForgetMountTable();

    if (!local_joined || !remote_joined)
    {
        /* A lane may still be running and using the event queue, the wakeup
         * channel, the watcher registry and the states, so leak them rather
         * than risk a use-after-free. We're shutting down anyway. */
        return;
    }
    IdSetDestroy(&local_lane.dirty_ids);
    local_lane.all_dirty = false;
    IdSetDestroy(&remote_lane.dirty_ids);
    remote_lane.all_dirty = false;

    ThreadedQueueDestroy(event_queue);
    event_queue = NULL;
    WakeupChannelClose(&wakeup_channel);

    WatcherRegistryFinalize();
}

void WatcherMarkDirty(uint64_t watcher_id)
{
    /* Only wakes the lane of the watcher up, so a busy local file doesn't
     * make the remote lane settle and pass for nothing */
    WatcherLane *lane = LaneOfId(watcher_id);
    ThreadLock(&dirty_mutex);
    IdSetAdd(&lane->dirty_ids, watcher_id);
    LaneWakeLocked(lane);
    ThreadUnlock(&dirty_mutex);
}

void WatcherMarkAllDirty(void)
{
    WatcherLane *const lanes[] = { &local_lane, &remote_lane };
    ThreadLock(&dirty_mutex);
    for (size_t i = 0; i < sizeof(lanes) / sizeof(lanes[0]); i++)
    {
        lanes[i]->all_dirty = true;
        LaneWakeLocked(lanes[i]);
    }
    ThreadUnlock(&dirty_mutex);
}
