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

#include "var_expressions.h"

#include "cf3.defs.h"
#include "buffer.h"
#include "misc_lib.h"
#include "string_lib.h"


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
    if (qualified)
    {
        if (ref->ns)
        {
            BufferAppend(buf, ref->ns, strlen(ref->ns));
            BufferAppend(buf, ":", sizeof(char));
        }
        if (ref->scope)
        {
            BufferAppend(buf, ref->scope, strlen(ref->scope));
            BufferAppend(buf, ".", sizeof(char));
        }
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
}

bool VarRefIsQualified(const VarRef *ref)
{
    return ref->scope != NULL;
}
