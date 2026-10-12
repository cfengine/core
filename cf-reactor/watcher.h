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

#ifndef CFENGINE_WATCHER_H
#define CFENGINE_WATCHER_H

#include <cf3.defs.h>   /* Bundle */

typedef enum
{
  EVENT_FILE_DELETED,
} EventType;

typedef enum
{
  WATCHER_CHECK_NO_EVENT,
  WATCHER_CHECK_EVENT,
  WATCHER_CHECK_ERROR,      /* unable to tell whether the event happened */
} WatcherCheckResult;

typedef WatcherCheckResult (*WatcherCheckFn)(void *state);
typedef void (*WatcherStateDestroyFn)(void *state);

/**
 * @brief Called on the main thread for every event of a registered watcher,
 *        and whenever a watcher starts failing to check for its event.
 *
 * @param pp the (unexpanded) events promise the watcher was registered for
 * @param promiser the expanded promiser the watcher was registered with,
 *                 selecting the iteration of the events promise
 * @param check WATCHER_CHECK_EVENT if the event happened, or
 *              WATCHER_CHECK_ERROR if the watcher failed to check for it
 *              (only reported once until a check succeeds again)
 * @note Called without the watcher registry locked, but it must not call
 *       WatcherRegistryClear(), as the events of the watchers are being
 *       handled.
 */
typedef void (*WatcherEventFn)(EvalContext *ctx, const Promise *pp, const char *promiser, WatcherCheckResult check);

void WatcherRegistryInitialize(void);
void WatcherRegistryFinalize(void);

/**
 * @brief Discard every currently registered watcher and start over. Call
 * this before re-registering watchers from a freshly (re-)read policy.
 *
 * @note No events of the watchers may be queued, i.e. once the event watcher
 *       is initialized, call EventWatcherPause() first.
 */
void WatcherRegistryClear(void);

/**
 * @brief Register a specific watcher instance.
 *
 * @param promiser the expanded promiser of the iteration of the events
 *                 promise. Together with pp, identifies the watcher.
 * @param type the type of watcher, defined in when bodies
 * @param state the data used for by the watcher, depending on the type
 * @param pp the unexpanded events promise, passed back to the WatcherEventFn
 *           on event. Not owned, so the registry must be cleared before the
 *           policy holding it is destroyed.
 * @param interval interval between runs
 */
bool WatcherRegister(const char *promiser, EventType type, void *state, const Promise *pp, time_t interval);
bool EventWatcherInitialize(int *fd);
void EventWatcherHandleEvents(EvalContext *ctx, WatcherEventFn on_event, int fd, fd_set *readfds);

/**
 * @brief Stop checking for events, and handle the events already queued, so
 * that the watchers can be cleared (see WatcherRegistryClear()) without
 * losing any event. Events are checked for again after EventWatcherResume().
 *
 * @note Only the events already detected are guaranteed to be handled:
 *       whether an event happening while paused is detected afterwards
 *       depends on the state the watcher is registered again with.
 */
void EventWatcherPause(EvalContext *ctx, WatcherEventFn on_event);

/**
 * @brief Check for events again, after EventWatcherPause().
 */
void EventWatcherResume(void);
void EventWatcherFinalize(void);

#endif
