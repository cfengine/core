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
#include <misc_lib.h>

struct PromiseIterator_
{
    bool started;
    Rlist *ctx;
};

static bool NullIteratorsInternal(const Rlist *iterator);
static bool EndOfIterationInternal(const Rlist *iterator);

/*****************************************************************************/

static Rlist *RlistAppendOrthog(Rlist **start, CfAssoc *assoc)
{
    Rlist *rp = xmalloc(sizeof(Rlist));

    if (*start == NULL)
    {
        *start = rp;
    }
    else
    {
        Rlist *lp = NULL;
        for (lp = *start; lp->next != NULL; lp = lp->next)
        {
        }

        lp->next = rp;
    }

    // Note, we pad all iterators will a blank so the ptr arithmetic works
    // else EndOfIteration will not see lists with only one element

    Rlist *lp = RlistPrependScalar((Rlist **) &(assoc->rval), CF_NULL_VALUE);
    rp->state_ptr = lp->next;   // Always skip the null value
    RlistAppendScalar((Rlist **) &(assoc->rval), CF_NULL_VALUE);

    rp->item = assoc;
    rp->type = RVAL_TYPE_LIST;
    rp->next = NULL;
    return rp;
}

PromiseIterator *PromiseIteratorNew(EvalContext *ctx, const Promise *pp, const Rlist *namelist)
{
    PromiseIterator *iter_ctx = xmalloc(sizeof(PromiseIterator));

    iter_ctx->ctx = NULL;
    iter_ctx->started = false;

    if (!namelist)
    {
        return iter_ctx;
    }

    for (const Rlist *rp = namelist; rp != NULL; rp = rp->next)
    {
        VarRef *ref = VarRefParseFromBundle(rp->item, PromiseGetBundle(pp));

        Rval retval;
        DataType dtype = DATA_TYPE_NONE;
        if (!EvalContextVariableGet(ctx, ref, &retval, &dtype))
        {
            Log(LOG_LEVEL_ERR, "Couldn't locate variable '%s' apparently in '%s'", RlistScalarValue(rp), PromiseGetBundle(pp)->name);
            VarRefDestroy(ref);
            continue;
        }

        VarRefDestroy(ref);

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

        CfAssoc *new_var = NewAssoc(rp->item, retval, dtype);
        RlistAppendOrthog(&iter_ctx->ctx, new_var);
    }

    for (size_t i = 0; i < 5 && NullIteratorsInternal(iter_ctx->ctx); i++)
    {
        PromiseIteratorNext(iter_ctx);
    }

    // We now have a control list of list-variables, with internal state in state_ptr
    return iter_ctx;
}

/*****************************************************************************/

static void DeleteReferenceRlist(Rlist *list)
{
    if (list)
    {
        DeleteAssoc((CfAssoc *)list->item);
        DeleteReferenceRlist(list->next);
        free(list);
    }
}

void PromiseIteratorDestroy(PromiseIterator *iter_ctx)
{
    if (iter_ctx)
    {
        DeleteReferenceRlist(iter_ctx->ctx);
    }
}

/*****************************************************************************/

static bool IncrementIterationContextInternal(Rlist *iterator)
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

            if (IncrementIterationContextInternal(iterator->next))
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

        while (NullIteratorsInternal(iterator))
        {
            if (IncrementIterationContextInternal(iterator->next))
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

        if (EndOfIterationInternal(iterator))
        {
            return false;
        }

        return true;
    }
}

bool PromiseIteratorNext(PromiseIterator *iter_ctx)
{
    iter_ctx->started = true;
    return IncrementIterationContextInternal(iter_ctx->ctx);
}

static bool EndOfIterationInternal(const Rlist *iterator)
{
    if (!iterator)
    {
        return true;
    }

    // When all the wheels are at NULL, we have reached the end
    for (const Rlist *rp = iterator; rp != NULL; rp = rp->next)
    {
        const Rlist *state = rp->state_ptr;

        if (state == NULL)
        {
            continue;
        }

        if (state && state->next)
        {
            return false;
        }
    }

    return true;
}

bool PromiseIteratorHasMore(const PromiseIterator *iter_ctx)
{
    if (iter_ctx->ctx)
    {
        return !EndOfIterationInternal(iter_ctx->ctx);
    }
    else
    {
        return !iter_ctx->started;
    }

    bool end = iter_ctx->ctx && EndOfIterationInternal(iter_ctx->ctx);
    return !end;
}

/*****************************************************************************/

static bool NullIteratorsInternal(const Rlist *iterator)
{
    if (!iterator)
    {
        return false;
    }

    // When all the wheels are at NULL, we have reached the end
    for (const Rlist *rp = iterator; rp != NULL; rp = rp->next)
    {
        const Rlist *state = rp->state_ptr;

        if (state)
        {
            switch (state->type)
            {
            case RVAL_TYPE_SCALAR:
                if (strcmp(RlistScalarValue(state), CF_NULL_VALUE) == 0)
                {
                    return true;
                }
                break;
            case RVAL_TYPE_FNCALL:
                if (strcmp(RlistFnCallValue(state)->name, CF_NULL_VALUE) == 0)
                {
                    return true;
                }
                break;
            default:
                ProgrammingError("Unexpected rval type %d in iterator", state->type);
            }
        }
    }

    return false;
}

bool NullIterators(const PromiseIterator *iter_ctx)
{
    return NullIteratorsInternal(iter_ctx->ctx);
}


void PromiseIteratorUpdateVariable(const PromiseIterator *iter_ctx, Variable *var)
{
    for (const Rlist *rp = iter_ctx->ctx; rp; rp = rp->next)
    {
        CfAssoc *cplist = rp->item;

        char *legacy_lval = VarRefToString(var->ref, false);

        if (strcmp(cplist->lval, legacy_lval) == 0)
        {
            if (rp->state_ptr == NULL || rp->state_ptr->type == RVAL_TYPE_FNCALL)
            {
                free(legacy_lval);
                return;
            }

            assert(rp->state_ptr->type == RVAL_TYPE_SCALAR);

            if (rp->state_ptr)
            {
                RvalDestroy(var->rval);
                var->rval.item = xstrdup(RlistScalarValue(rp->state_ptr));
            }

            switch (var->type)
            {
            case DATA_TYPE_STRING_LIST:
                var->type = DATA_TYPE_STRING;
                var->rval.type = RVAL_TYPE_SCALAR;
                break;
            case DATA_TYPE_INT_LIST:
                var->type = DATA_TYPE_INT;
                var->rval.type = RVAL_TYPE_SCALAR;
                break;
            case DATA_TYPE_REAL_LIST:
                var->type = DATA_TYPE_REAL;
                var->rval.type = RVAL_TYPE_SCALAR;
                break;
            default:
                /* Only lists need to be converted */
                break;
            }
        }

        free(legacy_lval);
    }
}
