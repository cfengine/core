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

/* Upper bound on how long the watcher thread ever sleeps in one go, so that
 * IsPendingTermination() is re-checked at least this often during shutdown,
 * regardless of what poll intervals watchers asked for. */
#define MAX_WATCHER_THREAD_SLEEP_SECS 1

#define WATCHER_THREAD_EXIT_TIMEOUT_SECS 5

typedef struct
{
    WatcherCheckFn check_callback;
    WatcherStateDestroyFn destroy_state;
    void *state;
    const Promise *promise; // the unexpanded events promise, owned by the policy
    char *promise_hash; // hash of the expanded events promise, see WatcherPromiseHash()
    char *promiser; // expanded promiser of the iteration of `promise`
    time_t poll_interval_secs;
    time_t next_due;
} Watcher;

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

/* Watchers that fired, queued by the watcher thread for the main thread to
 * handle. Not owned: the watcher thread only queues watchers of the registry
 * while holding watchers_mutex, and EventWatcherPause() empties the queue
 * before setting the watchers aside, so the queued watchers are always
 * registered. */
static ThreadedQueue *event_queue = NULL;
static StoppableThread *watcher_thread = NULL;

/*****************************************************************************/

/* The hash covers the whole expanded promise, including what is watched
 * (the constraints of the 'when' body) and what is run ('then'), so that a
 * state is only taken over by a watcher of the same type, on the same
 * thing. */
void WatcherPromiseHash(const Promise *pp, char *hash, size_t hash_size)
{
    assert(pp != NULL);
    assert(hash != NULL);

    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    PromiseRuntimeHash(pp, "events_promise_hash", digest, CF_DEFAULT_DIGEST);
    HashPrintSafe(hash, hash_size, digest, CF_DEFAULT_DIGEST, true);
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
void WatcherRegister(const Promise *pp, WatcherOptions opt, time_t interval)
{
    assert(pp != NULL);
    assert(pp->promiser != NULL);
    assert(opt.check_callback != NULL);
    assert(opt.destroy_state != NULL);
    assert(opt.state != NULL);
    assert(watchers != NULL);
    assert(interval > 0);
    
    ThreadLock(&watchers_mutex);
    char promise_hash[CF_HOSTKEY_STRING_SIZE];
    WatcherPromiseHash(pp, promise_hash, sizeof(promise_hash));

    Watcher *prev = MapGet(watchers, promise_hash);
    if (prev != NULL)
    {
        // ignore duplicates
        ThreadUnlock(&watchers_mutex);
        opt.destroy_state(opt.state);
        return;
    }

    Watcher *w = xmalloc(sizeof(Watcher));
    w->promise_hash = xstrdup(promise_hash);
    w->promise = pp->org_pp;
    w->promiser = xstrdup(pp->promiser);
    w->poll_interval_secs = interval;
    w->next_due = 0; /* due immediately on the watcher thread's first pass */
    w->check_callback = opt.check_callback;
    w->destroy_state = opt.destroy_state;

    // Take over the state of an identical watcher
    prev = (old_watchers != NULL) ? MapGet(old_watchers, promise_hash) : NULL;
    if (prev == NULL)
    {
        w->state = opt.state;
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "Reactor watcher '%s' takes over the state of an identical watcher", w->promiser);
        w->state = prev->state;
        prev->state = NULL; /* so not destroyed with prev */
        opt.destroy_state(opt.state);
    }

    MapInsert(watchers, w->promise_hash, w);

    ThreadUnlock(&watchers_mutex);
}

static void WatcherDestroy(void *item)
{
    Watcher *w = item;
    // w->state is NULL when it was borrowed by a new watcher
    if (w->destroy_state != NULL && w->state != NULL)
    {
        w->destroy_state(w->state);
    }
    free(w->promise_hash);
    free(w->promiser);
    free(w);
}

static void WatcherThreadMain(StoppableThread *thread, ARG_UNUSED void *unused)
{
    /* Keep termination signals landing on the main thread (which owns the
     * daemon's HandleSignalsForDaemon()-based shutdown), never on this one.
     * No-op on Windows -- see signal_lib.h. */
#ifndef __MINGW32__
    MaskTerminationSignalsInThread();
#endif

    while (!IsPendingTermination() && !StoppableThreadShouldStop(thread))
    {
        time_t now = time(NULL);
        time_t sleep_for = MAX_WATCHER_THREAD_SLEEP_SECS;
        bool any_event = false;

        ThreadLock(&watchers_mutex);

        MapIterator it = MapIteratorInit(watchers);
        MapKeyValue *item;
        while (!paused && (item = MapIteratorNext(&it)) != NULL)
        {
            Watcher *w = item->value;

            // check_callback cannot be null if it is correctly assigned during policy evaluation
            assert(w->check_callback != NULL);

            if (now >= w->next_due)
            {
                bool fired = w->check_callback(w->state);
                w->next_due = now + w->poll_interval_secs;
                if (fired)
                {
                    ThreadedQueuePush(event_queue, w);
                    any_event = true;
                }
            }

            time_t until_due = (w->next_due > now) ? (w->next_due - now) : 0;
            sleep_for = MIN(sleep_for, until_due);
        }

        ThreadUnlock(&watchers_mutex);

        if (any_event)
        {
            WakeupChannelNotify(&wakeup_channel);
        }

        StoppableThreadSleep(thread, sleep_for);
    }
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

    event_queue = ThreadedQueueNew(16, NULL);

    watcher_thread = StoppableThreadStart(WatcherThreadMain, NULL);
    if (watcher_thread == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to start cf-reactor watcher thread");
        ThreadedQueueDestroy(event_queue);
        event_queue = NULL;
        WakeupChannelClose(&wakeup_channel);
        return false;
    }

    *fd = WakeupChannelReadFd(&wakeup_channel);

    Log(LOG_LEVEL_VERBOSE, "Started cf-reactor event watcher with %zu watcher(s)", MapSize(watchers));
    return true;
}

/* Watchers are only destroyed by the main thread (EventWatcherResume()),
 * which is also the one handling events, and never while events are queued,
 * so the queued watchers can be used without holding watchers_mutex. Their
 * promiser and promise are never modified by the watcher thread. */
static void HandleQueuedEvents(EvalContext *ctx, WatcherEventFn on_event)
{
    assert(on_event != NULL);

    void *item;
    while (ThreadedQueuePop(event_queue, &item, 0))
    {
        const Watcher *w = item;
        Log(LOG_LEVEL_NOTICE, "Reactor watcher '%s' fired", w->promiser);
        on_event(ctx, w->promise, w->promise_hash);
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
}

void EventWatcherFinalize(void)
{
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
