#include <class.h>

#include <rb-tree.h>
#include <alloc.h>
#include <string_lib.h>
#include <files_names.h>

struct ClassTable_
{
    RBTree *classes;
};

struct ClassTableIterator_
{
    RBTreeIterator *iter;
    char *ns;
    bool is_hard;
    bool is_soft;
};

static size_t ClassRefHash(const char *ns, const char *name)
{
    unsigned hash = 0;

    hash = StringHash(ns ? ns : "default", hash, INT_MAX);
    hash = StringHash(name, hash, INT_MAX);

    return hash;
}

void ClassInit(Class *cls, const char *ns, const char *name, bool is_soft)
{
    if (ns)
    {
        cls->ns = xstrdup(ns);
    }
    else
    {
        cls->ns = NULL;
    }

    cls->name = xstrdup(name);
    CanonifyNameInPlace(cls->name);

    cls->is_soft = is_soft;

    cls->hash = ClassRefHash(cls->ns, cls->name);
}

void ClassDestroy(Class *cls)
{
    if (cls)
    {
        free(cls->ns);
        free(cls->name);
    }
}

ClassTable *ClassTableNew(void)
{
    ClassTable *table = xmalloc(sizeof(ClassTable));

    table->classes = RBTreeNew(NULL, NULL, NULL, NULL, NULL, (RBTreeValueDestroyFn *)ClassDestroy);

    return table;
}

void ClassTableDestroy(ClassTable *table)
{
    if (table)
    {
        RBTreeDestroy(table->classes);
    }
}

bool ClassTablePut(ClassTable *table, const char *ns, const char *name, bool is_soft)
{
    assert(name);
    assert(strchr(name, ':') == NULL);

    if (ns && strcmp("default", ns) == 0)
    {
        ns = NULL;
    }

    Class *cls = ClassTableGet(table, ns, name);
    if (cls)
    {
        ClassDestroy(cls);
        ClassInit(cls, ns, name, is_soft);
        return true;
    }
    else
    {
        cls = xmalloc(sizeof(Class));
        ClassInit(cls, ns, name, is_soft);
        return RBTreePut(table->classes, (void *)cls->hash, cls);
    }
}

Class *ClassTableGet(const ClassTable *table, const char *ns, const char *name)
{
    size_t hash = ClassRefHash(ns, name);
    return RBTreeGet(table->classes, (void *)hash);
}

bool ClassTableRemove(ClassTable *table, const char *ns, const char *name)
{
    size_t hash = ClassRefHash(ns, name);
    return RBTreeRemove(table->classes, (void *)hash);
}

bool ClassTableClear(ClassTable *table)
{
    bool has_classes = RBTreeSize(table->classes) > 0;
    RBTreeClear(table->classes);
    return has_classes;
}

ClassTableIterator *ClassTableIteratorNew(const ClassTable *table, const char *ns, bool is_hard, bool is_soft)
{
    ClassTableIterator *iter = xmalloc(sizeof(ClassTableIterator));

    iter->ns = ns ? xstrdup(ns) : NULL;
    iter->iter = RBTreeIteratorNew(table->classes);
    iter->is_soft = is_soft;
    iter->is_hard = is_hard;

    return iter;
}

Class *ClassTableIteratorNext(ClassTableIterator *iter)
{
    void *key_ref = NULL;
    Class *cls = NULL;

    while (RBTreeIteratorNext(iter->iter, &key_ref, (void **)&cls))
    {
        const char *key_ns = cls->ns ? cls->ns : "default";

        if (iter->ns && strcmp(key_ns, iter->ns) != 0)
        {
            continue;
        }

        if (iter->is_soft && !iter->is_soft)
        {
            continue;
        }
        if (iter->is_hard && !iter->is_hard)
        {
            continue;
        }

        return cls;
    }

    return NULL;
}

void ClassTableIteratorDestroy(ClassTableIterator *iter)
{
    if (iter)
    {
        free(iter->ns);
        RBTreeIteratorDestroy(iter->iter);
        free(iter);
    }
}

ClassRef ClassRefParse(const char *expr)
{
    char *name_start = strchr(expr, ':');
    if (!name_start)
    {
        return (ClassRef) { NULL, xstrdup(expr) };
    }
    else
    {
        char *ns = NULL;
        if (strncmp("default", expr, name_start - expr) != 0)
        {
            ns = xstrndup(expr, name_start - expr);
        }
        char *name = xstrdup(name_start + 1);
        return (ClassRef) { ns, name };
    }
}

char *ClassRefToString(const char *ns, const char *name)
{
    if (!ns || strcmp("default", ns) == 0)
    {
        return xstrdup(name);
    }
    else
    {
        return StringConcatenate(3, ns, ":", name);
    }
}

void ClassRefDestroy(ClassRef ref)
{
    free(ref.ns);
    free(ref.name);
}
