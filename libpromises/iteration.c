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
#include <string_lib.h>

struct PromiseIterator_
{
    bool started;
    Seq *vars;
    Seq *var_states;
};

static bool EndOfIterationInternal(const PromiseIterator *iter, size_t index);
static bool NullIteratorsInternal(PromiseIterator *iter, size_t index);

static Rlist *ContainerToRlist(const JsonElement *container)
{
    Rlist *list = NULL;

    JsonIterator iter = JsonIteratorInit(container);
    const JsonElement *child = NULL;

    while ((child = JsonIteratorNextValue(&iter)))
    {
        if (JsonGetElementType(child) != JSON_ELEMENT_TYPE_PRIMITIVE)
        {
            continue;
        }

        switch (JsonGetPrimitiveType(child))
        {
        case JSON_PRIMITIVE_TYPE_BOOL:
            RlistAppendScalar(&list, JsonPrimitiveGetAsBool(child) ? "true" : "false");
            break;
        case JSON_PRIMITIVE_TYPE_INTEGER:
            {
                char *str = StringFromLong(JsonPrimitiveGetAsInteger(child));
                RlistAppendScalar(&list, str);
                free(str);
            }
            break;
        case JSON_PRIMITIVE_TYPE_REAL:
            {
                char *str = StringFromDouble(JsonPrimitiveGetAsReal(child));
                RlistAppendScalar(&list, str);
                free(str);
            }
            break;
        case JSON_PRIMITIVE_TYPE_STRING:
            RlistAppendScalar(&list, JsonPrimitiveGetAsString(child));
            break;

        case JSON_PRIMITIVE_TYPE_NULL:
            break;
        }
    }

    return list;
}

static void AppendIterationVariable(PromiseIterator *iter, CfAssoc *new_var)
{
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

PromiseIterator *PromiseIteratorNew(EvalContext *ctx, const Promise *pp, const Rlist *lists, const Rlist *containers)
{
    PromiseIterator *iter = xmalloc(sizeof(PromiseIterator));

    iter->vars = SeqNew(RlistLen(lists), NULL);
    iter->var_states = SeqNew(RlistLen(lists), NULL);
    iter->started = false;

    for (const Rlist *rp = lists; rp != NULL; rp = rp->next)
    {
        VarRef *ref = VarRefParseFromBundle(rp->item, PromiseGetBundle(pp));

        Rval rval;
        DataType dtype = DATA_TYPE_NONE;
        if (!EvalContextVariableGet(ctx, ref, &rval, &dtype))
        {
            Log(LOG_LEVEL_ERR, "Couldn't locate variable '%s' apparently in '%s'", RlistScalarValue(rp), PromiseGetBundle(pp)->name);
            VarRefDestroy(ref);
            continue;
        }

        VarRefDestroy(ref);

        for (Rlist *rps = RvalRlistValue(rval); rps; rps = rps->next)
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

        CfAssoc *new_var = NewAssoc(RlistScalarValue(rp), rval, dtype);
        AppendIterationVariable(iter, new_var);
    }

    for (const Rlist *rp = containers; rp; rp = rp->next)
    {
        VarRef *ref = VarRefParseFromBundle(rp->item, PromiseGetBundle(pp));

        Rval rval;
        DataType dtype = DATA_TYPE_NONE;
        if (!EvalContextVariableGet(ctx, ref, &rval, &dtype))
        {
            Log(LOG_LEVEL_ERR, "Couldn't locate variable '%s' apparently in '%s'", RlistScalarValue(rp), PromiseGetBundle(pp)->name);
            VarRefDestroy(ref);
            continue;
        }

        VarRefDestroy(ref);

        assert(rval.type == RVAL_TYPE_CONTAINER);
        assert(dtype == DATA_TYPE_CONTAINER);

        CfAssoc *new_var = xmalloc(sizeof(CfAssoc));
        new_var->lval = xstrdup(RlistScalarValue(rp));
        new_var->rval = (Rval) { ContainerToRlist(RvalContainerValue(rval)), RVAL_TYPE_LIST };
        new_var->dtype = DATA_TYPE_STRING_LIST;

        AppendIterationVariable(iter, new_var);
    }

    // We now have a control list of list-variables, with internal state in state_ptr
    return iter;
}

void PromiseIteratorDestroy(PromiseIterator *iter)
{
    if (iter)
    {
        for (size_t i = 0; i < SeqLength(iter->vars); i++)
        {
            CfAssoc *var = SeqAt(iter->vars, i);
            void *state = SeqAt(iter->var_states, i);

            if (var->rval.type == RVAL_TYPE_CONTAINER)
            {
                free(state);
            }
        }

        SeqDestroy(iter->var_states);
        SeqDestroy(iter->vars);
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
        const CfAssoc *var = SeqAt(iter->vars, i);

        if (var->rval.type == RVAL_TYPE_LIST)
        {
            const Rlist *state = SeqAt(iter->var_states, i);

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

            case RVAL_TYPE_CONTAINER:
                return false;

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

    CfAssoc *var = SeqAt(iter->vars, index);

    switch (var->rval.type)
    {
    case RVAL_TYPE_LIST:
        {
            Rlist *state = SeqAt(iter->var_states, index);
            SeqSet(iter->var_states, index, state->next);
        }
        break;

    case RVAL_TYPE_CONTAINER:
        {
            JsonIterator *container_iter = SeqAt(iter->var_states, index);
            JsonIteratorNextValue(container_iter);
        }
        break;

    default:
        ProgrammingError("Unhandled case in switch");
    }
}

static void VariableStateReset(PromiseIterator *iter, size_t index)
{
    assert(index < SeqLength(iter->var_states));

    CfAssoc *var = SeqAt(iter->vars, index);

    switch (var->rval.type)
    {
    case RVAL_TYPE_LIST:
        {
            Rlist *state = RvalRlistValue(var->rval);
            state = state->next;
            SeqSet(iter->var_states, index, state);
        }
        break;

    case RVAL_TYPE_CONTAINER:
        {
            JsonIterator *container_iter = SeqAt(iter->var_states, index);
            *container_iter = JsonIteratorInit(RvalContainerValue(var->rval));
            JsonIteratorNextValue(container_iter);
        }
        break;

    default:
        ProgrammingError("Unhandled case in switch");
    }
}

static bool VariableStateHasMore(const PromiseIterator *iter, size_t index)
{
    CfAssoc *var = SeqAt(iter->vars, index);
    switch (var->rval.type)
    {
    case RVAL_TYPE_LIST:
        {
            const Rlist *state = SeqAt(iter->var_states, index);
            return state->next;
        }

    case RVAL_TYPE_CONTAINER:
        {
            const JsonIterator *state = SeqAt(iter->var_states, index);
            return JsonIteratorHasMore(state);
        }

    case RVAL_TYPE_FNCALL:
    case RVAL_TYPE_NOPROMISEE:
    case RVAL_TYPE_SCALAR:
        ProgrammingError("Unhandled case in switch %d", var->rval.type);
    }

    return false;
}

static bool IncrementIterationContextInternal(PromiseIterator *iter, size_t index)
{
    if (index == SeqLength(iter->vars))
    {
        return false;
    }

    // Go ahead and increment
    if (!VariableStateHasMore(iter, index))
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
        CfAssoc *var = SeqAt(iter->vars, i);

        switch (var->rval.type)
        {
        case RVAL_TYPE_LIST:
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
            break;

        case RVAL_TYPE_CONTAINER:
            assert(false);
            break;

        default:
            ProgrammingError("Unhandled value in switch %d", var->rval.type);
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

static void UpdateListVariable(const PromiseIterator *iter, Variable *var, size_t index)
{
    const Rlist *state = SeqAt(iter->var_states, index);

    if (!state || state->type == RVAL_TYPE_FNCALL)
    {
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
    case DATA_TYPE_CONTAINER:
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
        break;
    }
}

static void UpdateContainerVariable(const PromiseIterator *iter, Variable *var, size_t i)
{
    JsonIterator *state = SeqAt(iter->var_states, i);

    const JsonElement *item = JsonIteratorCurrentValue(state);

    if (JsonGetElementType(item) != JSON_ELEMENT_TYPE_PRIMITIVE)
    {
        return;
    }

    switch (JsonGetPrimitiveType(item))
    {
    case JSON_PRIMITIVE_TYPE_BOOL:
        {
            RvalDestroy(var->rval);
            var->rval.item = xstrdup(JsonPrimitiveGetAsBool(item) ? "true" : "false");
            var->rval.type = RVAL_TYPE_SCALAR;
            var->type = DATA_TYPE_STRING;
        }
        break;
    case JSON_PRIMITIVE_TYPE_INTEGER:
        {
            RvalDestroy(var->rval);
            var->rval.item = StringFromLong(JsonPrimitiveGetAsInteger(item));
            var->rval.type = RVAL_TYPE_SCALAR;
            var->type = DATA_TYPE_STRING;
        }
        break;
    case JSON_PRIMITIVE_TYPE_REAL:
        {
            RvalDestroy(var->rval);
            var->rval.item = StringFromDouble(JsonPrimitiveGetAsReal(item));
            var->rval.type = RVAL_TYPE_SCALAR;
            var->type = DATA_TYPE_STRING;
        }
        break;
    case JSON_PRIMITIVE_TYPE_STRING:
        {
            RvalDestroy(var->rval);
            var->rval.item = xstrdup(JsonPrimitiveGetAsString(item));
            var->rval.type = RVAL_TYPE_SCALAR;
            var->type = DATA_TYPE_STRING;
        }
        break;
    case JSON_PRIMITIVE_TYPE_NULL:
        break;
    }
}

void PromiseIteratorUpdateVariable(const PromiseIterator *iter, Variable *var)
{
    for (size_t i = 0; i < SeqLength(iter->vars); i++)
    {
        CfAssoc *iter_var = SeqAt(iter->vars, i);

        char *legacy_lval = VarRefToString(var->ref, false);

        if (strcmp(iter_var->lval, legacy_lval) == 0)
        {
            switch (iter_var->rval.type)
            {
            case RVAL_TYPE_LIST:
            case RVAL_TYPE_CONTAINER:
                UpdateListVariable(iter, var, i);
                break;

            case RVAL_TYPE_SCALAR:
            case RVAL_TYPE_FNCALL:
            case RVAL_TYPE_NOPROMISEE:
                ProgrammingError("Unhandled case in switch %d", iter_var->rval.type);
            }
        }

        free(legacy_lval);
    }
}
