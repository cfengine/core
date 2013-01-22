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

static void BundleDestroy(Bundle *bundle);
static void BodyDestroy(Body *body);

Policy *PolicyNew(void)
{
    Policy *policy = xcalloc(1, sizeof(Policy));

    policy->current_namespace = xstrdup("default");

    policy->bundles = SeqNew(100, BundleDestroy);
    policy->bodies = SeqNew(100, BodyDestroy);

    return policy;
}

/*************************************************************************/

void PolicyDestroy(Policy *policy)
{
    if (policy)
    {
        SeqDestroy(policy->bundles);
        SeqDestroy(policy->bodies);

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
            if (cp->rval.rtype == CF_FNCALL)
            {
                const FnCall *call = (const FnCall *)cp->rval.item;
                const Bundle *callee = GetBundle(PolicyFromPromise(pp), call->name, "agent");

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

    for (const Promise *pp = subtype->promiselist; pp; pp = pp->next)
    {
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

    for (const SubType *type = bundle->subtypes; type; type = type->next)
    {
        success &= PolicyCheckSubType(type, errors);
    }

    return success;
}

/*************************************************************************/

bool PolicyCheck(const Policy *policy, Seq *errors)
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

/*************************************************************************/

Bundle *AppendBundle(Policy *policy, const char *name, const char *type, Rlist *args,
                     const char *source_path)
{
    CfDebug("Appending new bundle %s %s (", type, name);

    if (DEBUG)
    {
        ShowRlist(stdout, args);
    }
    CfDebug(")\n");

    Bundle *bundle = xcalloc(1, sizeof(Bundle));
    bundle->parent_policy = policy;

    SeqAppend(policy->bundles, bundle);

    if (strcmp(policy->current_namespace,"default") == 0)
    {
        bundle->name = xstrdup(name);
    }
    else
    {
        char fqname[CF_BUFSIZE];
        snprintf(fqname,CF_BUFSIZE-1, "%s:%s",policy->current_namespace,name);
        bundle->name = xstrdup(fqname);
    }

    bundle->type = xstrdup(type);
    bundle->namespace = xstrdup(policy->current_namespace);
    bundle->args = CopyRlist(args);
    bundle->source_path = SafeStringDuplicate(source_path);

    return bundle;
}

/*******************************************************************/

Body *AppendBody(Policy *policy, const char *name, const char *type, Rlist *args,
                 const char *source_path)
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

    if (strcmp(policy->current_namespace,"default") == 0)
       {
       body->name = xstrdup(name);
       }
    else
       {
       char fqname[CF_BUFSIZE];
       snprintf(fqname,CF_BUFSIZE-1, "%s:%s",policy->current_namespace,name);
       body->name = xstrdup(fqname);
       }

    body->type = xstrdup(type);
    body->namespace = xstrdup(policy->current_namespace);
    body->args = CopyRlist(args);
    body->source_path = SafeStringDuplicate(source_path);
    body->conlist = SeqNew(10, ConstraintDestroy);

    return body;
}

/*******************************************************************/

SubType *AppendSubType(Bundle *bundle, char *typename)
{
    SubType *tp, *lp;

    CfDebug("Appending new type section %s\n", typename);

    if (bundle == NULL)
    {
        ProgrammingError("Attempt to add a type without a bundle");
    }

    for (lp = bundle->subtypes; lp != NULL; lp = lp->next)
    {
        if (strcmp(lp->name, typename) == 0)
        {
            return lp;
        }
    }

    tp = xcalloc(1, sizeof(SubType));

    if (bundle->subtypes == NULL)
    {
        bundle->subtypes = tp;
    }
    else
    {
        for (lp = bundle->subtypes; lp->next != NULL; lp = lp->next)
        {
        }

        lp->next = tp;
    }

    tp->parent_bundle = bundle;
    tp->name = xstrdup(typename);

    return tp;
}

/*******************************************************************/

Promise *AppendPromise(SubType *type, char *promiser, Rval promisee, char *classes, char *bundle, char *bundletype, char *namespace)
{
    Promise *pp, *lp;
    char *sp = NULL, *spe = NULL;
    char output[CF_BUFSIZE];

    if (type == NULL)
    {
        yyerror("Software error. Attempt to add a promise without a type\n");
        FatalError("Stopped");
    }

/* Check here for broken promises - or later with more info? */

    CfDebug("Appending Promise from bundle %s %s if context %s\n", bundle, promiser, classes);

    pp = xcalloc(1, sizeof(Promise));

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

    if (type->promiselist == NULL)
    {
        type->promiselist = pp;
    }
    else
    {
        for (lp = type->promiselist; lp->next != NULL; lp = lp->next)
        {
        }

        lp->next = pp;
    }

    pp->parent_subtype = type;
    pp->audit = AUDITPTR;
    pp->bundle = xstrdup(bundle);
    pp->namespace = xstrdup(namespace);
    pp->promiser = sp;
    pp->promisee = promisee;
    pp->classes = spe;
    pp->donep = &(pp->done);
    pp->has_subbundles = false;
    pp->conlist = SeqNew(10, ConstraintDestroy);
    pp->org_pp = NULL;

    pp->bundletype = xstrdup(bundletype);       /* cache agent,common,server etc */
    pp->agentsubtype = type->name;      /* Cache the typename */
    pp->ref_alloc = 'n';

    return pp;
}

/*******************************************************************/

static void DeleteSubTypes(SubType *tp)
{
    if (tp == NULL)
    {
        return;
    }

    if (tp->next != NULL)
    {
        DeleteSubTypes(tp->next);
    }

    DeletePromises(tp->promiselist);

    if (tp->name != NULL)
    {
        free(tp->name);
    }

    free(tp);
}

static void BundleDestroy(Bundle *bundle)
{
    if (bundle)
    {
        free(bundle->name);
        free(bundle->type);

        DeleteRlist(bundle->args);
        DeleteSubTypes(bundle->subtypes);
        free(bundle);
    }
}

static void BodyDestroy(Body *body)
{
    if (body)
    {
        free(body->name);
        free(body->type);

        DeleteRlist(body->args);
        SeqDestroy(body->conlist);
        free(body);
    }
}

/*******************************************************************/

void DeletePromise(Promise *pp)
{
    if (pp == NULL)
    {
        return;
    }

    CfDebug("DeletePromise(%s->[%c])\n", pp->promiser, pp->promisee.rtype);

    ThreadLock(cft_policy);

    if (pp->promiser != NULL)
    {
        free(pp->promiser);
    }

    if (pp->promisee.item != NULL)
    {
        DeleteRvalItem(pp->promisee);
    }

    free(pp->bundle);
    free(pp->bundletype);
    free(pp->classes);
    free(pp->namespace);

// ref and agentsubtype are only references, do not free

    SeqDestroy(pp->conlist);

    free((char *) pp);
    ThreadUnlock(cft_policy);
}

/*******************************************************************/

void DeletePromises(Promise *pp)
{
    if (pp == NULL)
    {
        return;
    }

    if (pp->this_server != NULL)
    {
        ThreadLock(cft_policy);
        free(pp->this_server);
        ThreadUnlock(cft_policy);
    }

    if (pp->next != NULL)
    {
        DeletePromises(pp->next);
    }

    if (pp->ref_alloc == 'y')
    {
        ThreadLock(cft_policy);
        free(pp->ref);
        ThreadUnlock(cft_policy);
    }

    DeletePromise(pp);
}

/*******************************************************************/

Bundle *GetBundle(const Policy *policy, const char *name, const char *agent)
{

    // We don't need to check for the namespace here, as it is prefixed to the name already

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);

        if (strcmp(bp->name, name) == 0)
        {
            if (agent)
            {
                if ((strcmp(bp->type, agent) == 0) || (strcmp(bp->type, "common") == 0))
                {
                    return bp;
                }
                else
                {
                    CfOut(cf_verbose, "", "The bundle called %s is not of type %s\n", name, agent);
                }
            }
            else
            {
                return bp;
            }
        }
    }

    return NULL;
}

/*******************************************************************/

SubType *GetSubTypeForBundle(char *type, Bundle *bp)
{
    SubType *sp;

    if (bp == NULL)
    {
        return NULL;
    }

    for (sp = bp->subtypes; sp != NULL; sp = sp->next)
    {
        if (strcmp(type, sp->name) == 0)
        {
            return sp;
        }
    }

    return NULL;
}
