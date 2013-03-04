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

#include "scope.h"

#include "vars.h"
#include "expand.h"
#include "hashes.h"
#include "unix.h"
#include "cfstream.h"
#include "fncall.h"
#include "transaction.h"
#include "logging.h"
#include "misc_lib.h"
#include "rlist.h"
#include "conversion.h"
#include "syntax.h"

#include <assert.h>

/*******************************************************************/

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
    
    CfDebug("Searching for scope context %s\n", scope);

    for (Scope *cp = VSCOPE; cp != NULL; cp = cp->next)
    {
        if (strcmp(cp->scope, name) == 0)
        {
            CfDebug("Found scope reference %s\n", scope);
            return cp;
        }
    }

    return NULL;
}

/*******************************************************************/

void ScopeSet(char *id)
{
    strlcpy(CONTEXTID, id, CF_MAXVARSIZE);
}

/*******************************************************************/

void ScopeSetNew(char *id)
{
    ScopeNew(id);
    ScopeSet(id);
}

/*******************************************************************/

void ScopeNew(const char *name)
/*
 * Thread safe
 */
{
    Scope *ptr;

    CfDebug("Adding scope data %s\n", name);

    if (!ThreadLock(cft_vscope))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not lock VSCOPE");
        return;
    }

    for (ptr = VSCOPE; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->scope, name) == 0)
        {
            ThreadUnlock(cft_vscope);
            CfDebug("SCOPE Object %s already exists\n", name);
            return;
        }
    }

    ptr = xcalloc(1, sizeof(Scope));

    ptr->next = VSCOPE;
    ptr->scope = xstrdup(name);
    ptr->hashtable = HashInit();
    VSCOPE = ptr;
    ThreadUnlock(cft_vscope);
}

/*******************************************************************/

void ScopeAugment(EvalContext *ctx, char *scope, char *ns, Rlist *lvals, Rlist *rvals)
{
    Scope *ptr;
    Rlist *rpl, *rpr;
    Rval retval;
    char *lval, naked[CF_BUFSIZE];
    AssocHashTableIterator i;
    CfAssoc *assoc;

    if (RlistLen(lvals) != RlistLen(rvals))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "While constructing scope \"%s\"\n", scope);
        fprintf(stderr, "Formal = ");
        RlistShow(stderr, lvals);
        fprintf(stderr, ", Actual = ");
        RlistShow(stderr, rvals);
        fprintf(stderr, "\n");
        FatalError("Augment scope, formal and actual parameter mismatch is fatal");
    }

    for (rpl = lvals, rpr = rvals; rpl != NULL; rpl = rpl->next, rpr = rpr->next)
    {
        lval = (char *) rpl->item;

        CfOut(OUTPUT_LEVEL_VERBOSE, "", "    ? Augment scope %s with %s (%c)\n", scope, lval, rpr->type);

        // CheckBundleParameters() already checked that there is no namespace collision
        // By this stage all functions should have been expanded, so we only have scalars left

        if (IsNakedVar(rpr->item, '@'))
        {
            DataType vtype;
            char qnaked[CF_MAXVARSIZE];
            
            GetNaked(naked, rpr->item);

            if (IsQualifiedVariable(naked) && strchr(naked, CF_NS) == NULL)
            {
                snprintf(qnaked, CF_MAXVARSIZE, "%s%c%s", ns, CF_NS, naked);
            }
            
            vtype = ScopeGetVariable(scope, qnaked, &retval);

            switch (vtype)
            {
            case DATA_TYPE_STRING_LIST:
            case DATA_TYPE_INT_LIST:
            case DATA_TYPE_REAL_LIST:
                ScopeNewList(scope, lval, RvalCopy((Rval) {retval.item, RVAL_TYPE_LIST}).item, DATA_TYPE_STRING_LIST);
                break;
            default:
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! List parameter \"%s\" not found while constructing scope \"%s\" - use @(scope.variable) in calling reference", qnaked, scope);
                ScopeNewScalar(scope, lval, rpr->item, DATA_TYPE_STRING);
                break;
            }
        }
        else
        {
            FnCall *subfp;
            Promise *pp = NULL; // This argument should really get passed down.

            switch(rpr->type)
            {
            case RVAL_TYPE_SCALAR:
                ScopeNewScalar(scope, lval, rpr->item, DATA_TYPE_STRING);
                break;

            case RVAL_TYPE_FNCALL:
                subfp = (FnCall *) rpr->item;
                Rval rval = FnCallEvaluate(ctx, subfp, pp).rval;
                if (rval.type == RVAL_TYPE_SCALAR)
                {
                    ScopeNewScalar(scope, lval, rval.item, DATA_TYPE_STRING);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "Only functions returning scalars can be used as arguments");
                }
                break;
            default:
                ProgrammingError("An argument neither a scalar nor a list seemed to appear. Impossible");
            }
        }
    }

/* Check that there are no danglers left to evaluate in the hash table itself */

    ptr = ScopeGet(scope);

    i = HashIteratorInit(ptr->hashtable);

    while ((assoc = HashIteratorNext(&i)))
    {
        retval = ExpandPrivateRval(scope, assoc->rval);
        // Retain the assoc, just replace rval
        RvalDestroy(assoc->rval);
        assoc->rval = retval;
    }

    return;
}

/*******************************************************************/

void ScopeDeleteAll()
{
    Scope *ptr, *this;

    CfDebug("Deleting all scoped variables\n");

    if (!ThreadLock(cft_vscope))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not lock VSCOPE");
        return;
    }

    ptr = VSCOPE;

    while (ptr != NULL)
    {
        this = ptr;
        CfDebug(" -> Deleting scope %s\n", ptr->scope);
        HashFree(this->hashtable);
        free(this->scope);
        ptr = this->next;
        free((char *) this);
    }

    VSCOPE = NULL;

    ThreadUnlock(cft_vscope);
}

/*******************************************************************/

void ScopeDelete(char *name)
/*
 * Thread safe
 */
{
    Scope *ptr, *prev = NULL;
    int found = false;

    CfDebug("Deleting scope %s\n", name);

    if (!ThreadLock(cft_vscope))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not lock VSCOPE");
        return;
    }

    for (ptr = VSCOPE; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->scope, name) == 0)
        {
            CfDebug("Object %s exists\n", name);
            found = true;
            break;
        }
        else
        {
            prev = ptr;
        }
    }

    if (!found)
    {
        CfDebug("No such scope to delete\n");
        ThreadUnlock(cft_vscope);
        return;
    }

    if (ptr == VSCOPE)
    {
        VSCOPE = ptr->next;
    }
    else
    {
        prev->next = ptr->next;
    }

    HashFree(ptr->hashtable);

    free(ptr->scope);
    free((char *) ptr);

    ThreadUnlock(cft_vscope);
}

/*******************************************************************/

void ScopeCopy(const char *new_scopename, const char *old_scopename)
/*
 * Thread safe
 */
{
    Scope *op, *np;

    CfDebug("\n*\nCopying scope data %s to %s\n*\n", old_scopename, new_scopename);

    ScopeNew(new_scopename);

    if (!ThreadLock(cft_vscope))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not lock VSCOPE");
        return;
    }

    if ((op = ScopeGet(old_scopename)))
    {
        np = ScopeGet(new_scopename);
        HashCopy(np->hashtable, op->hashtable);
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

    CF_STCKFRAME++;
    RlistPushStack(&CF_STCK, (void *) op);
    snprintf(name, CF_MAXVARSIZE, "this_%d", CF_STCKFRAME);
    free(op->scope);
    op->scope = xstrdup(name);
}

/*******************************************************************/

void ScopePopThis()
{
    Scope *op = NULL;

    if (CF_STCKFRAME > 0)
    {
        ScopeDelete("this");
        RlistPopStack(&CF_STCK, (void *) &op, sizeof(op));
        if (op == NULL)
        {
            return;
        }

        CF_STCKFRAME--;
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

void ScopeNewScalar(const char *scope, const char *lval, const char *rval, DataType dt)
{
    Rval rvald;
    Scope *ptr;

    CfDebug("NewScalar(%s,%s,%s)\n", scope, lval, rval);

    ptr = ScopeGet(scope);

    if (ptr == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Attempt to add variable \"%s\" to non-existant scope \"%s\" - ignored", lval, scope);
        return;
    }

// Newscalar allocates memory through NewAssoc

    if (ScopeGetVariable(scope, lval, &rvald) != DATA_TYPE_NONE)
    {
        ScopeDeleteScalar(scope, lval);
    }

/*
 * We know AddVariableHash does not change passed Rval structure or its
 * contents, but we have no easy way to express it in C type system, hence cast.
 */
    ScopeAddVariableHash(scope, lval, (Rval) {(char *) rval, RVAL_TYPE_SCALAR }, dt, NULL, 0);
}

/*******************************************************************/

void ScopeDeleteScalar(const char *scope_name, const char *lval)
{
    Scope *scope = ScopeGet(scope_name);

    if (scope == NULL)
    {
        return;
    }

    if (HashDeleteElement(scope->hashtable, lval) == false)
    {
        CfDebug("Attempt to delete non-existent variable %s in scope %s\n", lval, scope_name);
    }
}

/*******************************************************************/

void ScopeNewList(const char *scope, const char *lval, void *rval, DataType dt)
{
    Rval rvald;

    if (ScopeGetVariable(scope, lval, &rvald) != DATA_TYPE_NONE)
    {
        ScopeDeleteVariable(scope, lval);
    }

    ScopeAddVariableHash(scope, lval, (Rval) {rval, RVAL_TYPE_LIST }, dt, NULL, 0);
}

/*******************************************************************/

DataType ScopeGetVariable(const char *scope, const char *lval, Rval *returnv)
{
    Scope *ptr = NULL;
    char scopeid[CF_MAXVARSIZE], vlval[CF_MAXVARSIZE], sval[CF_MAXVARSIZE];
    char expbuf[CF_EXPANDSIZE];
    CfAssoc *assoc;

    CfDebug("GetVariable(%s,%s) type=(to be determined)\n", scope, lval);

    if (lval == NULL)
    {
        *returnv = (Rval) {NULL, RVAL_TYPE_SCALAR };
        return DATA_TYPE_NONE;
    }

    if (!IsExpandable(lval))
    {
        strncpy(sval, lval, CF_MAXVARSIZE - 1);
    }
    else
    {
        if (ExpandScalar(lval, expbuf))
        {
            strncpy(sval, expbuf, CF_MAXVARSIZE - 1);
        }
        else
        {
            /* C type system does not allow us to express the fact that returned
               value may contain immutable string. */
            *returnv = (Rval) {(char *) lval, RVAL_TYPE_SCALAR };
            CfDebug("Couldn't expand array-like variable (%s) due to undefined dependencies\n", lval);
            return DATA_TYPE_NONE;
        }
    }

    if (IsQualifiedVariable(sval))
    {
        scopeid[0] = '\0';
        sscanf(sval, "%[^.].%s", scopeid, vlval);
        CfDebug("Variable identifier \"%s\" is prefixed with scope id \"%s\"\n", vlval, scopeid);
        ptr = ScopeGet(scopeid);
    }
    else
    {
        strlcpy(vlval, sval, sizeof(vlval));
        strlcpy(scopeid, scope, sizeof(scopeid));
    }

    if (ptr == NULL)
    {
        /* Assume current scope */
        strcpy(vlval, lval);
        ptr = ScopeGet(scopeid);
    }

    if (ptr == NULL)
    {
        CfDebug("Scope \"%s\" for variable \"%s\" does not seem to exist\n", scopeid, vlval);
        /* C type system does not allow us to express the fact that returned
           value may contain immutable string. */
        *returnv = (Rval) {(char *) lval, RVAL_TYPE_SCALAR };
        return DATA_TYPE_NONE;
    }

    CfDebug("GetVariable(%s,%s): using scope '%s' for variable '%s'\n", scopeid, vlval, ptr->scope, vlval);

    assoc = HashLookupElement(ptr->hashtable, vlval);

    if (assoc == NULL)
    {
        CfDebug("No such variable found %s.%s\n\n", scopeid, lval);
        /* C type system does not allow us to express the fact that returned
           value may contain immutable string. */


        *returnv = (Rval) {(char *) lval, RVAL_TYPE_SCALAR };
        return DATA_TYPE_NONE;

    }

    CfDebug("return final variable type=%s, value={\n", CF_DATATYPES[assoc->dtype]);

    if (DEBUG)
    {
        RvalShow(stdout, assoc->rval);
    }
    CfDebug("}\n");

    *returnv = assoc->rval;
    return assoc->dtype;
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
        CfDebug("No variable matched %s\n", id);
    }
}

bool ScopeVariableExistsInThis(const char *name)
{
    Rval rval;

    if (name == NULL)
    {
        return false;
    }

    if (ScopeGetVariable("this", name, &rval) == DATA_TYPE_NONE)
    {
        return false;
    }

    return true;
}

/*******************************************************************/

static int CompareVariableValue(Rval rval, CfAssoc *ap)
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

int ScopeAddVariableHash(const char *scope, const char *lval, Rval rval, DataType dtype, const char *fname,
                    int lineno)
{
    Scope *ptr;
    const Rlist *rp;
    CfAssoc *assoc;

    if (rval.type == RVAL_TYPE_SCALAR)
    {
        CfDebug("AddVariableHash(%s.%s=%s (%s) rtype=%c)\n", scope, lval, (const char *) rval.item, CF_DATATYPES[dtype],
                rval.type);
    }
    else
    {
        CfDebug("AddVariableHash(%s.%s=(list) (%s) rtype=%c)\n", scope, lval, CF_DATATYPES[dtype], rval.type);
    }

    if (lval == NULL || scope == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "scope.value = %s.%s", scope, lval);
        ProgrammingError("Bad variable or scope in a variable assignment, should not happen - forgotten to register a function call in fncall.c?");
    }

    if (rval.item == NULL)
    {
        CfDebug("No value to assignment - probably a parameter in an unused bundle/body\n");
        return false;
    }

    if (strlen(lval) > CF_MAXVARSIZE)
    {
        ReportError("variable lval too long");
        return false;
    }

/* If we are not expanding a body template, check for recursive singularities */

    if (strcmp(scope, "body") != 0)
    {
        switch (rval.type)
        {
        case RVAL_TYPE_SCALAR:

            if (StringContainsVar((char *) rval.item, lval))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Scalar variable %s.%s contains itself (non-convergent): %s", scope, lval,
                      (char *) rval.item);
                return false;
            }
            break;

        case RVAL_TYPE_LIST:

            for (rp = rval.item; rp != NULL; rp = rp->next)
            {
                if (StringContainsVar((char *) rp->item, lval))
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "List variable %s contains itself (non-convergent)", lval);
                    return false;
                }
            }
            break;

        default:
            break;
        }
    }

    ptr = ScopeGet(scope);

    if (ptr == NULL)
    {
        return false;
    }

// Look for outstanding lists in variable rvals

    if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
    {
        Rlist *listvars = NULL, *scalarvars = NULL;

        if (strcmp(CONTEXTID, "this") != 0)
        {
            MapIteratorsFromRval(CONTEXTID, &scalarvars, &listvars, rval, NULL);

            if (listvars != NULL)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Redefinition of variable \"%s\" (embedded list in RHS) in context \"%s\"",
                      lval, CONTEXTID);
            }

            RlistDestroy(scalarvars);
            RlistDestroy(listvars);
        }
    }

    assoc = HashLookupElement(ptr->hashtable, lval);

    if (assoc)
    {
        if (CompareVariableValue(rval, assoc) == 0)
        {
            /* Identical value, keep as is */
        }
        else
        {
            /* Different value, bark and replace */
            if (!UnresolvedVariables(assoc, rval.type))
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", " !! Duplicate selection of value for variable \"%s\" in scope %s", lval, ptr->scope);
                if (fname)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", " !! Rule from %s at/before line %d\n", fname, lineno);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", " !! in bundle parameterization\n");
                }
            }
            RvalDestroy(assoc->rval);
            assoc->rval = RvalCopy(rval);
            assoc->dtype = dtype;
            CfDebug("Stored \"%s\" in context %s\n", lval, scope);
        }
    }
    else
    {
        if (!HashInsertElement(ptr->hashtable, lval, rval, dtype))
        {
            ProgrammingError("Hash table is full");
        }
    }

    CfDebug("Added Variable %s in scope %s with value (omitted)\n", lval, scope);
    return true;
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
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Name list %d, dereflist %d\n", len, RlistLen(dereflist));
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
                    CfDebug("Rewriting expanded type for %s from %s to %s\n", assoc->lval, CF_DATATYPES[assoc->dtype],
                            (char *) rp->state_ptr->item);

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

                CfDebug(" to %s\n", CF_DATATYPES[assoc->dtype]);
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

    CfDebug("MapBodyArgs(begin)\n");

    len1 = RlistLen(give);
    len2 = RlistLen(take);

    if (len1 != len2)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Argument mismatch in body template give[+args] = %d, take[-args] = %d", len1, len2);
        return false;
    }

    for (rpg = give, rpt = take; rpg != NULL && rpt != NULL; rpg = rpg->next, rpt = rpt->next)
    {
        dtg = StringDataType(scopeid, (char *) rpg->item);
        dtt = StringDataType(scopeid, (char *) rpt->item);

        if (dtg != dtt)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Type mismatch between logical/formal parameters %s/%s\n", (char *) rpg->item,
                  (char *) rpt->item);
            CfOut(OUTPUT_LEVEL_ERROR, "", "%s is %s whereas %s is %s\n", (char *) rpg->item, CF_DATATYPES[dtg],
                  (char *) rpt->item, CF_DATATYPES[dtt]);
        }

        switch (rpg->type)
        {
        case RVAL_TYPE_SCALAR:
            lval = (char *) rpt->item;
            rval = rpg->item;
            CfDebug("MapBodyArgs(SCALAR,%s,%s)\n", lval, (char *) rval);
            ScopeAddVariableHash(scopeid, lval, (Rval) { rval, RVAL_TYPE_SCALAR }, dtg, NULL, 0);
            break;

        case RVAL_TYPE_LIST:
            lval = (char *) rpt->item;
            rval = rpg->item;
            ScopeAddVariableHash(scopeid, lval, (Rval) { rval, RVAL_TYPE_LIST }, dtg, NULL, 0);
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
                // Unresolved variables
                if (VERBOSE)
                {
                    printf
                        (" !! Embedded function argument does not resolve to a name - probably too many evaluation levels for ");
                    FnCallShow(stdout, fp);
                    printf(" (try simplifying)\n");
                }
            }
            else
            {
                FnCallDestroy(fp);

                rpg->item = res.rval.item;
                rpg->type = res.rval.type;

                lval = (char *) rpt->item;
                rval = rpg->item;

                ScopeAddVariableHash(scopeid, lval, (Rval) {rval, RVAL_TYPE_SCALAR }, dtg, NULL, 0);
            }

            break;

        default:
            /* Nothing else should happen */
            ProgrammingError("Software error: something not a scalar/function in argument literal");
        }
    }

    CfDebug("MapBodyArgs(end)\n");
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
