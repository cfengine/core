#include "variable.h"

#include "alloc.h"
#include "rb-tree.h"
#include "rlist.h"

struct VariableTable_
{
    RBTree *vars;
};

struct VariableTableIterator_
{
    VarRef *ref;
    RBTreeIterator *iter;
};

void VariableDestroy(Variable *var)
{
    if (var)
    {
        VarRefDestroy(var->ref);
        RvalDestroy(var->rval);
        free(var);
    }
}

VariableTable *VariableTableNew(void)
{
    VariableTable *table = xmalloc(sizeof(VariableTable));

    table->vars = RBTreeNew(NULL, (RBTreeKeyCompareFn*)VarRefCompare, NULL,
                            NULL, NULL, (RBTreeValueDestroyFn*)VariableDestroy);

    return table;
}

void VariableTableDestroy(VariableTable *table)
{
    if (table)
    {
        RBTreeDestroy(table->vars);
        free(table);
    }
}

Variable *VariableTableGet(const VariableTable *table, const VarRef *ref)
{
    return RBTreeGet(table->vars, ref);
}

bool VariableTableRemove(VariableTable *table, const VarRef *ref)
{
    return RBTreeRemove(table->vars, ref);
}

static Variable *VariableNew(const VarRef *ref, const Rval *rval, DataType type)
{
    Variable *var = xmalloc(sizeof(Variable));

    var->ref = VarRefCopy(ref);
    var->rval = RvalCopy(*rval);
    var->type = type;

    return var;
}

bool VariableTablePut(VariableTable *table, const VarRef *ref, const Rval *rval, DataType type)
{
    assert(VarRefIsQualified(ref));

    Variable *var = VariableTableGet(table, ref);
    if (var)
    {
        RvalDestroy(var->rval);
        var->rval = RvalCopy(*rval);
        var->type = type;
        return true;
    }
    else
    {
        var = VariableNew(ref, rval, type);
        return RBTreePut(table->vars, var->ref, var);
    }
}

bool VariableTableClear(VariableTable *table, const char *ns, const char *scope, const char *lval)
{
    if (!ns && !scope && !lval)
    {
        bool has_vars = RBTreeSize(table->vars) > 0;
        RBTreeClear(table->vars);
        return has_vars;
    }

    RBTree *remove_set = RBTreeNew(NULL, (RBTreeKeyCompareFn*)VarRefCompare, NULL, NULL, NULL, NULL);

    {
        VariableTableIterator *iter = VariableTableIteratorNew(table, ns, scope, lval);
        for (Variable *v = VariableTableIteratorNext(iter); v; v = VariableTableIteratorNext(iter))
        {
            RBTreePut(remove_set, v->ref, NULL);
        }
        VariableTableIteratorDestroy(iter);
    }

    size_t remove_count = RBTreeSize(remove_set);
    if (remove_count == 0)
    {
        RBTreeDestroy(remove_set);
        return false;
    }

    size_t removed = 0;

    {
        RBTreeIterator *iter = RBTreeIteratorNew(remove_set);
        VarRef *ref_key = NULL;
        void *dummy = NULL;
        while (RBTreeIteratorNext(iter, (void **)&ref_key, &dummy))
        {
            if (VariableTableRemove(table, ref_key))
            {
                removed++;
            }
        }
        RBTreeIteratorDestroy(iter);
    }

    RBTreeDestroy(remove_set);
    assert(removed == remove_count);
    return true;
}

size_t VariableTableCount(const VariableTable *table, const char *ns, const char *scope, const char *lval)
{
    if (!ns && !scope && !lval)
    {
        return RBTreeSize(table->vars);
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
    iter->iter = RBTreeIteratorNew(table->vars);

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
    VarRef *key_ref = NULL;
    Variable *value_var = NULL;

    while (RBTreeIteratorNext(iter->iter, (void **)&key_ref, (void **)&value_var))
    {
        const char *key_ns = key_ref->ns ? key_ref->ns : "default";

        if (iter->ref->ns && strcmp(key_ns, iter->ref->ns) != 0)
        {
            continue;
        }

        if (iter->ref->scope && strcmp(key_ref->scope, iter->ref->scope) != 0)
        {
            continue;
        }

        if (iter->ref->lval && strcmp(key_ref->lval, iter->ref->lval) != 0)
        {
            continue;
        }

        if (iter->ref->num_indices > 0)
        {
            if (iter->ref->num_indices > key_ref->num_indices)
            {
                continue;
            }

            bool match = true;
            for (size_t i = 0; i < iter->ref->num_indices; i++)
            {
                if (strcmp(key_ref->indices[i], iter->ref->indices[i]) != 0)
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

        return value_var;
    }

    return NULL;
}

void VariableTableIteratorDestroy(VariableTableIterator *iter)
{
    if (iter)
    {
        VarRefDestroy(iter->ref);
        RBTreeIteratorDestroy(iter->iter);
        free(iter);
    }
}
