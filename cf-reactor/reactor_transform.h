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

/**
 * @brief Evaluate every `bundle reactor NAME { ... }` in the given policy:
 * keep its "meta", "vars" and "classes" promises, and register a watcher
 * (see watcher.h) for each of its "events" promises.
 *
 * Expects no watchers to be registered: on re-read of the policy, the
 * watchers of the previous policy must be set aside with
 * EventWatcherPause() before that policy is destroyed, as they refer to its
 * promises.
 */
void KeepReactorPromises(EvalContext *ctx, const Policy *policy);

/**
 * @brief Keep the events promise of a watcher that fired (see WatcherEventFn
 * in watcher.h): run the bundle of its 'then' attribute.
 *
 * @param pp the unexpanded events promise the watcher was registered for
 * @param promise_hash the hash of the expanded promise the watcher was
 *                     registered with, selecting the iteration of the events
 *                     promise to keep
 */
void HandleReactorEvent(EvalContext *ctx, const Promise *pp, const char *promise_hash);

#endif
