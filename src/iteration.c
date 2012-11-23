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

#include "cf3.defs.h"

#include "scope.h"
#include "unix.h"
#include "cfstream.h"
#include "fncall.h"

static void DeleteReferenceRlist(Rlist *list);

/*****************************************************************************/

Rlist *NewIterationContext(const char *scopeid, Rlist *namelist)
{
    Rlist *rps, *deref_listoflists = NULL;
    Rval retval;
    enum cfdatatype dtype;
    CfAssoc *new;
    Rval newret;

    CfDebug("\n*\nNewIterationContext(from %s)\n*\n", scopeid);

    CopyScope("this", scopeid);

    GetScope("this");

    if (namelist == NULL)
    {
        CfDebug("No lists to iterate over\n");
        return NULL;
    }

    for (Rlist *rp = namelist; rp != NULL; rp = rp->next)
    {
        dtype = GetVariable(scopeid, rp->item, &retval);

        if (dtype == cf_notype)
        {
            CfOut(cf_error, "", " !! Couldn't locate variable %s apparently in %s\n", ScalarValue(rp), scopeid);
            CfOut(cf_error, "",
                  " !! Could be incorrect use of a global iterator -- see reference manual on list substitution");
            continue;
        }

        /* Make a copy of list references in scope only, without the names */

        if (retval.rtype == CF_LIST)
        {
            for (rps = (Rlist *) retval.item; rps != NULL; rps = rps->next)
            {
                if (rps->type == CF_FNCALL)
                {
                    FnCall *fp = (FnCall *) rps->item;

                    newret = EvaluateFunctionCall(fp, NULL).rval;
                    DeleteFnCall(fp);
                    rps->item = newret.item;
                    rps->type = newret.rtype;
                }
            }
        }

        if ((new = NewAssoc(rp->item, retval, dtype)))
        {
            OrthogAppendRlist(&deref_listoflists, new, CF_LIST);
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
    DeleteScope("this");

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
