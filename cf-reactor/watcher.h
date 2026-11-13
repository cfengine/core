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
#include <reactor_watch_backend.h>

typedef enum
{
  WATCHER_CHECK_NO_EVENT,
  WATCHER_CHECK_EVENT,
  WATCHER_CHECK_ERROR,      /* unable to tell whether the event happened */
} WatcherCheckResult;

/* Called on the main thread when the policy is evaluated. ctx must not be
 * kept in the state: the EvalContext is not thread safe, so the watcher
 * thread can't use it. Returns NULL, after logging why, if the state can't
 * be created. */
typedef void *(*WatcherStateNewFn)(const EvalContext *ctx, const Rlist *args);
typedef WatcherCheckResult (*WatcherCheckFn)(void *state);
typedef void (*WatcherStateDestroyFn)(void *state);

/* Called on the watcher thread, before the first check after the watcher's
 * 'then' bundles ran. The state they left may never have been checked: a
 * wakeup source may not report the bundles' own changes, and a change made
 * right after the run may be checked first. If that change restored the state
 * the last check saw (the same bad line written again), comparing with it
 * misses the change, so the next check should report any sign of one. */
typedef void (*WatcherBundlesRanFn)(void *state);

/**
 * @brief What a watcher checks for, and how, depending on the type of event.
 */
typedef struct
{
  WatcherCheckFn check_callback; /* whether the event happened, updating state */
  WatcherStateDestroyFn destroy_state;
  WatcherBundlesRanFn bundles_ran; /* NULL if the checks can't tell */
  void *state;                   /* owned by the registry once registered */
  time_t interval;               /* seconds between two checks, > 0 */
  ReactorWatchTarget target;     /* what the event is about, for the wakeup sources */
  const Rlist *methods;          /* 'wakeup_sources', NULL for the default */
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
 * registered for an identical expanded promise, the new watcher shares its
 * state instead of using opt->state, so that the events happening while
 * paused are detected, with any event it observed but didn't run yet, and its
 * limit on how often the 'then' bundles run. This doesn't wait for a check of
 * that state still running (it may block): the new watcher's checks follow
 * it. Unless the watched file moved to or from a remote file system (see
 * WatchTargetIsRemote()), as the state is then about another file system:
 * the new watcher starts afresh. Otherwise opt->state is checked once right
 * away, as the reference for the next checks, so that only what happens from
 * now on fires. If that check fails, the watcher's first successful check
 * becomes the reference instead. So it does for a watcher of a file on a
 * remote file system, which isn't checked here, as that could block for as
 * long as its server doesn't respond: its checks run on a thread of their
 * own.
 *
 * @param pp the expanded events promise. The watcher keeps a copy of it
 *           (see PromiseCopy()), passed back to the WatcherEventFn on event.
 *           The copy refers to the policy holding pp, which must not be
 *           destroyed before EventWatcherPause().
 * @param opt what the watcher checks for, see WatcherOptions. opt->state is
 *            owned by the registry, which destroys it right away if not
 *            used, so it must not be used by the caller afterwards.
 *            opt->interval must be strictly greater than 0.
 *
 * The watcher is also watched with the first available method of
 * opt->methods (see WatchBackendsAdd()), which wakes it up to be checked
 * before its interval elapses.
 *
 * @return false if none of opt->methods is available and they don't include
 *         "poll", in which case the watcher is not registered, true otherwise.
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
 * @brief Wake the watcher threads up to check the registered watchers
 * immediately. Without watchers, a watcher thread sleeps until woken up, so
 * this must be called after registering watchers outside of an
 * EventWatcherPause()/EventWatcherResume() pair (which wakes it up itself),
 * i.e. after the watchers of the initial policy. Logs the number of
 * registered watchers.
 */
void EventWatcherWakeUp(void);
void EventWatcherFinalize(void);

/**
 * @brief Check the watcher with this id as soon as possible, regardless of
 *        its interval. Thread-safe, an id that isn't registered is ignored.
 *        See ReactorWatchHost.
 */
void WatcherMarkDirty(uint64_t watcher_id);

/** @brief Check all the watchers as soon as possible. Thread-safe. */
void WatcherMarkAllDirty(void);

#endif
