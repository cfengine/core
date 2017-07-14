/*
   Copyright 2017 Northern.tech AS

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

#include <mod_users.h>

#include <syntax.h>

static const ConstraintSyntax password_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewOption("format", "plaintext,hash", "The format of the given password, either plaintext or hash", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("data", "", "Password", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax password_body = BodySyntaxNew("password", password_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax users_constraints[] =
{
    ConstraintSyntaxNewOption("policy", "present,absent,locked", "The promised state of a given user", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("uid", CF_INTRANGE, "User id", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("password", &password_body, "User password", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("description", "", "User comment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("group_primary", "", "User primary group", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("groups_secondary", ".*", "User additional groups", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("home_dir", CF_ABSPATHRANGE, "User home directory", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBundle("home_bundle", "Specify the name of a bundle to run when creating a user", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("home_bundle_inherit", "If true this causes the home_bundle to inherit the private classes of its parent", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("shell", CF_ABSPATHRANGE, "User shell", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_USERS_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "users", users_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
