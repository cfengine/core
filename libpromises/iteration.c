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

#include <iteration.h>

#include <scope.h>
#include <vars.h>
#include <fncall.h>
#include <env_context.h>

static void DeleteReferenceRlist(Rlist *list);

/*****************************************************************************/

static Rlist *RlistAppendOrthog(Rlist **start, void *item, RvalType type)
   /* Allocates new memory for objects - careful, could leak!  */
{
    Rlist *rp, *lp;
    CfAssoc *cp;

    switch (type)
    {
    case RVAL_TYPE_LIST:
        Log(LOG_LEVEL_DEBUG, "Expanding and appending list object, orthogonally");
        break;
    default:
        Log(LOG_LEVEL_DEBUG, "Cannot append %c to rval-list '%s'", type, (char *) item);
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

Rlist *NewIterationContext(EvalContext *ctx, const Promise *pp, Rlist *namelist)
{
    if (namelist == NULL)
    {
        return NULL;
    }

    Rlist *deref_listoflists = NULL;
    for (Rlist *rp = namelist; rp != NULL; rp = rp->next)
    {
        VarRef *ref = VarRefParseFromBundle(rp->item, PromiseGetBundle(pp));

        Rval retval;
        DataType dtype = DATA_TYPE_NONE;
        if (!EvalContextVariableGet(ctx, ref, &retval, &dtype))
        {
            Log(LOG_LEVEL_ERR, "Couldn't locate variable %s apparently in %s", RlistScalarValue(rp), PromiseGetBundle(pp)->name);
            Log(LOG_LEVEL_ERR,
                  "Could be incorrect use of a global iterator -- see reference manual on list substitution");
            VarRefDestroy(ref);
            continue;
        }

        VarRefDestroy(ref);

        /* Make a copy of list references in scope only, without the names */

        if (retval.type == RVAL_TYPE_LIST)
        {
            for (Rlist *rps = RvalRlistValue(retval); rps; rps = rps->next)
            {
                if (rps->type == RVAL_TYPE_FNCALL)
                {
                    FnCall *fp = (FnCall *) rps->item;

                    Rval newret = FnCallEvaluate(ctx, fp, pp).rval;
                    FnCallDestroy(fp);
                    rps->item = newret.item;
                    rps->type = newret.type;
                }
            }
        }

        CfAssoc *new_var = NULL;
        if ((new_var = NewAssoc(rp->item, retval, dtype)))
        {
            RlistAppendOrthog(&deref_listoflists, new_var, RVAL_TYPE_LIST);
            rp->state_ptr = new_var->rval.item;

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
    if (deref != NULL)
    {
        DeleteReferenceRlist(deref);
    }
}

/*****************************************************************************/

static bool IncrementIterationContextInternal(Rlist *iterator, int level)
{
    if (iterator == NULL)
    {
        return false;
    }

    // iterator->next points to the next list
    // iterator->state_ptr points to the current item in the current list
    CfAssoc *cp = (CfAssoc *) iterator->item;
    Rlist *state = iterator->state_ptr;

    if (state == NULL)
    {
        return false;
    }

    // Go ahead and increment
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
