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
    unsigned h = 0;

    h = StringHash(ns ? ns : "default", h, INT_MAX);

    size_t len = strlen(name);
    for (size_t i = 0; i < len; i++)
    {
        h += (!isalnum(name[i]) || (name[i] == '.')) ? '_' : name[i];
        h += (h << 10);
        h ^= (h >> 6);
    }

    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);

    return (h & (INT_MAX - 1));
}

void ClassInit(Class *cls, const char *ns, const char *name, bool is_soft, ContextScope scope)
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
    cls->scope = scope;
    cls->tags = NULL;

    cls->hash = ClassRefHash(cls->ns, cls->name);
}

void ClassDestroy(Class *cls)
{
    if (cls)
    {
        free(cls->ns);
        free(cls->name);
        free(cls->tags);
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

bool ClassTablePut(ClassTable *table, const char *ns, const char *name, bool is_soft, ContextScope scope, char *tags)
{
    assert(name);
    assert(is_soft || (!ns || strcmp("default", ns) == 0)); // hard classes should have default namespace
    assert(is_soft || scope == CONTEXT_SCOPE_NAMESPACE); // hard classes cannot be local

    if (ns && strcmp("default", ns) == 0)
    {
        ns = NULL;
    }

    Class *cls = ClassTableGet(table, ns, name);
    if (cls)
    {
        ClassDestroy(cls);
        ClassInit(cls, ns, name, is_soft, scope);
        return true;
    }
    else
    {
        cls = xmalloc(sizeof(Class));
        ClassInit(cls, ns, name, is_soft, scope);
        // NULL tags are OK (but you want to give good tags, don't you?)
        cls->tags = StringSetFromString(tags, ',');
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
