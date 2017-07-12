/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
#include <string_lib.h>
#include <fncall.h>
#include <rlist.h>
#include <class.h>

static const char *const POLICY_ERROR_METHODS_BUNDLE_ARITY =
    "Conflicting arity in calling bundle %s, expected %d arguments, %d given";

static const ConstraintSyntax CF_METHOD_BODIES[] =
{
    ConstraintSyntaxNewBool("inherit", "If true this causes the sub-bundle to inherit the private classes of its parent", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBundle("usebundle", "Specify the name of a bundle to run as a parameterized method", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("useresult", CF_IDRANGE, "Specify the name of a local variable to contain any result/return value from the child", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static bool MethodsParseTreeCheck(const Promise *pp, Seq *errors)
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
                // HACK: exploiting the fact that class-references and call-references are similar
                FnCall *call = RvalFnCallValue(cp->rval);
                ClassRef ref = ClassRefParse(call->name);
                if (!ClassRefIsQualified(ref))
                {
                    ClassRefQualify(&ref, PromiseGetNamespace(pp));
                }

                const Bundle *callee = PolicyGetBundle(PolicyFromPromise(pp), ref.ns, "agent", ref.name);
                if (!callee)
                {
                    callee = PolicyGetBundle(PolicyFromPromise(pp), ref.ns, "common", ref.name);
                }

                ClassRefDestroy(ref);

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

const PromiseTypeSyntax CF_METHOD_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "methods", CF_METHOD_BODIES, &MethodsParseTreeCheck, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
