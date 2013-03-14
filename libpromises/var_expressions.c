#include "var_expressions.h"

#include "cf3.defs.h"
#include "buffer.h"

#include <assert.h>

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

VarRef VarRefParse(const char *qualified_name)
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
        indices = xmalloc(num_indices);

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

    return (VarRef) {
        .ns = ns,
        .scope = scope,
        .lval = lval,
        .indices = indices,
        .num_indices = num_indices
    };
}

void VarRefDestroy(VarRef ref)
{
    free(ref.ns);
    free(ref.scope);
    free(ref.lval);
}
