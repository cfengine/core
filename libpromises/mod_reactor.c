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

#define CF_NZERO_VALRANGE "1,99999999999"

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
    ConstraintSyntaxNewString(REACTOR_FILE_REPLACED, CF_ANYSTRING, "File to react for on replacement by another file (inode)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString(REACTOR_FILE_TYPE_CHANGED, CF_ANYSTRING, "File to react for on change of its type", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString(REACTOR_SYMLINK_TARGET_CHANGED, CF_ANYSTRING, "Symlink to react for on change of its target", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString(REACTOR_FILE_SIZE_EXCEEDS, CF_ANYSTRING, "File to react for when its size grows above '" REACTOR_SIZE_THRESHOLD "'", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt(REACTOR_SIZE_THRESHOLD, CF_VALRANGE, "Size in bytes above which '" REACTOR_FILE_SIZE_EXCEEDS "' fires", SYNTAX_STATUS_NORMAL),
    /* default */
    ConstraintSyntaxNewInt("interval", CF_NZERO_VALRANGE, "Seconds between two checks for the event", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax when_body = BodySyntaxNew("when", when_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax CF_EVENT_BODIES[] =
{
    ConstraintSyntaxNewBody("when", &when_body, "Event to react to", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBundleList("then", "Bundle or list of bundles to run in order on event", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static bool EventsParseTreeCheck(const Promise *pp, Seq *errors)
{
    return PromiseCheckBundleCallArity(pp, "then", errors);
}

const PromiseTypeSyntax CF_REACTOR_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("reactor", "events", CF_EVENT_BODIES, &EventsParseTreeCheck, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
