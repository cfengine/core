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
#include <signal_lib.h>         // MaskTerminationSignalsInThread()
#include <signals.h>            // IsPendingTermination()
#include <logging.h>
#include <alloc.h>
#include <string_lib.h>         // StringHash_untyped(), StringEqual_untyped()
#include <map.h>
#include <sequence.h>
#include <threaded_queue.h>
#include <pthread.h>

/* Upper bound on how long the watcher thread ever sleeps in one go, so that
 * IsPendingTermination() is re-checked at least this often during shutdown,
 * regardless of what poll intervals watchers asked for. */
#define MAX_WATCHER_THREAD_SLEEP_SECS 1

/* Not currently caller-configurable -- WatcherRegister() doesn't take an
 * interval parameter -- so every watcher polls at this rate for now. */
#define DEFAULT_WATCHER_POLL_INTERVAL_SECS 10

typedef struct
{
    char *key;
    WatcherCheckFn check_callback; // resolved from `type` at WatcherRegister() time
    WatcherStateDestroyFn destroy_state;
    void *state;
    time_t poll_interval_secs;
    time_t next_due;
} Watcher;

static void WatcherDestroy(void *item); /* defined below, next to WatcherRegister() */

/* Guards `watchers` (and, for the same atomic-rebuild reason, its paired
 * `event_to_bundle`) against the watcher thread (WatcherThreadMain())
 * iterating over `watchers` concurrently with the main thread re-registering
 * watchers from a freshly (re-)read policy (WatcherRegister(),
 * WatcherRegistryClear()). */
static pthread_mutex_t watchers_mutex = PTHREAD_MUTEX_INITIALIZER;
static Seq *watchers = NULL;
static Map *event_to_bundle = NULL;

static WakeupChannel wakeup_channel;
static ThreadedQueue *event_queue = NULL;
static pthread_t watcher_thread;

/*****************************************************************************/

void WatcherRegistryInitialize(void)
{
    assert(watchers == NULL);
    assert(event_to_bundle == NULL);

    watchers = SeqNew(4, WatcherDestroy);
    event_to_bundle = MapNew(StringHash_untyped, StringEqual_untyped, NULL, NULL);
}

void WatcherRegistryFinalize(void)
{
    SeqDestroy(watchers);
    watchers = NULL;
    MapDestroy(event_to_bundle);
    event_to_bundle = NULL;
}

void WatcherRegistryClear(void)
{
    assert(watchers != NULL && event_to_bundle != NULL);

    pthread_mutex_lock(&watchers_mutex);

    SeqDestroy(watchers);
    watchers = SeqNew(4, WatcherDestroy);

    MapDestroy(event_to_bundle);
    event_to_bundle = MapNew(StringHash_untyped, StringEqual_untyped, NULL, NULL);

    pthread_mutex_unlock(&watchers_mutex);
}

void WatcherRegister(const char *key, EventType type, void *state, Bundle *bundle, time_t interval)
{
    assert(key != NULL);
    assert(bundle != NULL);
    assert(watchers != NULL && event_to_bundle != NULL);

    WatcherCheckFn check_callback = NULL;
    WatcherStateDestroyFn destroy_state = NULL;
    switch (type)
    {

    case EVENT_FILE_DELETED:
        // TODO: initialize check_callback and destroy_state
        break;

    default:
        ProgrammingError("Unknown reactor event type %d for watcher '%s'", (int) type, key);
    }

    pthread_mutex_lock(&watchers_mutex);

    if (MapHasKey(event_to_bundle, key))
    {
        Log(LOG_LEVEL_ERR, "Reactor watcher key '%s' is already registered, ignoring the duplicate", key);
        pthread_mutex_unlock(&watchers_mutex);
        if (destroy_state != NULL)
        {
            destroy_state(state);
        }
        return;
    }

    Watcher *w = xmalloc(sizeof(Watcher));
    w->key = xstrdup(key);
    w->state = state;
    w->poll_interval_secs = interval;
    w->next_due = 0; /* due immediately on the watcher thread's first pass */
    w->check_callback = check_callback;
    w->destroy_state = destroy_state;

    SeqAppend(watchers, w);
    MapInsert(event_to_bundle, w->key, bundle);

    pthread_mutex_unlock(&watchers_mutex);
}

static void WatcherDestroy(void *item)
{
    Watcher *w = item;
    if (w->destroy_state != NULL)
    {
        w->destroy_state(w->state);
    }
    free(w->key);
    free(w);
}

static void *WatcherThreadMain(ARG_UNUSED void *unused)
{
    /* Keep termination signals landing on the main thread (which owns the
     * daemon's HandleSignalsForDaemon()-based shutdown), never on this one.
     * No-op on Windows -- see signal_lib.h. */
#ifndef __MINGW32__
    MaskTerminationSignalsInThread();
#endif

    while (!IsPendingTermination())
    {
        time_t now = time(NULL);
        time_t sleep_for = MAX_WATCHER_THREAD_SLEEP_SECS;
        bool any_event = false;

        pthread_mutex_lock(&watchers_mutex);

        for (size_t i = 0; i < SeqLength(watchers); i++)
        {
            Watcher *w = SeqAt(watchers, i);

            if (now >= w->next_due)
            {
                bool fired = w->check_callback(w->state);
                w->next_due = now + w->poll_interval_secs;
                if (fired)
                {
                    ThreadedQueuePush(event_queue, SafeStringDuplicate(w->key));
                    any_event = true;
                }
            }

            time_t until_due = (w->next_due > now) ? (w->next_due - now) : 0;
            sleep_for = MIN(sleep_for, until_due);
        }

        pthread_mutex_unlock(&watchers_mutex);

        if (any_event)
        {
            WakeupChannelNotify(&wakeup_channel);
        }

        if (sleep_for > 0)
        {
            sleep((unsigned int) sleep_for);
        }
    }

    return NULL;
}

bool EventWatcherInitialize(int *fd)
{
    assert(fd != NULL);
    assert(watchers != NULL && event_to_bundle != NULL); /* WatcherRegistryInitialize() first */

    if (!WakeupChannelOpen(&wakeup_channel))
    {
        Log(LOG_LEVEL_ERR, "Failed to open wakeup channel for cf-reactor event watcher");
        return false;
    }

    event_queue = ThreadedQueueNew(16, free);

    int ret = pthread_create(&watcher_thread, NULL, WatcherThreadMain, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Unable to start cf-reactor watcher thread: %s", GetErrorStrFromCode(ret));
        ThreadedQueueDestroy(event_queue);
        event_queue = NULL;
        WakeupChannelClose(&wakeup_channel);
        return false;
    }

    *fd = WakeupChannelReadFd(&wakeup_channel);

    Log(LOG_LEVEL_VERBOSE, "Started reactor watcher subsystem with %zu watcher(s)", SeqLength(watchers));
    return true;
}

void EventWatcherHandleEvents(int fd, fd_set *readfds)
{
    assert(readfds != NULL);

    if (!FD_ISSET(fd, readfds))
    {
        return;
    }

    WakeupChannelDrain(&wakeup_channel);

    void *item;
    while (ThreadedQueuePop(event_queue, &item, 0))
    {
        const char *key = item;

        pthread_mutex_lock(&watchers_mutex);
        const Bundle *bundle = MapGet(event_to_bundle, key);
        pthread_mutex_unlock(&watchers_mutex);

        if (bundle == NULL)
        {
            Log(LOG_LEVEL_VERBOSE, "Reactor watcher '%s' fired but is no longer registered, ignoring", key);
            return;
        }
        else
        {
            Log(LOG_LEVEL_NOTICE, "Reactor watcher '%s' fired", key);
            // TODO: run bundle
        }

        free(item);
    }
}

void EventWatcherFinalize(void)
{
    pthread_join(watcher_thread, NULL);
    ThreadedQueueDestroy(event_queue);
    event_queue = NULL;
    WakeupChannelClose(&wakeup_channel);

    WatcherRegistryFinalize();
}
