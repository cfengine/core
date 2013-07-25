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

Scope *SCOPE_MATCH = NULL;

/*******************************************************************/

const char *SpecialScopeToString(SpecialScope scope)
{
    switch (scope)
    {
    case SPECIAL_SCOPE_CONST:
        return "const";
    case SPECIAL_SCOPE_EDIT:
        return "edit";
    case SPECIAL_SCOPE_MATCH:
        return "match";
    case SPECIAL_SCOPE_MON:
        return "mon";
    case SPECIAL_SCOPE_SYS:
        return "sys";
    case SPECIAL_SCOPE_THIS:
        return "this";
    default:
        ProgrammingError("Unhandled special scope");
    }
}

SpecialScope SpecialScopeFromString(const char *scope)
{
    if (strcmp("const", scope) == 0)
    {
        return SPECIAL_SCOPE_CONST;
    }
    else if (strcmp("edit", scope) == 0)
    {
        return SPECIAL_SCOPE_EDIT;
    }
    else if (strcmp("match", scope) == 0)
    {
        return SPECIAL_SCOPE_MATCH;
    }
    else if (strcmp("mon", scope) == 0)
    {
        return SPECIAL_SCOPE_MON;
    }
    else if (strcmp("sys", scope) == 0)
    {
        return SPECIAL_SCOPE_SYS;
    }
    else if (strcmp("this", scope) == 0)
    {
        return SPECIAL_SCOPE_THIS;
    }
    else
    {
        return SPECIAL_SCOPE_NONE;
    }
}

Scope *ScopeNew(const char *ns, const char *scope)
{
    assert(scope);
    assert(strcmp(scope, "edit") != 0);

    if (!ns)
    {
        ns = "default";
    }

    if (!ThreadLock(cft_vscope))
    {
        Log(LOG_LEVEL_ERR, "Could not lock VSCOPE");
        return NULL;
    }

    for (Scope *ptr = VSCOPE; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->ns, ns) == 0 && strcmp(ptr->scope, scope) == 0)
        {
            ThreadUnlock(cft_vscope);
            return NULL;
        }
    }

    Scope *ptr = xcalloc(1, sizeof(Scope));

    ptr->hashtable = HashInit();
    ptr->ns = xstrdup(ns);
    ptr->scope = xstrdup(scope);
    assert(ptr->scope);
    ptr->next = VSCOPE;
    VSCOPE = ptr;

    assert(VSCOPE->scope);

    ThreadUnlock(cft_vscope);

    return ptr;
}

void ScopePutMatch(int index, const char *value)
{
    if (!SCOPE_MATCH)
    {
        SCOPE_MATCH = ScopeNew(NULL, "match");
    }
    Scope *ptr = SCOPE_MATCH;

    char lval[4] = { 0 };
    snprintf(lval, 3, "%d", index);

    Rval rval = (Rval) { value, RVAL_TYPE_SCALAR };

    CfAssoc *assoc = HashLookupElement(ptr->hashtable, lval);

    if (assoc)
    {
        if (CompareVariableValue(rval, assoc->rval) == 0)
        {
            /* Identical value, keep as is */
        }
        else
        {
            /* Different value, bark and replace */
            if (!UnresolvedVariables(assoc->rval, RVAL_TYPE_SCALAR))
            {
                Log(LOG_LEVEL_INFO, "Duplicate selection of value for variable '%s' in scope '%s'", lval, ptr->scope);
            }
            RvalDestroy(assoc->rval);
            assoc->rval = RvalCopy(rval);
            assoc->dtype = DATA_TYPE_STRING;
            Log(LOG_LEVEL_DEBUG, "Stored '%s' in context '%s'", lval, "match");
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

Scope *ScopeGet(const char *ns, const char *scope)
/* 
 * Not thread safe - returns pointer to global memory
 */
{
    if (!scope)
    {
        return NULL;
    }

    if (!ns)
    {
        ns = "default";
    }

    if (strcmp("match", scope) == 0)
    {
        return SCOPE_MATCH;
    }

    for (Scope *cp = VSCOPE; cp != NULL; cp = cp->next)
    {
        if (strcmp(cp->ns, ns) == 0 && strcmp(cp->scope, scope) == 0)
        {
            return cp;
        }
    }

    return NULL;
}

bool ScopeExists(const char *ns, const char *name)
{
    return ScopeGet(ns, name) != NULL;
}

void ScopeAugment(EvalContext *ctx, const Bundle *bp, const Promise *pp, const Rlist *arguments)
{
    if (RlistLen(bp->args) != RlistLen(arguments))
    {
        Log(LOG_LEVEL_ERR, "While constructing scope '%s'", bp->name);
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
                VarRef *ref = VarRefParseFromBundle(naked, pbp);
                EvalContextVariableGet(ctx, ref, &retval, &vtype);
                VarRefDestroy(ref);
            }
            else
            {
                VarRef *ref = VarRefParseFromBundle(naked, bp);
                EvalContextVariableGet(ctx, ref, &retval, &vtype);
                VarRefDestroy(ref);
            }

            switch (vtype)
            {
            case DATA_TYPE_STRING_LIST:
            case DATA_TYPE_INT_LIST:
            case DATA_TYPE_REAL_LIST:
                {
                    VarRef *ref = VarRefParseFromBundle(lval, bp);
                    EvalContextVariablePut(ctx, ref, (Rval) { retval.item, RVAL_TYPE_LIST}, DATA_TYPE_STRING_LIST);
                    VarRefDestroy(ref);
                }
                break;
            default:
                {
                    Log(LOG_LEVEL_ERR, "List parameter '%s' not found while constructing scope '%s' - use @(scope.variable) in calling reference", naked, bp->name);
                    VarRef *ref = VarRefParseFromBundle(lval, bp);
                    EvalContextVariablePut(ctx, ref, (Rval) { rpr->item, RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
                    VarRefDestroy(ref);
                }
                break;
            }
        }
        else
        {
            switch(rpr->type)
            {
            case RVAL_TYPE_SCALAR:
                {
                    VarRef *ref = VarRefParseFromBundle(lval, bp);
                    EvalContextVariablePut(ctx, ref, (Rval) { rpr->item, RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
                    VarRefDestroy(ref);
                }
                break;

            case RVAL_TYPE_FNCALL:
                {
                    FnCall *subfp = rpr->item;
                    Rval rval = FnCallEvaluate(ctx, subfp, pp).rval;
                    if (rval.type == RVAL_TYPE_SCALAR)
                    {
                        VarRef *ref = VarRefParseFromBundle(lval, bp);
                        EvalContextVariablePut(ctx, ref, (Rval) { rval.item, RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
                        VarRefDestroy(ref);
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

    return;
}

/*******************************************************************/

static void ScopeDelete(Scope *scope)
{
    if (!ThreadLock(cft_vscope))
    {
        Log(LOG_LEVEL_ERR, "Could not lock VSCOPE");
        return;
    }

    Scope *prev = NULL;

    for (Scope *curr = VSCOPE; curr; prev = curr, curr = curr->next)
    {
        if (curr != scope)
        {
            continue;
        }

        if (!prev)
        {
            VSCOPE = scope->next;
        }
        else
        {
            prev->next = curr->next;
        }

        free(scope->scope);
        HashFree(scope->hashtable);
        free(scope);
        break;
    }

    ThreadUnlock(cft_vscope);
}

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

    ThreadUnlock(cft_vscope);
}

/*******************************************************************/

void ScopeClear(const char *ns, const char *name)
{
    assert(name);
    assert(!ScopeIsReserved(name));

    if (!ns)
    {
        ns = "default";
    }

    if (!ThreadLock(cft_vscope))
    {
        Log(LOG_LEVEL_ERR, "Could not lock VSCOPE");
        return;
    }

    Scope *scope = ScopeGet(ns, name);
    if (!scope)
    {
        Log(LOG_LEVEL_DEBUG, "No scope '%s' to clear", name);
        ThreadUnlock(cft_vscope);
        return;
    }

    HashFree(scope->hashtable);
    scope->hashtable = HashInit();
    Log(LOG_LEVEL_DEBUG, "Scope '%s' cleared", name);

    ThreadUnlock(cft_vscope);
}

void ScopeClearSpecial(SpecialScope scope)
{
    if (!ThreadLock(cft_vscope))
    {
        Log(LOG_LEVEL_ERR, "Could not lock VSCOPE");
        return;
    }

    Scope *ptr = ScopeGet(NULL, SpecialScopeToString(scope));
    if (!ptr)
    {
        Log(LOG_LEVEL_DEBUG, "No special scope '%s' to clear", SpecialScopeToString(scope));
        ThreadUnlock(cft_vscope);
        return;
    }

    HashFree(ptr->hashtable);
    ptr->hashtable = HashInit();
    Log(LOG_LEVEL_DEBUG, "Special scope '%s' cleared", SpecialScopeToString(scope));

    ThreadUnlock(cft_vscope);
}

/*******************************************************************/

void ScopeCopy(const char *new_ns, const char *new_scopename, const Scope *old_scope)
/*
 * Thread safe
 */
{
    ScopeNew(new_ns, new_scopename);

    if (!ThreadLock(cft_vscope))
    {
        Log(LOG_LEVEL_ERR, "Could not lock VSCOPE");
        return;
    }

    if (old_scope)
    {
        Scope *np = ScopeGet(new_ns, new_scopename);
        HashCopy(np->hashtable, old_scope->hashtable);
    }

    ThreadUnlock(cft_vscope);
}

/*******************************************************************/
/* Stack frames                                                    */
/*******************************************************************/

void ScopePushThis()
{
    static const char RVAL_TYPE_STACK = 'k';

    Scope *op = ScopeGet(NULL, "this");
    if (!op)
    {
        return;
    }

    int frame_index = RlistLen(CF_STCK);
    char name[CF_MAXVARSIZE];
    snprintf(name, CF_MAXVARSIZE, "this_%d", frame_index + 1);
    free(op->scope);
    free(op->ns);
    op->scope = xstrdup(name);

    Rlist *rp = xmalloc(sizeof(Rlist));

    rp->next = CF_STCK;
    rp->item = op;
    rp->type = RVAL_TYPE_STACK;
    CF_STCK = rp;

    ScopeNew(NULL, "this");
}

/*******************************************************************/

void ScopePopThis()
{
    if (RlistLen(CF_STCK) > 0)
    {
        Scope *current_this = ScopeGet(NULL, "this");
        if (current_this)
        {

            ScopeDelete(current_this);
        }

        Rlist *rp = CF_STCK;
        CF_STCK = CF_STCK->next;

        Scope *new_this = rp->item;
        free(new_this->scope);
        new_this->scope = xstrdup("this");
        new_this->ns = xstrdup("default");

        free(rp);
    }
    else
    {
        ProgrammingError("Attempt to pop from empty stack");
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

/**
 * @WARNING Don't call ScopeDelete*() before this, it's unnecessary.
 */
void ScopeNewSpecial(EvalContext *ctx, SpecialScope scope, const char *lval, const void *rval, DataType dt)
{
    Rval rvald;
    VarRef *ref = VarRefParseFromScope(lval, SpecialScopeToString(scope));
    if (EvalContextVariableGet(ctx, ref, &rvald, NULL))
    {
        ScopeDeleteSpecial(scope, lval);
    }

    EvalContextVariablePut(ctx, ref, (Rval) { rval, DataTypeToRvalType(dt) }, dt);

    VarRefDestroy(ref);
}

/*******************************************************************/

void ScopeDeleteVariable(const VarRef *ref)
{
    assert(!ScopeIsReserved(ref->scope));

    Scope *scope = ScopeGet(ref->ns, ref->scope);

    if (scope == NULL)
    {
        return;
    }

    char *legacy_lval = VarRefToString(ref, false);

    if (HashDeleteElement(scope->hashtable, legacy_lval) == false)
    {
        Log(LOG_LEVEL_DEBUG, "Attempt to delete non-existent variable '%s' in scope '%s'", ref->lval, ref->scope);
    }

    free(legacy_lval);
}

void ScopeDeleteSpecial(SpecialScope scope, const char *lval)
{
    Scope *scope_ptr = ScopeGet(NULL, SpecialScopeToString(scope));

    if (scope_ptr == NULL)
    {
        Log(LOG_LEVEL_WARNING,
            "Attempt to delete variable '%s' in non-existent scope '%s'",
            lval, SpecialScopeToString(scope));
        return;
    }

    if (HashDeleteElement(scope_ptr->hashtable, lval) == false)
    {
        Log(LOG_LEVEL_WARNING,
            "Attempt to delete non-existent variable '%s' in scope '%s'",
            lval, SpecialScopeToString(scope));
        return;
    }

    Log(LOG_LEVEL_DEBUG, "Deleted existent variable '%s' in scope '%s'",
        lval, SpecialScopeToString(scope));
}

/*******************************************************************/

int CompareVariableValue(Rval a, Rval b)
{
    const Rlist *list, *rp;

    if (!a.item || !b.item)
    {
        return 1;
    }

    switch (a.type)
    {
    case RVAL_TYPE_SCALAR:
        return strcmp(b.item, a.item);

    case RVAL_TYPE_LIST:
        list = (const Rlist *) a.item;

        for (rp = list; rp != NULL; rp = rp->next)
        {
            if (!CompareVariableValue((Rval) {rp->item, rp->type}, b))
            {
                return -1;
            }
        }

        return 0;

    default:
        return 0;
    }

    return strcmp(b.item, a.item);
}

bool UnresolvedVariables(Rval rval, RvalType rtype)
{
    switch (rtype)
    {
    case RVAL_TYPE_SCALAR:
        return IsCf3VarString(rval.item);

    case RVAL_TYPE_LIST:
        {
            for (const Rlist *rp = rval.item; rp != NULL; rp = rp->next)
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

void ScopeDeRefListsInThisScope(const Rlist *dereflist)
// Go through scope and for each variable in name-list, replace with a
// value from the deref "lol" (list of lists) clock
{
    Scope *ptr = ScopeGet(NULL, "this");
    AssocHashTableIterator i = HashIteratorInit(ptr->hashtable);

    CfAssoc *assoc = NULL;
    while ((assoc = HashIteratorNext(&i)))
    {
        for (const Rlist *rp = dereflist; rp != NULL; rp = rp->next)
        {
            CfAssoc *cplist = rp->item;

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

void ScopeMapBodyArgs(EvalContext *ctx, const Body *body, const Rlist *args)
{
    const Rlist *arg = NULL;
    const Rlist *param = NULL;

    for (arg = args, param = body->args; arg != NULL && param != NULL; arg = arg->next, param = param->next)
    {
        DataType arg_type = StringDataType(ctx, RlistScalarValue(arg));
        DataType param_type = StringDataType(ctx, RlistScalarValue(param));

        if (arg_type != param_type)
        {
            Log(LOG_LEVEL_ERR, "Type mismatch between logical/formal parameters %s/%s", (char *) arg->item,
                  (char *) param->item);
            Log(LOG_LEVEL_ERR, "%s is %s whereas %s is %s", (char *) arg->item, DataTypeToString(arg_type),
                  (char *) param->item, DataTypeToString(param_type));
        }

        switch (arg->type)
        {
        case RVAL_TYPE_SCALAR:
            {
                const char *lval = RlistScalarValue(param);
                void *rval = arg->item;
                VarRef *ref = VarRefParseFromNamespaceAndScope(lval, NULL, "body", CF_NS, '.');
                EvalContextVariablePut(ctx, ref, (Rval) { rval, RVAL_TYPE_SCALAR }, arg_type);
            }
            break;

        case RVAL_TYPE_LIST:
            {
                const char *lval = RlistScalarValue(param->item);
                void *rval = arg->item;
                VarRef *ref = VarRefParseFromNamespaceAndScope(lval, NULL, "body", CF_NS, '.');
                EvalContextVariablePut(ctx, ref, (Rval) { rval, RVAL_TYPE_LIST }, arg_type);
                VarRefDestroy(ref);
            }
            break;

        case RVAL_TYPE_FNCALL:
            {
                FnCall *fp = arg->item;
                arg_type = DATA_TYPE_NONE;
                {
                    const FnCallType *fncall_type = FnCallTypeGet(fp->name);
                    if (fncall_type)
                    {
                        arg_type = fncall_type->dtype;
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

                    const char *lval = RlistScalarValue(param);
                    void *rval = res.rval.item;

                    VarRef *ref = VarRefParseFromNamespaceAndScope(lval, NULL, "body", CF_NS, '.');
                    EvalContextVariablePut(ctx, ref, (Rval) {rval, RVAL_TYPE_SCALAR }, res.rval.type);
                    VarRefDestroy(ref);
                }
            }

            break;

        default:
            /* Nothing else should happen */
            ProgrammingError("Software error: something not a scalar/function in argument literal");
        }
    }
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
        strncpy(bundle_out, split_point + 1, CF_MAXVARSIZE);
    }
    else
    {
        strncpy(bundle_out, scope, CF_MAXVARSIZE);
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
