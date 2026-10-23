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
#include <string_lib.h>

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
    ConstraintSyntaxNewBundleList("then", "Bundle or list of bundles to run in order on event", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

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

    success &= PromiseCheckBundleCallArity(pp, "then", errors);
    return success;
}

const PromiseTypeSyntax CF_REACTOR_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("reactor", "events", CF_EVENT_BODIES, &EventsParseTreeCheck, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
