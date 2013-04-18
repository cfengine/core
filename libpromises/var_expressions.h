#ifndef CFENGINE_VAR_EXPRESSIONS_H
#define CFENGINE_VAR_EXPRESSIONS_H

#include "string_expressions.h"

#include "platform.h"
#include "policy.h"

typedef struct
{
    const char *const ns;
    const char *const scope;
    const char *const lval;
    const char *const *const indices;
    const size_t num_indices;
    const bool allocated; /* Mark that VarRef was allocated by VarRefParse */
} VarRef;

VarRef VarRefParse(const char *var_ref_string);

/**
 * @brief Parse the variable reference in the context of a bundle. This means that the VarRef will inherit scope and namespace
 *        of the bundle if these are not specified explicitly in the string.
 */
VarRef VarRefParseFromBundle(const char *var_ref_string, const Bundle *bundle);

void VarRefDestroy(VarRef ref);

char *VarRefToString(VarRef ref, bool qualified);

#endif
