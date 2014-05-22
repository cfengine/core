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
#include <eval_context.h>
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

static void GetReturnValue(EvalContext *ctx, const Bundle *callee, const Promise *caller);

/*****************************************************************************/

/*
 * This function should only be called from the evaluator so that methods promises
 * never report REPAIRED compliance (the promises inside will do that already).
 *
 * Promise types that delegate to bundles (services and users) should call VerifyMethod,
 * which maintains the REPAIRED compliance so that it bubbles up correctly to the parent
 * promise type.
 */

PromiseResult VerifyMethodsPromise(EvalContext *ctx, const Promise *pp)
{
    Attributes a = GetMethodAttributes(ctx, pp);

    const Constraint *cp = PromiseGetConstraint(pp, "usebundle");
    if (!cp)
    {
        Log(LOG_LEVEL_VERBOSE, "Promise had no attribute 'usebundle', cannot call method");
        return PROMISE_RESULT_FAIL;
    }

    PromiseResult result = VerifyMethod(ctx, cp->rval, a, pp);

    return result;
}

/*****************************************************************************/

PromiseResult VerifyMethod(EvalContext *ctx, const Rval call, Attributes a, const Promise *pp)
{
    assert(a.havebundle);

    const Rlist *args = NULL;
    Buffer *method_name = BufferNew();
    switch (call.type)
    {
    case RVAL_TYPE_FNCALL:
        {
            const FnCall *fp = RvalFnCallValue(call);
            ExpandScalar(ctx, PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name, fp->name, method_name);
            args = fp->args;
        }
        break;

    case RVAL_TYPE_SCALAR:
        {
            ExpandScalar(ctx, PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name,
                         RvalScalarValue(call), method_name);
            args = NULL;
        }
        break;

    default:
        BufferDestroy(method_name);
        return PROMISE_RESULT_NOOP;
    }

    char lockname[CF_BUFSIZE];
    GetLockName(lockname, "method", pp->promiser, args);
    CfLock thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        BufferDestroy(method_name);
        return PROMISE_RESULT_SKIPPED;
    }

    PromiseBanner(pp);

    const Bundle *bp = EvalContextResolveBundleExpression(ctx, PromiseGetPolicy(pp), BufferData(method_name), "agent");
    if (!bp)
    {
        bp = EvalContextResolveBundleExpression(ctx, PromiseGetPolicy(pp), BufferData(method_name), "common");
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
        case PROMISE_RESULT_SKIPPED:
        case PROMISE_RESULT_NOOP:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Method '%s' verified", bp->name);
            break;

        case PROMISE_RESULT_WARN:
            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a, "Method '%s' invoked repairs, but only warnings promised", bp->name);
            break;

        case PROMISE_RESULT_CHANGE:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Method '%s' invoked repairs", bp->name);
            break;

        case PROMISE_RESULT_FAIL:
        case PROMISE_RESULT_DENIED:
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Method '%s' failed in some repairs", bp->name);
            break;

        default: // PROMISE_RESULT_INTERRUPTED, TIMEOUT
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a, "Method '%s' aborted in some repairs", bp->name);
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
        if (IsCf3VarString(BufferData(method_name)))
        {
            Log(LOG_LEVEL_ERR,
                  "A variable seems to have been used for the name of the method. In this case, the promiser also needs to contain the unique name of the method");
        }

        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "A method attempted to use a bundle '%s' that was apparently not defined",
             BufferData(method_name));
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

    YieldCurrentLock(thislock);
    BufferDestroy(method_name);
    return result;
}

/***********************************************************************/

static void GetReturnValue(EvalContext *ctx, const Bundle *callee, const Promise *caller)
{
    char *result = PromiseGetConstraintAsRval(caller, "useresult", RVAL_TYPE_SCALAR);

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

            EvalContextVariablePut(ctx, new_ref, result_var->rval.item, result_var->type, "source=bundle");

            VarRefDestroy(new_ref);
        }

        VarRefDestroy(ref);
        VariableTableIteratorDestroy(iter);
    }

}
