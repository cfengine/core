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

#include <assert.h>

/*******************************************************************/

Scope *GetScope(const char *scope)
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

void SetScope(char *id)
{
    strlcpy(CONTEXTID, id, CF_MAXVARSIZE);
}

/*******************************************************************/

void SetNewScope(char *id)
{
    NewScope(id);
    SetScope(id);
}

/*******************************************************************/

void NewScope(const char *name)
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

void AugmentScope(char *scope, char *ns, Rlist *lvals, Rlist *rvals)
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
            
            vtype = GetVariable(scope, qnaked, &retval); 

            switch (vtype)
            {
            case DATA_TYPE_STRING_LIST:
            case DATA_TYPE_INT_LIST:
            case DATA_TYPE_REAL_LIST:
                NewList(scope, lval, RvalCopy((Rval) {retval.item, RVAL_TYPE_LIST}).item, DATA_TYPE_STRING_LIST);
                break;
            default:
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! List parameter \"%s\" not found while constructing scope \"%s\" - use @(scope.variable) in calling reference", qnaked, scope);
                NewScalar(scope, lval, rpr->item, DATA_TYPE_STRING);
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
            NewScalar(scope, lval, rpr->item, DATA_TYPE_STRING);
            break;

        case RVAL_TYPE_FNCALL:
            subfp = (FnCall *) rpr->item;
            Rval rval = FnCallEvaluate(subfp, pp).rval;
            if (rval.type == RVAL_TYPE_SCALAR)
            {
                NewScalar(scope, lval, rval.item, DATA_TYPE_STRING);
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

    ptr = GetScope(scope);

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

void DeleteAllScope()
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

void DeleteScope(char *name)
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

void DeleteFromScope(char *scope, Rlist *args)
{
    Rlist *rp;
    char *lval;

    for (rp = args; rp != NULL; rp = rp->next)
    {
        lval = (char *) rp->item;
        DeleteScalar(scope, lval);
    }
}

/*******************************************************************/

void CopyScope(const char *new_scopename, const char *old_scopename)
/*
 * Thread safe
 */
{
    Scope *op, *np;

    CfDebug("\n*\nCopying scope data %s to %s\n*\n", old_scopename, new_scopename);

    NewScope(new_scopename);

    if (!ThreadLock(cft_vscope))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not lock VSCOPE");
        return;
    }

    if ((op = GetScope(old_scopename)))
    {
        np = GetScope(new_scopename);
        HashCopy(np->hashtable, op->hashtable);
    }

    ThreadUnlock(cft_vscope);
}

/*******************************************************************/
/* Stack frames                                                    */
/*******************************************************************/

void PushThisScope()
{
    Scope *op;
    char name[CF_MAXVARSIZE];

    op = GetScope("this");

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

void PopThisScope()
{
    Scope *op = NULL;

    if (CF_STCKFRAME > 0)
    {
        DeleteScope("this");
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
