/*
  Copyright 2024 Northern.tech AS

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

#include <mod_methods.h>

#include <syntax.h>
#include <policy.h>

static const ConstraintSyntax CF_METHOD_BODIES[] =
{
    ConstraintSyntaxNewBool("inherit", "If true this causes the sub-bundle to inherit the private classes of its parent", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBundle("usebundle", "Specify the name of a bundle to run as a parameterized method", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("useresult", CF_IDRANGE, "Specify the name of a local variable to contain any result/return value from the child", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static bool MethodsParseTreeCheck(const Promise *pp, Seq *errors)
{
    return PromiseCheckBundleCallArity(pp, "usebundle", errors);
}

const PromiseTypeSyntax CF_METHOD_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "methods", CF_METHOD_BODIES, &MethodsParseTreeCheck, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
