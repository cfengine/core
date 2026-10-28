/*
  Copyright 2026 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <mod_reactor.h>

#include <syntax.h>
#include <policy.h>
#include <rlist.h>
#include <fncall.h>
#include <class.h>              /* ClassRefParse() */
#include <string_lib.h>         /* StringEqual(), IsStrIn() */

static const char *const POLICY_ERROR_EVENTS_MISSING_ATTRIBUTE =
    "Missing required attribute '%s' for promise type 'events'";
static const char *const POLICY_ERROR_EVENTS_EMPTY_THEN =
    "Empty list of bundles in attribute 'then' for promise type 'events'";
static const char *const POLICY_ERROR_WHEN_BODY_NO_EVENT =
    "Body when %s requires an event attribute, such as " REACTOR_FILE_DELETED;

static const ConstraintSyntax when_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,

    /* Row models */
    ConstraintSyntaxNewString(REACTOR_FILE_DELETED, CF_ANYSTRING, "File to react for on deletion", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString(REACTOR_FILE_CREATED, CF_ANYSTRING, "File to react for on creation", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString(REACTOR_FILE_MODIFIED, CF_ANYSTRING, "File to react for on change of its modification time", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString(REACTOR_FILE_CONTENT_MODIFIED, CF_ANYSTRING, "File to react for on change of its content hash", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString(REACTOR_FILE_PERMISSIONS_CHANGED, CF_ANYSTRING, "File to react for on change of its permissions", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString(REACTOR_FILE_OWNER_CHANGED, CF_ANYSTRING, "File to react for on change of its owning user or group", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString(REACTOR_FILE_SIZE_EXCEEDS, CF_ANYSTRING, "File to react for when its size grows above '" REACTOR_SIZE_THRESHOLD "'", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt(REACTOR_SIZE_THRESHOLD, CF_VALRANGE, "Size in bytes above which '" REACTOR_FILE_SIZE_EXCEEDS "' fires", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("interval", REACTOR_INTERVAL_RANGE, "Seconds between two checks for the event", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

/* Checks that a 'when' body has an event attribute, i.e. any attribute other
 * than the global ones (meta, inherit_from) and 'interval', which only sets
 * how the event is checked for. A body inheriting from another one may get
 * its event from it, and that one is checked on its own. */
static bool WhenBodyCheck(const Body *body, Seq *errors)
{
    assert(body != NULL);

    if (BodyHasConstraint(body, "inherit_from"))
    {
        return true;
    }

    for (size_t i = 0; i < SeqLength(body->conlist); i++)
    {
        const Constraint *cp = SeqAt(body->conlist, i);
        if (!StringEqual(cp->lval, "meta") && !StringEqual(cp->lval, "interval"))
        {
            return true;
        }
    }

    SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_BODY, body,
                                     POLICY_ERROR_WHEN_BODY_NO_EVENT, body->name));
    return false;
}

static const BodySyntax when_body = BodySyntaxNew("when", when_constraints, &WhenBodyCheck, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax CF_EVENT_BODIES[] =
{
    ConstraintSyntaxNewBody("when", &when_body, "Event to react to", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBundleList("then", "Agent, common or reactor bundle (without events promises) or list of bundles to run in order on event", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const char *const REACTOR_THEN_BUNDLE_TYPES[] = { CF_AGENTC, CF_COMMONC, CF_REACTORC, NULL };

const char *const REACTOR_EVENTS_BUNDLE_PROMISE_TYPES[] = { "meta", "vars", "classes", "events", NULL };

bool BundleIsReactorEventsBundle(const Bundle *bp)
{
    assert(bp != NULL);
    return StringEqual(bp->type, CF_REACTORC) && (BundleGetSection(bp, "events") != NULL);
}

static bool ReactorEventsBundleCheckSections(const Seq *sections, Seq *errors)
{
    bool success = true;
    for (size_t i = 0; i < SeqLength(sections); i++)
    {
        const BundleSection *sp = SeqAt(sections, i);
        if (!IsStrIn(sp->promise_type, REACTOR_EVENTS_BUNDLE_PROMISE_TYPES))
        {
            SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_BUNDLE_SECTION, sp,
                                             "Promise type '%s' not supported by reactor bundles with events promises, "
                                             "which can only have meta, vars, classes and events promises "
                                             "(the other promise types go in reactor bundles without events promises, "
                                             "run from the 'then' attribute of events promises)",
                                             sp->promise_type));
            success = false;
        }
    }
    return success;
}

bool ReactorBundleCheck(const Bundle *bp, Seq *errors)
{
    assert(bp != NULL);
    assert(StringEqual(bp->type, CF_REACTORC));

    if (!BundleIsReactorEventsBundle(bp))
    {
        return true;
    }

    bool success = true;
    success &= ReactorEventsBundleCheckSections(bp->sections, errors);
    success &= ReactorEventsBundleCheckSections(bp->custom_sections, errors);
    return success;
}

/* Checks that an entry of the 'then' attribute of the events promise, either
 * "name" or name(args), does not refer to a reactor bundle with events
 * promises, if the bundle is found. */
static bool ThenBundleCheck(const Promise *pp, const Constraint *cp, Rval rval, Seq *errors)
{
    assert(pp != NULL);
    assert(cp != NULL);

    const char *name = NULL;
    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        name = RvalScalarValue(rval);
        break;
    case RVAL_TYPE_FNCALL:
        name = RvalFnCallValue(rval)->name;
        break;
    default:
        return true;
    }

    // HACK: exploiting the fact that class-references and call-references are similar
    ClassRef ref = ClassRefParse(name);
    if (!ClassRefIsQualified(ref))
    {
        ClassRefQualify(&ref, PromiseGetNamespace(pp));
    }

    const Bundle *callee = NULL;
    for (size_t i = 0; callee == NULL && REACTOR_THEN_BUNDLE_TYPES[i] != NULL; i++)
    {
        callee = PolicyGetBundle(PromiseGetPolicy(pp), ref.ns, REACTOR_THEN_BUNDLE_TYPES[i], ref.name);
    }

    ClassRefDestroy(ref);

    if (callee != NULL && BundleIsReactorEventsBundle(callee))
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, cp,
                                         "Events promise cannot run reactor bundle '%s', it has events promises "
                                         "(reactor bundles with events promises are only evaluated when cf-reactor reads the policy)",
                                         name));
        return false;
    }
    return true;
}

static bool EventsParseTreeCheck(const Promise *pp, Seq *errors)
{
    bool success = true;

    if (PromiseGetConstraint(pp, "when") == NULL)
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_PROMISE, pp,
                                         POLICY_ERROR_EVENTS_MISSING_ATTRIBUTE, "when"));
        success = false;
    }

    /* Only a literally empty list can be caught here, a list made empty by an
     * empty @(list) is caught on evaluation. */
    const Constraint *then = PromiseGetConstraint(pp, "then");
    if (then == NULL)
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_PROMISE, pp,
                                         POLICY_ERROR_EVENTS_MISSING_ATTRIBUTE, "then"));
        success = false;
    }
    else if (then->rval.type == RVAL_TYPE_LIST && RvalRlistValue(then->rval) == NULL)
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, then,
                                         POLICY_ERROR_EVENTS_EMPTY_THEN));
        success = false;
    }
    else if (then->rval.type == RVAL_TYPE_LIST)
    {
        for (const Rlist *rp = RvalRlistValue(then->rval); rp != NULL; rp = rp->next)
        {
            success &= ThenBundleCheck(pp, then, rp->val, errors);
        }
    }
    else
    {
        success &= ThenBundleCheck(pp, then, then->rval, errors);
    }

    success &= PromiseCheckBundleCallArity(pp, "then", REACTOR_THEN_BUNDLE_TYPES, errors);
    return success;
}

const PromiseTypeSyntax CF_REACTOR_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("reactor", "events", CF_EVENT_BODIES, &EventsParseTreeCheck, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
