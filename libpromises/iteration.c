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
    Seq *vars;
    Seq *var_states;
};

static bool EndOfIterationInternal(const PromiseIterator *iter, size_t index);
static bool NullIteratorsInternal(PromiseIterator *iter, size_t index);

PromiseIterator *PromiseIteratorNew(EvalContext *ctx, const Promise *pp, const Rlist *namelist)
{
    PromiseIterator *iter = xmalloc(sizeof(PromiseIterator));

    iter->vars = SeqNew(RlistLen(namelist), NULL);
    iter->var_states = SeqNew(RlistLen(namelist), NULL);
    iter->started = false;

    if (!namelist)
    {
        return iter;
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
        SeqAppend(iter->vars, new_var);

        {
            Rlist *list_value = RvalRlistValue(new_var->rval);
            Rlist *state = new_var->rval.item = RlistPrependScalar(&list_value, CF_NULL_VALUE);
            RlistAppendScalar(&list_value, CF_NULL_VALUE);

            while (state && (strcmp(state->item, CF_NULL_VALUE) == 0))
            {
                state = state->next;
            }

            SeqAppend(iter->var_states, state);
        }
    }

    // We now have a control list of list-variables, with internal state in state_ptr
    return iter;
}

void PromiseIteratorDestroy(PromiseIterator *iter)
{
    if (iter)
    {
        SeqDestroy(iter->vars);
        SeqDestroy(iter->var_states);
    }
}

/*****************************************************************************/

static bool NullIteratorsInternal(PromiseIterator *iter, size_t index)
{
    if (index >= SeqLength(iter->vars))
    {
        return false;
    }

    for (size_t i = index; i < SeqLength(iter->var_states); i++)
    {
        const Rlist *state = SeqAt(iter->var_states, i);

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

static void VariableStateIncrement(PromiseIterator *iter, size_t index)
{
    assert(index < SeqLength(iter->var_states));

    Rlist *state = SeqAt(iter->var_states, index);
    SeqSet(iter->var_states, index, state->next);
}

static void VariableStateReset(PromiseIterator *iter, size_t index)
{
    assert(index < SeqLength(iter->var_states));

    CfAssoc *var = SeqAt(iter->vars, index);
    Rlist *state = RvalRlistValue(var->rval);
    state = state->next;

    SeqSet(iter->var_states, index, state);
}

static bool IncrementIterationContextInternal(PromiseIterator *iter, size_t index)
{
    if (index == SeqLength(iter->vars))
    {
        return false;
    }

    CfAssoc *cp = SeqAt(iter->vars, index);
    Rlist *state = SeqAt(iter->var_states, index);

    if (state == NULL)
    {
        return false;
    }

    // Go ahead and increment
    if (state->next == NULL)
    {
        /* This wheel has come to full revolution, so move to next */
        if (index < (SeqLength(iter->vars) - 1))
        {
            /* Increment next wheel */
            if (IncrementIterationContextInternal(iter, index + 1))
            {
                /* Not at end yet, so reset this wheel */
                VariableStateReset(iter, index);
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
        VariableStateIncrement(iter, index);

        while (NullIteratorsInternal(iter, index))
        {
            if (IncrementIterationContextInternal(iter, index + 1))
            {
                // If we are at the end of this wheel, we need to shift to next wheel
                VariableStateReset(iter, index);
                return true;
            }
            else
            {
                // Otherwise increment this wheel
                VariableStateIncrement(iter, index);
                break;
            }
        }

        if (EndOfIterationInternal(iter, index))
        {
            return false;
        }

        return true;
    }
}

bool PromiseIteratorNext(PromiseIterator *iter_ctx)
{
    iter_ctx->started = true;
    return IncrementIterationContextInternal(iter_ctx, 0);
}

static bool EndOfIterationInternal(const PromiseIterator *iter, size_t index)
{
    if (index >= SeqLength(iter->vars))
    {
        return true;
    }

    for (size_t i = index; i < SeqLength(iter->var_states); i++)
    {
        const Rlist *state = SeqAt(iter->var_states, i);
        if (!state)
        {
            continue;
        }
        else if (state->next)
        {
            return false;
        }
    }

    return true;
}

bool PromiseIteratorHasMore(const PromiseIterator *iter)
{
    if (SeqLength(iter->vars) > 0)
    {
        return !EndOfIterationInternal(iter, 0);
    }
    else
    {
        return !iter->started;
    }
}

void PromiseIteratorUpdateVariable(const PromiseIterator *iter, Variable *var)
{
    for (size_t i = 0; i < SeqLength(iter->vars); i++)
    {
        CfAssoc *cplist = SeqAt(iter->vars, i);

        char *legacy_lval = VarRefToString(var->ref, false);

        if (strcmp(cplist->lval, legacy_lval) == 0)
        {
            const Rlist *state = SeqAt(iter->var_states, i);

            if (!state || state->type == RVAL_TYPE_FNCALL)
            {
                free(legacy_lval);
                return;
            }

            assert(state->type == RVAL_TYPE_SCALAR);

            if (state)
            {
                RvalDestroy(var->rval);
                var->rval.item = xstrdup(RlistScalarValue(state));
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
