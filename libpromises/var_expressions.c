#include "var_expressions.h"

#include "cf3.defs.h"

VarRef VarRefParse(const char *qualified_name)
{
    char *ns = NULL;

    const char *start = strchr(qualified_name, CF_NS);
    if (start)
    {
        ns = xstrndup(qualified_name, start - qualified_name);
        start++;
    }
    else
    {
        start = qualified_name;
    }

    char *scope = NULL;

    const char *stop = strchr(start, '.');
    if (stop)
    {
        scope = xstrndup(start, stop - start);
    }

    return (VarRef) {
        .ns = ns,
        .scope = scope,
        .lval = xstrdup(stop ? stop + 1 : start)
    };
}

void VarRefDestroy(VarRef ref)
{
    free(ref.ns);
    free(ref.scope);
    free(ref.lval);
}
