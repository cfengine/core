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

#ifndef CFENGINE_AUGMENTS_H
#define CFENGINE_AUGMENTS_H

#include <eval_context.h>
#include <generic_agent.h>      /* GenericAgentConfig */

/**
 * @brief Load augments data (def_preferred.json or def.json in the policy
 *        input directory) and install the defined variables and classes
 *        into the evaluation context.
 */
void LoadAugments(EvalContext *ctx, GenericAgentConfig *config);

#endif  /* CFENGINE_AUGMENTS_H */
