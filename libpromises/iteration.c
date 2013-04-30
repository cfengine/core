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

#include "iteration.h"

#include "scope.h"
#include "vars.h"
#include "logging_old.h"
#include "fncall.h"
#include "env_context.h"

static void DeleteReferenceRlist(Rlist *list);

/*****************************************************************************/

static Rlist *RlistAppendOrthog(Rlist **start, void *item, RvalType type)
   /* Allocates new memory for objects - careful, could leak!  */
{
    Rlist *rp, *lp;
    CfAssoc *cp;

    CfDebug("OrthogAppendRlist\n");

    switch (type)
    {
    case RVAL_TYPE_LIST:
        CfDebug("Expanding and appending list object, orthogonally\n");
        break;
    default:
        CfDebug("Cannot append %c to rval-list [%s]\n", type, (char *) item);
        return NULL;
    }

    rp = xmalloc(sizeof(Rlist));

    if (*start == NULL)
    {
        *start = rp;
    }
    else
    {
        for (lp = *start; lp->next != NULL; lp = lp->next)
        {
        }

        lp->next = rp;
    }

// This is item is in fact a CfAssoc pointing to a list

    cp = (CfAssoc *) item;

// Note, we pad all iterators will a blank so the ptr arithmetic works
// else EndOfIteration will not see lists with only one element

    lp = RlistPrependScalar((Rlist **) &(cp->rval), CF_NULL_VALUE);
    rp->state_ptr = lp->next;   // Always skip the null value
    RlistAppendScalar((Rlist **) &(cp->rval), CF_NULL_VALUE);

    rp->item = item;
    rp->type = RVAL_TYPE_LIST;
    rp->next = NULL;
    return rp;
}

Rlist *NewIterationContext(EvalContext *ctx, const char *scopeid, Rlist *namelist)
{
    Rlist *rps, *deref_listoflists = NULL;
    Rval retval;
    DataType dtype;
    CfAssoc *new;
    Rval newret;

    CfDebug("\n*\nNewIterationContext(from %s)\n*\n", scopeid);

    ScopeCopy("this", ScopeGet(scopeid));

    ScopeGet("this");

    if (namelist == NULL)
    {
        CfDebug("No lists to iterate over\n");
        return NULL;
    }

    for (Rlist *rp = namelist; rp != NULL; rp = rp->next)
    {
        dtype = DATA_TYPE_NONE;
        if (!EvalContextVariableGet(ctx, (VarRef) { NULL, scopeid, rp->item }, &retval, &dtype))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Couldn't locate variable %s apparently in %s\n", RlistScalarValue(rp), scopeid);
            CfOut(OUTPUT_LEVEL_ERROR, "",
                  " !! Could be incorrect use of a global iterator -- see reference manual on list substitution");
            continue;
        }

        /* Make a copy of list references in scope only, without the names */

        if (retval.type == RVAL_TYPE_LIST)
        {
            for (rps = (Rlist *) retval.item; rps != NULL; rps = rps->next)
            {
                if (rps->type == RVAL_TYPE_FNCALL)
                {
                    FnCall *fp = (FnCall *) rps->item;

                    newret = FnCallEvaluate(ctx, fp, NULL).rval;
                    FnCallDestroy(fp);
                    rps->item = newret.item;
                    rps->type = newret.type;
                }
            }
        }

        if ((new = NewAssoc(rp->item, retval, dtype)))
        {
            RlistAppendOrthog(&deref_listoflists, new, RVAL_TYPE_LIST);
            rp->state_ptr = new->rval.item;

            while ((rp->state_ptr) && (strcmp(rp->state_ptr->item, CF_NULL_VALUE) == 0))
            {
                if (rp->state_ptr)
                {
                    rp->state_ptr = rp->state_ptr->next;
                }
            }
        }
    }

/* We now have a control list of list-variables, with internal state in state_ptr */

    return deref_listoflists;
}

/*****************************************************************************/

void DeleteIterationContext(Rlist *deref)
{
    ScopeClear("this");

    if (deref != NULL)
    {
        DeleteReferenceRlist(deref);
    }
}

/*****************************************************************************/

static int IncrementIterationContextInternal(Rlist *iterator, int level)
{
    Rlist *state;
    CfAssoc *cp;

    if (iterator == NULL)
    {
        return false;
    }

// iterator->next points to the next list
// iterator->state_ptr points to the current item in the current list

    cp = (CfAssoc *) iterator->item;
    state = iterator->state_ptr;

    if (state == NULL)
    {
        return false;
    }

/* Go ahead and increment */

    CfDebug(" -> Incrementing (%s - level %d) from \"%s\"\n", cp->lval, level, (char *) iterator->state_ptr->item);

    if (state->next == NULL)
    {
        /* This wheel has come to full revolution, so move to next */

        if (iterator->next != NULL)
        {
            /* Increment next wheel */

            if (IncrementIterationContextInternal(iterator->next, level + 1))
            {
                /* Not at end yet, so reset this wheel */
                iterator->state_ptr = cp->rval.item;
                iterator->state_ptr = iterator->state_ptr->next;
                return true;
            }
            else
            {
                /* Reached last variable wheel - pass up */
                return false;
            }
        }
        else
        {
            /* Reached last variable wheel - waiting for end detection */
            return false;
        }
    }
    else
    {
        /* Update the current wheel */
        iterator->state_ptr = state->next;

        CfDebug(" <- Incrementing wheel (%s) to \"%s\"\n", cp->lval, (char *) iterator->state_ptr->item);

        while (NullIterators(iterator))
        {
            if (IncrementIterationContextInternal(iterator->next, level + 1))
            {
                // If we are at the end of this wheel, we need to shift to next wheel
                iterator->state_ptr = cp->rval.item;
                iterator->state_ptr = iterator->state_ptr->next;
                return true;
            }
            else
            {
                // Otherwise increment this wheel
                iterator->state_ptr = iterator->state_ptr->next;
                break;
            }
        }

        if (EndOfIteration(iterator))
        {
            return false;
        }

        return true;
    }
}

/*****************************************************************************/

int IncrementIterationContext(Rlist *iterator)
{
    return IncrementIterationContextInternal(iterator, 1);
}

/*****************************************************************************/

int EndOfIteration(Rlist *iterator)
{
    Rlist *rp, *state;

    if (iterator == NULL)
    {
        return true;
    }

/* When all the wheels are at NULL, we have reached the end*/

    for (rp = iterator; rp != NULL; rp = rp->next)
    {
        state = rp->state_ptr;

        if (state == NULL)
        {
            continue;
        }

        if (state && (state->next != NULL))
        {
            return false;
        }
    }

    return true;
}

/*****************************************************************************/

int NullIterators(Rlist *iterator)
{
    Rlist *rp, *state;

    if (iterator == NULL)
    {
        return false;
    }

/* When all the wheels are at NULL, we have reached the end*/

    for (rp = iterator; rp != NULL; rp = rp->next)
    {
        state = rp->state_ptr;

        if (state && (strcmp(state->item, CF_NULL_VALUE) == 0))
        {
            return true;
        }
    }

    return false;
}

/*******************************************************************/

static void DeleteReferenceRlist(Rlist *list)
/* Delete all contents, hash table in scope has own copy */
{
    if (list == NULL)
    {
        return;
    }

    DeleteAssoc((CfAssoc *) list->item);

    DeleteReferenceRlist(list->next);
    free((char *) list);
}
