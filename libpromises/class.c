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
#include <class.h>

#include <rb-tree.h>
#include <alloc.h>
#include <string_lib.h> /* StringHash,StringConcatenate */
#include <regex.h>      /* CompileRegex,StringMatchFullWithPrecompiledRegex */
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

static size_t ClassRefHash(const char *ns, const char *name, bool is_collision)
{
    ns = ns ? ns : "default";

    // if there's collision, append "__"
    char *str = is_collision ? StringConcatenate(3, ns, name, "__") : StringConcatenate(2, ns, name);
    CanonifyNameInPlace(str);

    // no need to mask, we are generating 32bit integer anyway
    return MurmurHash3_32(str, 0);
}

/**
 * @param #tags is a comma separated string of words.
 *              Both "" or NULL are equivalent.
 * @param #hash is the hash generated for the class + namespace
 */
void ClassInit(Class *cls, const char *ns, const char *name, bool is_soft,
               ContextScope scope, const char *tags, size_t hash)
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
    cls->hash = hash;

    cls->tags = StringSetFromString(tags, ',');
    if (!is_soft && !StringSetContains(cls->tags, "hardclass"))
    {
        StringSetAdd(cls->tags, xstrdup("hardclass"));
    }
}

static void ClassDestroySoft(Class *cls)
{
    if (cls)
    {
        free(cls->ns);
        free(cls->name);
        StringSetDestroy(cls->tags);
    }
}

void ClassDestroy(Class *cls)
{
    if (cls)
    {
        ClassDestroySoft(cls);
        free(cls);
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
        free(table);
    }
}

Class *ClassTableGetCollision(const ClassTable *table, const char *ns, const char *name, unsigned int *hashp)
{
    size_t hash = ClassRefHash(ns, name, false);
    Class *cls = RBTreeGet(table->classes, (void *)hash);

    if (hashp)
    {
        *hashp = hash;
    }

    const char *classns = cls && cls->ns ? cls->ns : "default";
    ns = ns ? ns : "default";

    if (cls && !strcmp(cls->name, name) && !strcmp(classns, ns))
    {
        return cls;
    }

    // we detect a collision if hash matches but class name and namespace do not
    Class *cls2;
    hash = ClassRefHash(ns, name, true);
    cls2 = RBTreeGet(table->classes, (void *)hash);

    // 1st level collision resolution
    if (cls)
    {
        if (hashp)
        {
            *hashp = hash;
        }
        return cls2;
    }

    return cls;
}

bool ClassTablePut(ClassTable *table, const char *ns, const char *name, bool is_soft, ContextScope scope, const char *tags)
{
    assert(name);
    assert(is_soft || (!ns || strcmp("default", ns) == 0)); // hard classes should have default namespace
    assert(is_soft || scope == CONTEXT_SCOPE_NAMESPACE); // hard classes cannot be local

    if (ns && strcmp("default", ns) == 0)
    {
        ns = NULL;
    }

    size_t hash = 0;
    Class *cls = ClassTableGetCollision(table, ns, name, (unsigned int *)&hash);

    if (cls)
    {
        /* Saves a malloc() and an RBTreePut() call. */
        ClassDestroySoft(cls);
        ClassInit(cls, ns, name, is_soft, scope, tags, hash);
        return true;
    }
    else
    {
        if (ns == NULL ||
            strcmp("default", ns) == 0)
        {
            Log(LOG_LEVEL_DEBUG, "Setting %sclass %s",
                is_soft ? "" : "hard ",
                name);
        }
        else                                        /* also print namespace */
        {
            Log(LOG_LEVEL_DEBUG, "Setting %sclass %s:%s",
                is_soft ? "" : "hard ",
                ns, name);
        }

        cls = xmalloc(sizeof(Class));
        // use the already generated hash, saves duplicate hash function calls
        ClassInit(cls, ns, name, is_soft, scope, tags, hash);
        return RBTreePut(table->classes, (void *)cls->hash, cls);
    }
}

Class *ClassTableGet(const ClassTable *table, const char *ns, const char *name)
{
    return ClassTableGetCollision(table, ns, name, NULL);
}

Class *ClassTableMatch(const ClassTable *table, const char *regex)
{
    ClassTableIterator *it = ClassTableIteratorNew(table, NULL, true, true);
    Class *cls = NULL;

    pcre *pattern = CompileRegex(regex);
    if (pattern == NULL)
    {
        // TODO: perhaps pcre has can give more info on this error?
        Log(LOG_LEVEL_ERR, "Unable to pcre compile regex '%s'", regex);
        return false;
    }

    while ((cls = ClassTableIteratorNext(it)))
    {
        bool matched;
        if (cls->ns)
        {
            char *class_expr = ClassRefToString(cls->ns, cls->name);
            matched = StringMatchFullWithPrecompiledRegex(pattern, class_expr);
            free(class_expr);
        }
        else
        {
            matched = StringMatchFullWithPrecompiledRegex(pattern, cls->name);
        }

        if (matched)
        {
            break;
        }
    }

    pcre_free(pattern);

    ClassTableIteratorDestroy(it);
    return cls;
}

bool ClassTableRemove(ClassTable *table, const char *ns, const char *name)
{
    Class *cls = ClassTableGet(table, ns, name);
    if (!cls)
    {
        return false;
    }

    return RBTreeRemove(table->classes, (void *)cls->hash);
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
        return (ClassRef) { .ns = NULL, .name = xstrdup(expr) };
    }
    else
    {
        char *ns = NULL;
        if ((name_start - expr) > 0)
        {
            ns = xstrndup(expr, name_start - expr);
        }
        else
        {
            // this would be invalid syntax
            ns = xstrdup("");
        }
        char *name = xstrdup(name_start + 1);
        return (ClassRef) { .ns = ns, .name = name };
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

bool ClassRefIsQualified(ClassRef ref)
{
    return ref.ns != NULL;
}

void ClassRefQualify(ClassRef *ref, const char *ns)
{
    free(ref->ns);
    ref->ns = xstrdup(ns);
}

void ClassRefDestroy(ClassRef ref)
{
    free(ref.ns);
    free(ref.name);
}
