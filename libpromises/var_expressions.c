#include "var_expressions.h"

#include "cf3.defs.h"
#include "buffer.h"
#include "misc_lib.h"

#include <assert.h>

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

VarRef VarRefParseFromBundle(const char *qualified_name, const Bundle *bundle)
{
    char *ns = NULL;

    const char *scope_start = strchr(qualified_name, CF_NS);
    if (scope_start)
    {
        ns = xstrndup(qualified_name, scope_start - qualified_name);
        scope_start++;
    }
    else
    {
        scope_start = qualified_name;
    }

    char *scope = NULL;

    const char *lval_start = strchr(scope_start, '.');
    if (lval_start)
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

    const char *indices_start = strchr(lval_start, '[');
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

    if (!scope)
    {
        assert(ns == NULL && "A variable missing a scope should not have a namespace");
    }

    return (VarRef) {
        .ns = scope ? ns : (bundle ? xstrdup(bundle->ns) : NULL),
        .scope = scope ? scope : (bundle ? xstrdup(bundle->name) : NULL),
        .lval = lval,
        .indices = (const char *const *const)indices,
        .num_indices = num_indices,
        .allocated = true,
    };
}

VarRef VarRefParse(const char *var_ref_string)
{
    return VarRefParseFromBundle(var_ref_string, NULL);
}

void VarRefDestroy(VarRef ref)
{
    if (!ref.allocated)
    {
        ProgrammingError("Static VarRef has been passed to VarRefDestroy");
    }
    free((char *)ref.ns);
    free((char *)ref.scope);
    free((char *)ref.lval);
    for (int i = 0; i < ref.num_indices; ++i)
    {
        free((char *)ref.indices[i]);
    }
}

char *VarRefToString(VarRef ref, bool qualified)
{
    assert(ref.lval);

    Buffer *buf = BufferNew();
    if (qualified)
    {
        if (ref.ns)
        {
            BufferAppend(buf, ref.ns, strlen(ref.ns));
            BufferAppend(buf, ":", sizeof(char));
        }
        if (ref.scope)
        {
            BufferAppend(buf, ref.scope, strlen(ref.scope));
            BufferAppend(buf, ".", sizeof(char));
        }
    }

    BufferAppend(buf, ref.lval, strlen(ref.lval));

    for (size_t i = 0; i < ref.num_indices; i++)
    {
        BufferAppend(buf, "[", sizeof(char));
        BufferAppend(buf, ref.indices[i], strlen(ref.indices[i]));
        BufferAppend(buf, "]", sizeof(char));
    }

    char *var_string = xstrdup(BufferData(buf));
    BufferDestroy(&buf);
    return var_string;
}
