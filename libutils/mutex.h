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

#ifndef CFENGINE_MUTEX_H
#define CFENGINE_MUTEX_H

#include <platform.h>

#define THREAD_BLOCK_INDEFINITELY  -1

#define ThreadLock(m)       __ThreadLock(m, __func__, __FILE__, __LINE__)
#define ThreadUnlock(m)   __ThreadUnlock(m, __func__, __FILE__, __LINE__)
#define ThreadWait(m, n, t) __ThreadWait(m, n, t, __func__, __FILE__, __LINE__)

void __ThreadLock(pthread_mutex_t *mutex,
                  const char *funcname, const char *filename, int lineno);
void __ThreadUnlock(pthread_mutex_t *mutex,
                    const char *funcname, const char *filename, int lineno);

/**
  @brief Function to wait for `timeout` seconds or until signalled.
  @note Can use THREAD_BLOCK_INDEFINITELY to block until a signal is received.
  @param [in] cond Thread condition to wait for.
  @param [in] mutex Mutex lock to acquire once condition is met.
  @param [in] timeout Seconds to wait for, can be THREAD_BLOCK_INDEFINITELY
  @return 0 on success, -1 if timed out, exits if locking fails
 */
int __ThreadWait(pthread_cond_t *cond, pthread_mutex_t *mutex, int timeout,
                 const char *funcname, const char *filename, int lineno);

#endif
