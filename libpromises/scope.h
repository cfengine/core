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

/**
 * @brief Sets CONTEXTID = id
 * @param id
 */
void ScopeSet(char *id);

/**
 * @brief NewScope; SetScope;
 * @param id
 */
void ScopeSetNew(char *id);

/**
 * @brief alloc a Scope, idempotent prepend to VSCOPE
 * @param name
 */
void ScopeNew(const char *name);

/**
 * @brief remove a Scope from VSCOPE, and dealloc it. removes only the first it finds in the list.
 * @param name
 */
void ScopeDelete(char *name);

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
void ScopeCopy(const char *new_scopename, const char *old_scopename);

/**
 * @brief clear VSCOPE
 */
void ScopeDeleteAll(void);

/**
 * @brief augments a scope, expecting corresponding lists of lvals and rvals (implying same length).
 *        in addition to copying them in, also attempts to do one-pass resolution of variables,
 *        and evaluates function calls, and attempts expansion on senior scope members.
 */
void ScopeAugment(EvalContext *ctx, char *scope, char *ns, Rlist *lvals, Rlist *rvals);

/**
 * @brief prepend GetScope("this") to CF_STCK
 */
void ScopePushThis(void);

/**
 * @brief pop a scope from CF_STCK, names the scope "this" by force, not sure why because the Scope is dealloced
 */
void ScopePopThis(void);


void ScopeToList(Scope *sp, Rlist **list);
void ScopeNewScalar(const char *scope, const char *lval, const char *rval, DataType dt);
void ScopeDeleteScalar(const char *scope, const char *lval);
void ScopeNewList(const char *scope, const char *lval, void *rval, DataType dt);
/*
 * Do not modify returned Rval, its contents may be constant and statically
 * allocated.
 */
DataType ScopeGetVariable(const char *scope, const char *lval, Rval *returnv);
void ScopeDeleteVariable(const char *scope, const char *id);
bool ScopeVariableExistsInThis(const char *name);

int ScopeAddVariableHash(const char *scope, const char *lval, Rval rval, DataType dtype, const char *fname, int no);
void ScopeDeRefListsInHashtable(char *scope, Rlist *list, Rlist *reflist);

int ScopeMapBodyArgs(EvalContext *ctx, const char *scopeid, Rlist *give, const Rlist *take);

// TODO: namespacing utility functions. there are probably a lot of these floating around, but probably best
// leave them until we get a proper symbol table
void SplitScopeName(const char *scope_name, char namespace_out[CF_MAXVARSIZE], char bundle_out[CF_MAXVARSIZE]);
void JoinScopeName(const char *ns, const char *bundle, char scope_out[CF_MAXVARSIZE]);

#endif
