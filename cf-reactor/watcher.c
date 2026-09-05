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
#include <signal_lib.h>         /* MaskTerminationSignalsInThread() */
#include <signals.h>            /* IsPendingTermination() */
#include <logging.h>
#include <alloc.h>
#include <string_lib.h>         /* StringHash_untyped(), StringEqual_untyped() */
#include <map.h>
#include <sequence.h>
#include <threaded_queue.h>
#include <pthread.h>
#include <file_watcher.h>

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
    WatcherCheckFn check_fn;       /* resolved from `type` at WatcherRegister() time */
    WatcherPayloadDestroyFn destroy_payload;
    void *payload;
    time_t poll_interval_secs;
    time_t next_due;
} Watcher;

static void WatcherDestroy(void *item); /* defined below, next to WatcherRegister() */

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

/*****************************************************************************/
/* WatcherRegister() / WatcherDestroy() -- create and destroy one Watcher. */
/*****************************************************************************/

void WatcherRegister(const char *key, EventType type, void *payload, Bundle *bundle, time_t interval)
{
    assert(key != NULL);
    assert(bundle != NULL);
    assert(watchers != NULL && event_to_bundle != NULL);

    WatcherCheckFn check_fn;
    WatcherPayloadDestroyFn destroy_payload;
    switch (type)
    {
    case EVENT_FILE_DELETED:
        check_fn = CheckFileExists;
        destroy_payload = DestroyFileWatcherPayload;
        break;
    // TODO: add more cases

    default:
        ProgrammingError("Unknown reactor event type %d for watcher '%s'", (int) type, key);
    }

    if (MapHasKey(event_to_bundle, key))
    {
        Log(LOG_LEVEL_ERR, "Reactor watcher key '%s' is already registered, ignoring the duplicate", key);
        if (destroy_payload != NULL)
        {
            destroy_payload(payload);
        }
        return;
    }

    Watcher *w = xmalloc(sizeof(Watcher));
    w->key = xstrdup(key);
    w->payload = payload;
    w->poll_interval_secs = interval;
    w->next_due = 0; /* due immediately on the watcher thread's first pass */
    w->check_fn = check_fn;
    w->destroy_payload = destroy_payload;

    SeqAppend(watchers, w);
    MapInsert(event_to_bundle, w->key, bundle);
}

static void WatcherDestroy(void *item)
{
    Watcher *w = item;
    if (w->destroy_payload != NULL)
    {
        w->destroy_payload(w->payload);
    }
    free(w->key);
    free(w);
}

/*****************************************************************************/
/* Watcher thread                                                           */
/*****************************************************************************/

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

        for (size_t i = 0; i < SeqLength(watchers); i++)
        {
            Watcher *w = SeqAt(watchers, i);

            if (now >= w->next_due)
            {
                bool fired = w->check_fn(w->payload);
                w->next_due = now + w->poll_interval_secs;
                if (fired)
                {
                    ThreadedQueuePush(event_queue, w->key);
                    any_event = true;
                }
            }

            time_t until_due = (w->next_due > now) ? (w->next_due - now) : 0;
            sleep_for = MIN(sleep_for, until_due);
        }

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

/*****************************************************************************/
/* Subsystem lifecycle                                                      */
/*****************************************************************************/

bool EventWatcherInitialize(int *fds, size_t max_size, size_t *num_fds)
{
    assert(fds != NULL);
    assert(num_fds != NULL);
    assert(watchers != NULL && event_to_bundle != NULL); /* WatcherRegistryInitialize() first */

    if (*num_fds >= max_size)
    {
        ProgrammingError("No fd slots left for the reactor watcher subsystem (allocated %zu)", max_size);
    }

    if (!WakeupChannelOpen(&wakeup_channel))
    {
        return false;
    }

    event_queue = ThreadedQueueNew(16, NULL);

    int ret = pthread_create(&watcher_thread, NULL, WatcherThreadMain, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Unable to start cf-reactor watcher thread: %s", GetErrorStrFromCode(ret));
        ThreadedQueueDestroy(event_queue);
        event_queue = NULL;
        WakeupChannelClose(&wakeup_channel);
        return false;
    }

    fds[*num_fds] = WakeupChannelReadFd(&wakeup_channel);
    (*num_fds)++;

    Log(LOG_LEVEL_VERBOSE, "Started reactor watcher subsystem with %zu watcher(s)", SeqLength(watchers));
    return true;
}

void EventWatcherHandleEvents(fd_set *readfds)
{
    assert(readfds != NULL);
    if (!FD_ISSET(WakeupChannelReadFd(&wakeup_channel), readfds))
    {
        return;
    }

    WakeupChannelDrain(&wakeup_channel);

    void *item;
    while (ThreadedQueuePop(event_queue, &item, 0))
    {
        const char *key = item;
        ARG_UNUSED const Bundle *bundle = MapGet(event_to_bundle, key); /* never NULL, see the invariant above */
        Log(LOG_LEVEL_NOTICE, "Reactor watcher '%s' fired", key);
        // TODO: run bundle
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
