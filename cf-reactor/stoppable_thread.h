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

#ifndef CFENGINE_STOPPABLE_THREAD_H
#define CFENGINE_STOPPABLE_THREAD_H

#include <platform.h>

/**
 * @brief A background thread that the thread that started it can ask to stop,
 * and give up on if it doesn't stop in time.
 *
 * The thread routine is expected to loop until StoppableThreadShouldStop()
 * returns true, and to sleep with StoppableThreadSleep() so that
 * StoppableThreadStop() can wake it up immediately.
 */
typedef struct StoppableThread_ StoppableThread;

typedef void (*StoppableThreadFn)(StoppableThread *thread, void *arg);

/**
 * @brief Start a thread running fn(thread, arg).
 * @return the thread handle, or NULL on failure (the cause is logged).
 */
StoppableThread *StoppableThreadStart(StoppableThreadFn fn, void *arg);

/**
 * @brief To be called from the thread routine.
 * @return true once StoppableThreadStop() has been called.
 */
bool StoppableThreadShouldStop(StoppableThread *thread);

/**
 * @brief To be called from the thread routine. Sleep for up to secs seconds,
 * returning early if StoppableThreadStop() is called. Returns immediately if
 * secs <= 0 or a stop has already been requested.
 */
void StoppableThreadSleep(StoppableThread *thread, time_t secs);

/**
 * @brief Ask the thread to stop and wait up to timeout_secs for it to return.
 *
 * If the thread doesn't return in time, it's cancelled (where supported) and
 * detached, since a thread stuck outside a cancellation point would otherwise
 * block pthread_join() forever.
 *
 * In both cases the handle must not be used afterwards.
 *
 * @return true if the thread returned and was joined, false if it was
 *         abandoned. In the latter case it may still be running, so anything
 *         it uses must not be freed.
 */
bool StoppableThreadStop(StoppableThread *thread, int timeout_secs);

#endif /* CFENGINE_STOPPABLE_THREAD_H */
