/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_SCOPE_H
#define CFENGINE_SCOPE_H

#include "cf3.defs.h"

#include "var_expressions.h"
#include "assoc.h"

/**
 * @deprecated
 */
Scope *ScopeNew(const char *name);

void ScopePutMatch(int index, const char *value);

bool ScopeExists(const char *name);

/**
 * @deprecated
 */
void ScopeSetCurrent(const char *name);

/**
 * @deprecated
 */
Scope *ScopeGetCurrent(void);

/**
 * @brief Clears all variables from a scope
 * @param name
 */
void ScopeClear(const char *name);

/**
 * @brief find a Scope in VSCOPE
 * @param scope
 * @return
 */
Scope *ScopeGet(const char *scope);

/**
 * @brief copy an existing Scope, prepend to VSCOPE with a new name
 * @param new_scopename
 * @param old_scopename
 */
void ScopeCopy(const char *new_scopename, const Scope *old_scope);

/**
 * @brief clear VSCOPE
 */
void ScopeDeleteAll(void);

/**
 * @brief augments a scope, expecting corresponding lists of lvals and rvals (implying same length).
 *        in addition to copying them in, also attempts to do one-pass resolution of variables,
 *        and evaluates function calls, and attempts expansion on senior scope members.
 */
void ScopeAugment(EvalContext *ctx, const Bundle *bp, const Promise *pp, const Rlist *arguments);

/**
 * @brief prepend GetScope("this") to CF_STCK
 */
void ScopePushThis(void);

/**
 * @brief pop a scope from CF_STCK, names the scope "this" by force, not sure why because the Scope is dealloced
 */
void ScopePopThis(void);


void ScopeToList(Scope *sp, Rlist **list);
void ScopeNewScalar(EvalContext *ctx, VarRef lval, const char *rval, DataType dt);
void ScopeNewSpecialScalar(EvalContext *ctx, const char *scope, const char *lval, const char *rval, DataType dt);
void ScopeDeleteScalar(VarRef lval);
void ScopeDeleteSpecialScalar(const char *scope, const char *lval);
void ScopeNewList(EvalContext *ctx, VarRef lval, void *rval, DataType dt);
void ScopeNewSpecialList(EvalContext *ctx, const char *scope, const char *lval, void *rval, DataType dt);
bool ScopeIsReserved(const char *scope);

void ScopeDeleteVariable(const char *scope, const char *id);

void ScopeDeRefListsInHashtable(char *scope, Rlist *list, Rlist *reflist);

int ScopeMapBodyArgs(EvalContext *ctx, const char *scopeid, Rlist *give, const Rlist *take);

int CompareVariableValue(Rval rval, CfAssoc *ap);
bool UnresolvedVariables(const CfAssoc *ap, RvalType rtype);

// TODO: namespacing utility functions. there are probably a lot of these floating around, but probably best
// leave them until we get a proper symbol table
void SplitScopeName(const char *scope_name, char namespace_out[CF_MAXVARSIZE], char bundle_out[CF_MAXVARSIZE]);
void JoinScopeName(const char *ns, const char *bundle, char scope_out[CF_MAXVARSIZE]);

#endif
