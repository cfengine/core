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

#include <watch_backends.h>
#include <mod_reactor.h>        /* REACTOR_WATCH_METHOD_POLL */
#include <prototypes3.h>        /* ReactorEnterpriseWatchBackends() */
#include <logging.h>
#include <alloc.h>
#include <string_lib.h>         /* StringEqual() */
#include <string_sequence.h>    /* SeqStringFromString() */
#include <misc_lib.h>           /* ProgrammingError() */

/* Preferred first wherever available: the methods closest to the kernel see
 * the most, and the order doesn't change what fires, only how fast. */
static const char *const DEFAULT_WATCH_METHODS[] =
{
    "ebpf", "fanotify", "inotify", "usn", "rdcw", REACTOR_WATCH_METHOD_POLL, NULL
};

#define MAX_WATCH_BACKENDS 16

/* A method that failed to start (e.g. out of fanotify marks or locked memory)
 * is tried again when a watcher asks for it after this long */
#define BACKEND_START_RETRY_SECS 60

typedef enum
{
    BACKEND_NOT_STARTED,
    BACKEND_STARTED,
    BACKEND_UNAVAILABLE,  /* not supported here */
    BACKEND_FAILED,       /* start() failed at `failed_at` */
} BackendStatus;

typedef struct
{
    const ReactorWatchBackend *backend;
    BackendStatus status;
    time_t failed_at;     /* on BackendClockNow()'s clock */
} BackendEntry;

static BackendEntry backends[MAX_WATCH_BACKENDS];
static size_t num_backends = 0;
static const ReactorWatchHost *watch_host = NULL;

static void BackendAppend(const ReactorWatchBackend *backend)
{
    assert(backend != NULL);
    assert(backend->name != NULL);

    if (num_backends >= MAX_WATCH_BACKENDS)
    {
        Log(LOG_LEVEL_ERR, "Too many cf-reactor watch methods, ignoring '%s'", backend->name);
        return;
    }
    backends[num_backends].backend = backend;
    backends[num_backends].status = BACKEND_NOT_STARTED;
    backends[num_backends].failed_at = 0;
    num_backends++;
}

void WatchBackendsInitialize(const ReactorWatchHost *host)
{
    assert(host != NULL);
    assert(num_backends == 0);

    watch_host = host;

    const ReactorWatchBackend *const *extra = ReactorEnterpriseWatchBackends();
    for (size_t i = 0; extra != NULL && extra[i] != NULL; i++)
    {
        BackendAppend(extra[i]);
    }

    for (size_t i = 0; i < num_backends; i++)
    {
        Log(LOG_LEVEL_DEBUG, "cf-reactor watch method '%s' is built in", backends[i].backend->name);
    }
}

void WatchBackendsFinalize(void)
{
    for (size_t i = 0; i < num_backends; i++)
    {
        if (backends[i].status == BACKEND_STARTED)
        {
            backends[i].backend->stop();
        }
    }
    num_backends = 0;
    watch_host = NULL;
}

/**
 * Seconds on a clock that wall clock steps don't move, for the start retry
 */
static time_t BackendClockNow(void)
{
    struct timespec ts;
#ifdef CLOCK_MONOTONIC
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return ts.tv_sec;
}

static BackendEntry *BackendLookup(const char *name)
{
    assert(name != NULL);

    for (size_t i = 0; i < num_backends; i++)
    {
        if (StringEqual(backends[i].backend->name, name))
        {
            return &backends[i];
        }
    }
    return NULL;
}

/**
 * Starts the method on first use, so that methods no policy asks for (e.g.
 * eBPF, which loads a program into the kernel) are never set up
 */
static bool BackendEnsureStarted(BackendEntry *entry)
{
    assert(entry != NULL);
    assert(watch_host != NULL);

    if (entry->status == BACKEND_FAILED &&
        BackendClockNow() - entry->failed_at >= BACKEND_START_RETRY_SECS)
    {
        entry->status = BACKEND_NOT_STARTED;
    }

    if (entry->status == BACKEND_NOT_STARTED)
    {
        const ReactorWatchBackend *b = entry->backend;
        if (!b->available())
        {
            Log(LOG_LEVEL_VERBOSE, "cf-reactor watch method '%s' is not available on this host", b->name);
            entry->status = BACKEND_UNAVAILABLE;
        }
        else if (!b->start(watch_host))
        {
            Log(LOG_LEVEL_ERR, "Failed to start cf-reactor watch method '%s', trying it again in %d seconds",
                b->name, BACKEND_START_RETRY_SECS);
            entry->status = BACKEND_FAILED;
            entry->failed_at = BackendClockNow();
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Started cf-reactor watch method '%s'", b->name);
            entry->status = BACKEND_STARTED;
        }
    }
    return (entry->status == BACKEND_STARTED);
}

/**
 * Tries method `name` for the watcher, returns its static name on success.
 * `requested` is whether the policy named the method, rather than it coming
 * from the default list: then a method missing from this build is worth
 * telling at verbose level, it's why the watcher may end up with none.
 */
static const char *TryMethod(const char *name, uint64_t watcher_id,
                             const ReactorWatchTarget *target, const char *promiser,
                             bool requested)
{
    assert(name != NULL);
    assert(target != NULL);

    if (StringEqual(name, REACTOR_WATCH_METHOD_POLL))
    {
        return REACTOR_WATCH_METHOD_POLL;
    }

    BackendEntry *entry = BackendLookup(name);
    if (entry == NULL)
    {
        Log(requested ? LOG_LEVEL_VERBOSE : LOG_LEVEL_DEBUG,
            "Reactor watcher '%s': watch method '%s' isn't part of this build, trying the next one",
            promiser, name);
        return NULL;
    }
    if (!BackendEnsureStarted(entry))
    {
        return NULL;
    }
    if (!entry->backend->add(watcher_id, target))
    {
        Log(LOG_LEVEL_VERBOSE,
            "Reactor watcher '%s': watch method '%s' can't watch this, trying the next one",
            promiser, name);
        return NULL;
    }
    return entry->backend->name;
}

const char *WatchBackendsAdd(uint64_t watcher_id, const ReactorWatchTarget *target,
                             const Rlist *methods, const char *promiser)
{
    assert(target != NULL);
    assert(promiser != NULL);

    if (methods == NULL)
    {
        const char *override = getenv(WATCH_METHODS_TEST_OVERRIDE_ENV);
        if (override != NULL && override[0] != '\0')
        {
            Seq *names = SeqStringFromString(override, ',');
            const char *used = NULL;
            bool poll_allowed = false;
            const size_t length = SeqLength(names);
            for (size_t i = 0; i < length && used == NULL; i++)
            {
                const char *name = SeqAt(names, i);
                poll_allowed = poll_allowed || StringEqual(name, REACTOR_WATCH_METHOD_POLL);
                used = TryMethod(name, watcher_id, target, promiser, true);
            }
            SeqDestroy(names);
            return (used != NULL || !poll_allowed) ? used : REACTOR_WATCH_METHOD_POLL;
        }

        for (size_t i = 0; DEFAULT_WATCH_METHODS[i] != NULL; i++)
        {
            const char *used = TryMethod(DEFAULT_WATCH_METHODS[i], watcher_id, target, promiser,
                                         false);
            if (used != NULL)
            {
                return used;
            }
        }
        ProgrammingError("The default list of watch methods doesn't end with '%s'",
                         REACTOR_WATCH_METHOD_POLL);
    }

    for (const Rlist *rp = methods; rp != NULL; rp = rp->next)
    {
        const char *used = TryMethod(RlistScalarValue(rp), watcher_id, target, promiser, true);
        if (used != NULL)
        {
            return used;
        }
    }
    return NULL;
}

void WatchBackendsRemove(uint64_t watcher_id, const char *method)
{
    if (method == NULL || StringEqual(method, REACTOR_WATCH_METHOD_POLL))
    {
        return;
    }

    BackendEntry *entry = BackendLookup(method);
    if (entry != NULL && entry->status == BACKEND_STARTED)
    {
        entry->backend->remove(watcher_id);
    }
}
