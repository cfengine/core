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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_VAR_EXPRESSIONS_H
#define CFENGINE_VAR_EXPRESSIONS_H

#include "string_expressions.h"

#include "platform.h"
#include "policy.h"

typedef struct
{
    const char *const ns;
    const char *const scope;
    const char *const lval;
    const char *const *const indices;
    const size_t num_indices;
    const bool allocated; /* Mark that VarRef was allocated by VarRefParse */
} VarRef;

VarRef VarRefParse(const char *var_ref_string);

/**
 * @brief Parse the variable reference in the context of a bundle. This means that the VarRef will inherit scope and namespace
 *        of the bundle if these are not specified explicitly in the string.
 */
VarRef VarRefParseFromBundle(const char *var_ref_string, const Bundle *bundle);

void VarRefDestroy(VarRef ref);

char *VarRefToString(VarRef ref, bool qualified);

#endif
