/*
   Copyright (C) CFEngine AS

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

#include <verify_methods.h>

#include <actuator.h>
#include <env_context.h>
#include <vars.h>
#include <expand.h>
#include <files_names.h>
#include <scope.h>
#include <hashes.h>
#include <unix.h>
#include <attributes.h>
#include <locks.h>
#include <generic_agent.h> // HashVariables
#include <fncall.h>
#include <rlist.h>
#include <ornaments.h>
#include <string_lib.h>

static void GetReturnValue(EvalContext *ctx, const Bundle *callee, Promise *caller);

/*****************************************************************************/

PromiseResult VerifyMethodsPromise(EvalContext *ctx, Promise *pp)
{
    Attributes a = { {0} };

    a = GetMethodAttributes(ctx, pp);

    PromiseResult result = VerifyMethod(ctx, "usebundle", a, pp);
    EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser");

    return result;
}

/*****************************************************************************/

PromiseResult VerifyMethod(EvalContext *ctx, char *attrname, Attributes a, Promise *pp)
{
    Bundle *bp;
    void *vp;
    FnCall *fp;
    char method_name[CF_EXPANDSIZE];
    Rlist *args = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    if (a.havebundle)
    {
        if ((vp = ConstraintGetRvalValue(ctx, attrname, pp, RVAL_TYPE_FNCALL)))
        {
            fp = (FnCall *) vp;
            ExpandScalar(ctx, PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name, fp->name, method_name);
            args = fp->args;
        }
        else if ((vp = ConstraintGetRvalValue(ctx, attrname, pp, RVAL_TYPE_SCALAR)))
        {
            ExpandScalar(ctx, PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name, (char *) vp, method_name);
            args = NULL;
        }
        else
        {
            return PROMISE_RESULT_NOOP;
        }
    }

    GetLockName(lockname, "method", pp->promiser, args);

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);

    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_NOOP;
    }

    PromiseBanner(pp);

    char ns[CF_MAXVARSIZE] = "";
    char bundle_name[CF_MAXVARSIZE] = "";
    SplitScopeName(method_name, ns, bundle_name);
    
    bp = PolicyGetBundle(PolicyFromPromise(pp), EmptyString(ns) ? NULL : ns, "agent", bundle_name);
    if (!bp)
    {
        bp = PolicyGetBundle(PolicyFromPromise(pp), EmptyString(ns) ? NULL : ns, "common", bundle_name);
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (bp)
    {
        BannerSubBundle(bp, args);

        EvalContextStackPushBundleFrame(ctx, bp, args, a.inherit);
        BundleResolve(ctx, bp);

        result = ScheduleAgentOperations(ctx, bp);

        GetReturnValue(ctx, bp, pp);

        EvalContextStackPopFrame(ctx);

        switch (result)
        {
        case PROMISE_RESULT_FAIL:
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a, "Method '%s' failed in some repairs or aborted", bp->name);
            break;

        case PROMISE_RESULT_CHANGE:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Method '%s' invoked repairs", bp->name);
            break;

        default:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Method '%s' verified", bp->name);
            break;

        }

        for (const Rlist *rp = bp->args; rp; rp = rp->next)
        {
            const char *lval = RlistScalarValue(rp);
            VarRef *ref = VarRefParseFromBundle(lval, bp);
            EvalContextVariableRemove(ctx, ref);
            VarRefDestroy(ref);
        }
    }
    else
    {
        if (IsCf3VarString(method_name))
        {
            Log(LOG_LEVEL_ERR,
                  "A variable seems to have been used for the name of the method. In this case, the promiser also needs to contain the unique name of the method");
        }
        if (bp && (bp->name))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Method '%s' was used but was not defined", bp->name);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "A method attempted to use a bundle '%s' that was apparently not defined", method_name);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
    }

    
    YieldCurrentLock(thislock);
    return result;
}

/***********************************************************************/

static void GetReturnValue(EvalContext *ctx, const Bundle *callee, Promise *caller)
{
    char *result = ConstraintGetRvalValue(ctx, "useresult", caller, RVAL_TYPE_SCALAR);

    if (result)
    {
        VarRef *ref = VarRefParseFromBundle("last-result", callee);
        VariableTableIterator *iter = EvalContextVariableTableIteratorNew(ctx, ref->ns, ref->scope, ref->lval);
        Variable *result_var = NULL;
        while ((result_var = VariableTableIteratorNext(iter)))
        {
            assert(result_var->ref->num_indices == 1);
            if (result_var->ref->num_indices != 1)
            {
                continue;
            }

            VarRef *new_ref = VarRefParseFromBundle(result, PromiseGetBundle(caller));
            VarRefAddIndex(new_ref, result_var->ref->indices[0]);

            EvalContextVariablePut(ctx, new_ref, result_var->rval.item, result_var->type, "goal=state,source=bundle");

            VarRefDestroy(new_ref);
        }

        VarRefDestroy(ref);
        VariableTableIteratorDestroy(iter);
    }

}

