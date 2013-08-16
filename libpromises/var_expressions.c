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

#include <var_expressions.h>

#include <cf3.defs.h>
#include <buffer.h>
#include <misc_lib.h>
#include <string_lib.h>
#include <hashes.h>

static size_t VarRefHash(const VarRef *ref)
{
    unsigned int h = 0;

    if (VarRefIsQualified(ref))
    {
        const char *ns = "default";
        int len = sizeof("default") - 1;
        if (ref->ns)
        {
            ns = ref->ns;
            len = strlen(ref->ns);
        }

        for (int i = 0; i < len; i++)
        {
            h += ns[i];
            h += (h << 10);
            h ^= (h >> 6);
        }

        len = strlen(ref->scope);
        for (int i = 0; i < len; i++)
        {
            h += ref->scope[i];
            h += (h << 10);
            h ^= (h >> 6);
        }
    }

    int len = strlen(ref->lval);
    for (int i = 0; i < len; i++)
    {
        h += ref->lval[i];
        h += (h << 10);
        h ^= (h >> 6);
    }

    for (size_t k = 0; k < ref->num_indices; k++)
    {
        len = strlen(ref->indices[k]);
        for (int i = 0; i < len; i++)
        {
            h += ref->indices[k][i];
            h += (h << 10);
            h ^= (h >> 6);
        }
    }

    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);

    return (h & (INT_MAX - 1));
}

VarRef *VarRefCopy(const VarRef *ref)
{
    VarRef *copy = xmalloc(sizeof(VarRef));

    copy->hash = ref->hash;
    copy->ns = ref->ns ? xstrdup(ref->ns) : NULL;
    copy->scope = ref->scope ? xstrdup(ref->scope) : NULL;
    copy->lval = ref->lval ? xstrdup(ref->lval) : NULL;

    copy->num_indices = ref->num_indices;
    if (ref->num_indices > 0)
    {
        copy->indices = xmalloc(ref->num_indices * sizeof(char*));
        for (size_t i = 0; i < ref->num_indices; i++)
        {
            copy->indices[i] = xstrdup(ref->indices[i]);
        }
    }
    else
    {
        copy->indices = NULL;
    }

    return copy;
}

VarRef *VarRefCopyLocalized(const VarRef *ref)
{
    VarRef *copy = xmalloc(sizeof(VarRef));

    copy->ns = NULL;
    copy->scope = xstrdup("this");
    copy->lval = ref->lval ? xstrdup(ref->lval) : NULL;

    copy->num_indices = ref->num_indices;
    if (ref->num_indices > 0)
    {
        copy->indices = xmalloc(ref->num_indices * sizeof(char*));
        for (size_t i = 0; i < ref->num_indices; i++)
        {
            copy->indices[i] = xstrdup(ref->indices[i]);
        }
    }
    else
    {
        copy->indices = NULL;
    }

    copy->hash = VarRefHash(copy);

    return copy;
}


#ifndef NDEBUG
static bool IndexBracketsBalance(const char *var_string)
{
    int count = 0;
    for (const char *c = var_string; *c != '\0'; c++)
    {
        if (*c == '[')
        {
            count++;
        }
        if (*c == ']')
        {
            count--;
        }
    }

    return count == 0;
}
#endif

static size_t IndexCount(const char *var_string)
{
    size_t count = 0;
    for (const char *c = var_string; *c != '\0'; c++)
    {
        if (*c == '[')
        {
            count++;
        }
    }

    return count;
}

VarRef *VarRefParseFromNamespaceAndScope(const char *qualified_name, const char *_ns, const char *_scope, char ns_separator, char scope_separator)
{
    char *ns = NULL;

    const char *indices_start = strchr(qualified_name, '[');

    const char *scope_start = strchr(qualified_name, ns_separator);
    if (scope_start && (!indices_start || scope_start < indices_start))
    {
        ns = xstrndup(qualified_name, scope_start - qualified_name);
        scope_start++;
    }
    else
    {
        scope_start = qualified_name;
    }

    char *scope = NULL;

    const char *lval_start = strchr(scope_start, scope_separator);

    if (lval_start && (!indices_start || lval_start < indices_start))
    {
        lval_start++;
        scope = xstrndup(scope_start, lval_start - scope_start - 1);
    }
    else
    {
        lval_start = scope_start;
    }

    char *lval = NULL;
    char **indices = NULL;
    size_t num_indices = 0;

    if (indices_start)
    {
        indices_start++;
        lval = xstrndup(lval_start, indices_start - lval_start - 1);

        assert("Index brackets in variable expression did not balance" && IndexBracketsBalance(indices_start - 1));

        num_indices = IndexCount(indices_start - 1);
        indices = xmalloc(num_indices * sizeof(char *));

        Buffer *buf = BufferNew();
        size_t cur_index = 0;
        for (const char *c = indices_start; *c != '\0'; c++)
        {
            if (*c == '[')
            {
                cur_index++;
            }
            else if (*c == ']')
            {
                indices[cur_index] = xstrdup(BufferData(buf));
                BufferZero(buf);
            }
            else
            {
                BufferAppend(buf, c, sizeof(char));
            }
        }
        BufferDestroy(&buf);
    }
    else
    {
        lval = xstrdup(lval_start);
    }

    assert(lval);

    if (!scope && !_scope)
    {
        assert(ns == NULL && "A variable missing a scope should not have a namespace");
    }

    VarRef *ref = xmalloc(sizeof(VarRef));

    ref->ns = ns ? ns : (_ns ? xstrdup(_ns) : NULL);
    ref->scope = scope ? scope : (_scope ? xstrdup(_scope) : NULL);
    ref->lval = lval;
    ref->indices = indices;
    ref->num_indices = num_indices;

    ref->hash = VarRefHash(ref);

    return ref;
}

VarRef *VarRefParse(const char *var_ref_string)
{
    return VarRefParseFromNamespaceAndScope(var_ref_string, NULL, NULL, CF_NS, '.');
}

VarRef *VarRefParseFromScope(const char *var_ref_string, const char *scope)
{
    if (!scope)
    {
        return VarRefParseFromNamespaceAndScope(var_ref_string, NULL, NULL, CF_NS, '.');
    }

    const char *scope_start = strchr(scope, CF_NS);
    if (scope_start)
    {
        char *ns = xstrndup(scope, scope_start - scope);
        VarRef *ref = VarRefParseFromNamespaceAndScope(var_ref_string, ns, scope_start + 1, CF_NS, '.');
        free(ns);
        return ref;
    }
    else
    {
        return VarRefParseFromNamespaceAndScope(var_ref_string, NULL, scope, CF_NS, '.');
    }
}

VarRef *VarRefParseFromBundle(const char *var_ref_string, const Bundle *bundle)
{
    if (bundle)
    {
        return VarRefParseFromNamespaceAndScope(var_ref_string, bundle->ns, bundle->name, CF_NS, '.');
    }
    else
    {
        return VarRefParse(var_ref_string);
    }
}

void VarRefDestroy(VarRef *ref)
{
    if (ref)
    {
        free(ref->ns);
        free(ref->scope);
        free(ref->lval);
        if (ref->num_indices > 0)
        {
            for (int i = 0; i < ref->num_indices; ++i)
            {
                free(ref->indices[i]);
            }
            free(ref->indices);
        }

        free(ref);
    }

}

char *VarRefToString(const VarRef *ref, bool qualified)
{
    assert(ref->lval);

    Buffer *buf = BufferNew();
    if (qualified && VarRefIsQualified(ref))
    {
        const char *ns = ref->ns ? ref->ns : "default";

        BufferAppend(buf, ns, strlen(ns));
        BufferAppend(buf, ":", sizeof(char));
        BufferAppend(buf, ref->scope, strlen(ref->scope));
        BufferAppend(buf, ".", sizeof(char));
    }

    BufferAppend(buf, ref->lval, strlen(ref->lval));

    for (size_t i = 0; i < ref->num_indices; i++)
    {
        BufferAppend(buf, "[", sizeof(char));
        BufferAppend(buf, ref->indices[i], strlen(ref->indices[i]));
        BufferAppend(buf, "]", sizeof(char));
    }

    char *var_string = xstrdup(BufferData(buf));
    BufferDestroy(&buf);
    return var_string;
}

char *VarRefMangle(const VarRef *ref)
{
    char *suffix = VarRefToString(ref, false);

    if (!ref->scope)
    {
        return suffix;
    }
    else
    {
        if (ref->ns)
        {
            char *mangled = StringFormat("%s*%s#%s", ref->ns, ref->scope, suffix);
            free(suffix);
            return mangled;
        }
        else
        {
            char *mangled = StringFormat("%s#%s", ref->scope, suffix);
            free(suffix);
            return mangled;
        }
    }
}

VarRef *VarRefDeMangle(const char *mangled_var_ref)
{
    return VarRefParseFromNamespaceAndScope(mangled_var_ref, NULL, NULL, '*', '#');
}

static bool VarRefIsMeta(VarRef *ref)
{
    return StringEndsWith(ref->scope, "_meta");
}

void VarRefSetMeta(VarRef *ref, bool enabled)
{
    if (enabled)
    {
        if (!VarRefIsMeta(ref))
        {
            char *tmp = ref->scope;
            memcpy(ref->scope, StringConcatenate(2, ref->scope, "_meta"), sizeof(char*));
            free(tmp);
        }
    }
    else
    {
        if (VarRefIsMeta(ref))
        {
            char *tmp = ref->scope;
            size_t len = strlen(ref->scope);
            memcpy(ref->scope, StringSubstring(ref->scope, len, 0, len - strlen("_meta")), sizeof(char*));
            free(tmp);
        }
    }

    ref->hash = VarRefHash(ref);
}

bool VarRefIsQualified(const VarRef *ref)
{
    return ref->scope != NULL;
}

void VarRefQualify(VarRef *ref, const char *ns, const char *scope)
{
    assert(scope);

    free(ref->ns);
    ref->ns = NULL;

    free(ref->scope);
    ref->scope = NULL;

    if (ns)
    {
        ref->ns = xstrdup(ns);
    }
    ref->scope = xstrdup(scope);

    ref->hash = VarRefHash(ref);
}

void VarRefAddIndex(VarRef *ref, const char *index)
{
    if (ref->indices)
    {
        assert(ref->num_indices > 0);
        ref->indices = xrealloc(ref->indices, sizeof(char *) * (ref->num_indices + 1));
    }
    else
    {
        assert(ref->num_indices == 0);
        ref->indices = xmalloc(sizeof(char *));
    }

    ref->indices[ref->num_indices] = xstrdup(index);
    ref->num_indices++;

    ref->hash = VarRefHash(ref);
}

int VarRefCompare(const VarRef *a, const VarRef *b)
{
    int ret = strcmp(a->lval, b->lval);
    if (ret != 0)
    {
        return ret;
    }

    ret = strcmp(NULLStringToEmpty(a->scope), NULLStringToEmpty(b->scope));
    if (ret != 0)
    {
        return ret;
    }

    const char *a_ns = a->ns ? a->ns : "default";
    const char *b_ns = b->ns ? b->ns : "default";

    ret = strcmp(a_ns, b_ns);
    if (ret != 0)
    {
        return ret;
    }

    ret = a->num_indices - b->num_indices;
    if (ret != 0)
    {
        return ret;
    }

    for (size_t i = 0; i < a->num_indices; i++)
    {
        ret = strcmp(a->indices[i], b->indices[i]);
        if (ret != 0)
        {
            return ret;
        }
    }

    return 0;
}
