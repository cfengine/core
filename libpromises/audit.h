/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#ifndef CFENGINE_AUDIT_H
#define CFENGINE_AUDIT_H

/*
 * This module keeps track of amount and value of promises kept/repaired/not-kept
 */

#include <cf3.defs.h>
#include <policy.h>
#include <eval_context.h>

void BeginAudit(void);

void UpdatePromiseCounters(PromiseResult status);

void EndAudit(const EvalContext *ctx, int background_tasks);

/*
 * FatalError causes EndAudit, so don't call it from the low-memory or corrupted stack situations.
 */
void FatalError(const EvalContext *ctx, char *s, ...) FUNC_ATTR_NORETURN FUNC_ATTR_PRINTF(2, 3);

#endif

