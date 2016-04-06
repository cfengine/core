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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/
#include <variable.h>

#include <map.h>
#include <rlist.h>
#include <writer.h>


static void VariableDestroy(Variable *var);                 /* forward declaration */

static void VariableDestroy_untyped(void *var)
{
    VariableDestroy(var);
}


/**
   Define VarMap.
   Key:   VarRef
   Value: Variable
*/

TYPED_MAP_DECLARE(Var, VarRef *, Variable *)

TYPED_MAP_DEFINE(Var, VarRef *, Variable *,
                 VarRefHash_untyped,
                 VarRefEqual_untyped,
                 VarRefDestroy_untyped,
                 VariableDestroy_untyped)


struct VariableTable_
{
    VarMap *vars;
};

struct VariableTableIterator_
{
    VarRef *ref;
    MapIterator iter;
};

/* DO NOT EXPORT, this is for internal (hash table) use only, and it doesn't
 * free everything in Variable, in particular it leaves var->ref to be handled
 * by the Map implementation calling the key-destroy function. */
static void VariableDestroy(Variable *var)
{
    if (var)
    {
        RvalDestroy(var->rval);
        StringSetDestroy(var->tags);
        // Nothing to do for ->promise

        free(var);
    }
}

VariableTable *VariableTableNew(void)
{
    VariableTable *table = xmalloc(sizeof(VariableTable));

    table->vars = VarMapNew();

    return table;
}

void VariableTableDestroy(VariableTable *table)
{
    if (table)
    {
        VarMapDestroy(table->vars);
        free(table);
    }
}

Variable *VariableTableGet(const VariableTable *table, const VarRef *ref)
{
    return VarMapGet(table->vars, ref);
}

bool VariableTableRemove(VariableTable *table, const VarRef *ref)
{
    return VarMapRemove(table->vars, ref);
}

static Variable *VariableNew(VarRef *ref, Rval rval, DataType type,
                             StringSet *tags, const Promise *promise)
{
    Variable *var = xmalloc(sizeof(Variable));

    var->ref = ref;
    var->rval = rval;
    var->type = type;
    if (tags == NULL)
    {
        var->tags = StringSetFromString("", ',');
    }
    else
    {
        var->tags = tags;
    }
    var->promise = promise;

    return var;
}

bool VariableTablePut(VariableTable *table, const VarRef *ref,
                      const Rval *rval, DataType type,
                      const char *tags, const Promise *promise)
{
    assert(VarRefIsQualified(ref));

    Variable *var = VariableNew(VarRefCopy(ref), RvalCopy(*rval), type,
                                StringSetFromString(tags, ','), promise);
    return VarMapInsert(table->vars, var->ref, var);
}

bool VariableTableClear(VariableTable *table, const char *ns, const char *scope, const char *lval)
{
    const size_t vars_num = VarMapSize(table->vars);

    if (!ns && !scope && !lval)
    {
        VarMapClear(table->vars);
        bool has_vars = (vars_num > 0);
        return has_vars;
    }

    /* We can't remove elements from the hash table while we are iterating
     * over it. So we first store the VarRef pointers on a list. */

    VarRef **to_remove = xmalloc(vars_num * sizeof(*to_remove));
    size_t remove_count = 0;

    {
        VariableTableIterator *iter = VariableTableIteratorNew(table, ns, scope, lval);

        for (Variable *v = VariableTableIteratorNext(iter);
             v != NULL;
             v = VariableTableIteratorNext(iter))
        {
            to_remove[remove_count] = v->ref;
            remove_count++;
        }
        VariableTableIteratorDestroy(iter);
    }

    if (remove_count == 0)
    {
        free(to_remove);
        return false;
    }

    size_t removed = 0;
    for(size_t i = 0; i < remove_count; i++)
    {
        if (VariableTableRemove(table, to_remove[i]))
        {
            removed++;
        }
    }

    free(to_remove);
    assert(removed == remove_count);
    return true;
}

size_t VariableTableCount(const VariableTable *table, const char *ns, const char *scope, const char *lval)
{
    if (!ns && !scope && !lval)
    {
        return VarMapSize(table->vars);
    }

    VariableTableIterator *iter = VariableTableIteratorNew(table, ns, scope, lval);
    size_t count = 0;
    while (VariableTableIteratorNext(iter))
    {
        count++;
    }
    VariableTableIteratorDestroy(iter);
    return count;
}

VariableTableIterator *VariableTableIteratorNewFromVarRef(const VariableTable *table, const VarRef *ref)
{
    VariableTableIterator *iter = xmalloc(sizeof(VariableTableIterator));

    iter->ref = VarRefCopy(ref);
    iter->iter = MapIteratorInit(table->vars->impl);

    return iter;
}

VariableTableIterator *VariableTableIteratorNew(const VariableTable *table, const char *ns, const char *scope, const char *lval)
{
    VarRef ref = { 0 };
    ref.ns = (char *)ns;
    ref.scope = (char *)scope;
    ref.lval = (char *)lval;

    return VariableTableIteratorNewFromVarRef(table, &ref);
}

Variable *VariableTableIteratorNext(VariableTableIterator *iter)
{
    MapKeyValue *keyvalue;

    while ((keyvalue = MapIteratorNext(&iter->iter)) != NULL)
    {
        Variable *var = keyvalue->value;
        const char *key_ns = var->ref->ns ? var->ref->ns : "default";

        if (iter->ref->ns && strcmp(key_ns, iter->ref->ns) != 0)
        {
            continue;
        }

        if (iter->ref->scope && strcmp(var->ref->scope, iter->ref->scope) != 0)
        {
            continue;
        }

        if (iter->ref->lval && strcmp(var->ref->lval, iter->ref->lval) != 0)
        {
            continue;
        }

        if (iter->ref->num_indices > 0)
        {
            if (iter->ref->num_indices > var->ref->num_indices)
            {
                continue;
            }

            bool match = true;
            for (size_t i = 0; i < iter->ref->num_indices; i++)
            {
                if (strcmp(var->ref->indices[i], iter->ref->indices[i]) != 0)
                {
                    match = false;
                    break;
                }
            }

            if (!match)
            {
                continue;
            }
        }

        return var;
    }

    return NULL;
}

void VariableTableIteratorDestroy(VariableTableIterator *iter)
{
    if (iter)
    {
        VarRefDestroy(iter->ref);
        free(iter);
    }
}

VariableTable *VariableTableCopyLocalized(const VariableTable *table, const char *ns, const char *scope)
{
    VariableTable *localized_copy = VariableTableNew();

    VariableTableIterator *iter = VariableTableIteratorNew(table, ns, scope, NULL);
    Variable *foreign_var = NULL;
    while ((foreign_var = VariableTableIteratorNext(iter)))
    {
        /* TODO why is tags NULL here? Shouldn't it be an exact copy of
         * foreign_var->tags? */
        Variable *localized_var = VariableNew(VarRefCopyLocalized(foreign_var->ref),
                                              RvalCopy(foreign_var->rval), foreign_var->type,
                                              NULL, foreign_var->promise);
        VarMapInsert(localized_copy->vars, localized_var->ref, localized_var);
    }
    VariableTableIteratorDestroy(iter);

    return localized_copy;
}
