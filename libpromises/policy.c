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

#include "constraints.h"
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

const char *NamespaceFromConstraint(const Constraint *cp)
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
        bool require_comments = GetRawBooleanConstraint("require_comments", common_control->conlist);
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
                char *handle = GetConstraintValue("handle", promise, RVAL_TYPE_SCALAR);

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

SubType *BundleAppendSubType(Bundle *bundle, char *name)
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
        if ((isdigit((int)*promiser)) && (Str2Int(promiser) != CF_NOINT))
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
        PostCheckConstraint("none", "none", lval, rval);
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

static JsonElement *AttributeValueToJson(Rval rval)
{
    JsonElement *json_attribute = JsonObjectCreate(10);

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
    {
        char buffer[CF_BUFSIZE];

        EscapeQuotes((const char *) rval.item, buffer, sizeof(buffer));

        JsonObjectAppendString(json_attribute, "type", "string");
        JsonObjectAppendString(json_attribute, "value", buffer);
    }
        return json_attribute;

    case RVAL_TYPE_LIST:
    {
        Rlist *rp = NULL;
        JsonElement *list = JsonArrayCreate(10);

        JsonObjectAppendString(json_attribute, "type", "list");

        for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
        {
            JsonArrayAppendObject(list, AttributeValueToJson((Rval) {rp->item, rp->type}));
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
                JsonArrayAppendObject(arguments, AttributeValueToJson((Rval) {argp->item, argp->type}));
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

static JsonElement *BodyClassesToJson(const Seq *constraints)
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
        JsonObjectAppendObject(json_attribute, "rval", AttributeValueToJson(cp->rval));
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

static JsonElement *BundleClassesToJson(const Seq *promises)
{
    JsonElement *json_contexts = JsonArrayCreate(10);
    JsonElement *json_promises = JsonArrayCreate(10);
    char *current_context = "any";
    size_t context_offset_start = -1;
    size_t context_offset_end = -1;

    for (size_t ppi = 0; ppi < SeqLength(promises); ppi++)
    {
        Promise *pp = SeqAt(promises, ppi);

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
                JsonObjectAppendObject(json_attribute, "rval", AttributeValueToJson(cp->rval));
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
            JsonObjectAppendArray(json_promise_type, "classes", BundleClassesToJson(sp->promises));

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

    JsonObjectAppendArray(json_body, "classes", BodyClassesToJson(body->conlist));

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

void PolicyPrint(const Policy *policy, Writer *writer)
{
    ProgrammingError("Not implemented");
}
