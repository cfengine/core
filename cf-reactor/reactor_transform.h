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
 * @brief Evaluate every `bundle reactor NAME { ... }` in the given policy.
 *
 * Only the "meta", "vars" and "classes" promise types are evaluated;
 * "events" promises are not evaluated yet (see watcher.c).
 */
void KeepReactorPromises(EvalContext *ctx, const Policy *policy);

#endif
