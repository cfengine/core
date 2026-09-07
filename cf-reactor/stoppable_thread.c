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

#include <stoppable_thread.h>
#include <logging.h>
#include <alloc.h>
#include <mutex.h>              /* ThreadLock(), ThreadUnlock(), ThreadWait() */
#include <pthread.h>

struct StoppableThread_
{
    pthread_t thread;
    StoppableThreadFn fn;
    void *arg;

    /* Protects stop_requested and exited. cond is signalled when either of
     * them changes. */
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool stop_requested; /* set by StoppableThreadStop() */
    bool exited;         /* set when fn returns */
};

static void StoppableThreadDestroy(StoppableThread *thread)
{
    assert(thread != NULL);
    pthread_cond_destroy(&thread->cond);
    pthread_mutex_destroy(&thread->lock);
    free(thread);
}

static void *StoppableThreadMain(void *data)
{
    StoppableThread *thread = data;

    thread->fn(thread, thread->arg);

    ThreadLock(&thread->lock);
    thread->exited = true;
    pthread_cond_broadcast(&thread->cond);
    ThreadUnlock(&thread->lock);

    return NULL;
}

StoppableThread *StoppableThreadStart(StoppableThreadFn fn, void *arg)
{
    assert(fn != NULL);

    StoppableThread *thread = xcalloc(1, sizeof(StoppableThread));
    thread->fn = fn;
    thread->arg = arg;
    pthread_mutex_init(&thread->lock, NULL);
    pthread_cond_init(&thread->cond, NULL);

    int ret = pthread_create(&thread->thread, NULL, StoppableThreadMain, thread);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Unable to start thread: %s", GetErrorStrFromCode(ret));
        StoppableThreadDestroy(thread);
        return NULL;
    }

    return thread;
}

bool StoppableThreadShouldStop(StoppableThread *thread)
{
    assert(thread != NULL);

    ThreadLock(&thread->lock);
    bool ret = thread->stop_requested;
    ThreadUnlock(&thread->lock);
    return ret;
}

void StoppableThreadSleep(StoppableThread *thread, time_t secs)
{
    assert(thread != NULL);

    ThreadLock(&thread->lock);
    if (!thread->stop_requested && secs > 0)
    {
        ThreadWait(&thread->cond, &thread->lock, (int) secs);
    }
    ThreadUnlock(&thread->lock);
}

bool StoppableThreadStop(StoppableThread *thread, int timeout_secs)
{
    assert(thread != NULL);

    ThreadLock(&thread->lock);
    thread->stop_requested = true;
    pthread_cond_broadcast(&thread->cond);

    const time_t deadline = time(NULL) + timeout_secs;
    time_t now;
    while (!thread->exited && (now = time(NULL)) < deadline)
    {
        ThreadWait(&thread->cond, &thread->lock, (int) (deadline - now));
    }
    const bool exited = thread->exited;
    ThreadUnlock(&thread->lock);

    if (exited)
    {
        pthread_join(thread->thread, NULL);
        StoppableThreadDestroy(thread);
        return true;
    }

    Log(LOG_LEVEL_ERR, "Thread did not exit within %d seconds, cancelling it", timeout_secs);
#ifdef HAVE_PTHREAD_CANCEL
    int ret = pthread_cancel(thread->thread);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to cancel thread: %s", GetErrorStrFromCode(ret));
    }
#endif
    /* Cancellation is only acted upon at a cancellation point, which the
     * thread may never reach, so don't risk blocking in pthread_join(). The
     * handle is leaked on purpose: the thread may still be using it. */
    pthread_detach(thread->thread);
    return false;
}
