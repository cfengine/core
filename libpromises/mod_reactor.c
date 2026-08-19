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

static const ConstraintSyntax when_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,

    /* Row models */
    ConstraintSyntaxNewStringList("files_deleted", CF_ANYSTRING, "List of files to react for on deletion", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax when_body = BodySyntaxNew("when", when_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax CF_EVENT_BODIES[] =
{
    ConstraintSyntaxNewBody("when", &when_body, "Event to react to", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBundle("then", "Bundle to run on event", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_REACTOR_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("reactor", "events", CF_EVENT_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
