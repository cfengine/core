#include <variable.h>

#include <alloc.h>
#include <rb-tree.h>
#include <rlist.h>
#include <writer.h>

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
        StringSetDestroy(var->tags);
        // Nothing to do for ->promise

        free(var);
    }
}

VariableTable *VariableTableNew(void)
{
    VariableTable *table = xmalloc(sizeof(VariableTable));

    table->vars = RBTreeNew(NULL, NULL, NULL,
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
    return RBTreeGet(table->vars, (void *)ref->hash);
}

bool VariableTableRemove(VariableTable *table, const VarRef *ref)
{
    return RBTreeRemove(table->vars, (void *)ref->hash);
}

static Variable *VariableNew(VarRef *ref, Rval rval, DataType type, StringSet *tags, const Promise *promise)
{
    Variable *var = xmalloc(sizeof(Variable));

    var->ref = ref;
    var->rval = rval;
    var->type = type;
    var->tags = tags;
    var->promise = promise;

    return var;
}

bool VariableTablePut(VariableTable *table, const VarRef *ref,
                      const Rval *rval, DataType type,
                      const char *tags, const Promise *promise)
{
    assert(VarRefIsQualified(ref));
    bool result;

    Variable *var = VariableTableGet(table, ref);
    if (var == NULL)
    {
        var = VariableNew(VarRefCopy(ref), RvalCopy(*rval), type, StringSetFromString(tags, ','), promise);
        result = RBTreePut(table->vars, (void *)var->ref->hash, var);
    }
    else // if (!RvalsEqual(var->rval *rval) // TODO: implement-me !
    {
        RvalDestroy(var->rval);
        var->rval = RvalCopy(*rval);
        var->type = type;
        var->promise = promise;
        result = true;
    }
    return result;
}

bool VariableTableClear(VariableTable *table, const char *ns, const char *scope, const char *lval)
{
    if (!ns && !scope && !lval)
    {
        bool has_vars = RBTreeSize(table->vars) > 0;
        RBTreeClear(table->vars);
        return has_vars;
    }

    RBTree *remove_set = RBTreeNew(NULL, NULL, NULL, NULL, NULL, NULL);

    {
        VariableTableIterator *iter = VariableTableIteratorNew(table, ns, scope, lval);
        for (Variable *v = VariableTableIteratorNext(iter); v; v = VariableTableIteratorNext(iter))
        {
            RBTreePut(remove_set, (void *)v->ref->hash, v);
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
        void *ref_key = NULL;
        Variable *var = NULL;
        while (RBTreeIteratorNext(iter, &ref_key, (void **)&var))
        {
            if (VariableTableRemove(table, var->ref))
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
    void *key_ref = NULL;
    Variable *var = NULL;

    while (RBTreeIteratorNext(iter->iter, &key_ref, (void **)&var))
    {
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
        RBTreeIteratorDestroy(iter->iter);
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
        Variable *localized_var = VariableNew(VarRefCopyLocalized(foreign_var->ref),
                                              RvalCopy(foreign_var->rval), foreign_var->type,
                                              NULL, foreign_var->promise);
        RBTreePut(localized_copy->vars, (void *)localized_var->ref->hash, localized_var);
    }
    VariableTableIteratorDestroy(iter);

    return localized_copy;
}
