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
#include <hash.h>       /* CF_HOSTKEY_STRING_SIZE */

typedef enum
{
  WATCHER_CHECK_NO_EVENT,
  WATCHER_CHECK_EVENT,
  WATCHER_CHECK_ERROR,      /* unable to tell whether the event happened */
} WatcherCheckResult;

typedef void *(*WatcherStateNewFn)(const Rlist *args);
typedef WatcherCheckResult (*WatcherCheckFn)(void *state);
typedef void (*WatcherStateDestroyFn)(void *state);

/**
 * @brief What a watcher checks for, and how, depending on the type of event.
 */
typedef struct
{
  WatcherCheckFn check_callback; /* whether the event happened, updating state */
  WatcherStateDestroyFn destroy_state;
  void *state;                   /* owned by the registry once registered */
  time_t interval;               /* seconds between two checks, > 0 */
} WatcherOptions;

/**
 * @brief Called on the main thread for every event of a registered watcher,
 *        and whenever a watcher starts failing to check for its event.
 *
 * @param pp the expanded events promise the watcher was registered with (a
 *           copy of it, owned by the watcher)
 * @param check WATCHER_CHECK_EVENT if the event happened, or
 *              WATCHER_CHECK_ERROR if the watcher failed to check for it
 *              (only reported once until a check succeeds again)
 * @note Called without the watcher registry locked, but it must not call
 *       EventWatcherPause(), as the events of the watchers are being
 *       handled.
 */
typedef void (*WatcherEventFn)(EvalContext *ctx, const Promise *pp, WatcherCheckResult check);

/**
 * @brief Hash of an expanded events promise, identifying its watcher.
 *
 * @param pp the expanded events promise
 * @param hash where to write the hash, of size CF_HOSTKEY_STRING_SIZE
 */
void WatcherPromiseHash(const Promise *pp, char *hash, size_t hash_size);

void WatcherRegistryInitialize(void);
void WatcherRegistryFinalize(void);

/**
 * @brief Register a specific watcher instance.
 *
 * Watchers are identified by the hash of their expanded promise (see
 * WatcherPromiseHash()). A watcher identical to a registered one is
 * ignored, so that the 'then' bundle isn't run twice for the same event.
 *
 * If a watcher of the previous policy (see EventWatcherPause()) was
 * registered for an identical expanded promise, the new watcher takes over
 * its state instead of opt->state, so that the events happening while paused
 * are detected. Otherwise opt->state is checked once right away, as the
 * reference for the next checks, so that only what happens from now on
 * fires. If that check fails, the watcher is not registered.
 *
 * @param pp the expanded events promise. The watcher keeps a copy of it
 *           (see PromiseCopy()), passed back to the WatcherEventFn on event.
 *           The copy refers to the policy holding pp, which must not be
 *           destroyed before EventWatcherPause().
 * @param opt what the watcher checks for, see WatcherOptions. opt->state is
 *            owned by the registry, which destroys it right away if not
 *            used, so it must not be used by the caller afterwards.
 *            opt->interval must be strictly greater than 0.
 * @return false if the check of opt->state failed (WATCHER_CHECK_ERROR), in
 *         which case the watcher is not registered, true otherwise.
 */
bool WatcherRegister(const Promise *pp, WatcherOptions *opt);
bool EventWatcherInitialize(int *fd);
void EventWatcherHandleEvents(EvalContext *ctx, WatcherEventFn on_event, int fd, fd_set *readfds);

/**
 * @brief Stop checking for events, handle the events already queued, and
 * set the registered watchers aside, so that the policy they refer to can be
 * destroyed and the watchers of the new policy registered (see
 * WatcherRegister()). Events are checked for again after
 * EventWatcherResume().
 *
 * @note An event happening while paused is detected after
 *       EventWatcherResume() if a watcher for an identical promise is
 *       registered again, see WatcherRegister().
 */
void EventWatcherPause(EvalContext *ctx, WatcherEventFn on_event);

/**
 * @brief Check for events again, after EventWatcherPause(). The watchers set
 * aside by EventWatcherPause() are destroyed, and the registered ones are
 * checked immediately.
 */
void EventWatcherResume(void);

/**
 * @brief Wake the watcher thread up to check the registered watchers
 * immediately. Without watchers, the watcher thread sleeps until woken up, so
 * this must be called after registering watchers outside of an
 * EventWatcherPause()/EventWatcherResume() pair (which wakes it up itself),
 * i.e. after the watchers of the initial policy. Logs the number of
 * registered watchers.
 */
void EventWatcherWakeUp(void);
void EventWatcherFinalize(void);

#endif
