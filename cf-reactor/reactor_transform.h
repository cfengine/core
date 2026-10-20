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

#ifndef CFENGINE_REACTOR_TRANSFORM_H
#define CFENGINE_REACTOR_TRANSFORM_H

#include <eval_context.h>
#include <policy.h>
#include <watcher.h>     /* WatcherCheckResult */

/**
 * @brief Evaluate every `bundle reactor NAME { ... }` with events promises
 * and without arguments in the given policy: keep its "meta", "vars" and
 * "classes" promises, then register a watcher (see watcher.h) for each of
 * its "events" promises. The reactor bundles without events promises are
 * not evaluated, they are only run from the 'then' attribute of events
 * promises (see BundleIsReactorEventsBundle()).
 *
 * Expects no watchers to be registered: on re-read of the policy, the
 * watchers of the previous policy must be set aside with
 * EventWatcherPause() before that policy is destroyed, as they refer to its
 * promises.
 */
void KeepReactorPromises(EvalContext *ctx, const Policy *policy);

/**
 * @brief Keep the events promise of a watcher that fired (see WatcherEventFn
 * in watcher.h): run the agent, common or reactor bundles (without events
 * promises) of its 'then' attribute like cf-agent does (see
 * ScheduleAgentOperations()), and record the outcome of the bundles as the
 * outcome of the events promise. If the watcher failed to check for its
 * event, record a failure instead.
 *
 * @param pp the expanded events promise the watcher was registered with,
 *           kept as is: its class guard, if/unless and functions are not
 *           evaluated again
 * @param check whether the event happened or the watcher failed to check for
 *              it (see WatcherEventFn)
 */
void HandleReactorEvent(EvalContext *ctx, const Promise *pp, WatcherCheckResult check);

#endif
