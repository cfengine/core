/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

static pthread_mutex_t MUTEXES[] = /* GLOBAL_T */
{
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
};

pthread_mutex_t *cft_lock = &MUTEXES[0]; /* GLOBAL_T */
pthread_mutex_t *cft_count = &MUTEXES[1]; /* GLOBAL_T */
pthread_mutex_t *cft_getaddr = &MUTEXES[2]; /* GLOBAL_T */
pthread_mutex_t *cft_server_children = &MUTEXES[3]; /* GLOBAL_T */
pthread_mutex_t *cft_server_filter = &MUTEXES[4]; /* GLOBAL_T */

#define MUTEX_NAME_SIZE 32

static void GetMutexName(const pthread_mutex_t *mutex, char *mutexname)
{
    if (mutex >= cft_lock && mutex <= cft_server_filter)
    {
        sprintf(mutexname, "mutex %ld", (long) (mutex - cft_lock));
    }
    else
    {
        sprintf(mutexname, "unknown mutex 0x%p", mutex);
    }
}

int ThreadLock(pthread_mutex_t *mutex)
{
    int result = pthread_mutex_lock(mutex);

    if (result != 0)
    {
        char mutexname[MUTEX_NAME_SIZE];

        GetMutexName(mutex, mutexname);

        printf("!! Could not lock %s: %s\n", mutexname, strerror(result));
        return false;
    }
    return true;
}

int ThreadUnlock(pthread_mutex_t *mutex)
{
    int result = pthread_mutex_unlock(mutex);

    if (result != 0)
    {
        char mutexname[MUTEX_NAME_SIZE];

        GetMutexName(mutex, mutexname);

        printf("!! Could not unlock %s: %s\n", mutexname, strerror(result));
        return false;
    }

    return true;
}
