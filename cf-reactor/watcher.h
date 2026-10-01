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

typedef bool (*WatcherCheckFn)(void *state);
typedef void (*WatcherStateDestroyFn)(void *state);

/**
 * @brief Called on the main thread for every event of a registered watcher.
 *
 * @param pp the (unexpanded) events promise the watcher was registered for
 * @param promiser the expanded promiser the watcher was registered with,
 *                 selecting the iteration of the events promise
 * @note Called without the watcher registry locked, but it must not call
 *       WatcherRegistryClear(), as the events of the watchers are being
 *       handled.
 */
typedef void (*WatcherEventFn)(EvalContext *ctx, const Promise *pp, const char *promiser);

void WatcherRegistryInitialize(void);
void WatcherRegistryFinalize(void);

/**
 * @brief Discard every currently registered watcher and start over. Call
 * this before re-registering watchers from a freshly (re-)read policy.
 *
 * The discarded watchers are kept until EventWatcherResume(): a watcher
 * registered again in the meantime with the same key (see WatcherRegister())
 * takes over the state of the discarded watcher, so that the events
 * happening while paused are detected after EventWatcherResume().
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
 * @param key identifies the watcher across policy reloads: watchers with the
 *            same key watch the same thing in the same way, so the watcher
 *            registered with the key of a watcher discarded by
 *            WatcherRegistryClear() takes over its state
 * @param type the type of watcher, defined in when bodies
 * @param state the data used for by the watcher, depending on the type
 * @param pp the unexpanded events promise, passed back to the WatcherEventFn
 *           on event. Not owned, so the registry must be cleared before the
 *           policy holding it is destroyed.
 * @param interval interval between runs
 */
bool WatcherRegister(const char *promiser, const char *key, EventType type, void *state,
                     const Promise *pp, time_t interval);
bool EventWatcherInitialize(int *fd);
void EventWatcherHandleEvents(EvalContext *ctx, WatcherEventFn on_event, int fd, fd_set *readfds);

/**
 * @brief Stop checking for events, and handle the events already queued, so
 * that the watchers can be cleared (see WatcherRegistryClear()) without
 * losing any event. Events are checked for again after EventWatcherResume().
 *
 * @note An event happening while paused is detected after
 *       EventWatcherResume() if its watcher is registered again, see
 *       WatcherRegistryClear().
 */
void EventWatcherPause(EvalContext *ctx, WatcherEventFn on_event);

/**
 * @brief Check for events again, after EventWatcherPause(). The watchers
 * discarded by WatcherRegistryClear() and not registered again are destroyed,
 * and the registered ones are checked immediately.
 */
void EventWatcherResume(void);
void EventWatcherFinalize(void);

#endif
