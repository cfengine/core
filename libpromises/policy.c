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
#include "logging.h"
#include "conversion.h"
#include "reporting.h"
#include "transaction.h"
#include "cfstream.h"
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

#include <assert.h>

//************************************************************************

static const char *DEFAULT_NAMESPACE = "default";

static const char *POLICY_ERROR_POLICY_NOT_RUNNABLE = "Policy is not runnable (does not contain a body common control)";
static const char *POLICY_ERROR_VARS_CONSTRAINT_DUPLICATE_TYPE = "Variable contains existing data type contstraint %s, tried to redefine with %s";
static const char *POLICY_ERROR_METHODS_BUNDLE_ARITY = "Conflicting arity in calling bundle %s, expected %d arguments, %d given";
static const char *POLICY_ERROR_BUNDLE_NAME_RESERVED = "Use of a reserved container name as a bundle name \"%s\"";
static const char *POLICY_ERROR_BUNDLE_REDEFINITION = "Duplicate definition of bundle %s with type %s";
static const char *POLICY_ERROR_BUNDLE_UNDEFINED = "Undefined bundle %s with type %s";
static const char *POLICY_ERROR_BODY_REDEFINITION = "Duplicate definition of body %s with type %s";
static const char *POLICY_ERROR_BODY_UNDEFINED = "Undefined body %s with type %s";
static const char *POLICY_ERROR_SUBTYPE_MISSING_NAME = "Missing promise type category for %s bundle";
static const char *POLICY_ERROR_SUBTYPE_INVALID = "%s is not a valid type category for bundle %s";
static const char *POLICY_ERROR_PROMISE_UNCOMMENTED = "Promise is missing a comment attribute, and comments are required by policy";
static const char *POLICY_ERROR_PROMISE_DUPLICATE_HANDLE = "Duplicate promise handle %s found";

//************************************************************************

static void BundleDestroy(Bundle *bundle);
static void BodyDestroy(Body *body);
static void ConstraintPostCheck(const char *bundle_subtype, const char *lval, Rval rval);

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
        return cp->parent.promise->parent_subtype->parent_bundle->ns;

    default:
        ProgrammingError("Constraint has parent type: %d", cp->type);
    }
}

/*************************************************************************/

Policy *PolicyFromPromise(const Promise *promise)
{
    assert(promise);

    SubType *subtype = promise->parent_subtype;
    assert(subtype);

    Bundle *bundle = subtype->parent_bundle;
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
        const char *ns = bundle->ns ? bundle->ns : DEFAULT_NAMESPACE;
        return StringConcatenate(3, ns, ":", bundle->name);  // CF_NS == ':'
    }

    return NULL;
}

/*************************************************************************/

static bool PolicyCheckPromiseVars(const Promise *pp, Seq *errors)
{
    bool success = true;

    // ensure variables are declared with only one type.
    {
        char *data_type = NULL;

        for (size_t i = 0; i < SeqLength(pp->conlist); i++)
        {
            Constraint *cp = SeqAt(pp->conlist, i);

            if (IsDataType(cp->lval))
            {
                if (data_type != NULL)
                {
                    SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, cp,
                                                          POLICY_ERROR_VARS_CONSTRAINT_DUPLICATE_TYPE,
                                                          data_type, cp->lval));
                    success = false;
                }
                data_type = cp->lval;
            }
        }
    }

    return success;
}

/*************************************************************************/

static bool PolicyCheckPromiseMethods(const Promise *pp, Seq *errors)
{
    bool success = true;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        const Constraint *cp = SeqAt(pp->conlist, i);

        // ensure: if call and callee are resolved, then they have matching arity
        if (StringSafeEqual(cp->lval, "usebundle"))
        {
            if (cp->rval.type == RVAL_TYPE_FNCALL)
            {
                const FnCall *call = (const FnCall *)cp->rval.item;
                const Bundle *callee = PolicyGetBundle(PolicyFromPromise(pp), NULL, "agent", call->name);
                if (!callee)
                {
                    callee = PolicyGetBundle(PolicyFromPromise(pp), NULL, "common", call->name);
                }

                if (callee)
                {
                    if (RlistLen(call->args) != RlistLen(callee->args))
                    {
                        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, cp,
                                                              POLICY_ERROR_METHODS_BUNDLE_ARITY,
                                                              call->name, RlistLen(callee->args), RlistLen(call->args)));
                        success = false;
                    }
                }
            }
        }
    }

    return success;
}

/*************************************************************************/

bool PolicyCheckPromise(const Promise *pp, Seq *errors)
{
    bool success = true;

    if (StringSafeCompare(pp->agentsubtype, "vars") == 0)
    {
        success &= PolicyCheckPromiseVars(pp, errors);
    }
    else if (StringSafeCompare(pp->agentsubtype, "methods") == 0)
    {
        success &= PolicyCheckPromiseMethods(pp, errors);
    }

    return success;
}

/*************************************************************************/

static bool PolicyCheckSubType(const SubType *subtype, Seq *errors)
{
    assert(subtype);
    assert(subtype->parent_bundle);
    bool success = true;

    // ensure subtype name is defined
    // FIX: shouldn't this be a syntax error in the parser?
    // FIX: this was copied from syntax:CheckSubType
    // FIX: if you are able to write a unit test for this error, please do
    if (!subtype->name)
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_SUBTYPE, subtype,
                                              POLICY_ERROR_SUBTYPE_MISSING_NAME,
                                              subtype->parent_bundle));
        success = false;
    }

    // ensure subtype is allowed in bundle (type)
    if (!SubTypeSyntaxLookup(subtype->parent_bundle->type, subtype->name).subtype)
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_SUBTYPE, subtype,
                                              POLICY_ERROR_SUBTYPE_INVALID,
                                              subtype->name, subtype->parent_bundle->name));
        success = false;
    }

    for (size_t i = 0; i < SeqLength(subtype->promises); i++)
    {
        const Promise *pp = SeqAt(subtype->promises, i);
        success &= PolicyCheckPromise(pp, errors);
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

    for (size_t i = 0; i < SeqLength(bundle->subtypes); i++)
    {
        const SubType *type = SeqAt(bundle->subtypes, i);
        success &= PolicyCheckSubType(type, errors);
    }

    return success;
}

/*************************************************************************/

static const BodySyntax *ConstraintGetSyntax(const Constraint *constraint)
{
    if (constraint->type != POLICY_ELEMENT_TYPE_PROMISE)
    {
        ProgrammingError("Attempted to get the syntax for a constraint not belonging to a promise");
    }

    const Promise *promise = constraint->parent.promise;
    const SubType *subtype = promise->parent_subtype;
    const Bundle *bundle = subtype->parent_bundle;

    const SubTypeSyntax subtype_syntax = SubTypeSyntaxLookup(bundle->type, subtype->name);

    for (size_t i = 0; subtype_syntax.bs[i].lval != NULL; i++)
    {
        const BodySyntax *body_syntax = &subtype_syntax.bs[i];
        if (strcmp(body_syntax->lval, constraint->lval) == 0)
        {
            return body_syntax;
        }
    }

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

    return NULL;
}

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
static char *RvalNamespaceComponent(const Rval *rval)
{
    if (strchr(RvalFullSymbol(rval), CF_NS))
    {
        char ns[CF_BUFSIZE] = { 0 };
        sscanf(RvalFullSymbol(rval), "%[^:]", ns);

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
static char *RvalSymbolComponent(const Rval *rval)
{
    char *sep = strchr(RvalFullSymbol(rval), CF_NS);
    if (sep)
    {
        return xstrdup(sep + 1);
    }
    else
    {
        return xstrdup(RvalFullSymbol(rval));
    }
}

static bool PolicyCheckUndefinedBodies(const Policy *policy, Seq *errors)
{
    bool success = true;

    for (size_t bpi = 0; bpi < SeqLength(policy->bundles); bpi++)
    {
        Bundle *bundle = SeqAt(policy->bundles, bpi);

        for (size_t sti = 0; sti < SeqLength(bundle->subtypes); sti++)
        {
            SubType *subtype = SeqAt(bundle->subtypes, sti);

            for (size_t ppi = 0; ppi < SeqLength(subtype->promises); ppi++)
            {
                Promise *promise = SeqAt(subtype->promises, ppi);

                for (size_t cpi = 0; cpi < SeqLength(promise->conlist); cpi++)
                {
                    Constraint *constraint = SeqAt(promise->conlist, cpi);

                    const BodySyntax *syntax = ConstraintGetSyntax(constraint);
                    if (syntax->dtype == DATA_TYPE_BODY)
                    {
                        char *ns = RvalNamespaceComponent(&constraint->rval);
                        char *symbol = RvalSymbolComponent(&constraint->rval);

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
        } // subtypes
    } // bundles

    return success;
}

static bool PolicyCheckUndefinedBundles(const Policy *policy, Seq *errors)
{
    bool success = true;

    for (size_t bpi = 0; bpi < SeqLength(policy->bundles); bpi++)
    {
        Bundle *bundle = SeqAt(policy->bundles, bpi);

        for (size_t sti = 0; sti < SeqLength(bundle->subtypes); sti++)
        {
            SubType *subtype = SeqAt(bundle->subtypes, sti);

            for (size_t ppi = 0; ppi < SeqLength(subtype->promises); ppi++)
            {
                Promise *promise = SeqAt(subtype->promises, ppi);

                for (size_t cpi = 0; cpi < SeqLength(promise->conlist); cpi++)
                {
                    Constraint *constraint = SeqAt(promise->conlist, cpi);

                    const BodySyntax *syntax = ConstraintGetSyntax(constraint);
                    if (syntax->dtype == DATA_TYPE_BUNDLE && !IsCf3VarString(RvalFullSymbol(&constraint->rval)))
                    {
                        char *ns = RvalNamespaceComponent(&constraint->rval);
                        char *symbol = RvalSymbolComponent(&constraint->rval);

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
        } // subtypes
    } // bundles

    return success;
}

static bool PolicyCheckRequiredComments(const Policy *policy, Seq *errors)
{
    const Body *common_control = PolicyGetBody(policy, NULL, "common", "control");
    if (common_control)
    {
        bool require_comments = ConstraintsGetAsBoolean("require_comments", common_control->conlist);
        if (!require_comments)
        {
            return true;
        }

        bool success = true;

        for (size_t bpi = 0; bpi < SeqLength(policy->bundles); bpi++)
        {
            Bundle *bundle = SeqAt(policy->bundles, bpi);

            for (size_t sti = 0; sti < SeqLength(bundle->subtypes); sti++)
            {
                SubType *subtype = SeqAt(bundle->subtypes, sti);

                for (size_t ppi = 0; ppi < SeqLength(subtype->promises); ppi++)
                {
                    Promise *promise = SeqAt(subtype->promises, ppi);

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
            } // subtypes
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

    Set *used_handles = SetNew((unsigned int (*)(const void*, unsigned int))GetHash, (bool (*)(const void *, const void *))StringSafeEqual, NULL);

    for (size_t bpi = 0; bpi < SeqLength(policy->bundles); bpi++)
    {
        Bundle *bundle = SeqAt(policy->bundles, bpi);

        for (size_t sti = 0; sti < SeqLength(bundle->subtypes); sti++)
        {
            SubType *subtype = SeqAt(bundle->subtypes, sti);

            for (size_t ppi = 0; ppi < SeqLength(subtype->promises); ppi++)
            {
                Promise *promise = SeqAt(subtype->promises, ppi);
                char *handle = ConstraintGetRvalValue("handle", promise, RVAL_TYPE_SCALAR);

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
                        SetAdd(used_handles, handle);
                    }
                }
            }
        }
    }

    SetDestroy(used_handles);

    return success;
}

bool PolicyCheckRunnable(const Policy *policy, Seq *errors, bool ignore_missing_bundles)
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

    success &= PolicyCheckRequiredComments(policy, errors);
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

        case POLICY_ELEMENT_TYPE_SUBTYPE:
        {
            const SubType *type = (const SubType *)element;
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

        case POLICY_ELEMENT_TYPE_SUBTYPE:
        {
            const SubType *type = (const SubType *)element;
            return PolicyElementSourceFile(POLICY_ELEMENT_TYPE_BUNDLE, type->parent_bundle);
        }

        case POLICY_ELEMENT_TYPE_PROMISE:
        {
            const Promise *promise = (const Promise *)element;
            return PolicyElementSourceFile(POLICY_ELEMENT_TYPE_SUBTYPE, promise->parent_subtype);
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

static void SubTypeDestroy(SubType *subtype)
{
    if (subtype)
    {
        for (size_t i = 0; i < SeqLength(subtype->promises); i++)
        {
            Promise *pp = SeqAt(subtype->promises, i);

            if (pp->this_server != NULL)
            {
                ThreadLock(cft_policy);
                free(pp->this_server);
                ThreadUnlock(cft_policy);
            }
            if (pp->ref_alloc == 'y')
            {
                ThreadLock(cft_policy);
                free(pp->ref);
                ThreadUnlock(cft_policy);
            }
        }

        SeqDestroy(subtype->promises);


        free(subtype->name);
        free(subtype);
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
    bundle->subtypes = SeqNew(10, SubTypeDestroy);

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

SubType *BundleAppendSubType(Bundle *bundle, const char *name)
{
    CfDebug("Appending new type section %s\n", name);

    if (bundle == NULL)
    {
        ProgrammingError("Attempt to add a type without a bundle");
    }

    // TODO: review SeqLookup
    for (size_t i = 0; i < SeqLength(bundle->subtypes); i++)
    {
        SubType *existing = SeqAt(bundle->subtypes, i);
        if (strcmp(existing->name, name) == 0)
        {
            return existing;
        }
    }

    SubType *tp = xcalloc(1, sizeof(SubType));

    tp->parent_bundle = bundle;
    tp->name = xstrdup(name);
    tp->promises = SeqNew(10, PromiseDestroy);

    SeqAppend(bundle->subtypes, tp);

    return tp;
}

/*******************************************************************/

Promise *SubTypeAppendPromise(SubType *type, const char *promiser, Rval promisee, const char *classes)
{
    char *sp = NULL, *spe = NULL;
    char output[CF_BUFSIZE];

    if (type == NULL)
    {
        yyerror("Software error. Attempt to add a promise without a type\n");
        FatalError("Stopped");
    }

/* Check here for broken promises - or later with more info? */

    CfDebug("Appending Promise from bundle %s %s if context %s\n", type->parent_bundle->name, promiser, classes);

    Promise *pp = xcalloc(1, sizeof(Promise));

    sp = xstrdup(promiser);

    if (strlen(classes) > 0)
    {
        spe = xstrdup(classes);
    }
    else
    {
        spe = xstrdup("any");
    }

    if ((strcmp(type->name, "classes") == 0) || (strcmp(type->name, "vars") == 0))
    {
        if ((isdigit((int)*promiser)) && (IntFromString(promiser) != CF_NOINT))
        {
            yyerror("Variable or class identifier is purely numerical, which is not allowed");
        }
    }

    if (strcmp(type->name, "vars") == 0)
    {
        if (!CheckParseVariableName(promiser))
        {
            snprintf(output, CF_BUFSIZE, "Use of a reserved or illegal variable name \"%s\" ", promiser);
            ReportError(output);
        }
    }

    SeqAppend(type->promises, pp);

    pp->parent_subtype = type;
    pp->audit = AUDITPTR;
    pp->bundle = xstrdup(type->parent_bundle->name);
    pp->ns = xstrdup(type->parent_bundle->ns);
    pp->promiser = sp;
    pp->promisee = promisee;
    pp->classes = spe;
    pp->donep = &(pp->done);
    pp->has_subbundles = false;
    pp->conlist = SeqNew(10, ConstraintDestroy);
    pp->org_pp = NULL;

    pp->bundletype = xstrdup(type->parent_bundle->type);       /* cache agent,common,server etc */

    pp->agentsubtype = type->name;      /* Cache the typename */

    pp->ref_alloc = 'n';

    return pp;
}

static void BundleDestroy(Bundle *bundle)
{
    if (bundle)
    {
        free(bundle->name);
        free(bundle->type);

        RlistDestroy(bundle->args);
        SeqDestroy(bundle->subtypes);
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
        ThreadLock(cft_policy);

        free(pp->promiser);

        if (pp->promisee.item)
        {
            RvalDestroy(pp->promisee);
        }

        free(pp->bundle);
        free(pp->bundletype);
        free(pp->classes);
        free(pp->ns);

        // ref and agentsubtype are only references, do not free
        SeqDestroy(pp->conlist);

        free((char *) pp);
        ThreadUnlock(cft_policy);
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

    // Check class
    if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
    {
        ConstraintPostCheck("none", lval, rval);
    }

    Constraint *cp = xcalloc(1, sizeof(Constraint));

    cp->lval = SafeStringDuplicate(lval);
    cp->rval = rval;

    cp->audit = AUDITPTR;
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

SubType *BundleGetSubType(Bundle *bp, const char *name)
{
    // TODO: hiding error, remove and see what will crash
    if (bp == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < SeqLength(bp->subtypes); i++)
    {
        SubType *sp = SeqAt(bp->subtypes, i);

        if (strcmp(name, sp->name) == 0)
        {
            return sp;
        }
    }

    return NULL;
}

/****************************************************************************/

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
        FatalError("Attempted to export attribute of type: %c", rval.type);
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

        JsonObjectAppendInteger(json_attribute, "offset", cp->offset.start);
        JsonObjectAppendInteger(json_attribute, "offsetEnd", cp->offset.end);

        context_offset_start = cp->offset.context;
        context_offset_end = cp->offset.end;

        JsonObjectAppendString(json_attribute, "lval", cp->lval);
        JsonObjectAppendObject(json_attribute, "rval", AttributeValueToJson(cp->rval, false));
        JsonArrayAppendObject(json_attributes, json_attribute);



        if (i == (SeqLength(constraints) - 1) || strcmp(current_context, ((Constraint *)SeqAt(constraints, i + 1))->classes) != 0)
        {
            JsonArrayAppendObject(json_contexts,
                                  CreateContextAsJson(current_context,
                                                      context_offset_start,
                                                      context_offset_end, "attributes", json_attributes));

            current_context = cp->classes;
        }
    }

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

        if (ppi == (SeqLength(promises) - 1) || strcmp(current_context, ((Promise *)SeqAt(promises, ppi + 1))->classes) != 0)
        {
            JsonArrayAppendObject(json_contexts,
                                  CreateContextAsJson(current_context,
                                                      context_offset_start,
                                                      context_offset_end, "promises", json_promises));

            current_context = pp->classes;
        }
    }

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

        for (size_t i = 0; i < SeqLength(bundle->subtypes); i++)
        {
            const SubType *sp = SeqAt(bundle->subtypes, i);

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


static void AttributeToString(Writer *writer, Constraint *attribute, bool symbolic_reference, int indent_level)
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
        AttributeToString(writer, cp, false, 2);
    }

    WriterWrite(writer, "\n}\n");
}


void BundleToString(Writer *writer, Bundle *bundle)
{
    WriterWriteF(writer, "bundle %s %s", bundle->type, bundle->name);
    ArgumentsToString(writer, bundle->args);
    WriterWrite(writer, "\n{");

    for (size_t i = 0; i < SeqLength(bundle->subtypes); i++)
    {
        SubType *promise_type = SeqAt(bundle->subtypes, i);

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
                AttributeToString(writer, cp, cp->references_body, 3);
            }
        }

        if (i == (SeqLength(bundle->subtypes) - 1))
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

static Promise *SubTypeAppendPromiseJson(SubType *subtype, JsonElement *json_promise, const char *context)
{
    const char *promiser = JsonObjectGetAsString(json_promise, "promiser");

    Promise *promise = SubTypeAppendPromise(subtype, promiser, (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, context);

    JsonElement *json_attributes = JsonObjectGetAsArray(json_promise, "attributes");
    for (size_t i = 0; i < JsonElementLength(json_attributes); i++)
    {
        JsonElement *json_attribute = JsonArrayGetAsObject(json_attributes, i);
        PromiseAppendConstraintJson(promise, json_attribute, context);
    }

    return promise;
}

static SubType *BundleAppendSubTypeJson(Bundle *bundle, JsonElement *json_subtype)
{
    const char *name = JsonObjectGetAsString(json_subtype, "name");

    SubType *subtype = BundleAppendSubType(bundle, name);

    JsonElement *json_contexts = JsonObjectGetAsArray(json_subtype, "contexts");
    for (size_t i = 0; i < JsonElementLength(json_contexts); i++)
    {
        JsonElement *json_context = JsonArrayGetAsObject(json_contexts, i);

        const char *context = JsonObjectGetAsString(json_context, "name");

        JsonElement *json_context_promises = JsonObjectGetAsArray(json_context, "promises");
        for (size_t j = 0; j < JsonElementLength(json_context_promises); j++)
        {
            JsonElement *json_promise = JsonArrayGetAsObject(json_context_promises, j);
            SubTypeAppendPromiseJson(subtype, json_promise, context);
        }
    }

    return subtype;
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
        JsonElement *json_subtypes = JsonObjectGetAsArray(json_bundle, "promiseTypes");
        for (size_t i = 0; i < JsonElementLength(json_subtypes); i++)
        {
            JsonElement *json_subtype = JsonArrayGetAsObject(json_subtypes, i);
            BundleAppendSubTypeJson(bundle, json_subtype);
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

Constraint *EffectiveConstraint(Seq *constraints)
{
    for (size_t i = 0; i < SeqLength(constraints); i++)
    {
        Constraint *cp = SeqAt(constraints, i);

        const char *context = ConstraintContext(cp);
        const char *ns = ConstraintGetNamespace(cp);

        if (IsDefinedClass(context, ns))
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

int PromiseGetConstraintAsBoolean(const char *lval, const Promise *pp)
{
    int retval = CF_UNDEFINED;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
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
                FatalError("Aborted");
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

int ConstraintsGetAsBoolean(const char *lval, const Seq *constraints)
{
    int retval = CF_UNDEFINED;

    for (size_t i = 0; i < SeqLength(constraints); i++)
    {
        Constraint *cp = SeqAt(constraints, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, NULL))
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
                FatalError("Aborted");
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

bool PromiseBundleConstraintExists(const char *lval, const Promise *pp)
{
    int retval = CF_UNDEFINED;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        const Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
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
                FatalError("Aborted");
            }

            return true;
        }
    }

    return false;
}

/*****************************************************************************/

int PromiseGetConstraintAsInt(const char *lval, const Promise *pp)
{
    int retval = CF_NOINT;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
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
                FatalError("Aborted");
            }

            retval = (int) IntFromString((char *) cp->rval.item);
        }
    }

    return retval;
}

/*****************************************************************************/

double PromiseGetConstraintAsReal(const char *lval, const Promise *pp)
{
    double retval = CF_NODOUBLE;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
            {
                if (retval != CF_NODOUBLE)
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
                FatalError("Aborted");
            }

            retval = DoubleFromString((char *) cp->rval.item);
        }
    }

    return retval;
}

/*****************************************************************************/

static mode_t Str2Mode(const char *s)
{
    int a = CF_UNDEFINED;
    char output[CF_BUFSIZE];

    if (s == NULL)
    {
        return 0;
    }

    sscanf(s, "%o", &a);

    if (a == CF_UNDEFINED)
    {
        snprintf(output, CF_BUFSIZE, "Error reading assumed octal value %s\n", s);
        FatalError("%s", output);
    }

    return (mode_t) a;
}

mode_t PromiseGetConstraintAsOctal(const char *lval, const Promise *pp)
{
    mode_t retval = 077;

// We could handle units here, like kb,b,mb

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
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
                FatalError("Aborted");
            }

            retval = Str2Mode((char *) cp->rval.item);
        }
    }

    return retval;
}

/*****************************************************************************/

#ifdef __MINGW32__

uid_t GetUidConstraint(const char *lval, const Promise *pp)
{                               // we use sids on windows instead
    return CF_SAME_OWNER;
}

#else /* !__MINGW32__ */

uid_t PromiseGetConstraintAsUid(const char *lval, const Promise *pp)
{
    int retval = CF_SAME_OWNER;
    char buffer[CF_MAXVARSIZE];

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
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
                FatalError("Aborted");
            }

            retval = Str2Uid((char *) cp->rval.item, buffer, pp);
        }
    }

    return retval;
}

#endif /* !__MINGW32__ */

/*****************************************************************************/

#ifdef __MINGW32__

gid_t GetGidConstraint(char *lval, const Promise *pp)
{                               // not applicable on windows: processes have no group
    return CF_SAME_GROUP;
}

#else /* !__MINGW32__ */

gid_t PromiseGetConstraintAsGid(char *lval, const Promise *pp)
{
    int retval = CF_SAME_GROUP;
    char buffer[CF_MAXVARSIZE];

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
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
                FatalError("Aborted");
            }

            retval = Str2Gid((char *) cp->rval.item, buffer, pp);
        }
    }

    return retval;
}
#endif /* !__MINGW32__ */

/*****************************************************************************/

Rlist *PromiseGetConstraintAsList(const char *lval, const Promise *pp)
{
    Rlist *retval = NULL;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
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
                FatalError("Aborted");
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
    SubTypeSyntax ss;
    int i, j, l, m;
    const BodySyntax *bs, *bs2;
    const SubTypeSyntax *ssp;

    CfDebug("  Verify Constrant name %s\n", lval);

    for (i = 0; i < CF3_MODULES; i++)
    {
        if ((ssp = CF_ALL_SUBTYPES[i]) == NULL)
        {
            continue;
        }

        for (j = 0; ssp[j].bundle_type != NULL; j++)
        {
            ss = ssp[j];

            if (ss.subtype != NULL)
            {
                bs = ss.bs;

                for (l = 0; bs[l].lval != NULL; l++)
                {
                    if (bs[l].dtype == DATA_TYPE_BUNDLE)
                    {
                    }
                    else if (bs[l].dtype == DATA_TYPE_BODY)
                    {
                        bs2 = (BodySyntax *) bs[l].range;

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

Constraint *PromiseGetConstraint(const Promise *pp, const char *lval)
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
            if (IsDefinedClass(cp->classes, pp->ns))
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

/*****************************************************************************/

void *ConstraintGetRvalValue(const char *lval, const Promise *pp, RvalType rtype)
{
    const Constraint *constraint = PromiseGetConstraint(pp, lval);

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

void PromiseRecheckAllConstraints(Promise *pp)
{
    static Item *EDIT_ANCHORS = NULL;
    Item *ptr;

/* Special promise type checks */

    if (SHOWREPORTS)
    {
        NewPromiser(pp);
    }

    if (!IsDefinedClass(pp->classes, pp->ns))
    {
        return;
    }

    char *sp = NULL;
    if (VarClassExcluded(pp, &sp))
    {
        return;
    }

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);
        ConstraintPostCheck(pp->agentsubtype, cp->lval, cp->rval);
    }

    if (strcmp(pp->agentsubtype, "insert_lines") == 0)
    {
        /* Multiple additions with same criterion will not be convergent -- but ignore for empty file baseline */

        if ((sp = ConstraintGetRvalValue("select_line_matching", pp, RVAL_TYPE_SCALAR)))
        {
            if ((ptr = ReturnItemIn(EDIT_ANCHORS, sp)))
            {
                if (strcmp(ptr->classes, pp->bundle) == 0)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "",
                          " !! insert_lines promise uses the same select_line_matching anchor (\"%s\") as another promise. This will lead to non-convergent behaviour unless \"empty_file_before_editing\" is set.",
                          sp);
                    PromiseRef(OUTPUT_LEVEL_INFORM, pp);
                }
            }
            else
            {
                PrependItem(&EDIT_ANCHORS, sp, pp->bundle);
            }
        }
    }

    PreSanitizePromise(pp);
}

/*****************************************************************************/

static void ConstraintPostCheck(const char *bundle_subtype, const char *lval, Rval rval)
{
    SubTypeSyntax ss;
    int i, j, l, m;
    const BodySyntax *bs, *bs2;
    const SubTypeSyntax *ssp;

    CfDebug("  Post Check Constraint %s: %s =>", bundle_subtype, lval);

    if (DEBUG)
    {
        RvalShow(stdout, rval);
        printf("\n");
    }

// Check class

    for (i = 0; CF_CLASSBODY[i].lval != NULL; i++)
    {
        if (strcmp(lval, CF_CLASSBODY[i].lval) == 0)
        {
            SyntaxTypeMatch err = CheckConstraintTypeMatch(lval, rval, CF_CLASSBODY[i].dtype, CF_CLASSBODY[i].range, 0);
            if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
            {
                FatalError("%s: %s", lval, SyntaxTypeMatchToString(err));
            }
        }
    }

    for (i = 0; i < CF3_MODULES; i++)
    {
        if ((ssp = CF_ALL_SUBTYPES[i]) == NULL)
        {
            continue;
        }

        for (j = 0; ssp[j].bundle_type != NULL; j++)
        {
            ss = ssp[j];

            if (ss.subtype != NULL)
            {
                if (strcmp(ss.subtype, bundle_subtype) == 0)
                {
                    bs = ss.bs;

                    for (l = 0; bs[l].lval != NULL; l++)
                    {
                        if (bs[l].dtype == DATA_TYPE_BUNDLE)
                        {
                        }
                        else if (bs[l].dtype == DATA_TYPE_BODY)
                        {
                            bs2 = (BodySyntax *) bs[l].range;

                            for (m = 0; bs2[m].lval != NULL; m++)
                            {
                                if (strcmp(lval, bs2[m].lval) == 0)
                                {
                                    SyntaxTypeMatch err = CheckConstraintTypeMatch(lval, rval, bs2[m].dtype, (char *) (bs2[m].range), 0);
                                    if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
                                    {
                                        FatalError("%s: %s", lval, SyntaxTypeMatchToString(err));
                                    }
                                    return;
                                }
                            }
                        }

                        if (strcmp(lval, bs[l].lval) == 0)
                        {
                            SyntaxTypeMatch err = CheckConstraintTypeMatch(lval, rval, bs[l].dtype, (char *) (bs[l].range), 0);
                            if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
                            {
                                FatalError("%s: %s", lval, SyntaxTypeMatchToString(err));
                            }
                            return;
                        }
                    }
                }
            }
        }
    }

/* Now check the functional modules - extra level of indirection */

    for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
    {
        if (CF_COMMON_BODIES[i].dtype == DATA_TYPE_BODY)
        {
            continue;
        }

        if (strcmp(lval, CF_COMMON_BODIES[i].lval) == 0)
        {
            CfDebug("Found a match for lval %s in the common constraint attributes\n", lval);
            SyntaxTypeMatch err = CheckConstraintTypeMatch(lval, rval, CF_COMMON_BODIES[i].dtype, (char *) (CF_COMMON_BODIES[i].range), 0);
            if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
            {
                FatalError("%s: %s", lval, SyntaxTypeMatchToString(err));
            }
            return;
        }
    }
}
