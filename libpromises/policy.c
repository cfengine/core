/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.

*/

#include "policy.h"

#include "syntax.h"
#include "string_lib.h"
#include "conversion.h"
#include "reporting.h"
#include "mutex.h"
#include "logging.h"
#include "misc_lib.h"
#include "mod_files.h"
#include "vars.h"
#include "fncall.h"
#include "rlist.h"
#include "set.h"
#include "hashes.h"
#include "env_context.h"
#include "promises.h"
#include "item_lib.h"
#include "files_hashes.h"
#include "audit.h"

#include <assert.h>

//************************************************************************

static const char *POLICY_ERROR_POLICY_NOT_RUNNABLE = "Policy is not runnable (does not contain a body common control)";



static const char *POLICY_ERROR_BUNDLE_NAME_RESERVED = "Use of a reserved container name as a bundle name \"%s\"";
static const char *POLICY_ERROR_BUNDLE_REDEFINITION = "Duplicate definition of bundle %s with type %s";
static const char *POLICY_ERROR_BUNDLE_UNDEFINED = "Undefined bundle %s with type %s";
static const char *POLICY_ERROR_BODY_REDEFINITION = "Duplicate definition of body %s with type %s";
static const char *POLICY_ERROR_BODY_UNDEFINED = "Undefined body %s with type %s";
static const char *POLICY_ERROR_PROMISE_UNCOMMENTED = "Promise is missing a comment attribute, and comments are required by policy";
static const char *POLICY_ERROR_PROMISE_DUPLICATE_HANDLE = "Duplicate promise handle %s found";
static const char *POLICY_ERROR_LVAL_INVALID = "Promise type %s has unknown attribute %s";

static const char *POLICY_ERROR_CONSTRAINT_TYPE_MISMATCH = "Type mismatch in constraint: %s";

static const char *POLICY_ERROR_EMPTY_VARREF = "Empty variable reference";

//************************************************************************

static void BundleDestroy(Bundle *bundle);
static void BodyDestroy(Body *body);
static SyntaxTypeMatch ConstraintCheckType(const Constraint *cp);
static bool PromiseCheck(const Promise *pp, Seq *errors);


const char *NamespaceDefault(void)
{
    return "default";
}

Policy *PolicyNew(void)
{
    Policy *policy = xcalloc(1, sizeof(Policy));

    policy->bundles = SeqNew(100, BundleDestroy);
    policy->bodies = SeqNew(100, BodyDestroy);

    return policy;
}

int PolicyCompare(const void *a, const void *b)
{
    return a - b;
}

void PolicyDestroy(Policy *policy)
{
    if (policy)
    {
        SeqDestroy(policy->bundles);
        SeqDestroy(policy->bodies);

        free(policy);
    }
}

static char *StripNamespace(const char *full_symbol)
{
    char *sep = strchr(full_symbol, CF_NS);
    if (sep)
    {
        return xstrdup(sep + 1);
    }
    else
    {
        return xstrdup(full_symbol);
    }
}

Body *PolicyGetBody(const Policy *policy, const char *ns, const char *type, const char *name)
{
    for (size_t i = 0; i < SeqLength(policy->bodies); i++)
    {
        Body *bp = SeqAt(policy->bodies, i);

        char *body_symbol = StripNamespace(bp->name);

        if (strcmp(bp->type, type) == 0 && strcmp(body_symbol, name) == 0)
        {
            free(body_symbol);

            // allow namespace to be optionally matched
            if (ns && strcmp(bp->ns, ns) != 0)
            {
                continue;
            }

            return bp;
        }

        free(body_symbol);
    }

    return NULL;
}

Bundle *PolicyGetBundle(const Policy *policy, const char *ns, const char *type, const char *name)
{
    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);

        char *bundle_symbol = StripNamespace(bp->name);

        if ((!type || strcmp(bp->type, type) == 0) && ((strcmp(bundle_symbol, name) == 0) || (strcmp(bp->name, name) == 0)))
        {
            free(bundle_symbol);

            // allow namespace to be optionally matched
            if (ns && strcmp(bp->ns, ns) != 0)
            {
                continue;
            }

            return bp;
        }

        free(bundle_symbol);
    }

    return NULL;
}

bool PolicyIsRunnable(const Policy *policy)
{
    return PolicyGetBody(policy, NULL, "common", "control") != NULL;
}

Policy *PolicyMerge(Policy *a, Policy *b)
{
    Policy *result = PolicyNew();

    SeqAppendSeq(result->bundles, a->bundles);
    SeqAppendSeq(result->bundles, b->bundles);

    for (size_t i = 0; i < SeqLength(result->bundles); i++)
    {
        Bundle *bp = SeqAt(result->bundles, i);
        bp->parent_policy = result;
    }

    SeqAppendSeq(result->bodies, a->bodies);
    SeqAppendSeq(result->bodies, b->bodies);

    for (size_t i = 0; i < SeqLength(result->bodies); i++)
    {
        Body *bdp = SeqAt(result->bodies, i);
        bdp->parent_policy = result;
    }

    free(a);
    free(b);

    return result;
}

const char *ConstraintGetNamespace(const Constraint *cp)
{
    switch (cp->type)
    {
    case POLICY_ELEMENT_TYPE_BODY:
        return cp->parent.body->ns;

    case POLICY_ELEMENT_TYPE_PROMISE:
        return cp->parent.promise->parent_promise_type->parent_bundle->ns;

    default:
        ProgrammingError("Constraint has parent type: %d", cp->type);
    }
}

/*************************************************************************/

Policy *PolicyFromPromise(const Promise *promise)
{
    assert(promise);

    PromiseType *promise_type = promise->parent_promise_type;
    assert(promise_type);

    Bundle *bundle = promise_type->parent_bundle;
    assert(bundle);

    return bundle->parent_policy;
}

char *BundleQualifiedName(const Bundle *bundle)
{
    assert(bundle);
    if (!bundle)
    {
        return NULL;
    }

    if (bundle->name)
    {
        const char *ns = bundle->ns ? bundle->ns : NamespaceDefault();
        return StringConcatenate(3, ns, ":", bundle->name);  // CF_NS == ':'
    }

    return NULL;
}

static bool RvalTypeCheckDataType(RvalType rval_type, DataType expected_datatype)
{
    if (rval_type == RVAL_TYPE_FNCALL)
    {
        return true;
    }

    switch (expected_datatype)
    {
    case DATA_TYPE_BODY:
    case DATA_TYPE_BUNDLE:
        return rval_type == RVAL_TYPE_SCALAR;

    case DATA_TYPE_CONTEXT:
    case DATA_TYPE_COUNTER:
    case DATA_TYPE_INT:
    case DATA_TYPE_INT_RANGE:
    case DATA_TYPE_OPTION:
    case DATA_TYPE_REAL:
    case DATA_TYPE_REAL_RANGE:
    case DATA_TYPE_STRING:
        return rval_type == RVAL_TYPE_SCALAR;

    case DATA_TYPE_CONTEXT_LIST:
    case DATA_TYPE_INT_LIST:
    case DATA_TYPE_OPTION_LIST:
    case DATA_TYPE_REAL_LIST:
    case DATA_TYPE_STRING_LIST:
        return (rval_type == RVAL_TYPE_SCALAR) || (rval_type == RVAL_TYPE_LIST);

    default:
        ProgrammingError("Unhandled expected datatype in switch: %d", expected_datatype);
    }
}

/*************************************************************************/

/* Check if a constraint's syntax is correct according to its promise_type and
   lvalue.
*/
static bool ConstraintCheckSyntax(const Constraint *constraint, Seq *errors)
{
    if (constraint->type != POLICY_ELEMENT_TYPE_PROMISE)
    {
        ProgrammingError("Attempted to check the syntax for a constraint"
                         " not belonging to a promise");
    }

    const PromiseType *promise_type = constraint->parent.promise->parent_promise_type;
    const Bundle *bundle = promise_type->parent_bundle;

    /* Check if lvalue is valid for the bundle's specific promise_type. */
    const PromiseTypeSyntax *promise_type_syntax = PromiseTypeSyntaxLookup(bundle->type, promise_type->name);
    for (size_t i = 0; promise_type_syntax->constraint_set.constraints[i].lval != NULL; i++)
    {
        const ConstraintSyntax *body_syntax = &promise_type_syntax->constraint_set.constraints[i];
        if (strcmp(body_syntax->lval, constraint->lval) == 0)
        {
            if (!RvalTypeCheckDataType(constraint->rval.type, body_syntax->dtype))
            {
                SeqAppend(errors,
                          PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, constraint,
                                         POLICY_ERROR_CONSTRAINT_TYPE_MISMATCH, constraint->lval));
                return false;
            }
            return true;
        }
    }
    /* FIX: Call a VerifyConstraint() hook for the specific promise_type, defined
       in verify_TYPE.c, that checks for promise_type-specific constraint syntax. */

    /* Check if lvalue is valid for all bodies. */
    for (size_t i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
    {
        if (strcmp(constraint->lval, CF_COMMON_BODIES[i].lval) == 0)
        {
            if (!RvalTypeCheckDataType(constraint->rval.type, CF_COMMON_BODIES[i].dtype))
            {
                SeqAppend(errors,
                          PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, constraint,
                                         POLICY_ERROR_CONSTRAINT_TYPE_MISMATCH, constraint->lval));
                return false;
            }
            return true;
        }
    }
    for (size_t i = 0; CF_COMMON_EDITBODIES[i].lval != NULL; i++)
    {
        if (strcmp(constraint->lval, CF_COMMON_EDITBODIES[i].lval) == 0)
        {
            if (!RvalTypeCheckDataType(constraint->rval.type, CF_COMMON_EDITBODIES[i].dtype))
            {
                SeqAppend(errors,
                          PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, constraint,
                                         POLICY_ERROR_CONSTRAINT_TYPE_MISMATCH, constraint->lval));
                return false;
            }
            return true;
        }
    }
    for (size_t i = 0; CF_COMMON_XMLBODIES[i].lval != NULL; i++)
    {
        if (strcmp(constraint->lval, CF_COMMON_XMLBODIES[i].lval) == 0)
        {
            if (!RvalTypeCheckDataType(constraint->rval.type, CF_COMMON_XMLBODIES[i].dtype))
            {
                SeqAppend(errors,
                          PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, constraint,
                                         POLICY_ERROR_CONSTRAINT_TYPE_MISMATCH, constraint->lval));
                return false;
            }
            return true;
        }
    }

    /* lval is unknown for this promise type */
    SeqAppend(errors,
              PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, constraint,
                             POLICY_ERROR_LVAL_INVALID,
                             constraint->parent.promise->parent_promise_type->name,
                             constraint->lval));

    return false;
}

/*************************************************************************/

static bool PolicyCheckPromiseType(const PromiseType *promise_type, Seq *errors)
{
    assert(promise_type);
    assert(promise_type->parent_bundle);
    bool success = true;

    for (size_t i = 0; i < SeqLength(promise_type->promises); i++)
    {
        const Promise *pp = SeqAt(promise_type->promises, i);
        success &= PromiseCheck(pp, errors);
    }

    return success;
}

/*************************************************************************/

static bool PolicyCheckBundle(const Bundle *bundle, Seq *errors)
{
    assert(bundle);
    bool success = true;

    // ensure no reserved bundle names are used
    {
        static const char *reserved_names[] = { "sys", "const", "mon", "edit", "match", "mon", "this", NULL };
        if (IsStrIn(bundle->name, reserved_names))
        {
            SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_BUNDLE, bundle,
                                                  POLICY_ERROR_BUNDLE_NAME_RESERVED, bundle->name));
            success = false;
        }
    }

    for (size_t i = 0; i < SeqLength(bundle->promise_types); i++)
    {
        const PromiseType *type = SeqAt(bundle->promise_types, i);
        success &= PolicyCheckPromiseType(type, errors);
    }

    return success;
}

/*************************************************************************/

/* Get the syntax of a constraint according to its promise_type and lvalue.
   Make sure you've already checked the constraint's validity.
*/
static const ConstraintSyntax *ConstraintGetSyntax(const Constraint *constraint)
{
    if (constraint->type != POLICY_ELEMENT_TYPE_PROMISE)
    {
        ProgrammingError("Attempted to get the syntax for a constraint not belonging to a promise");
    }

    const Promise *promise = constraint->parent.promise;
    const PromiseType *promise_type = promise->parent_promise_type;
    const Bundle *bundle = promise_type->parent_bundle;

    const PromiseTypeSyntax *promise_type_syntax = PromiseTypeSyntaxLookup(bundle->type, promise_type->name);

    /* Check if lvalue is valid for the bundle's specific promise_type. */
    for (size_t i = 0; promise_type_syntax->constraint_set.constraints[i].lval != NULL; i++)
    {
        const ConstraintSyntax *body_syntax = &promise_type_syntax->constraint_set.constraints[i];
        if (strcmp(body_syntax->lval, constraint->lval) == 0)
        {
            return body_syntax;
        }
    }

    /* Check if lvalue is valid for all bodies. */
    for (size_t i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
    {
        if (strcmp(constraint->lval, CF_COMMON_BODIES[i].lval) == 0)
        {
            return &CF_COMMON_BODIES[i];
        }
    }
    for (size_t i = 0; CF_COMMON_EDITBODIES[i].lval != NULL; i++)
    {
        if (strcmp(constraint->lval, CF_COMMON_EDITBODIES[i].lval) == 0)
        {
            return &CF_COMMON_EDITBODIES[i];
        }
    }
    for (size_t i = 0; CF_COMMON_XMLBODIES[i].lval != NULL; i++)
    {
        if (strcmp(constraint->lval, CF_COMMON_XMLBODIES[i].lval) == 0)
        {
            return &CF_COMMON_XMLBODIES[i];
        }
    }

    /* Syntax must have been checked first during PolicyCheckPartial(). */
    ProgrammingError("ConstraintGetSyntax() was called for constraint with "
                     "invalid lvalue: %s", constraint->lval);
    return NULL;
}

/*************************************************************************/

/**
 * @return A reference to the full symbol value of the Rval regardless of type, e.g. "foo:bar" -> "foo:bar"
 */
static const char *RvalFullSymbol(const Rval *rval)
{
    switch (rval->type)
    {
    case RVAL_TYPE_SCALAR:
        return rval->item;
        break;

    case RVAL_TYPE_FNCALL:
        return ((FnCall *)rval->item)->name;

    default:
        ProgrammingError("Cannot get full symbol value from Rval of type %c", rval->type);
        return NULL;
    }
}

/**
 * @return A copy of the namespace compoent of an Rval, or NULL. e.g. "foo:bar" -> "foo"
 */
char *QualifiedNameNamespaceComponent(const char *qualified_name)
{
    if (strchr(qualified_name, CF_NS))
    {
        char ns[CF_BUFSIZE] = { 0 };
        sscanf(qualified_name, "%[^:]", ns);

        return xstrdup(ns);
    }
    else
    {
        return NULL;
    }
}

/**
 * @return A copy of the symbol compoent of an Rval, or NULL. e.g. "foo:bar" -> "bar"
 */
char *QualifiedNameScopeComponent(const char *qualified_name)
{
    char *sep = strchr(qualified_name, CF_NS);
    if (sep)
    {
        return xstrdup(sep + 1);
    }
    else
    {
        return xstrdup(qualified_name);
    }
}

static bool PolicyCheckUndefinedBodies(const Policy *policy, Seq *errors)
{
    bool success = true;

    for (size_t bpi = 0; bpi < SeqLength(policy->bundles); bpi++)
    {
        Bundle *bundle = SeqAt(policy->bundles, bpi);

        for (size_t sti = 0; sti < SeqLength(bundle->promise_types); sti++)
        {
            PromiseType *promise_type = SeqAt(bundle->promise_types, sti);

            for (size_t ppi = 0; ppi < SeqLength(promise_type->promises); ppi++)
            {
                Promise *promise = SeqAt(promise_type->promises, ppi);

                for (size_t cpi = 0; cpi < SeqLength(promise->conlist); cpi++)
                {
                    Constraint *constraint = SeqAt(promise->conlist, cpi);

                    const ConstraintSyntax *syntax = ConstraintGetSyntax(constraint);
                    if (syntax->dtype == DATA_TYPE_BODY)
                    {
                        char *ns = QualifiedNameNamespaceComponent(RvalFullSymbol(&constraint->rval));
                        char *symbol = QualifiedNameScopeComponent(RvalFullSymbol(&constraint->rval));

                        Body *referenced_body = PolicyGetBody(policy, ns, constraint->lval, symbol);
                        if (!referenced_body)
                        {
                            SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, constraint,
                                                             POLICY_ERROR_BODY_UNDEFINED, symbol, constraint->lval));
                            success = false;
                        }

                        free(ns);
                        free(symbol);
                    }
                } // constraints
            } // promises
        } // promise_types
    } // bundles

    return success;
}

static bool PolicyCheckUndefinedBundles(const Policy *policy, Seq *errors)
{
    bool success = true;

    for (size_t bpi = 0; bpi < SeqLength(policy->bundles); bpi++)
    {
        Bundle *bundle = SeqAt(policy->bundles, bpi);

        for (size_t sti = 0; sti < SeqLength(bundle->promise_types); sti++)
        {
            PromiseType *promise_type = SeqAt(bundle->promise_types, sti);

            for (size_t ppi = 0; ppi < SeqLength(promise_type->promises); ppi++)
            {
                Promise *promise = SeqAt(promise_type->promises, ppi);

                for (size_t cpi = 0; cpi < SeqLength(promise->conlist); cpi++)
                {
                    Constraint *constraint = SeqAt(promise->conlist, cpi);

                    const ConstraintSyntax *syntax = ConstraintGetSyntax(constraint);
                    if (syntax->dtype == DATA_TYPE_BUNDLE &&
                        !IsCf3VarString(RvalFullSymbol(&constraint->rval)))
                    {
                        char *ns = QualifiedNameNamespaceComponent(RvalFullSymbol(&constraint->rval));
                        char *symbol = QualifiedNameScopeComponent(RvalFullSymbol(&constraint->rval));

                        const Bundle *referenced_bundle = NULL;
                        if (strcmp(constraint->lval, "usebundle") == 0)
                        {
                            referenced_bundle = PolicyGetBundle(policy, ns, "agent", symbol);
                            if (!referenced_bundle)
                            {
                                referenced_bundle = PolicyGetBundle(policy, ns, "common", symbol);
                            }
                        }
                        else
                        {
                            referenced_bundle = PolicyGetBundle(policy, ns, constraint->lval, symbol);
                        }

                        if (!referenced_bundle)
                        {
                            SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, constraint,
                                                             POLICY_ERROR_BUNDLE_UNDEFINED, symbol, constraint->lval));
                            success = false;
                        }

                        free(ns);
                        free(symbol);
                    }
                } // constraints
            } // promises
        } // promise_types
    } // bundles

    return success;
}

static bool PolicyCheckRequiredComments(const EvalContext *ctx, const Policy *policy, Seq *errors)
{
    const Body *common_control = PolicyGetBody(policy, NULL, "common", "control");
    if (common_control)
    {
        bool require_comments = ConstraintsGetAsBoolean(ctx, "require_comments", common_control->conlist);
        if (!require_comments)
        {
            return true;
        }

        bool success = true;

        for (size_t bpi = 0; bpi < SeqLength(policy->bundles); bpi++)
        {
            Bundle *bundle = SeqAt(policy->bundles, bpi);

            for (size_t sti = 0; sti < SeqLength(bundle->promise_types); sti++)
            {
                PromiseType *promise_type = SeqAt(bundle->promise_types, sti);

                for (size_t ppi = 0; ppi < SeqLength(promise_type->promises); ppi++)
                {
                    Promise *promise = SeqAt(promise_type->promises, ppi);

                    bool promise_has_comment = false;
                    for (size_t cpi = 0; cpi < SeqLength(promise->conlist); cpi++)
                    {
                        Constraint *constraint = SeqAt(promise->conlist, cpi);

                        if (strcmp(constraint->lval, "comment") == 0)
                        {
                            promise_has_comment = true;
                            break;
                        }
                    } // constraints

                    if (!promise_has_comment)
                    {
                        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, promise,
                                                         POLICY_ERROR_PROMISE_UNCOMMENTED));
                        success = false;
                    }
                } // promises
            } // promise_types
        } // bundles

        return success;
    }
    else
    {
        return true;
    }
}

bool PolicyCheckDuplicateHandles(const Policy *policy, Seq *errors)
{
    bool success = true;

    Set *used_handles = SetNew((unsigned int (*)(const void*, unsigned int))OatHash, (bool (*)(const void *, const void *))StringSafeEqual, NULL);

    for (size_t bpi = 0; bpi < SeqLength(policy->bundles); bpi++)
    {
        Bundle *bundle = SeqAt(policy->bundles, bpi);

        for (size_t sti = 0; sti < SeqLength(bundle->promise_types); sti++)
        {
            PromiseType *promise_type = SeqAt(bundle->promise_types, sti);

            for (size_t ppi = 0; ppi < SeqLength(promise_type->promises); ppi++)
            {
                Promise *promise = SeqAt(promise_type->promises, ppi);

                const char *handle = PromiseGetHandle(promise);

                if (handle)
                {
                    if (SetContains(used_handles, handle))
                    {
                        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_PROMISE, promise,
                                                         POLICY_ERROR_PROMISE_DUPLICATE_HANDLE, handle));
                        success = false;
                    }
                    else
                    {
                        SetAdd(used_handles, (void *)handle); /* This Set does not free values */
                    }
                }
            }
        }
    }

    SetDestroy(used_handles);

    return success;
}

bool PolicyCheckRunnable(const EvalContext *ctx, const Policy *policy, Seq *errors, bool ignore_missing_bundles)
{
    // check has body common control
    {
        const Body *common_control = PolicyGetBody(policy, NULL, "common", "control");
        if (!common_control)
        {
            SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_POLICY, policy,
                                             POLICY_ERROR_POLICY_NOT_RUNNABLE));
            return false;
        }
    }

    bool success = true;

    success &= PolicyCheckRequiredComments(ctx, policy, errors);
    success &= PolicyCheckUndefinedBodies(policy, errors);

    if (!ignore_missing_bundles)
    {
        success &= PolicyCheckUndefinedBundles(policy, errors);
    }

    success &= PolicyCheckDuplicateHandles(policy, errors);

    return success;
}

bool PolicyCheckPartial(const Policy *policy, Seq *errors)
{
    bool success = true;

    // ensure bundle names are not duplicated
    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);

        for (size_t j = 0; j < SeqLength(policy->bundles); j++)
        {
            Bundle *bp2 = SeqAt(policy->bundles, j);

            if (bp != bp2 &&
                StringSafeEqual(bp->name, bp2->name) &&
                StringSafeEqual(bp->type, bp2->type))
            {
                SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_BUNDLE, bp,
                                                      POLICY_ERROR_BUNDLE_REDEFINITION,
                                                      bp->name, bp->type));
                success = false;
            }
        }
    }

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);
        success &= PolicyCheckBundle(bp, errors);
    }


    // ensure body names are not duplicated
    for (size_t i = 0; i < SeqLength(policy->bodies); i++)
    {
        const Body *bp = SeqAt(policy->bodies, i);

        for (size_t j = 0; j < SeqLength(policy->bodies); j++)
        {
            const Body *bp2 = SeqAt(policy->bodies, j);

            if (bp != bp2 &&
                StringSafeEqual(bp->name, bp2->name) &&
                StringSafeEqual(bp->type, bp2->type))
            {
                if (strcmp(bp->type,"file") != 0)
                {
                    SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_BODY, bp,
                                                     POLICY_ERROR_BODY_REDEFINITION,
                                                     bp->name, bp->type));
                    success = false;
                }
            }
        }
    }

    for (size_t i = 0; i < SeqLength(policy->bodies); i++)
    {
        const Body *bp = SeqAt(policy->bodies, i);

        for (size_t j = 0; j < SeqLength(bp->conlist); j++)
        {
            Constraint *cp = SeqAt(bp->conlist, j);
            SyntaxTypeMatch err = ConstraintCheckType(cp);
            if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
            {
                SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, cp,
                                                 POLICY_ERROR_CONSTRAINT_TYPE_MISMATCH,
                                                 cp->lval));
                success = false;
            }
        }
    }

    success &= PolicyCheckDuplicateHandles(policy, errors);

    return success;
}

/*************************************************************************/

PolicyError *PolicyErrorNew(PolicyElementType type, const void *subject, const char *error_msg, ...)
{
    PolicyError *error = xmalloc(sizeof(PolicyError));

    error->type = type;
    error->subject = subject;

    va_list args;
    va_start(args, error_msg);
    xvasprintf(&error->message, error_msg, args);
    va_end(args);

    return error;
}

/*************************************************************************/

void PolicyErrorDestroy(PolicyError *error)
{
    free(error->message);
    free(error);
}

/*************************************************************************/

static SourceOffset PolicyElementSourceOffset(PolicyElementType type, const void *element)
{
    assert(element);

    switch (type)
    {
        case POLICY_ELEMENT_TYPE_POLICY:
        {
            return (SourceOffset) { 0 };
        }

        case POLICY_ELEMENT_TYPE_BUNDLE:
        {
            const Bundle *bundle = (const Bundle *)element;
            return bundle->offset;
        }

        case POLICY_ELEMENT_TYPE_BODY:
        {
            const Body *body = (const Body *)element;
            return body->offset;
        }

        case POLICY_ELEMENT_TYPE_PROMISE_TYPE:
        {
            const PromiseType *type = (const PromiseType *)element;
            return type->offset;
        }

        case POLICY_ELEMENT_TYPE_PROMISE:
        {
            const Promise *promise = (const Promise *)element;
            return promise->offset;
        }

        case POLICY_ELEMENT_TYPE_CONSTRAINT:
        {
            const Constraint *constraint = (const Constraint *)element;
            return constraint->offset;
        }

        default:
            assert(false && "Invalid policy element");
            return (SourceOffset) { 0 };
    }
}

/*************************************************************************/

static const char *PolicyElementSourceFile(PolicyElementType type, const void *element)
{
    assert(element);

    switch (type)
    {
        case POLICY_ELEMENT_TYPE_POLICY:
            return "";

        case POLICY_ELEMENT_TYPE_BUNDLE:
        {
            const Bundle *bundle = (const Bundle *)element;
            return bundle->source_path;
        }

        case POLICY_ELEMENT_TYPE_BODY:
        {
            const Body *body = (const Body *)element;
            return body->source_path;
        }

        case POLICY_ELEMENT_TYPE_PROMISE_TYPE:
        {
            const PromiseType *type = (const PromiseType *)element;
            return PolicyElementSourceFile(POLICY_ELEMENT_TYPE_BUNDLE, type->parent_bundle);
        }

        case POLICY_ELEMENT_TYPE_PROMISE:
        {
            const Promise *promise = (const Promise *)element;
            return PolicyElementSourceFile(POLICY_ELEMENT_TYPE_PROMISE_TYPE, promise->parent_promise_type);
        }

        case POLICY_ELEMENT_TYPE_CONSTRAINT:
        {
            const Constraint *constraint = (const Constraint *)element;
            switch (constraint->type)
            {
                case POLICY_ELEMENT_TYPE_BODY:
                    return PolicyElementSourceFile(POLICY_ELEMENT_TYPE_BODY, constraint->parent.body);

                case POLICY_ELEMENT_TYPE_PROMISE:
                    return PolicyElementSourceFile(POLICY_ELEMENT_TYPE_PROMISE, constraint->parent.promise);

                default:
                    assert(false && "Constraint has invalid parent element type");
                    return NULL;
            }
        }

        default:
            assert(false && "Invalid policy element");
            return NULL;
    }
}

/*************************************************************************/

void PolicyErrorWrite(Writer *writer, const PolicyError *error)
{
    SourceOffset offset = PolicyElementSourceOffset(error->type, error->subject);
    const char *path = PolicyElementSourceFile(error->type, error->subject);

    // FIX: need to track columns in SourceOffset
    WriterWriteF(writer, "%s:%d:%d: error: %s\n", path, (int)offset.line, 0, error->message);
}

/*************************************************************************/

void PromiseTypeDestroy(PromiseType *promise_type)
{
    if (promise_type)
    {
        SeqDestroy(promise_type->promises);

        free(promise_type->name);
        free(promise_type);
    }
}

Bundle *PolicyAppendBundle(Policy *policy, const char *ns, const char *name, const char *type, Rlist *args,
                     const char *source_path)
{
    CfDebug("Appending new bundle %s %s (", type, name);

    if (DEBUG)
    {
        RlistShow(stdout, args);
    }
    CfDebug(")\n");

    Bundle *bundle = xcalloc(1, sizeof(Bundle));

    bundle->parent_policy = policy;

    SeqAppend(policy->bundles, bundle);

    if (strcmp(ns, "default") == 0)
    {
        bundle->name = xstrdup(name);
    }
    else
    {
        char fqname[CF_BUFSIZE];
        snprintf(fqname,CF_BUFSIZE-1, "%s:%s", ns, name);
        bundle->name = xstrdup(fqname);
    }

    bundle->type = xstrdup(type);
    bundle->ns = xstrdup(ns);
    bundle->args = RlistCopy(args);
    bundle->source_path = SafeStringDuplicate(source_path);
    bundle->promise_types = SeqNew(10, PromiseTypeDestroy);

    return bundle;
}

/*******************************************************************/

Body *PolicyAppendBody(Policy *policy, const char *ns, const char *name, const char *type, Rlist *args, const char *source_path)
{
    CfDebug("Appending new promise body %s %s(", type, name);

    for (const Rlist *rp = args; rp; rp = rp->next)
    {
        CfDebug("%s,", (char *) rp->item);
    }
    CfDebug(")\n");

    Body *body = xcalloc(1, sizeof(Body));
    body->parent_policy = policy;

    SeqAppend(policy->bodies, body);

    if (strcmp(ns, "default") == 0)
    {
        body->name = xstrdup(name);
    }
    else
    {
        char fqname[CF_BUFSIZE];
        snprintf(fqname, CF_BUFSIZE-1, "%s:%s", ns, name);
        body->name = xstrdup(fqname);
    }

    body->type = xstrdup(type);
    body->ns = xstrdup(ns);
    body->args = RlistCopy(args);
    body->source_path = SafeStringDuplicate(source_path);
    body->conlist = SeqNew(10, ConstraintDestroy);

    return body;
}

PromiseType *BundleAppendPromiseType(Bundle *bundle, const char *name)
{
    CfDebug("Appending new type section %s\n", name);

    if (bundle == NULL)
    {
        ProgrammingError("Attempt to add a type without a bundle");
    }

    // TODO: review SeqLookup
    for (size_t i = 0; i < SeqLength(bundle->promise_types); i++)
    {
        PromiseType *existing = SeqAt(bundle->promise_types, i);
        if (strcmp(existing->name, name) == 0)
        {
            return existing;
        }
    }

    PromiseType *tp = xcalloc(1, sizeof(PromiseType));

    tp->parent_bundle = bundle;
    tp->name = xstrdup(name);
    tp->promises = SeqNew(10, PromiseDestroy);

    SeqAppend(bundle->promise_types, tp);

    return tp;
}

/*******************************************************************/

Promise *PromiseTypeAppendPromise(PromiseType *type, const char *promiser, Rval promisee, const char *classes)
{
    char *sp = NULL, *spe = NULL;

    if (!type)
    {
        ProgrammingError("Attempt to add a promise without a type\n");
    }

/* Check here for broken promises - or later with more info? */

    CfDebug("Appending Promise from bundle %s %s if context %s\n", type->parent_bundle->name, promiser, classes);

    Promise *pp = xcalloc(1, sizeof(Promise));

    sp = xstrdup(promiser);

    if (classes && strlen(classes) > 0)
    {
        spe = xstrdup(classes);
    }
    else
    {
        spe = xstrdup("any");
    }

    SeqAppend(type->promises, pp);

    pp->parent_promise_type = type;

    pp->promiser = sp;
    pp->promisee = promisee;
    pp->classes = spe;
    pp->has_subbundles = false;
    pp->conlist = SeqNew(10, ConstraintDestroy);
    pp->org_pp = pp;

    return pp;
}

static void BundleDestroy(Bundle *bundle)
{
    if (bundle)
    {
        free(bundle->name);
        free(bundle->type);

        RlistDestroy(bundle->args);
        SeqDestroy(bundle->promise_types);
        free(bundle);
    }
}

static void BodyDestroy(Body *body)
{
    if (body)
    {
        free(body->name);
        free(body->type);

        RlistDestroy(body->args);
        SeqDestroy(body->conlist);
        free(body);
    }
}


void PromiseDestroy(Promise *pp)
{
    if (pp)
    {
        free(pp->promiser);

        if (pp->promisee.item)
        {
            RvalDestroy(pp->promisee);
        }

        free(pp->classes);
        free(pp->comment);

        SeqDestroy(pp->conlist);

        free(pp);
    }
}

/*******************************************************************/

static Constraint *ConstraintNew(const char *lval, Rval rval, const char *classes, bool references_body)
{
    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        CfDebug("   Appending Constraint: %s => %s\n", lval, (const char *) rval.item);
        break;
    case RVAL_TYPE_FNCALL:
        CfDebug("   Appending a function call to rhs\n");
        break;
    case RVAL_TYPE_LIST:
        CfDebug("   Appending a list to rhs\n");
        break;
    default:
        break;
    }

    Constraint *cp = xcalloc(1, sizeof(Constraint));

    cp->lval = SafeStringDuplicate(lval);
    cp->rval = rval;

    cp->classes = SafeStringDuplicate(classes);
    cp->references_body = references_body;

    return cp;
}

Constraint *PromiseAppendConstraint(Promise *promise, const char *lval, Rval rval, const char *classes,
                                    bool references_body)
{
    Constraint *cp = ConstraintNew(lval, rval, classes, references_body);
    cp->type = POLICY_ELEMENT_TYPE_PROMISE;
    cp->parent.promise = promise;

    SeqAppend(promise->conlist, cp);

    return cp;
}

Constraint *BodyAppendConstraint(Body *body, const char *lval, Rval rval, const char *classes,
                                 bool references_body)
{
    Constraint *cp = ConstraintNew(lval, rval, classes, references_body);
    cp->type = POLICY_ELEMENT_TYPE_BODY;
    cp->parent.body = body;

    SeqAppend(body->conlist, cp);

    return cp;
}

/*******************************************************************/

PromiseType *BundleGetPromiseType(Bundle *bp, const char *name)
{
    // TODO: hiding error, remove and see what will crash
    if (bp == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < SeqLength(bp->promise_types); i++)
    {
        PromiseType *sp = SeqAt(bp->promise_types, i);

        if (strcmp(name, sp->name) == 0)
        {
            return sp;
        }
    }

    return NULL;
}

/****************************************************************************/

static char *EscapeQuotes(const char *s, char *out, int outSz)
{
    char *spt;
    const char *spf;
    int i = 0;

    memset(out, 0, outSz);

    for (spf = s, spt = out; (i < outSz - 2) && (*spf != '\0'); spf++, spt++, i++)
    {
        switch (*spf)
        {
        case '\'':
        case '\"':
            *spt++ = '\\';
            *spt = *spf;
            i += 3;
            break;

        default:
            *spt = *spf;
            i++;
            break;
        }
    }

    return out;
}

static JsonElement *AttributeValueToJson(Rval rval, bool symbolic_reference)
{
    JsonElement *json_attribute = JsonObjectCreate(10);

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        {
            char buffer[CF_BUFSIZE];

            EscapeQuotes((const char *) rval.item, buffer, sizeof(buffer));

            if (symbolic_reference)
            {
                JsonObjectAppendString(json_attribute, "type", "symbol");
            }
            else
            {
                JsonObjectAppendString(json_attribute, "type", "string");
            }
            JsonObjectAppendString(json_attribute, "value", buffer);

            return json_attribute;
        }


    case RVAL_TYPE_LIST:
        {
            Rlist *rp = NULL;
            JsonElement *list = JsonArrayCreate(10);

            JsonObjectAppendString(json_attribute, "type", "list");

            for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
            {
                JsonArrayAppendObject(list, AttributeValueToJson((Rval) {rp->item, rp->type}, false));
            }

            JsonObjectAppendArray(json_attribute, "value", list);
            return json_attribute;
        }

    case RVAL_TYPE_FNCALL:
        {
            Rlist *argp = NULL;
            FnCall *call = (FnCall *) rval.item;

            JsonObjectAppendString(json_attribute, "type", "functionCall");
            JsonObjectAppendString(json_attribute, "name", call->name);

            {
                JsonElement *arguments = JsonArrayCreate(10);

                for (argp = call->args; argp != NULL; argp = argp->next)
                {
                    JsonArrayAppendObject(arguments, AttributeValueToJson((Rval) {argp->item, argp->type}, false));
                }

                JsonObjectAppendArray(json_attribute, "arguments", arguments);
            }

            return json_attribute;
        }

    default:
        ProgrammingError("Attempted to export attribute of type: %c", rval.type);
        return NULL;
    }
}

static JsonElement *CreateContextAsJson(const char *name, size_t offset,
                                        size_t offset_end, const char *children_name, JsonElement *children)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendString(json, "name", name);
    JsonObjectAppendInteger(json, "offset", offset);
    JsonObjectAppendInteger(json, "offsetEnd", offset_end);
    JsonObjectAppendArray(json, children_name, children);

    return json;
}

static JsonElement *BodyContextsToJson(const Seq *constraints)
{
    JsonElement *json_contexts = JsonArrayCreate(10);
    JsonElement *json_attributes = JsonArrayCreate(10);
    char *current_context = "any";
    size_t context_offset_start = -1;
    size_t context_offset_end = -1;

    for (size_t i = 0; i < SeqLength(constraints); i++)
    {
        Constraint *cp = SeqAt(constraints, i);

        JsonElement *json_attribute = JsonObjectCreate(10);

        if (strcmp(current_context, cp->classes) != 0)
        {
            JsonArrayAppendObject(json_contexts,
                                  CreateContextAsJson(current_context,
                                                      context_offset_start,
                                                      context_offset_end, "attributes", json_attributes));
            json_attributes = JsonArrayCreate(10);
            current_context = cp->classes;
        }

        JsonObjectAppendInteger(json_attribute, "offset", cp->offset.start);
        JsonObjectAppendInteger(json_attribute, "offsetEnd", cp->offset.end);

        context_offset_start = cp->offset.context;
        context_offset_end = cp->offset.end;

        JsonObjectAppendString(json_attribute, "lval", cp->lval);
        JsonObjectAppendObject(json_attribute, "rval", AttributeValueToJson(cp->rval, false));
        JsonArrayAppendObject(json_attributes, json_attribute);
    }

    JsonArrayAppendObject(json_contexts,
                          CreateContextAsJson(current_context,
                                              context_offset_start,
                                              context_offset_end, "attributes", json_attributes));

    return json_contexts;
}

static JsonElement *BundleContextsToJson(const Seq *promises)
{
    JsonElement *json_contexts = JsonArrayCreate(10);
    JsonElement *json_promises = JsonArrayCreate(10);
    char *current_context = NULL;
    size_t context_offset_start = -1;
    size_t context_offset_end = -1;

    for (size_t ppi = 0; ppi < SeqLength(promises); ppi++)
    {
        Promise *pp = SeqAt(promises, ppi);

        if (!current_context)
        {
            current_context = pp->classes;
        }

        JsonElement *json_promise = JsonObjectCreate(10);

        if (strcmp(current_context, pp->classes) != 0)
        {
            JsonArrayAppendObject(json_contexts,
                                  CreateContextAsJson(current_context,
                                                      context_offset_start,
                                                      context_offset_end, "promises", json_promises));
            json_promises = JsonArrayCreate(10);
            current_context = pp->classes;
        }

        JsonObjectAppendInteger(json_promise, "offset", pp->offset.start);

        {
            JsonElement *json_promise_attributes = JsonArrayCreate(10);

            for (size_t k = 0; k < SeqLength(pp->conlist); k++)
            {
                Constraint *cp = SeqAt(pp->conlist, k);

                JsonElement *json_attribute = JsonObjectCreate(10);

                JsonObjectAppendInteger(json_attribute, "offset", cp->offset.start);
                JsonObjectAppendInteger(json_attribute, "offsetEnd", cp->offset.end);

                context_offset_end = cp->offset.end;

                JsonObjectAppendString(json_attribute, "lval", cp->lval);
                JsonObjectAppendObject(json_attribute, "rval", AttributeValueToJson(cp->rval, cp->references_body));
                JsonArrayAppendObject(json_promise_attributes, json_attribute);
            }

            JsonObjectAppendInteger(json_promise, "offsetEnd", context_offset_end);

            JsonObjectAppendString(json_promise, "promiser", pp->promiser);

            switch (pp->promisee.type)
            {
            case RVAL_TYPE_SCALAR:
                JsonObjectAppendString(json_promise, "promisee", pp->promisee.item);
                break;

            case RVAL_TYPE_LIST:
                {
                    JsonElement *promisee_list = JsonArrayCreate(10);
                    for (const Rlist *rp = pp->promisee.item; rp; rp = rp->next)
                    {
                        JsonArrayAppendString(promisee_list, RlistScalarValue(rp));
                    }
                    JsonObjectAppendArray(json_promise, "promisee", promisee_list);
                }
                break;

            default:
                break;
            }

            JsonObjectAppendArray(json_promise, "attributes", json_promise_attributes);
        }
        JsonArrayAppendObject(json_promises, json_promise);
    }

    JsonArrayAppendObject(json_contexts,
                          CreateContextAsJson(current_context,
                                              context_offset_start,
                                              context_offset_end, "promises", json_promises));

    return json_contexts;
}

static JsonElement *BundleToJson(const Bundle *bundle)
{
    JsonElement *json_bundle = JsonObjectCreate(10);

    if (bundle->source_path)
    {
        JsonObjectAppendString(json_bundle, "sourcePath", bundle->source_path);
    }
    JsonObjectAppendInteger(json_bundle, "offset", bundle->offset.start);
    JsonObjectAppendInteger(json_bundle, "offsetEnd", bundle->offset.end);

    JsonObjectAppendString(json_bundle, "namespace", bundle->ns);
    JsonObjectAppendString(json_bundle, "name", bundle->name);
    JsonObjectAppendString(json_bundle, "bundleType", bundle->type);

    {
        JsonElement *json_args = JsonArrayCreate(10);
        Rlist *argp = NULL;

        for (argp = bundle->args; argp != NULL; argp = argp->next)
        {
            JsonArrayAppendString(json_args, argp->item);
        }

        JsonObjectAppendArray(json_bundle, "arguments", json_args);
    }

    {
        JsonElement *json_promise_types = JsonArrayCreate(10);

        for (size_t i = 0; i < SeqLength(bundle->promise_types); i++)
        {
            const PromiseType *sp = SeqAt(bundle->promise_types, i);

            JsonElement *json_promise_type = JsonObjectCreate(10);

            JsonObjectAppendInteger(json_promise_type, "offset", sp->offset.start);
            JsonObjectAppendInteger(json_promise_type, "offsetEnd", sp->offset.end);
            JsonObjectAppendString(json_promise_type, "name", sp->name);
            JsonObjectAppendArray(json_promise_type, "contexts", BundleContextsToJson(sp->promises));

            JsonArrayAppendObject(json_promise_types, json_promise_type);
        }

        JsonObjectAppendArray(json_bundle, "promiseTypes", json_promise_types);
    }

    return json_bundle;
}


static JsonElement *BodyToJson(const Body *body)
{
    JsonElement *json_body = JsonObjectCreate(10);

    JsonObjectAppendInteger(json_body, "offset", body->offset.start);
    JsonObjectAppendInteger(json_body, "offsetEnd", body->offset.end);

    JsonObjectAppendString(json_body, "namespace", body->ns);
    JsonObjectAppendString(json_body, "name", body->name);
    JsonObjectAppendString(json_body, "bodyType", body->type);

    {
        JsonElement *json_args = JsonArrayCreate(10);
        Rlist *argp = NULL;

        for (argp = body->args; argp != NULL; argp = argp->next)
        {
            JsonArrayAppendString(json_args, argp->item);
        }

        JsonObjectAppendArray(json_body, "arguments", json_args);
    }

    JsonObjectAppendArray(json_body, "contexts", BodyContextsToJson(body->conlist));

    return json_body;
}

JsonElement *PolicyToJson(const Policy *policy)
{
    JsonElement *json_policy = JsonObjectCreate(10);

    {
        JsonElement *json_bundles = JsonArrayCreate(10);

        for (size_t i = 0; i < SeqLength(policy->bundles); i++)
        {
            const Bundle *bp = SeqAt(policy->bundles, i);
            JsonArrayAppendObject(json_bundles, BundleToJson(bp));
        }

        JsonObjectAppendArray(json_policy, "bundles", json_bundles);
    }

    {
        JsonElement *json_bodies = JsonArrayCreate(10);

        for (size_t i = 0; i < SeqLength(policy->bodies); i++)
        {
            const Body *bdp = SeqAt(policy->bodies, i);

            JsonArrayAppendObject(json_bodies, BodyToJson(bdp));
        }

        JsonObjectAppendArray(json_policy, "bodies", json_bodies);
    }

    return json_policy;
}

/****************************************************************************/


static void IndentPrint(Writer *writer, int indent_level)
{
    static const int PRETTY_PRINT_SPACES_PER_INDENT = 2;

    int i = 0;

    for (i = 0; i < PRETTY_PRINT_SPACES_PER_INDENT * indent_level; i++)
    {
        WriterWriteChar(writer, ' ');
    }
}


static void RvalToString(Writer *writer, Rval rval, bool symbolic_reference)
{
    if (rval.type == RVAL_TYPE_SCALAR && !symbolic_reference)
    {
        WriterWriteChar(writer, '"');
        RvalWrite(writer, rval);
        WriterWriteChar(writer, '"');
    }
    else
    {
        RvalWrite(writer, rval);
    }
}


static void AttributeToString(Writer *writer, Constraint *attribute, bool symbolic_reference)
{
    WriterWriteF(writer, "%s => ", attribute->lval);
    RvalToString(writer, attribute->rval, symbolic_reference);
}


static void ArgumentsToString(Writer *writer, Rlist *args)
{
    Rlist *argp = NULL;

    WriterWriteChar(writer, '(');
    for (argp = args; argp != NULL; argp = argp->next)
    {
        WriterWriteF(writer, "%s", (char *) argp->item);

        if (argp->next != NULL)
        {
            WriterWrite(writer, ", ");
        }
    }
    WriterWriteChar(writer, ')');
}


void BodyToString(Writer *writer, Body *body)
{
    char *current_class = NULL;

    WriterWriteF(writer, "body %s %s", body->type, body->name);
    ArgumentsToString(writer, body->args);
    WriterWrite(writer, "\n{");

    for (size_t i = 0; i < SeqLength(body->conlist); i++)
    {
        Constraint *cp = SeqAt(body->conlist, i);

        if (current_class == NULL || strcmp(cp->classes, current_class) != 0)
        {
            current_class = cp->classes;

            if (strcmp(current_class, "any") == 0)
            {
                WriterWrite(writer, "\n");
            }
            else
            {
                WriterWriteF(writer, "\n\n%s::", current_class);
            }
        }

        WriterWriteChar(writer, '\n');
        IndentPrint(writer, 1);
        AttributeToString(writer, cp, false);
    }

    WriterWrite(writer, "\n}\n");
}


void BundleToString(Writer *writer, Bundle *bundle)
{
    WriterWriteF(writer, "bundle %s %s", bundle->type, bundle->name);
    ArgumentsToString(writer, bundle->args);
    WriterWrite(writer, "\n{");

    for (size_t i = 0; i < SeqLength(bundle->promise_types); i++)
    {
        PromiseType *promise_type = SeqAt(bundle->promise_types, i);

        WriterWriteF(writer, "\n%s:\n", promise_type->name);

        for (size_t ppi = 0; ppi < SeqLength(promise_type->promises); ppi++)
        {
            Promise *pp = SeqAt(promise_type->promises, ppi);
            char *current_class = NULL;

            if (current_class == NULL || strcmp(pp->classes, current_class) != 0)
            {
                current_class = pp->classes;

                if (strcmp(current_class, "any") != 0)
                {
                    IndentPrint(writer, 1);
                    WriterWriteF(writer, "%s::", current_class);
                }
            }

            IndentPrint(writer, 2);
            WriterWriteF(writer, "\"%s\"", pp->promiser);

            /* FIX: add support
             *
             if (pp->promisee != NULL)
             {
             fprintf(out, " -> %s", pp->promisee);
             }
             */

            for (size_t k = 0; k < SeqLength(pp->conlist); k++)
            {
                Constraint *cp = SeqAt(pp->conlist, k);

                WriterWriteChar(writer, '\n');
                IndentPrint(writer, 4);
                AttributeToString(writer, cp, cp->references_body);
            }
        }

        if (i == (SeqLength(bundle->promise_types) - 1))
        {
            WriterWriteChar(writer, '\n');
        }
    }

    WriterWrite(writer, "\n}\n");
}

void PolicyToString(const Policy *policy, Writer *writer)
{
    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bundle = SeqAt(policy->bundles, i);
        BundleToString(writer, bundle);
        WriterWriteChar(writer, '\n');
    }

    for (size_t i = 0; i < SeqLength(policy->bodies); i++)
    {
        Body *body = SeqAt(policy->bodies, i);
        BodyToString(writer, body);
        WriterWriteChar(writer, '\n');
    }

}

//*****************************************************************************

static Rval RvalFromJson(JsonElement *json_rval)
{
    const char *type = JsonObjectGetAsString(json_rval, "type");

    if (strcmp("string", type) == 0 || strcmp("symbol", type) == 0)
    {
        const char *value = JsonObjectGetAsString(json_rval, "value");
        return ((Rval) { xstrdup(value), RVAL_TYPE_SCALAR });
    }
    else if (strcmp("list", type) == 0)
    {
        JsonElement *json_list = JsonObjectGetAsArray(json_rval, "value");
        Rlist *rlist = NULL;

        for (size_t i = 0; i < JsonElementLength(json_list); i++)
        {
            Rval list_value = RvalFromJson(JsonArrayGetAsObject(json_list, i));
            RlistAppend(&rlist, list_value.item, list_value.type);
        }

        return ((Rval) { rlist, RVAL_TYPE_LIST });
    }
    else if (strcmp("functionCall", type) == 0)
    {
        const char *name = JsonObjectGetAsString(json_rval, "name");
        JsonElement *json_args = JsonObjectGetAsArray(json_rval, "arguments");
        Rlist *args = NULL;

        for (size_t i = 0; i < JsonElementLength(json_args); i++)
        {
            JsonElement *json_arg = JsonArrayGetAsObject(json_args, i);
            Rval arg = RvalFromJson(json_arg);

            RlistAppend(&args, arg.item, arg.type);
        }

        FnCall *fn = FnCallNew(name, args);

        return ((Rval) { fn, RVAL_TYPE_FNCALL });
    }
    else
    {
        ProgrammingError("Unexpected rval type: %s", type);
    }
}

static Constraint *PromiseAppendConstraintJson(Promise *promise, JsonElement *json_constraint, const char *context)
{
    const char *lval = JsonObjectGetAsString(json_constraint, "lval");

    JsonElement *json_rval = JsonObjectGetAsObject(json_constraint, "rval");
    const char *type = JsonObjectGetAsString(json_rval, "type");

    Rval rval = RvalFromJson(json_rval);

    Constraint *cp = PromiseAppendConstraint(promise, lval, rval, context, (strcmp("symbol", type) == 0));

    return cp;
}

static Promise *PromiseTypeAppendPromiseJson(PromiseType *promise_type, JsonElement *json_promise, const char *context)
{
    const char *promiser = JsonObjectGetAsString(json_promise, "promiser");

    Promise *promise = PromiseTypeAppendPromise(promise_type, promiser, (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, context);

    JsonElement *json_attributes = JsonObjectGetAsArray(json_promise, "attributes");
    for (size_t i = 0; i < JsonElementLength(json_attributes); i++)
    {
        JsonElement *json_attribute = JsonArrayGetAsObject(json_attributes, i);
        PromiseAppendConstraintJson(promise, json_attribute, context);
    }

    return promise;
}

static PromiseType *BundleAppendPromiseTypeJson(Bundle *bundle, JsonElement *json_promise_type)
{
    const char *name = JsonObjectGetAsString(json_promise_type, "name");

    PromiseType *promise_type = BundleAppendPromiseType(bundle, name);

    JsonElement *json_contexts = JsonObjectGetAsArray(json_promise_type, "contexts");
    for (size_t i = 0; i < JsonElementLength(json_contexts); i++)
    {
        JsonElement *json_context = JsonArrayGetAsObject(json_contexts, i);

        const char *context = JsonObjectGetAsString(json_context, "name");

        JsonElement *json_context_promises = JsonObjectGetAsArray(json_context, "promises");
        for (size_t j = 0; j < JsonElementLength(json_context_promises); j++)
        {
            JsonElement *json_promise = JsonArrayGetAsObject(json_context_promises, j);
            PromiseTypeAppendPromiseJson(promise_type, json_promise, context);
        }
    }

    return promise_type;
}

static Bundle *PolicyAppendBundleJson(Policy *policy, JsonElement *json_bundle)
{
    const char *ns = JsonObjectGetAsString(json_bundle, "namespace");
    const char *name = JsonObjectGetAsString(json_bundle, "name");
    const char *type = JsonObjectGetAsString(json_bundle, "bundleType");
    const char *source_path = JsonObjectGetAsString(json_bundle, "sourcePath");

    Rlist *args = NULL;
    {
        JsonElement *json_args = JsonObjectGetAsArray(json_bundle, "arguments");
        for (size_t i = 0; i < JsonElementLength(json_args); i++)
        {
            RlistAppendScalar(&args, JsonArrayGetAsString(json_args, i));
        }
    }

    Bundle *bundle = PolicyAppendBundle(policy, ns, name, type, args, source_path);

    {
        JsonElement *json_promise_types = JsonObjectGetAsArray(json_bundle, "promiseTypes");
        for (size_t i = 0; i < JsonElementLength(json_promise_types); i++)
        {
            JsonElement *json_promise_type = JsonArrayGetAsObject(json_promise_types, i);
            BundleAppendPromiseTypeJson(bundle, json_promise_type);
        }
    }

    return bundle;
}

static Constraint *BodyAppendConstraintJson(Body *body, JsonElement *json_constraint, const char *context)
{
    const char *lval = JsonObjectGetAsString(json_constraint, "lval");

    JsonElement *json_rval = JsonObjectGetAsObject(json_constraint, "rval");
    const char *type = JsonObjectGetAsString(json_rval, "type");

    Rval rval = RvalFromJson(json_rval);

    Constraint *cp = BodyAppendConstraint(body, lval, rval, context, (strcmp("symbol", type) == 0));

    return cp;
}

static Body *PolicyAppendBodyJson(Policy *policy, JsonElement *json_body)
{
    const char *ns = JsonObjectGetAsString(json_body, "namespace");
    const char *name = JsonObjectGetAsString(json_body, "name");
    const char *type = JsonObjectGetAsString(json_body, "bodyType");
    const char *source_path = JsonObjectGetAsString(json_body, "sourcePath");

    Rlist *args = NULL;
    {
        JsonElement *json_args = JsonObjectGetAsArray(json_body, "arguments");
        for (size_t i = 0; i < JsonElementLength(json_args); i++)
        {
            RlistAppendScalar(&args, JsonArrayGetAsString(json_args, i));
        }
    }

    Body *body = PolicyAppendBody(policy, ns, name, type, args, source_path);

    {
        JsonElement *json_contexts = JsonObjectGetAsArray(json_body, "contexts");
        for (size_t i = 0; i < JsonElementLength(json_contexts); i++)
        {
            JsonElement *json_context = JsonArrayGetAsObject(json_contexts, i);
            const char *context = JsonObjectGetAsString(json_context, "name");

            {
                JsonElement *json_attributes = JsonObjectGetAsArray(json_context, "attributes");
                for (size_t j = 0; j < JsonElementLength(json_attributes); j++)
                {
                    JsonElement *json_attribute = JsonArrayGetAsObject(json_attributes, j);
                    BodyAppendConstraintJson(body, json_attribute, context);
                }
            }
        }
    }

    return body;
}


Policy *PolicyFromJson(JsonElement *json_policy)
{
    Policy *policy = PolicyNew();

    {
        JsonElement *json_bundles = JsonObjectGetAsArray(json_policy, "bundles");
        for (size_t i = 0; i < JsonElementLength(json_bundles); i++)
        {
            JsonElement *json_bundle = JsonArrayGetAsObject(json_bundles, i);
            PolicyAppendBundleJson(policy, json_bundle);
        }
    }

    {
        JsonElement *json_bodies = JsonObjectGetAsArray(json_policy, "bodies");
        for (size_t i = 0; i < JsonElementLength(json_bodies); i++)
        {
            JsonElement *json_body = JsonArrayGetAsObject(json_bodies, i);
            PolicyAppendBodyJson(policy, json_body);
        }
    }

    return policy;
}


Seq *BodyGetConstraint(Body *body, const char *lval)
{
    Seq *matches = SeqNew(5, NULL);

    for (size_t i = 0; i < SeqLength(body->conlist); i++)
    {
        Constraint *cp = SeqAt(body->conlist, i);
        if (strcmp(cp->lval, lval) == 0)
        {
            SeqAppend(matches, cp);
        }
    }

    return matches;
}

const char *ConstraintContext(const Constraint *cp)
{
    switch (cp->type)
    {
    case POLICY_ELEMENT_TYPE_BODY:
        return cp->classes;

    case POLICY_ELEMENT_TYPE_BUNDLE:
        return cp->parent.promise->classes;

    default:
        ProgrammingError("Constraint had parent element type: %d", cp->type);
        return NULL;
    }
}

Constraint *EffectiveConstraint(const EvalContext *ctx, Seq *constraints)
{
    for (size_t i = 0; i < SeqLength(constraints); i++)
    {
        Constraint *cp = SeqAt(constraints, i);

        const char *context = ConstraintContext(cp);
        const char *ns = ConstraintGetNamespace(cp);

        if (IsDefinedClass(ctx, context, ns))
        {
            return cp;
        }
    }

    return NULL;
}

/*****************************************************************************/

void ConstraintSetScalarValue(Seq *conlist, const char *lval, const char *rval)
{
    for (size_t i = 0; i < SeqLength(conlist); i++)
    {
        Constraint *cp = SeqAt(conlist, i);

        if (strcmp(lval, cp->lval) == 0)
        {
            RvalDestroy(cp->rval);
            cp->rval = (Rval) { xstrdup(rval), RVAL_TYPE_SCALAR };
            return;
        }
    }
}

void ConstraintDestroy(Constraint *cp)
{
    if (cp)
    {
        RvalDestroy(cp->rval);
        free(cp->lval);
        free(cp->classes);

        free(cp);
    }
}

/*****************************************************************************/

int PromiseGetConstraintAsBoolean(const EvalContext *ctx, const char *lval, const Promise *pp)
{
    int retval = CF_UNDEFINED;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
            {
                if (retval != CF_UNDEFINED)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Multiple \"%s\" (boolean) constraints break this promise\n", lval);
                    PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Type mismatch on rhs - expected type (%c) for boolean constraint \"%s\"\n",
                      cp->rval.type, lval);
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                FatalError(ctx, "Aborted");
            }

            if (strcmp(cp->rval.item, "true") == 0 || strcmp(cp->rval.item, "yes") == 0)
            {
                retval = true;
                continue;
            }

            if (strcmp(cp->rval.item, "false") == 0 || strcmp(cp->rval.item, "no") == 0)
            {
                retval = false;
            }
        }
    }

    if (retval == CF_UNDEFINED)
    {
        retval = false;
    }

    return retval;
}

/*****************************************************************************/

int ConstraintsGetAsBoolean(const EvalContext *ctx, const char *lval, const Seq *constraints)
{
    int retval = CF_UNDEFINED;

    for (size_t i = 0; i < SeqLength(constraints); i++)
    {
        Constraint *cp = SeqAt(constraints, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(ctx, cp->classes, NULL))
            {
                if (retval != CF_UNDEFINED)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Multiple \"%s\" (boolean) body constraints break this promise\n", lval);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Type mismatch - expected type (%c) for boolean constraint \"%s\"\n",
                      cp->rval.type, lval);
                FatalError(ctx, "Aborted");
            }

            if (strcmp(cp->rval.item, "true") == 0 || strcmp(cp->rval.item, "yes") == 0)
            {
                retval = true;
                continue;
            }

            if (strcmp(cp->rval.item, "false") == 0 || strcmp(cp->rval.item, "no") == 0)
            {
                retval = false;
            }
        }
    }

    if (retval == CF_UNDEFINED)
    {
        retval = false;
    }

    return retval;
}

/*****************************************************************************/

bool PromiseBundleConstraintExists(const EvalContext *ctx, const char *lval, const Promise *pp)
{
    int retval = CF_UNDEFINED;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        const Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
            {
                if (retval != CF_UNDEFINED)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Multiple \"%s\" constraints break this promise\n", lval);
                    PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                }
            }
            else
            {
                continue;
            }

            if (!(cp->rval.type == RVAL_TYPE_FNCALL || cp->rval.type == RVAL_TYPE_SCALAR))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "",
                      "Anomalous type mismatch - type (%c) for bundle constraint %s did not match internals\n",
                      cp->rval.type, lval);
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                FatalError(ctx, "Aborted");
            }

            return true;
        }
    }

    return false;
}

static bool CheckScalarNotEmptyVarRef(const char *scalar)
{
    return (strcmp("$()", scalar) != 0) && (strcmp("${}", scalar) != 0);
}

static bool PromiseCheck(const Promise *pp, Seq *errors)
{
    bool success = true;

    if (!CheckScalarNotEmptyVarRef(pp->promiser))
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_PROMISE, pp,
                                         POLICY_ERROR_EMPTY_VARREF));
        success = false;
    }

    // check if promise's constraints are valid
    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *constraint = SeqAt(pp->conlist, i);
        success &= ConstraintCheckSyntax(constraint, errors);
    }

    const PromiseTypeSyntax *pts = PromiseTypeSyntaxLookup(pp->parent_promise_type->parent_bundle->type,
                                                           pp->parent_promise_type->name);

    if (pts->constraint_set.parse_tree_check)
    {
        success &= pts->constraint_set.parse_tree_check(pp, errors);
    }

    return success;
}

const char *PromiseGetNamespace(const Promise *pp)
{
    return pp->parent_promise_type->parent_bundle->ns;
}

const Bundle *PromiseGetBundle(const Promise *pp)
{
    return pp->parent_promise_type->parent_bundle;
}

const char *PromiseGetHandle(const Promise *pp)
{
    return (const char *)PromiseGetImmediateRvalValue("handle", pp, RVAL_TYPE_SCALAR);
}

int PromiseGetConstraintAsInt(const EvalContext *ctx, const char *lval, const Promise *pp)
{
    int retval = CF_NOINT;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
            {
                if (retval != CF_NOINT)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Multiple \"%s\" (int) constraints break this promise\n", lval);
                    PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "",
                      "Anomalous type mismatch - expected type for int constraint %s did not match internals\n", lval);
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                FatalError(ctx, "Aborted");
            }

            retval = (int) IntFromString((char *) cp->rval.item);
        }
    }

    return retval;
}

/*****************************************************************************/

bool PromiseGetConstraintAsReal(const EvalContext *ctx, const char *lval, const Promise *pp, double *value_out)
{
    bool found_constraint = false;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
            {
                if (found_constraint)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Multiple \"%s\" (real) constraints break this promise\n", lval);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "",
                      "Anomalous type mismatch - expected type for int constraint %s did not match internals\n", lval);
                FatalError(ctx, "Aborted");
            }

            *value_out = DoubleFromString((char *) cp->rval.item, value_out);
            found_constraint = true;
        }
    }

    return found_constraint;
}

/*****************************************************************************/

/**
 * @return true if successful
 */
static bool Str2Mode(const char *s, mode_t *mode_out)
{
    int a = CF_UNDEFINED;

    if (s == NULL)
    {
        *mode_out = (mode_t)0;
        return true;
    }

    sscanf(s, "%o", &a);

    if (a == CF_UNDEFINED)
    {
        return false;
    }

    *mode_out = (mode_t)a;
    return true;
}

mode_t PromiseGetConstraintAsOctal(const EvalContext *ctx, const char *lval, const Promise *pp)
{
    mode_t retval = 077;

// We could handle units here, like kb,b,mb

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
            {
                if (retval != 077)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Multiple \"%s\" (int,octal) constraints break this promise\n", lval);
                    PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "",
                      "Anomalous type mismatch - expected type for int constraint %s did not match internals\n", lval);
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                FatalError(ctx, "Aborted");
            }

            if (!Str2Mode(cp->rval.item, &retval))
            {
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                FatalError(ctx, "Error reading assumed octal value %s\n", (const char *)cp->rval.item);
            }
        }
    }

    return retval;
}

/*****************************************************************************/

#ifdef __MINGW32__

uid_t PromiseGetConstraintAsUid(const EvalContext *ctx, const char *lval, const Promise *pp)
{                               // we use sids on windows instead
    return CF_SAME_OWNER;
}

#else /* !__MINGW32__ */

uid_t PromiseGetConstraintAsUid(const EvalContext *ctx, const char *lval, const Promise *pp)
{
    int retval = CF_SAME_OWNER;
    char buffer[CF_MAXVARSIZE];

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
            {
                if (retval != CF_UNDEFINED)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Multiple \"%s\" (owner/uid) constraints break this promise\n", lval);
                    PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "",
                      "Anomalous type mismatch - expected type for owner constraint %s did not match internals\n",
                      lval);
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                FatalError(ctx, "Aborted");
            }

            retval = Str2Uid((char *) cp->rval.item, buffer, pp);
        }
    }

    return retval;
}

#endif /* !__MINGW32__ */

/*****************************************************************************/

#ifdef __MINGW32__

gid_t PromiseGetConstraintAsGid(const EvalContext *ctx, char *lval, const Promise *pp)
{                               // not applicable on windows: processes have no group
    return CF_SAME_GROUP;
}

#else /* !__MINGW32__ */

gid_t PromiseGetConstraintAsGid(const EvalContext *ctx, char *lval, const Promise *pp)
{
    int retval = CF_SAME_GROUP;
    char buffer[CF_MAXVARSIZE];

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
            {
                if (retval != CF_UNDEFINED)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Multiple \"%s\"  (group/gid) constraints break this promise\n", lval);
                    PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "",
                      "Anomalous type mismatch - expected type for group constraint %s did not match internals\n",
                      lval);
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                FatalError(ctx, "Aborted");
            }

            retval = Str2Gid((char *) cp->rval.item, buffer, pp);
        }
    }

    return retval;
}
#endif /* !__MINGW32__ */

/*****************************************************************************/

Rlist *PromiseGetConstraintAsList(const EvalContext *ctx, const char *lval, const Promise *pp)
{
    Rlist *retval = NULL;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
            {
                if (retval != NULL)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Multiple \"%s\" int constraints break this promise\n", lval);
                    PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_LIST)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Type mismatch on rhs - expected type for list constraint \"%s\" \n", lval);
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                FatalError(ctx, "Aborted");
            }

            retval = (Rlist *) cp->rval.item;
            break;
        }
    }

    return retval;
}

/*****************************************************************************/

static int VerifyConstraintName(const char *lval)
{
    PromiseTypeSyntax ss;
    int i, j, l, m;
    const ConstraintSyntax *bs, *bs2;
    const PromiseTypeSyntax *ssp;

    CfDebug("  Verify Constrant name %s\n", lval);

    for (i = 0; i < CF3_MODULES; i++)
    {
        if ((ssp = CF_ALL_PROMISE_TYPES[i]) == NULL)
        {
            continue;
        }

        for (j = 0; ssp[j].bundle_type != NULL; j++)
        {
            ss = ssp[j];

            if (ss.promise_type != NULL)
            {
                bs = ss.constraint_set.constraints;

                for (l = 0; bs[l].lval != NULL; l++)
                {
                    if (bs[l].dtype == DATA_TYPE_BUNDLE)
                    {
                    }
                    else if (bs[l].dtype == DATA_TYPE_BODY)
                    {
                        bs2 = bs[l].range.body_type_syntax;

                        for (m = 0; bs2[m].lval != NULL; m++)
                        {
                            if (strcmp(lval, bs2[m].lval) == 0)
                            {
                                return true;
                            }
                        }
                    }

                    if (strcmp(lval, bs[l].lval) == 0)
                    {
                        return true;
                    }
                }
            }
        }
    }

/* Now check the functional modules - extra level of indirection */

    for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
    {
        if (strcmp(lval, CF_COMMON_BODIES[i].lval) == 0)
        {
            return true;
        }
    }

    return false;
}

Constraint *PromiseGetConstraint(const EvalContext *ctx, const Promise *pp, const char *lval)
{
    Constraint *retval = NULL;

    if (pp == NULL)
    {
        return NULL;
    }

    if (!VerifyConstraintName(lval))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Self-diagnostic: Constraint type \"%s\" is not a registered type\n", lval);
    }

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
            {
                if (retval != NULL)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Inconsistent \"%s\" constraints break this promise\n", lval);
                    PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                }

                retval = cp;
                break;
            }
        }
    }

    return retval;
}

Constraint *PromiseGetImmediateConstraint(const Promise *pp, const char *lval)
{
    if (pp == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < SeqLength(pp->conlist); ++i)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            /* It would be nice to check whether the constraint we have asked
               for is defined in promise (not in referenced body), but there
               seem to be no way to do it easily.

               Checking for absence of classes does not work, as constrains
               obtain classes defined on promise itself.
            */

            return cp;
        }
    }

    return NULL;
}

void *PromiseGetImmediateRvalValue(const char *lval, const Promise *pp, RvalType rtype)
{
    const Constraint *constraint = PromiseGetImmediateConstraint(pp, lval);

    if (constraint && constraint->rval.type == rtype)
    {
        return constraint->rval.item;
    }
    else
    {
        return NULL;
    }
}

/*****************************************************************************/

void *ConstraintGetRvalValue(const EvalContext *ctx, const char *lval, const Promise *pp, RvalType rtype)
{
    const Constraint *constraint = PromiseGetConstraint(ctx, pp, lval);

    if (constraint && constraint->rval.type == rtype)
    {
        return constraint->rval.item;
    }
    else
    {
        return NULL;
    }
}

/*****************************************************************************/

void PromiseRecheckAllConstraints(EvalContext *ctx, Promise *pp)
{
    static Item *EDIT_ANCHORS = NULL;
    Item *ptr;

/* Special promise type checks */

    if (!IsDefinedClass(ctx, pp->classes, PromiseGetNamespace(pp)))
    {
        return;
    }

    char *sp = NULL;
    if (VarClassExcluded(ctx, pp, &sp))
    {
        return;
    }

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);
        SyntaxTypeMatch err = ConstraintCheckType(cp);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "%s: %s", cp->lval, SyntaxTypeMatchToString(err));
        }
    }

    if (strcmp(pp->parent_promise_type->name, "insert_lines") == 0)
    {
        /* Multiple additions with same criterion will not be convergent -- but ignore for empty file baseline */

        if ((sp = ConstraintGetRvalValue(ctx, "select_line_matching", pp, RVAL_TYPE_SCALAR)))
        {
            if ((ptr = ReturnItemIn(EDIT_ANCHORS, sp)))
            {
                if (strcmp(ptr->classes, PromiseGetBundle(pp)->name) == 0)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "",
                          " !! insert_lines promise uses the same select_line_matching anchor (\"%s\") as another promise. This will lead to non-convergent behaviour unless \"empty_file_before_editing\" is set.",
                          sp);
                    PromiseRef(OUTPUT_LEVEL_INFORM, pp);
                }
            }
            else
            {
                PrependItem(&EDIT_ANCHORS, sp, PromiseGetBundle(pp)->name);
            }
        }
    }
}

/*****************************************************************************/

static SyntaxTypeMatch ConstraintCheckType(const Constraint *cp)

{
    CfDebug("  Post Check Constraint: %s =>", cp->lval);

    if (DEBUG)
    {
        RvalShow(stdout, cp->rval);
        printf("\n");
    }

// Check class

    for (size_t i = 0; CF_CLASSBODY[i].lval != NULL; i++)
    {
        if (strcmp(cp->lval, CF_CLASSBODY[i].lval) == 0)
        {
            SyntaxTypeMatch err = CheckConstraintTypeMatch(cp->lval, cp->rval, CF_CLASSBODY[i].dtype, CF_CLASSBODY[i].range.validation_string, 0);
            if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
            {
                return err;
            }
        }
    }

    if (THIS_AGENT_TYPE != AGENT_TYPE_COMMON && cp->type == POLICY_ELEMENT_TYPE_PROMISE)
    {
        PromiseType *promise_type = cp->parent.promise->parent_promise_type;

        for (size_t i = 0; i < CF3_MODULES; i++)
        {
            const PromiseTypeSyntax *ssp = CF_ALL_PROMISE_TYPES[i];
            if (!ssp)
            {
                continue;
            }

            for (size_t j = 0; ssp[j].bundle_type != NULL; j++)
            {
                PromiseTypeSyntax ss = ssp[j];

                if (ss.promise_type != NULL)
                {
                    if (strcmp(ss.promise_type, promise_type->name) == 0)
                    {
                        const ConstraintSyntax *bs = ss.constraint_set.constraints;

                        for (size_t l = 0; bs[l].lval != NULL; l++)
                        {
                            if (bs[l].dtype == DATA_TYPE_BUNDLE)
                            {
                            }
                            else if (bs[l].dtype == DATA_TYPE_BODY)
                            {
                                const ConstraintSyntax *bs2 = bs[l].range.body_type_syntax;

                                for (size_t m = 0; bs2[m].lval != NULL; m++)
                                {
                                    if (strcmp(cp->lval, bs2[m].lval) == 0)
                                    {
                                        return CheckConstraintTypeMatch(cp->lval, cp->rval, bs2[m].dtype, bs2[m].range.validation_string, 0);
                                    }
                                }
                            }

                            if (strcmp(cp->lval, bs[l].lval) == 0)
                            {
                                return CheckConstraintTypeMatch(cp->lval, cp->rval, bs[l].dtype, bs[l].range.validation_string, 0);
                            }
                        }
                    }
                }
            }
        }
    }

/* Now check the functional modules - extra level of indirection */

    for (size_t i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
    {
        if (CF_COMMON_BODIES[i].dtype == DATA_TYPE_BODY)
        {
            continue;
        }

        if (strcmp(cp->lval, CF_COMMON_BODIES[i].lval) == 0)
        {
            CfDebug("Found a match for lval %s in the common constraint attributes\n", cp->lval);
            return CheckConstraintTypeMatch(cp->lval, cp->rval, CF_COMMON_BODIES[i].dtype, CF_COMMON_BODIES[i].range.validation_string, 0);
        }
    }

    return SYNTAX_TYPE_MATCH_OK;
}

/* FIXME: need to be done automatically */
bool BundleTypeCheck(const char *name)
{
    /* FIXME: export size of CF_AGENTTYPES somewhere */
    for (int i = 0; strcmp(CF_AGENTTYPES[i], "<notype>") != 0; ++i)
    {
        if (!strcmp(CF_AGENTTYPES[i], name))
        {
            return true;
        }
    }

    if (!strcmp("edit_line", name))
    {
        return true;
    }

    if (!strcmp("edit_xml", name))
    {
        return true;
    }

    return false;
}
