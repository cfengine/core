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

#include <assert.h>

//************************************************************************

static const char *DEFAULT_NAMESPACE = "default";

static const char *POLICY_ERROR_VARS_CONSTRAINT_DUPLICATE_TYPE = "Variable contains existing data type contstraint %s, tried to redefine with %s";
static const char *POLICY_ERROR_METHODS_BUNDLE_ARITY = "Conflicting arity in calling bundle %s, expected %d arguments, %d given";
static const char *POLICY_ERROR_BUNDLE_NAME_RESERVED = "Use of a reserved container name as a bundle name \"%s\"";
static const char *POLICY_ERROR_BUNDLE_REDEFINITION = "Duplicate definition of bundle %s with type %s";
static const char *POLICY_ERROR_BODY_REDEFINITION = "Duplicate definition of body %s with type %s";
static const char *POLICY_ERROR_SUBTYPE_MISSING_NAME = "Missing promise type category for %s bundle";
static const char *POLICY_ERROR_SUBTYPE_INVALID = "%s is not a valid type category for bundle %s";

//************************************************************************

Policy *PolicyNew(void)
{
    Policy *policy = xcalloc(1, sizeof(Policy));
    policy->current_namespace = xstrdup("default");
    return policy;
}

/*************************************************************************/

void PolicyDestroy(Policy *policy)
{
    if (policy)
    {
        DeleteBundles(policy->bundles);
        DeleteBodies(policy->bodies);
        free(policy->current_namespace);
        free(policy);
    }
}

/*************************************************************************/

void PolicySetNameSpace(Policy *policy, char *namespace)
{
    if (policy->current_namespace)
    {
        free(policy->current_namespace);
    }

    policy->current_namespace = xstrdup(namespace);
}

/*************************************************************************/

char *CurrentNameSpace(Policy *policy)
{
    return policy->current_namespace;
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
        const char *namespace = bundle->namespace ? bundle->namespace : DEFAULT_NAMESPACE;
        return StringConcatenate(3, namespace, ":", bundle->name);  // CF_NS == ':'
    }

    return NULL;
}

/*************************************************************************/

static bool PolicyCheckPromiseVars(const Promise *pp, Sequence *errors)
{
    bool success = true;

    // ensure variables are declared with only one type.
    {
        char *data_type = NULL;
        for (const Constraint *cp = pp->conlist; cp; cp = cp->next)
        {
            if (IsDataType(cp->lval))
            {
                if (data_type != NULL)
                {
                    SequenceAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, cp,
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

static bool PolicyCheckPromiseMethods(const Promise *pp, Sequence *errors)
{
    bool success = true;

    for (const Constraint *cp = pp->conlist; cp; cp = cp->next)
    {
        // ensure: if call and callee are resolved, then they have matching arity
        if (StringSafeEqual(cp->lval, "usebundle"))
        {
            if (cp->rval.rtype == CF_FNCALL)
            {
                const FnCall *call = (const FnCall *)cp->rval.item;
                const Bundle *callee = GetBundle(PolicyFromPromise(pp), call->name, "agent");

                if (callee)
                {
                    if (RlistLen(call->args) != RlistLen(callee->args))
                    {
                        SequenceAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, cp,
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

bool PolicyCheckPromise(const Promise *pp, Sequence *errors)
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

static bool PolicyCheckSubType(const SubType *subtype, Sequence *errors)
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
        SequenceAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_SUBTYPE, subtype,
                                              POLICY_ERROR_SUBTYPE_MISSING_NAME,
                                              subtype->parent_bundle));
        success = false;
    }

    // ensure subtype is allowed in bundle (type)
    if (!SubTypeSyntaxLookup(subtype->parent_bundle->type, subtype->name).subtype)
    {
        SequenceAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_SUBTYPE, subtype,
                                              POLICY_ERROR_SUBTYPE_INVALID,
                                              subtype->name, subtype->parent_bundle->name));
        success = false;
    }

    for (const Promise *pp = subtype->promiselist; pp; pp = pp->next)
    {
        success &= PolicyCheckPromise(pp, errors);
    }

    return success;
}

/*************************************************************************/

static bool PolicyCheckBundle(const Bundle *bundle, Sequence *errors)
{
    assert(bundle);
    bool success = true;

    // ensure no reserved bundle names are used
    {
        static const char *reserved_names[] = { "sys", "const", "mon", "edit", "match", "mon", "this", NULL };
        if (IsStrIn(bundle->name, reserved_names))
        {
            SequenceAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_BUNDLE, bundle,
                                                  POLICY_ERROR_BUNDLE_NAME_RESERVED, bundle->name));
            success = false;
        }
    }

    for (const SubType *type = bundle->subtypes; type; type = type->next)
    {
        success &= PolicyCheckSubType(type, errors);
    }

    return success;
}

/*************************************************************************/

bool PolicyCheck(const Policy *policy, Sequence *errors)
{
    bool success = true;

    // ensure bundle names are not duplicated
    for (const Bundle *bp = policy->bundles; bp; bp = bp->next)
    {
        for (const Bundle *bp2 = policy->bundles; bp2; bp2 = bp2->next)
        {
            if (bp != bp2 &&
                StringSafeEqual(bp->name, bp2->name) &&
                StringSafeEqual(bp->type, bp2->type))
            {
                SequenceAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_BUNDLE, bp,
                                                      POLICY_ERROR_BUNDLE_REDEFINITION,
                                                      bp->name, bp->type));
                success = false;
            }
        }
    }

    for (const Bundle *bp = policy->bundles; bp; bp = bp->next)
    {
        success &= PolicyCheckBundle(bp, errors);
    }

    
    // ensure body names are not duplicated

    for (const Body *bp = policy->bodies; bp; bp = bp->next)
    {
        for (const Body *bp2 = policy->bodies; bp2; bp2 = bp2->next)
        {
            if (bp != bp2 &&
                StringSafeEqual(bp->name, bp2->name) &&
                StringSafeEqual(bp->type, bp2->type))
            {
                if (strcmp(bp->type,"file") != 0)
                {
                    SequenceAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_BODY, bp,
                                                      POLICY_ERROR_BODY_REDEFINITION,
                                                      bp->name, bp->type));
                    success = false;
                }            
            }
        }
    }

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

static char *PolicyElementSourceFile(PolicyElementType type, const void *element)
{
    assert(element);

    switch (type)
    {
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
