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
#include <string_lib.h>         // StringEqual()
#include <sequence.h>
#include <threaded_queue.h>
#include <file_watcher.h>
#include <mutex.h>              // ThreadLock(), ThreadUnlock()
#include <map.h>

/* Upper bound on how long the watcher thread ever sleeps in one go, so that
 * IsPendingTermination() is re-checked at least this often during shutdown,
 * regardless of what poll intervals watchers asked for. */
#define MAX_WATCHER_THREAD_SLEEP_SECS 1

#define WATCHER_THREAD_EXIT_TIMEOUT_SECS 5

typedef struct
{
    char *promiser; // expanded promiser of the iteration of `promise`
    char *key; // identifies the watcher across policy reloads, see WatcherRegister()
    WatcherCheckFn check_callback; // resolved from `type` at WatcherRegister() time
    WatcherStateDestroyFn destroy_state;
    void *state;
    const Promise *promise; // the unexpanded events promise, owned by the policy
    time_t poll_interval_secs;
    time_t next_due;
} Watcher;

static void WatcherDestroy(void *item);

/* Guards `watchers` and `paused` against the watcher thread
 * (WatcherThreadMain()) iterating over `watchers` concurrently with the main
 * thread re-registering watchers from a freshly (re-)read policy
 * (WatcherRegister(), WatcherRegistryClear()). */
static pthread_mutex_t watchers_mutex = PTHREAD_MUTEX_INITIALIZER;
static Seq *watchers = NULL;
static bool paused = false;         /* see EventWatcherPause() */

/* Watchers discarded by WatcherRegistryClear(), kept until
 * EventWatcherResume() so that the watchers registered again take over their
 * state. Maps a watcher key to the Seq of discarded watchers with that key
 * (identical events promises have the same key). Only used by the main
 * thread. */
static Map *discarded_watchers = NULL;

static WakeupChannel wakeup_channel = { .fds = { -1, -1 } };

/* Watchers that fired, queued by the watcher thread for the main thread to
 * handle. Not owned: the watcher thread only queues watchers of the registry
 * while holding watchers_mutex, and WatcherRegistryClear() requires the queue
 * to be empty, so the queued watchers are always registered. */
static ThreadedQueue *event_queue = NULL;
static StoppableThread *watcher_thread = NULL;

/*****************************************************************************/

void WatcherRegistryInitialize(void)
{
    assert(watchers == NULL);

    watchers = SeqNew(4, WatcherDestroy);
}

static void WatcherSeqDestroy(void *seq)
{
    SeqDestroy(seq);
}

static void DiscardedWatchersDestroy(void)
{
    MapDestroy(discarded_watchers);
    discarded_watchers = NULL;
}

void WatcherRegistryFinalize(void)
{
    SeqDestroy(watchers);
    watchers = NULL;
    DiscardedWatchersDestroy();
}

void WatcherRegistryClear(void)
{
    assert(watchers != NULL);

    ThreadLock(&watchers_mutex);

    if (event_queue != NULL && !ThreadedQueueIsEmpty(event_queue))
    {
        ProgrammingError("Clearing the reactor watcher registry with events of its "
                         "watchers still queued, call EventWatcherPause() first");
    }

    /* Only the watchers of the last policy can be registered again */
    DiscardedWatchersDestroy();
    discarded_watchers = MapNew(StringHash_untyped, StringEqual_untyped, free, WatcherSeqDestroy);

    for (size_t i = 0; i < SeqLength(watchers); i++)
    {
        Watcher *w = SeqAt(watchers, i);
        Seq *same_key = MapGet(discarded_watchers, w->key);
        if (same_key == NULL)
        {
            same_key = SeqNew(1, WatcherDestroy);
            MapInsert(discarded_watchers, xstrdup(w->key), same_key);
        }
        SeqAppend(same_key, w);
    }

    /* The watchers are now owned by discarded_watchers */
    SeqSoftDestroy(watchers);
    watchers = SeqNew(4, WatcherDestroy);

    ThreadUnlock(&watchers_mutex);
}

/* Takes over the state of a discarded watcher with the same key, if any, so
 * that the events that happened since it was discarded are detected. Returns
 * the state to use, destroying the other. */
static void *TakeOverDiscardedState(const char *key, void *state, WatcherStateDestroyFn destroy_state)
{
    if (discarded_watchers == NULL)
    {
        return state;
    }

    Seq *same_key = MapGet(discarded_watchers, key);
    if (same_key == NULL || SeqLength(same_key) == 0)
    {
        return state;
    }

    Log(LOG_LEVEL_DEBUG, "Reactor watcher '%s' keeps its state from the previous policy", key);
    Watcher *discarded = SeqAt(same_key, 0);
    void *kept_state = discarded->state;
    discarded->state = NULL;
    SeqRemove(same_key, 0);
    if (destroy_state != NULL)
    {
        destroy_state(state);
    }
    return kept_state;
}

/* A watcher is identified by its events promise and the expanded promiser
 * of the iteration: the iterations of a promise share the same (unexpanded)
 * promise, and different promises can have the same promiser. */
static const Watcher *FindWatcher(const Promise *pp, const char *promiser)
{
    for (size_t i = 0; i < SeqLength(watchers); i++)
    {
        const Watcher *w = SeqAt(watchers, i);
        if (w->promise == pp && StringEqual(w->promiser, promiser))
        {
            return w;
        }
    }
    return NULL;
}

// Expects interval to be strictly greater than 0, otherwise the watcher thread will busy spin
bool WatcherRegister(const char *promiser, const char *key, EventType type, void *state,
                     const Promise *pp, time_t interval)
{
    assert(promiser != NULL);
    assert(key != NULL);
    assert(pp != NULL);
    assert(watchers != NULL);
    assert(interval > 0);

    WatcherCheckFn check_callback = NULL;
    WatcherStateDestroyFn destroy_state = NULL;
    switch (type)
    {
    case EVENT_FILE_DELETED:
        check_callback = CheckFileDeleted;
        destroy_state = DestroyFileWatcherState;
        break;

    // TODO: add more event types

    default:
        ProgrammingError("Unknown reactor event type %d for watcher '%s'", (int) type, promiser);
    }

    ThreadLock(&watchers_mutex);

    if (FindWatcher(pp, promiser) != NULL)
    {
        Log(LOG_LEVEL_ERR, "Reactor watcher '%s' is already registered, ignoring the duplicate", promiser);
        ThreadUnlock(&watchers_mutex);
        if (destroy_state != NULL)
        {
            destroy_state(state);
        }
        return false;
    }

    Watcher *w = xmalloc(sizeof(Watcher));
    w->promiser = xstrdup(promiser);
    w->key = xstrdup(key);
    w->state = TakeOverDiscardedState(key, state, destroy_state);
    w->promise = pp;
    w->poll_interval_secs = interval;
    w->next_due = 0; /* due immediately on the watcher thread's first pass */
    w->check_callback = check_callback;
    w->destroy_state = destroy_state;

    SeqAppend(watchers, w);

    ThreadUnlock(&watchers_mutex);
    return true;
}

static void WatcherDestroy(void *item)
{
    Watcher *w = item;
    /* No state if taken over by a watcher registered again */
    if (w->destroy_state != NULL && w->state != NULL)
    {
        w->destroy_state(w->state);
    }
    free(w->promiser);
    free(w->key);
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

        for (size_t i = 0; !paused && i < SeqLength(watchers); i++)
        {
            Watcher *w = SeqAt(watchers, i);

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

    Log(LOG_LEVEL_VERBOSE, "Started reactor watcher subsystem with %zu watcher(s)", SeqLength(watchers));
    return true;
}

/* Watchers are only destroyed by the main thread (WatcherRegistryClear()),
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
        on_event(ctx, w->promise, w->promiser);
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
}

void EventWatcherResume(void)
{
    ThreadLock(&watchers_mutex);
    DiscardedWatchersDestroy();
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
