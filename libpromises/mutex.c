/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.

*/

#include "mutex.h"

static pthread_mutex_t MUTEXES[] =
{
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
};

pthread_mutex_t *cft_system = &MUTEXES[0];
pthread_mutex_t *cft_lock = &MUTEXES[1];
pthread_mutex_t *cft_count = &MUTEXES[2];
pthread_mutex_t *cft_getaddr = &MUTEXES[3];
pthread_mutex_t *cft_vscope = &MUTEXES[4];
pthread_mutex_t *cft_server_children = &MUTEXES[5];

#define MUTEX_NAME_SIZE 32

static void GetMutexName(const pthread_mutex_t *mutex, char *mutexname)
{
    if (mutex >= cft_system && mutex <= cft_server_children)
    {
        sprintf(mutexname, "mutex %ld", (long) (mutex - cft_system));
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
