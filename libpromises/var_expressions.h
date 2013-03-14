#ifndef CFENGINE_VAR_EXPRESSIONS_H
#define CFENGINE_VAR_EXPRESSIONS_H

#include "string_expressions.h"

typedef struct
{
    char *ns;
    char *scope;
    char *lval;
} VarRef;

VarRef VarRefParse(const char *var_ref_string);
void VarRefDestroy(VarRef ref);

#endif
