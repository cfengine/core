/*
  Copyright 2024 Northern.tech AS

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

#ifndef CFENGINE_BUNDLE_SCHEDULE_H
#define CFENGINE_BUNDLE_SCHEDULE_H

#include <cf3.defs.h>
#include <actuator.h>             /* PromiseActuator */

/**
 * @brief Called once per promise type in `type_sequence`, before/after its
 * promises are evaluated (e.g. cf-agent uses this to reset per-type state
 * such as the mounted-filesystem list before "storage", or to run scheduled
 * package actions after "packages"). May be NULL if the caller has no such
 * per-type setup/teardown to do.
 */
typedef void (BundleTypeContextFn)(TypeSequence type);
typedef void (BundleTypeContextCleanupFn)(EvalContext *ctx, TypeSequence type);

/**
 * @brief Run every promise in `bp`, promise type by promise type, in the
 * order given by `type_sequence` (a NULL-terminated array of promise type
 * names, e.g. AGENT_TYPESEQUENCE in cf-agent.c), converging over up to
 * CF_DONEPASSES passes. Each promise is run via `actuator`.
 *
 * `new_type_context`/`delete_type_context`, if non-NULL, run immediately
 * before/after each promise type's promises. `defaults_actuator`, if
 * non-NULL, additionally runs a "defaults" promise type pass right after
 * "classes" (matching cf-agent's AGENT_TYPESEQUENCE, which has "defaults"
 * appear there) -- pass NULL if `type_sequence` has no "defaults" type to
 * skip this.
 */
PromiseResult ScheduleBundleOperationsNormalOrder(EvalContext *ctx, const Bundle *bp,
                                                   const char *const *type_sequence,
                                                   PromiseActuator *actuator,
                                                   BundleTypeContextFn *new_type_context,
                                                   BundleTypeContextCleanupFn *delete_type_context,
                                                   PromiseActuator *defaults_actuator);

/**
 * @brief Run every promise in `bp`, in the order they appear in the policy
 * source (ignoring promise type groupings), converging over up to
 * CF_DONEPASSES passes. Each promise is run via `actuator`.
 */
PromiseResult ScheduleBundleOperationsTopDownOrder(EvalContext *ctx, const Bundle *bp, PromiseActuator *actuator);

#endif
