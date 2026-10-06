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

#ifndef CFENGINE_AGENT_OPERATIONS_H
#define CFENGINE_AGENT_OPERATIONS_H

#include <cf3.defs.h>

/**
 * @brief Evaluates the promises of an agent or common bundle, like cf-agent
 *        does, using the evaluation order selected by
 *        EvalContextIsClassicOrder().
 *
 * Used by cf-agent (bundlesequence and methods promises) and cf-reactor
 * (bundle in the 'then' attribute of an events promise). Can be called
 * recursively through methods promises.
 *
 * The caller is responsible for pushing the bundle frame
 * (EvalContextStackPushBundleFrame()) before calling, and popping it after.
 *
 * @param ctx The evaluation context
 * @param bp  The bundle to evaluate
 * @return The aggregated result of all promises evaluated in the bundle
 */
PromiseResult ScheduleAgentOperations(EvalContext *ctx, const Bundle *bp);

/**
 * @brief Evaluates the promises of a bundle in normal order: by promise type,
 *        in AGENT_TYPESEQUENCE order, followed by custom promise types, in
 *        each evaluation pass.
 * @see ScheduleAgentOperations()
 */
PromiseResult ScheduleAgentOperationsNormalOrder(EvalContext *ctx, const Bundle *bp);

/**
 * @brief Evaluates the promises of a bundle in top-down order: in the order
 *        they are written in the policy, in each evaluation pass.
 * @see ScheduleAgentOperations()
 */
PromiseResult ScheduleAgentOperationsTopDownOrder(EvalContext *ctx, const Bundle *bp);

extern int CFA_BACKGROUND;              /* GLOBAL_X */
extern int CFA_BACKGROUND_LIMIT;        /* GLOBAL_P, body agent control: max_children */
extern Item *PROCESSREFRESH;            /* GLOBAL_P, body agent control: refresh_processes */

#endif
