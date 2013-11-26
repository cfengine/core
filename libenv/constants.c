/*
   Copyright (C) CFEngine AS

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

#include <constants.h>

#include <env_context.h>

void LoadSystemConstants(EvalContext *ctx)
{
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_CONST, "dollar", "$", DATA_TYPE_STRING, "goal=constants,source=agent");
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_CONST, "n", "\n", DATA_TYPE_STRING, "goal=constants,source=agent");
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_CONST, "r", "\r", DATA_TYPE_STRING, "goal=constants,source=agent");
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_CONST, "t", "\t", DATA_TYPE_STRING, "goal=constants,source=agent");
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_CONST, "endl", "\n", DATA_TYPE_STRING, "goal=constants,source=agent");
    /* NewScalar("const","0","\0",cf_str);  - this cannot work */
}
