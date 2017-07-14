/*
   Copyright 2017 Northern.tech AS

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

#ifndef CFENGINE_SCOPE_H
#define CFENGINE_SCOPE_H

#include <cf3.defs.h>

#include <var_expressions.h>

typedef enum
{
    SPECIAL_SCOPE_CONST,
    SPECIAL_SCOPE_EDIT,
    SPECIAL_SCOPE_MATCH,
    SPECIAL_SCOPE_MON,
    SPECIAL_SCOPE_SYS,
    SPECIAL_SCOPE_THIS,
    SPECIAL_SCOPE_BODY,
    SPECIAL_SCOPE_DEF,

    SPECIAL_SCOPE_NONE
} SpecialScope;

const char *SpecialScopeToString(SpecialScope scope);
SpecialScope SpecialScopeFromString(const char *scope);

/**
 * @brief augments a scope, expecting corresponding lists of lvals and rvals (implying same length).
 *        in addition to copying them in, also attempts to do one-pass resolution of variables,
 *        and evaluates function calls, and attempts expansion on senior scope members.
 */
void ScopeAugment(EvalContext *ctx, const Bundle *bp, const Promise *pp, const Rlist *arguments);

void ScopeMapBodyArgs(EvalContext *ctx, const Body *body, const Rlist *args);

// TODO: namespacing utility functions. there are probably a lot of these floating around, but probably best
// leave them until we get a proper symbol table
void SplitScopeName(const char *scope_name, char namespace_out[CF_MAXVARSIZE], char bundle_out[CF_MAXVARSIZE]);
void JoinScopeName(const char *ns, const char *bundle, char scope_out[CF_MAXVARSIZE]);

#endif
