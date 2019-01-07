/*
   Copyright 2019 Northern.tech AS

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

#include <mutex.h>

#include <logging.h>                                            /* Log */
#include <cleanup.h>


void __ThreadLock(pthread_mutex_t *mutex,
                  const char *funcname, const char *filename, int lineno)

{
    int result = pthread_mutex_lock(mutex);

    if (result != 0)
    {
        /* Since Log blocks on mutexes, using it would be unsafe. Therefore,
           we use fprintf instead */
        fprintf(stderr,
                "Locking failure at %s:%d function %s! "
                "(pthread_mutex_lock: %s)",
                filename, lineno, funcname, GetErrorStrFromCode(result));
        fflush(stdout);
        fflush(stderr);
        DoCleanupAndExit(101);
    }
}

void __ThreadUnlock(pthread_mutex_t *mutex,
                    const char *funcname, const char *filename, int lineno)
{
    int result = pthread_mutex_unlock(mutex);

    if (result != 0)
    {
        /* Since Log blocks on mutexes, using it would be unsafe. Therefore,
           we use fprintf instead */
        fprintf(stderr,
                "Locking failure at %s:%d function %s! "
                "(pthread_mutex_unlock: %s)",
                filename, lineno, funcname, GetErrorStrFromCode(result));
        fflush(stdout);
        fflush(stderr);
        DoCleanupAndExit(101);
    }
}

int __ThreadWait(pthread_cond_t *pcond, pthread_mutex_t *mutex, int timeout,
                    const char *funcname, const char *filename, int lineno)
{
    int result = 0;

    if (timeout == THREAD_BLOCK_INDEFINITELY)
    {
        result = pthread_cond_wait(pcond, mutex);
    }
    else
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);

        ts.tv_sec += timeout;
        result = pthread_cond_timedwait(pcond, mutex, &ts);
    }

    if (result != 0)
    {
        if (result == ETIMEDOUT)
        {
            Log(LOG_LEVEL_DEBUG,
                "Thread condition timed out at %s:%d function %s! "
                "(pthread_cond_timewait): %s)",
                filename, lineno, funcname, GetErrorStrFromCode(result));
        }
        else
        {
            /* Since Log blocks on mutexes, using it would be unsafe.
               Therefore, we use fprintf instead */
            fprintf(stderr,
                    "Failed to wait for thread condition at %s:%d function "
                    "%s! (pthread_cond_(wait|timewait)): %s)",
                    filename, lineno, funcname, GetErrorStrFromCode(result));
            fflush(stdout);
            fflush(stderr);
            DoCleanupAndExit(101);
        }
    }

    return result;
}
