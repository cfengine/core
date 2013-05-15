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

#include "scope.h"

#include "vars.h"
#include "expand.h"
#include "hashes.h"
#include "unix.h"
#include "fncall.h"
#include "mutex.h"
#include "misc_lib.h"
#include "rlist.h"
#include "conversion.h"
#include "syntax.h"
#include "policy.h"
#include "env_context.h"
#include "audit.h"

#include <assert.h>

Scope *SCOPE_CURRENT = NULL;

Scope *SCOPE_MATCH = NULL;

/*******************************************************************/

Scope *ScopeNew(const char *name)
{
    Scope *ptr;

    if (!ThreadLock(cft_vscope))
    {
        Log(LOG_LEVEL_ERR, "Could not lock VSCOPE");
        return NULL;
    }

    for (ptr = VSCOPE; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->scope, name) == 0)
        {
            ThreadUnlock(cft_vscope);
            return NULL;
        }
    }

    ptr = xcalloc(1, sizeof(Scope));

    ptr->next = VSCOPE;
    ptr->scope = xstrdup(name);
    ptr->hashtable = HashInit();
    VSCOPE = ptr;
    ThreadUnlock(cft_vscope);

    return ptr;
}

void ScopePutMatch(int index, const char *value)
{
    if (!SCOPE_MATCH)
    {
        SCOPE_MATCH = ScopeNew("match");
    }
    Scope *ptr = SCOPE_MATCH;

    char lval[4] = { 0 };
    snprintf(lval, 3, "%d", index);

    Rval rval = (Rval) { value, RVAL_TYPE_SCALAR };

    CfAssoc *assoc = HashLookupElement(ptr->hashtable, lval);

    if (assoc)
    {
        if (CompareVariableValue(rval, assoc) == 0)
        {
            /* Identical value, keep as is */
        }
        else
        {
            /* Different value, bark and replace */
            if (!UnresolvedVariables(assoc, RVAL_TYPE_SCALAR))
            {
                Log(LOG_LEVEL_INFO, "Duplicate selection of value for variable \"%s\" in scope %s", lval, ptr->scope);
            }
            RvalDestroy(assoc->rval);
            assoc->rval = RvalCopy(rval);
            assoc->dtype = DATA_TYPE_STRING;
            Log(LOG_LEVEL_DEBUG, "Stored \"%s\" in context %s\n", lval, "match");
        }
    }
    else
    {
        if (!HashInsertElement(ptr->hashtable, lval, rval, DATA_TYPE_STRING))
        {
            ProgrammingError("Hash table is full");
        }
    }
}

Scope *ScopeGet(const char *scope)
/* 
 * Not thread safe - returns pointer to global memory
 */
{
    const char *name = scope;

    if (strncmp(scope, "default:", strlen("default:")) == 0)  // CF_NS == ':'
    {
        name = scope + strlen("default:");
    }

    if (strcmp("match", name) == 0)
    {
        return SCOPE_MATCH;
    }

    for (Scope *cp = VSCOPE; cp != NULL; cp = cp->next)
    {
        if (strcmp(cp->scope, name) == 0)
        {
            return cp;
        }
    }

    return NULL;
}

bool ScopeExists(const char *name)
{
    return ScopeGet(name) != NULL;
}

void ScopeSetCurrent(const char *name)
{
    Scope *scope = ScopeGet(name);
    if (!scope)
    {
        scope = ScopeNew(name);
    }

    SCOPE_CURRENT = scope;
}

Scope *ScopeGetCurrent(void)
{
    return SCOPE_CURRENT;
}

void ScopeAugment(EvalContext *ctx, const Bundle *bp, const Promise *pp, const Rlist *arguments)
{
    if (RlistLen(bp->args) != RlistLen(arguments))
    {
        Log(LOG_LEVEL_ERR, "While constructing scope \"%s\"", bp->name);
        fprintf(stderr, "Formal = ");
        RlistShow(stderr, bp->args);
        fprintf(stderr, ", Actual = ");
        RlistShow(stderr, arguments);
        fprintf(stderr, "\n");
        FatalError(ctx, "Augment scope, formal and actual parameter mismatch is fatal");
    }

    const Bundle *pbp = NULL;
    if (pp != NULL)
    {
        pbp = PromiseGetBundle(pp);
    }

    for (const Rlist *rpl = bp->args, *rpr = arguments; rpl != NULL; rpl = rpl->next, rpr = rpr->next)
    {
        const char *lval = rpl->item;

        Log(LOG_LEVEL_VERBOSE, "Augment scope '%s' with variable '%s' (type: %c)", bp->name, lval, rpr->type);

        // CheckBundleParameters() already checked that there is no namespace collision
        // By this stage all functions should have been expanded, so we only have scalars left

        if (IsNakedVar(rpr->item, '@'))
        {
            DataType vtype;
            char naked[CF_BUFSIZE];
            
            GetNaked(naked, rpr->item);

            Rval retval;
            if (pbp != NULL)
            {
                EvalContextVariableGet(ctx, (VarRef) { pbp->ns, pbp->name, naked }, &retval, &vtype);
            }
            else
            {
                EvalContextVariableGet(ctx, (VarRef) { NULL, bp->name, naked }, &retval, &vtype);
            }

            switch (vtype)
            {
            case DATA_TYPE_STRING_LIST:
            case DATA_TYPE_INT_LIST:
            case DATA_TYPE_REAL_LIST:
                ScopeNewList(ctx, (VarRef) { NULL, bp->name, lval }, RvalCopy((Rval) { retval.item, RVAL_TYPE_LIST}).item, DATA_TYPE_STRING_LIST);
                break;
            default:
                Log(LOG_LEVEL_ERR, "List parameter \"%s\" not found while constructing scope \"%s\" - use @(scope.variable) in calling reference", naked, bp->name);
                ScopeNewScalar(ctx, (VarRef) { NULL, bp->name, lval }, rpr->item, DATA_TYPE_STRING);
                break;
            }
        }
        else
        {
            switch(rpr->type)
            {
            case RVAL_TYPE_SCALAR:
                ScopeNewScalar(ctx, (VarRef) { NULL, bp->name, lval }, rpr->item, DATA_TYPE_STRING);
                break;

            case RVAL_TYPE_FNCALL:
                {
                    FnCall *subfp = rpr->item;
                    Rval rval = FnCallEvaluate(ctx, subfp, pp).rval;
                    if (rval.type == RVAL_TYPE_SCALAR)
                    {
                        ScopeNewScalar(ctx, (VarRef) { NULL, bp->name, lval }, rval.item, DATA_TYPE_STRING);
                    }
                    else
                    {
                        Log(LOG_LEVEL_ERR, "Only functions returning scalars can be used as arguments");
                    }
                }
                break;
            default:
                ProgrammingError("An argument neither a scalar nor a list seemed to appear. Impossible");
            }
        }
    }

/* Check that there are no danglers left to evaluate in the hash table itself */

    {
        Scope *ptr = ScopeGet(bp->name);
        AssocHashTableIterator i = HashIteratorInit(ptr->hashtable);
        CfAssoc *assoc = NULL;
        while ((assoc = HashIteratorNext(&i)))
        {
            Rval retval = ExpandPrivateRval(ctx, bp->name, assoc->rval);
            // Retain the assoc, just replace rval
            RvalDestroy(assoc->rval);
            assoc->rval = retval;
        }
    }

    return;
}

/*******************************************************************/

void ScopeDeleteAll()
{
    Scope *ptr, *this;

    if (!ThreadLock(cft_vscope))
    {
        Log(LOG_LEVEL_ERR, "Could not lock VSCOPE");
        return;
    }

    ptr = VSCOPE;

    while (ptr != NULL)
    {
        this = ptr;
        HashFree(this->hashtable);
        free(this->scope);
        ptr = this->next;
        free((char *) this);
    }

    VSCOPE = NULL;
    SCOPE_CURRENT = NULL;

    ThreadUnlock(cft_vscope);
}

/*******************************************************************/

void ScopeClear(const char *name)
{
    if (!ThreadLock(cft_vscope))
    {
        Log(LOG_LEVEL_ERR, "Could not lock VSCOPE");
        return;
    }

    Scope *scope = ScopeGet(name);
    if (!scope)
    {
        Log(LOG_LEVEL_DEBUG, "No such scope to clear");
        ThreadUnlock(cft_vscope);
        return;
    }

    HashFree(scope->hashtable);
    scope->hashtable = HashInit();

    ThreadUnlock(cft_vscope);
}

/*******************************************************************/

void ScopeCopy(const char *new_scopename, const Scope *old_scope)
/*
 * Thread safe
 */
{
    ScopeNew(new_scopename);

    if (!ThreadLock(cft_vscope))
    {
        Log(LOG_LEVEL_ERR, "Could not lock VSCOPE");
        return;
    }

    if (old_scope)
    {
        Scope *np = ScopeGet(new_scopename);
        HashCopy(np->hashtable, old_scope->hashtable);
    }

    ThreadUnlock(cft_vscope);
}

/*******************************************************************/
/* Stack frames                                                    */
/*******************************************************************/

void ScopePushThis()
{
    Scope *op;
    char name[CF_MAXVARSIZE];

    op = ScopeGet("this");

    if (op == NULL)
    {
        return;
    }

    int frame_index = RlistLen(CF_STCK) - 1;
    {
        Rlist *rp = xmalloc(sizeof(Rlist));

        rp->next = CF_STCK;
        rp->item = op;
        rp->type = CF_STACK;
        CF_STCK = rp;
    }
    snprintf(name, CF_MAXVARSIZE, "this_%d", frame_index);
    free(op->scope);
    op->scope = xstrdup(name);
}

/*******************************************************************/

void ScopePopThis()
{
    Scope *op = NULL;

    if (RlistLen(CF_STCK) > 0)
    {
        ScopeClear("this");
        {
            Rlist *rp = CF_STCK;

            if (CF_STCK == NULL)
            {
                ProgrammingError("Attempt to pop from empty stack");
            }

            op = rp->item;

            if (rp->next == NULL)       /* only one left */
            {
                CF_STCK = (void *) NULL;
            }
            else
            {
                CF_STCK = rp->next;
            }

            free((char *) rp);
        }
        if (op == NULL)
        {
            return;
        }

        free(op->scope);
        op->scope = xstrdup("this");
    }
}

void ScopeToList(Scope *sp, Rlist **list)
{
    if (sp == NULL)
    {
        return;
    }

    AssocHashTableIterator i = HashIteratorInit(sp->hashtable);
    CfAssoc *assoc;

    while ((assoc = HashIteratorNext(&i)))
    {
        RlistPrependScalar(list, assoc->lval);
    }
}

bool ScopeIsReserved(const char *scope)
{
    return strcmp("const", scope) == 0
            || strcmp("edit", scope) == 0
            || strcmp("match", scope) == 0
            || strcmp("mon", scope) == 0
            || strcmp("sys", scope) == 0
            || strcmp("this", scope) == 0;
}

void ScopeNewScalar(EvalContext *ctx, VarRef lval, const char *rval, DataType dt)
{
    assert(!ScopeIsReserved(lval.scope));
    if (ScopeIsReserved(lval.scope))
    {
        ScopeNewSpecialScalar(ctx, lval.scope, lval.lval, rval, dt);
    }

    Rval rvald;
    if (EvalContextVariableGet(ctx, lval, &rvald, NULL))
    {
        ScopeDeleteScalar(lval);
    }

/*
 * We know AddVariableHash does not change passed Rval structure or its
 * contents, but we have no easy way to express it in C type system, hence cast.
 */
    EvalContextVariablePut(ctx, lval, (Rval) {(char *) rval, RVAL_TYPE_SCALAR }, dt);
}

void ScopeNewSpecialScalar(EvalContext *ctx, const char *scope, const char *lval, const char *rval, DataType dt)
{
    assert(ScopeIsReserved(scope));

    Rval rvald;
    if (EvalContextVariableGet(ctx, (VarRef) { NULL, scope, lval }, &rvald, NULL))
    {
        ScopeDeleteSpecialScalar(scope, lval);
    }

    EvalContextVariablePut(ctx, (VarRef) { NULL, scope, lval }, (Rval) {(char *) rval, RVAL_TYPE_SCALAR }, dt);
}

/*******************************************************************/

void ScopeDeleteScalar(VarRef lval)
{
    assert(!ScopeIsReserved(lval.scope));
    if (ScopeIsReserved(lval.scope))
    {
        ScopeDeleteSpecialScalar(lval.scope, lval.lval);
    }

    Scope *scope = ScopeGet(lval.scope);

    if (scope == NULL)
    {
        return;
    }

    if (HashDeleteElement(scope->hashtable, lval.lval) == false)
    {
        Log(LOG_LEVEL_DEBUG, "Attempt to delete non-existent variable %s in scope %s\n", lval.lval, lval.scope);
    }
}

void ScopeDeleteSpecialScalar(const char *scope, const char *lval)
{
    assert(ScopeIsReserved(scope));

    Scope *scope_ptr = ScopeGet(scope);

    if (scope_ptr == NULL)
    {
        return;
    }

    if (HashDeleteElement(scope_ptr->hashtable, lval) == false)
    {
        Log(LOG_LEVEL_DEBUG, "Attempt to delete non-existent variable '%s' in scope '%s'", lval, scope);
    }
}

/*******************************************************************/

void ScopeNewList(EvalContext *ctx, VarRef lval, void *rval, DataType dt)
{
    assert(!ScopeIsReserved(lval.scope));
    if (ScopeIsReserved(lval.scope))
    {
        ScopeNewSpecialScalar(ctx, lval.scope, lval.lval, rval, dt);
    }
    Rval rvald;

    if (EvalContextVariableGet(ctx, lval, &rvald, NULL))
    {
        ScopeDeleteVariable(lval.scope, lval.lval);
    }

    EvalContextVariablePut(ctx, lval, (Rval) {rval, RVAL_TYPE_LIST }, dt);
}

void ScopeNewSpecialList(EvalContext *ctx, const char *scope, const char *lval, void *rval, DataType dt)
{
    assert(ScopeIsReserved(scope));

    Rval rvald;

    if (EvalContextVariableGet(ctx, (VarRef) { NULL, scope, lval }, &rvald, NULL))
    {
        ScopeDeleteVariable(scope, lval);
    }

    EvalContextVariablePut(ctx, (VarRef) { NULL, scope, lval }, (Rval) {rval, RVAL_TYPE_LIST }, dt);
}

/*******************************************************************/

void ScopeDeleteVariable(const char *scope, const char *id)
{
    Scope *ptr = ScopeGet(scope);

    if (ptr == NULL)
    {
        return;
    }

    if (HashDeleteElement(ptr->hashtable, id) == false)
    {
        Log(LOG_LEVEL_DEBUG, "No variable matched '%s' for removal", id);
    }
}

/*******************************************************************/

int CompareVariableValue(Rval rval, CfAssoc *ap)
{
    const Rlist *list, *rp;

    if (ap == NULL || rval.item == NULL)
    {
        return 1;
    }

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        return strcmp(ap->rval.item, rval.item);

    case RVAL_TYPE_LIST:
        list = (const Rlist *) rval.item;

        for (rp = list; rp != NULL; rp = rp->next)
        {
            if (!CompareVariableValue((Rval) {rp->item, rp->type}, ap))
            {
                return -1;
            }
        }

        return 0;

    default:
        return 0;
    }

    return strcmp(ap->rval.item, rval.item);
}

bool UnresolvedVariables(const CfAssoc *ap, RvalType rtype)
{
    if (ap == NULL)
    {
        return false;
    }

    switch (rtype)
    {
    case RVAL_TYPE_SCALAR:
        return IsCf3VarString(ap->rval.item);

    case RVAL_TYPE_LIST:
        {
            for (const Rlist *rp = ap->rval.item; rp != NULL; rp = rp->next)
            {
                if (IsCf3VarString(rp->item))
                {
                    return true;
                }
            }
        }
        return false;

    default:
        return false;
    }
}



/*******************************************************************/

void ScopeDeRefListsInHashtable(char *scope, Rlist *namelist, Rlist *dereflist)
// Go through scope and for each variable in name-list, replace with a
// value from the deref "lol" (list of lists) clock
{
    int len;
    Scope *ptr;
    Rlist *rp;
    CfAssoc *cplist;
    AssocHashTableIterator i;
    CfAssoc *assoc;

    if ((len = RlistLen(namelist)) != RlistLen(dereflist))
    {
        Log(LOG_LEVEL_ERR, "Name list %d, dereflist %d", len, RlistLen(dereflist));
        ProgrammingError("Software Error DeRefLists... correlated lists not same length");
    }

    if (len == 0)
    {
        return;
    }

    ptr = ScopeGet(scope);
    i = HashIteratorInit(ptr->hashtable);

    while ((assoc = HashIteratorNext(&i)))
    {
        for (rp = dereflist; rp != NULL; rp = rp->next)
        {
            cplist = (CfAssoc *) rp->item;

            if (strcmp(cplist->lval, assoc->lval) == 0)
            {
                /* Link up temp hash to variable lol */

                if (rp->state_ptr == NULL || rp->state_ptr->type == RVAL_TYPE_FNCALL)
                {
                    /* Unexpanded function, or blank variable must be skipped. */
                    return;
                }

                if (rp->state_ptr)
                {
                    // must first free existing rval in scope, then allocate new (should always be string)
                    RvalDestroy(assoc->rval);

                    // avoids double free - borrowing value from lol (freed in DeleteScope())
                    assoc->rval.item = xstrdup(rp->state_ptr->item);
                }

                switch (assoc->dtype)
                {
                case DATA_TYPE_STRING_LIST:
                    assoc->dtype = DATA_TYPE_STRING;
                    assoc->rval.type = RVAL_TYPE_SCALAR;
                    break;
                case DATA_TYPE_INT_LIST:
                    assoc->dtype = DATA_TYPE_INT;
                    assoc->rval.type = RVAL_TYPE_SCALAR;
                    break;
                case DATA_TYPE_REAL_LIST:
                    assoc->dtype = DATA_TYPE_REAL;
                    assoc->rval.type = RVAL_TYPE_SCALAR;
                    break;
                default:
                    /* Only lists need to be converted */
                    break;
                }
            }
        }
    }
}

int ScopeMapBodyArgs(EvalContext *ctx, const char *scopeid, Rlist *give, const Rlist *take)
{
    Rlist *rpg = NULL;
    const Rlist *rpt = NULL;
    FnCall *fp;
    DataType dtg = DATA_TYPE_NONE, dtt = DATA_TYPE_NONE;
    char *lval;
    void *rval;
    int len1, len2;

    len1 = RlistLen(give);
    len2 = RlistLen(take);

    if (len1 != len2)
    {
        Log(LOG_LEVEL_ERR, "Argument mismatch in body template give[+args] = %d, take[-args] = %d", len1, len2);
        return false;
    }

    for (rpg = give, rpt = take; rpg != NULL && rpt != NULL; rpg = rpg->next, rpt = rpt->next)
    {
        dtg = StringDataType(ctx, scopeid, (char *) rpg->item);
        dtt = StringDataType(ctx, scopeid, (char *) rpt->item);

        if (dtg != dtt)
        {
            Log(LOG_LEVEL_ERR, "Type mismatch between logical/formal parameters %s/%s", (char *) rpg->item,
                  (char *) rpt->item);
            Log(LOG_LEVEL_ERR, "%s is %s whereas %s is %s", (char *) rpg->item, DataTypeToString(dtg),
                  (char *) rpt->item, DataTypeToString(dtt));
        }

        switch (rpg->type)
        {
        case RVAL_TYPE_SCALAR:
            lval = (char *) rpt->item;
            rval = rpg->item;
            EvalContextVariablePut(ctx, (VarRef) { NULL, scopeid, lval }, (Rval) { rval, RVAL_TYPE_SCALAR }, dtg);
            break;

        case RVAL_TYPE_LIST:
            lval = (char *) rpt->item;
            rval = rpg->item;
            EvalContextVariablePut(ctx, (VarRef) { NULL, scopeid, lval }, (Rval) { rval, RVAL_TYPE_LIST }, dtg);
            break;

        case RVAL_TYPE_FNCALL:
            fp = (FnCall *) rpg->item;
            dtg = DATA_TYPE_NONE;
            {
                const FnCallType *fncall_type = FnCallTypeGet(fp->name);
                if (fncall_type)
                {
                    dtg = fncall_type->dtype;
                }
            }

            FnCallResult res = FnCallEvaluate(ctx, fp, NULL);

            if (res.status == FNCALL_FAILURE && THIS_AGENT_TYPE != AGENT_TYPE_COMMON)
            {
                Log(LOG_LEVEL_VERBOSE, "Embedded function argument does not resolve to a name - probably too many evaluation levels for '%s'",
                    fp->name);
            }
            else
            {
                FnCallDestroy(fp);

                rpg->item = res.rval.item;
                rpg->type = res.rval.type;

                lval = (char *) rpt->item;
                rval = rpg->item;

                EvalContextVariablePut(ctx, (VarRef) { NULL, scopeid, lval }, (Rval) {rval, RVAL_TYPE_SCALAR }, dtg);
            }

            break;

        default:
            /* Nothing else should happen */
            ProgrammingError("Software error: something not a scalar/function in argument literal");
        }
    }

    return true;
}

/*******************************************************************/
/* Utility functions                                               */
/*******************************************************************/

void SplitScopeName(const char *scope, char ns_out[CF_MAXVARSIZE], char bundle_out[CF_MAXVARSIZE])
{
    assert(scope);

    char *split_point = strchr(scope, CF_NS);
    if (split_point)
    {
        strncpy(ns_out, scope, split_point - scope);
        strncpy(bundle_out, split_point + 1, 100);
    }
    else
    {
        strncpy(bundle_out, scope, 100);
    }
}

/*******************************************************************/

void JoinScopeName(const char *ns, const char *bundle, char scope_out[CF_MAXVARSIZE])
{
    assert(bundle);

    if (ns)
    {
        snprintf(scope_out, CF_MAXVARSIZE, "%s%c%s", ns, CF_NS, bundle);
    }
    else
    {
        snprintf(scope_out, CF_MAXVARSIZE, "%s", bundle);
    }
}
