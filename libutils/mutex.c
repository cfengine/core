/*
   Copyright 2018 Northern.tech AS

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
        fsync(STDOUT_FILENO);
        fsync(STDERR_FILENO);
        exit(101);
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
        fsync(STDOUT_FILENO);
        fsync(STDERR_FILENO);
        exit(101);
    }
}
