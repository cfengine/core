/*
   Copyright 2017 Northern.tech AS

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

#include <map.h>
#include <alloc.h>
#include <string_lib.h> /* String*() */
#include <regex.h>      /* CompileRegex,StringMatchFullWithPrecompiledRegex */
#include <files_names.h>


static void ClassDestroy(Class *cls);                /* forward declaration */

static void ClassDestroy_untyped(void *p)
{
    ClassDestroy(p);
}


/**
   Define ClassMap.
   Key: a string which is always the fully qualified class name,
        for example "default:127_0_0_1"
*/

TYPED_MAP_DECLARE(Class, char *, Class *)

TYPED_MAP_DEFINE(Class, char *, Class *,
                 StringHash_untyped,
                 StringSafeEqual_untyped,
                 free,
                 ClassDestroy_untyped)

struct ClassTable_
{
    ClassMap *classes;
};

struct ClassTableIterator_
{
    MapIterator iter;
    char *ns;
    bool is_hard;
    bool is_soft;
};


/**
 * @param #tags is a comma separated string of words.
 *              Both "" or NULL are equivalent.
 */
static void ClassInit(Class *cls,
                      const char *ns, const char *name,
                      bool is_soft, ContextScope scope, const char *tags)
{
    if (ns == NULL || strcmp(ns, "default") == 0)
    {
        cls->ns = NULL;
    }
    else
    {
        cls->ns = xstrdup(ns);
    }

    cls->name = xstrdup(name);
    CanonifyNameInPlace(cls->name);

    cls->is_soft = is_soft;
    cls->scope = scope;

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

static void ClassDestroy(Class *cls)
{
    if (cls)
    {
        ClassDestroySoft(cls);
        free(cls);
    }
}

ClassTable *ClassTableNew(void)
{
    ClassTable *table = xmalloc(sizeof(*table));

    table->classes = ClassMapNew();

    return table;
}

void ClassTableDestroy(ClassTable *table)
{
    if (table)
    {
        ClassMapDestroy(table->classes);
        free(table);
    }
}

bool ClassTablePut(ClassTable *table,
                   const char *ns, const char *name,
                   bool is_soft, ContextScope scope, const char *tags)
{
    assert(name);
    assert(is_soft || (!ns || strcmp("default", ns) == 0)); // hard classes should have default namespace
    assert(is_soft || scope == CONTEXT_SCOPE_NAMESPACE); // hard classes cannot be local

    if (ns == NULL)
    {
        ns = "default";
    }

    Class *cls = xmalloc(sizeof(*cls));
    ClassInit(cls, ns, name, is_soft, scope, tags);

    /* (cls->name != name) because canonification has happened. */
    char *fullname = StringConcatenate(3, ns, ":", cls->name);

    Log(LOG_LEVEL_DEBUG, "Setting %sclass: %s",
        is_soft ? "" : "hard ",
        fullname);

    return ClassMapInsert(table->classes, fullname, cls);
}

Class *ClassTableGet(const ClassTable *table, const char *ns, const char *name)
{
    if (ns == NULL)
    {
        ns = "default";
    }

    char fullname[ strlen(ns) + 1 + strlen(name) + 1 ];
    xsnprintf(fullname, sizeof(fullname), "%s:%s", ns, name);

    return ClassMapGet(table->classes, fullname);
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
    if (ns == NULL)
    {
        ns = "default";
    }

    char fullname[ strlen(ns) + 1 + strlen(name) + 1 ];
    xsnprintf(fullname, sizeof(fullname), "%s:%s", ns, name);

    return ClassMapRemove(table->classes, fullname);
}

bool ClassTableClear(ClassTable *table)
{
    bool has_classes = (ClassMapSize(table->classes) > 0);
    ClassMapClear(table->classes);
    return has_classes;
}

ClassTableIterator *ClassTableIteratorNew(const ClassTable *table,
                                          const char *ns,
                                          bool is_hard, bool is_soft)
{
    ClassTableIterator *iter = xmalloc(sizeof(*iter));

    iter->ns = ns ? xstrdup(ns) : NULL;
    iter->iter = MapIteratorInit(table->classes->impl);
    iter->is_soft = is_soft;
    iter->is_hard = is_hard;

    return iter;
}

Class *ClassTableIteratorNext(ClassTableIterator *iter)
{
    MapKeyValue *keyvalue;

    while ((keyvalue = MapIteratorNext(&iter->iter)) != NULL)
    {
        Class *cls = keyvalue->value;

        /* Make sure we never store "default" as namespace in the ClassTable,
         * instead we have always ns==NULL in that case. */
        CF_ASSERT_FIX(cls->ns == NULL ||
                      strcmp(cls->ns, "default") != 0,
                      (cls->ns = NULL),
                      "Class table contained \"default\" namespace,"
                      " should never happen!");

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
    assert(name != NULL);

    if (ns == NULL ||
        strcmp("default", ns) == 0)
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
