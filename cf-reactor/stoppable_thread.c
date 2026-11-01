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
#include <mutex.h>              /* ThreadLock(), ThreadUnlock(), THREAD_BLOCK_INDEFINITELY */
#include <pthread.h>
#include <time.h>               /* clock_gettime() */

/* The clock the timed waits on cond are measured against. The monotonic clock
 * isn't affected by steps of the wall clock (NTP, an administrator, a resumed
 * VM), so stepping it back doesn't stretch a sleep by the size of the step.
 * mingw is excluded, as winpthreads accepts the attribute but may not honor
 * it, which would make every timed wait return immediately. */
#if defined(HAVE_PTHREAD_CONDATTR_SETCLOCK) && defined(CLOCK_MONOTONIC) && !defined(__MINGW32__)
# define STOPPABLE_THREAD_CLOCK CLOCK_MONOTONIC
# define STOPPABLE_THREAD_CLOCK_IS_MONOTONIC 1
#else
# define STOPPABLE_THREAD_CLOCK CLOCK_REALTIME
#endif

struct StoppableThread_
{
    pthread_t thread;
    StoppableThreadFn fn;
    void *arg;

    /* Protects stop_requested, wake_requested and exited. cond is signalled
     * when any of them changes. */
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool stop_requested; /* set by StoppableThreadStop() */
    bool wake_requested; /* set by StoppableThreadWake(), reset by a sleep */
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

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
#ifdef STOPPABLE_THREAD_CLOCK_IS_MONOTONIC
    pthread_condattr_setclock(&cond_attr, STOPPABLE_THREAD_CLOCK);
#endif
    pthread_cond_init(&thread->cond, &cond_attr);
    pthread_condattr_destroy(&cond_attr);

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

/**
 * Seconds on a clock that only moves forward, for scheduling and deadlines, so
 * that stepping the wall clock back doesn't postpone everything. Also the
 * clock the cond of a StoppableThread waits on. Falls back to the wall clock
 * where there is no monotonic clock, or if reading it fails.
 */
time_t MonotonicClockNow(void)
{
    struct timespec ts = { 0 };
    if (clock_gettime(STOPPABLE_THREAD_CLOCK, &ts) != 0)
    {
        return time(NULL);
    }
    return ts.tv_sec;
}

/**
 * Like ThreadWait() on thread->cond, but with the deadline on the clock cond
 * was created with (ThreadWait() uses the wall clock). Needs thread->lock held.
 *
 * @param timeout seconds, or THREAD_BLOCK_INDEFINITELY
 */
static void StoppableThreadCondWait(StoppableThread *thread, int timeout)
{
    assert(thread != NULL);

    if (timeout == THREAD_BLOCK_INDEFINITELY)
    {
        pthread_cond_wait(&thread->cond, &thread->lock);
        return;
    }

    struct timespec deadline;
    clock_gettime(STOPPABLE_THREAD_CLOCK, &deadline);
    deadline.tv_sec += timeout;
    /* Timing out is the normal end of a sleep, and spurious wakeups are
     * handled by the callers re-checking their state */
    pthread_cond_timedwait(&thread->cond, &thread->lock, &deadline);
}

/* timeout is in seconds, or THREAD_BLOCK_INDEFINITELY */
static void StoppableThreadWaitForWakeup(StoppableThread *thread, int timeout)
{
    assert(thread != NULL);

    ThreadLock(&thread->lock);
    if (!thread->stop_requested && !thread->wake_requested)
    {
        StoppableThreadCondWait(thread, timeout);
    }
    /* The routine re-checks its state after any sleep, which covers the
     * wakeup, whether it arrived before or during the sleep */
    thread->wake_requested = false;
    ThreadUnlock(&thread->lock);
}

void StoppableThreadSleep(StoppableThread *thread, time_t secs)
{
    assert(thread != NULL);

    if (secs > 0)
    {
        StoppableThreadWaitForWakeup(thread, (int) secs);
    }
}

void StoppableThreadSleepUntilWoken(StoppableThread *thread)
{
    StoppableThreadWaitForWakeup(thread, THREAD_BLOCK_INDEFINITELY);
}

void StoppableThreadWake(StoppableThread *thread)
{
    assert(thread != NULL);

    ThreadLock(&thread->lock);
    thread->wake_requested = true;
    /* Broadcast, since StoppableThreadStop() may also be waiting on cond */
    pthread_cond_broadcast(&thread->cond);
    ThreadUnlock(&thread->lock);
}

bool StoppableThreadStop(StoppableThread *thread, int timeout_secs)
{
    assert(thread != NULL);

    ThreadLock(&thread->lock);
    thread->stop_requested = true;
    pthread_cond_broadcast(&thread->cond);

    const time_t deadline = MonotonicClockNow() + timeout_secs;
    time_t now;
    while (!thread->exited && (now = MonotonicClockNow()) < deadline)
    {
        StoppableThreadCondWait(thread, (int) (deadline - now));
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
