/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <iteration.h>

#include <scope.h>
#include <vars.h>
#include <fncall.h>
#include <eval_context.h>
#include <misc_lib.h>
#include <string_lib.h>
#include <assoc.h>

struct PromiseIterator_
{
    Seq *vars; // list of (list-)variables (CfAssoc) to iterate over
    Seq *var_states; // list of expanded values (Rlist) for each variable
    bool has_null_list; // true if any list is empty
};

static void RlistAppendContainerPrimitive(Rlist **list, const JsonElement *primitive)
{
    assert(JsonGetElementType(primitive) == JSON_ELEMENT_TYPE_PRIMITIVE);

    switch (JsonGetPrimitiveType(primitive))
    {
    case JSON_PRIMITIVE_TYPE_BOOL:
        RlistAppendScalar(list, JsonPrimitiveGetAsBool(primitive) ? "true" : "false");
        break;
    case JSON_PRIMITIVE_TYPE_INTEGER:
        {
            char *str = StringFromLong(JsonPrimitiveGetAsInteger(primitive));
            RlistAppendScalar(list, str);
            free(str);
        }
        break;
    case JSON_PRIMITIVE_TYPE_REAL:
        {
            char *str = StringFromDouble(JsonPrimitiveGetAsReal(primitive));
            RlistAppendScalar(list, str);
            free(str);
        }
        break;
    case JSON_PRIMITIVE_TYPE_STRING:
        RlistAppendScalar(list, JsonPrimitiveGetAsString(primitive));
        break;

    case JSON_PRIMITIVE_TYPE_NULL:
        break;
    }
}

Rlist *ContainerToRlist(const JsonElement *container)
{
    Rlist *list = NULL;

    switch (JsonGetElementType(container))
    {
    case JSON_ELEMENT_TYPE_PRIMITIVE:
        RlistAppendContainerPrimitive(&list, container);
        break;

    case JSON_ELEMENT_TYPE_CONTAINER:
        {
            JsonIterator iter = JsonIteratorInit(container);
            const JsonElement *child;

            while (NULL != (child = JsonIteratorNextValue(&iter)))
            {
                if (JsonGetElementType(child) == JSON_ELEMENT_TYPE_PRIMITIVE)
                {
                    RlistAppendContainerPrimitive(&list, child);
                }
            }
        }
        break;
    }

    return list;
}

static Rlist *FirstRealEntry(Rlist *entry)
{
    while (entry && entry->val.item &&
           entry->val.type == RVAL_TYPE_SCALAR &&
           strcmp(entry->val.item, CF_NULL_VALUE) == 0)
    {
        entry = entry->next;
    }
    return entry;
}

static bool AppendIterationVariable(PromiseIterator *iter, CfAssoc *new_var)
{
    Rlist *state = RvalRlistValue(new_var->rval);
    // move to the first non-null value
    state = FirstRealEntry(state);
    SeqAppend(iter->vars, new_var);
    SeqAppend(iter->var_states, state);
    return state != NULL;
}

PromiseIterator *PromiseIteratorNew(EvalContext *ctx, const Promise *pp, const Rlist *lists, const Rlist *containers)
{
    PromiseIterator *iter = xmalloc(sizeof(PromiseIterator));

    iter->vars = SeqNew(RlistLen(lists), DeleteAssoc);
    iter->var_states = SeqNew(RlistLen(lists), NULL);
    iter->has_null_list = false;

    for (const Rlist *rp = lists; rp != NULL; rp = rp->next)
    {
        VarRef *ref = VarRefParseFromBundle(RlistScalarValue(rp), PromiseGetBundle(pp));

        DataType dtype = CF_DATA_TYPE_NONE;
        const void *value = EvalContextVariableGet(ctx, ref, &dtype);
        if (!value)
        {
            Log(LOG_LEVEL_ERR, "Couldn't locate variable '%s' apparently in '%s'", RlistScalarValue(rp), PromiseGetBundle(pp)->name);
            VarRefDestroy(ref);
            continue;
        }

        VarRefDestroy(ref);

        CfAssoc *new_var = NewAssoc(RlistScalarValue(rp), (Rval) { (void *)value, DataTypeToRvalType(dtype) }, dtype);
        iter->has_null_list |= !AppendIterationVariable(iter, new_var);
    }

    for (const Rlist *rp = containers; rp; rp = rp->next)
    {
        VarRef *ref = VarRefParseFromBundle(RlistScalarValue(rp), PromiseGetBundle(pp));

        DataType dtype = CF_DATA_TYPE_NONE;
        const JsonElement *value = EvalContextVariableGet(ctx, ref, &dtype);
        if (!value)
        {
            Log(LOG_LEVEL_ERR, "Couldn't locate variable '%s' apparently in '%s'", RlistScalarValue(rp), PromiseGetBundle(pp)->name);
            VarRefDestroy(ref);
            continue;
        }

        VarRefDestroy(ref);

        assert(dtype == CF_DATA_TYPE_CONTAINER);

        /* Mimics NewAssoc() but bypassing extra copying of ->rval: */
        CfAssoc *new_var = xmalloc(sizeof(CfAssoc));
        new_var->lval = xstrdup(RlistScalarValue(rp));
        new_var->rval = (Rval) { ContainerToRlist(value), RVAL_TYPE_LIST };
        new_var->dtype = CF_DATA_TYPE_STRING_LIST;

        iter->has_null_list |= !AppendIterationVariable(iter, new_var);
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
        free(iter);
    }
}

bool NullIterators(const PromiseIterator *iter)
{
    return iter->has_null_list;
}

/*****************************************************************************/

static bool VariableStateIncrement(PromiseIterator *iter, size_t index)
{
    assert(index < SeqLength(iter->var_states));

    CfAssoc *var = SeqAt(iter->vars, index);

    switch (var->rval.type)
    {
    case RVAL_TYPE_LIST:
        {
            Rlist *state = SeqAt(iter->var_states, index);
            assert(state);
            // find the next valid value, return false if there is none
            state = FirstRealEntry(state->next);
            SeqSet(iter->var_states, index, state);
            return state != NULL;
        }
        break;

    default:
        ProgrammingError("Unhandled case in switch");
    }

    return false;
}

static bool VariableStateReset(PromiseIterator *iter, size_t index)
{
    assert(index < SeqLength(iter->var_states));

    CfAssoc *var = SeqAt(iter->vars, index);

    switch (var->rval.type)
    {
    case RVAL_TYPE_LIST:
        {
            Rlist *state = RvalRlistValue(var->rval);
            // find the first valid value, return false if there is none
            state = FirstRealEntry(state);
            SeqSet(iter->var_states, index, state);
            return state != NULL;
        }
        break;

    default:
        ProgrammingError("Unhandled case in switch");
    }

    return false;
}

static bool VariableStateHasMore(const PromiseIterator *iter, size_t index)
{
    CfAssoc *var = SeqAt(iter->vars, index);
    switch (var->rval.type)
    {
    case RVAL_TYPE_LIST:
        {
            const Rlist *state = SeqAt(iter->var_states, index);
            return state && state->next;
        }

    case RVAL_TYPE_CONTAINER:
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
        if (index < SeqLength(iter->vars))
        {
            /* Increment next wheel */
            if (IncrementIterationContextInternal(iter, index + 1))
            {
                /* Not at end yet, so reset this wheel */
                return VariableStateReset(iter, index);
            }
            else
            {
                /* Reached last variable wheel - pass up */
                return false;
            }
        }
        else
        {
            /* Reached last variable wheel - pass up */
            return false;
        }
    }
    else
    {
        /* Update the current wheel */
        return VariableStateIncrement(iter, index);
    }
}

bool PromiseIteratorNext(PromiseIterator *iter_ctx)
{
    return IncrementIterationContextInternal(iter_ctx, 0);
}

void PromiseIteratorUpdateVariable(EvalContext *ctx, const PromiseIterator *iter)
{
    for (size_t i = 0; i < SeqLength(iter->vars); i++)
    {
        CfAssoc *iter_var = SeqAt(iter->vars, i);

        const Rlist *state = SeqAt(iter->var_states, i);

        if (!state || state->val.type == RVAL_TYPE_FNCALL)
        {
            continue;
        }

        assert(state->val.type == RVAL_TYPE_SCALAR);

        switch (iter_var->dtype)
        {
        case CF_DATA_TYPE_STRING_LIST:
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, iter_var->lval, RlistScalarValue(state), CF_DATA_TYPE_STRING, "source=promise");
            break;
        case CF_DATA_TYPE_INT_LIST:
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, iter_var->lval, RlistScalarValue(state), CF_DATA_TYPE_INT, "source=promise");
            break;
        case CF_DATA_TYPE_REAL_LIST:
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, iter_var->lval, RlistScalarValue(state), CF_DATA_TYPE_REAL, "source=promise");
            break;
        default:
            assert(false);
            break;
        }
    }
}
