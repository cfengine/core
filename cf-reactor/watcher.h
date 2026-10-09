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

typedef bool (*WatcherCheckFn)(void *state);
typedef void (*WatcherStateDestroyFn)(void *state);

/**
 * @brief What a watcher checks for, and how, depending on the type of event.
 */
typedef struct
{
  WatcherCheckFn check_callback; /* whether the event happened, updating state */
  WatcherStateDestroyFn destroy_state;
  void *state;                   /* owned by the registry once registered */
} WatcherOptions;

/**
 * @brief Called on the main thread for every event of a registered watcher.
 *
 * @param pp the (unexpanded) events promise the watcher was registered for
 * @param promise_hash the hash of the expanded promise the watcher was
 *                     registered with (see WatcherPromiseHash()), selecting
 *                     the iteration of the events promise
 * @note Called without the watcher registry locked, but it must not call
 *       EventWatcherPause(), as the events of the watchers are being
 *       handled.
 */
typedef void (*WatcherEventFn)(EvalContext *ctx, const Promise *pp, const char *promise_hash);

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
 * its state instead of opt.state, so that the events happening while paused
 * are detected.
 *
 * @param pp the expanded events promise. Its hash and its unexpanded
 *           promise (pp->org_pp) are passed back to the WatcherEventFn on
 *           event. The unexpanded promise is not owned, so the policy
 *           holding it must not be destroyed before EventWatcherPause().
 * @param opt what the watcher checks for, see WatcherOptions. opt.state is
 *            owned by the registry.
 * @param interval interval between runs
 */
void WatcherRegister(const Promise *pp, WatcherOptions opt, time_t interval);
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
void EventWatcherFinalize(void);

#endif
