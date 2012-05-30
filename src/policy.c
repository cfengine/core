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

#include <assert.h>

Policy *PolicyNew(void)
{
    Policy *policy = xcalloc(1, sizeof(Policy));
    return policy;
}

void PolicyDestroy(Policy *policy)
{
    if (policy)
    {
        DeleteBundles(policy->bundles);
        DeleteBodies(policy->bodies);
        free(policy);
    }
}

Policy *PolicyFromPromise(const Promise *promise)
{
    assert(promise);

    SubType *subtype = promise->parent_subtype;
    assert(subtype);

    Bundle *bundle = subtype->parent_bundle;
    assert(bundle);

    return bundle->parent_policy;
}

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
                                                          "Variable contains existing data type contstraint %s, tried to redefine with %s",
                                                          data_type, cp->lval));
                    success = false;
                }
                data_type = cp->lval;
            }
        }
    }

    return success;
}

static bool PolicyCheckPromiseMethods(const Promise *pp, Sequence *errors)
{
    bool success = false;

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
                                                              "Conflicting arity in calling bundle %s, expected %d arguments, %d given",
                                                              call->name, RlistLen(callee->args), RlistLen(call->args)));
                        success = true;
                    }
                }
            }
        }
    }

    return success;
}

bool PolicyCheckPromise(const Promise *pp, Sequence *errors)
{
    bool success = false;

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

static bool PolicyCheckBundle(const Bundle *bp, Sequence *errors)
{
    bool success = false;

    for (const SubType *type = bp->subtypes; type; type = type->next)
    {
        for (const Promise *pp = type->promiselist; pp; pp = pp->next)
        {
            success &= PolicyCheckPromise(pp, errors);
        }
    }

    return success;
}


bool PolicyCheck(const Policy *policy, Sequence *errors)
{
    bool success = false;

    for (const Bundle *bp = policy->bundles; bp; bp = bp->next)
    {
        success &= PolicyCheckBundle(bp, errors);
    }

    return success;
}

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

void PolicyErrorDestroy(PolicyError *error)
{
    free(error->message);
    free(error);
}

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

void PolicyErrorWrite(Writer *writer, const PolicyError *error)
{
    SourceOffset offset = PolicyElementSourceOffset(error->type, error->subject);
    const char *path = PolicyElementSourceFile(error->type, error->subject);

    // FIX: need to track columns in SourceOffset
    WriterWriteF(writer, "%s:%d:%d: error: %s\n", path, (int)offset.line, 0, error->message);
}
