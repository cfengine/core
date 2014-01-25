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

#include <verify_services.h>

#include <actuator.h>
#include <verify_methods.h>
#include <promises.h>
#include <vars.h>
#include <attributes.h>
#include <fncall.h>
#include <locks.h>
#include <rlist.h>
#include <policy.h>
#include <scope.h>
#include <cf-agent-enterprise-stubs.h>
#include <ornaments.h>
#include <eval_context.h>

static int ServicesSanityChecks(Attributes a, const Promise *pp);
static void SetServiceDefaults(Attributes *a);
static PromiseResult DoVerifyServices(EvalContext *ctx, Attributes a, const Promise *pp);
static PromiseResult VerifyServices(EvalContext *ctx, Attributes a, const Promise *pp);


/*****************************************************************************/

PromiseResult VerifyServicesPromise(EvalContext *ctx, const Promise *pp)
{
    Attributes a = GetServicesAttributes(ctx, pp);

    SetServiceDefaults(&a);

    if (ServicesSanityChecks(a, pp))
    {
        return VerifyServices(ctx, a, pp);
    }
    else
    {
        return PROMISE_RESULT_NOOP;
    }
}

/*****************************************************************************/

static int ServicesSanityChecks(Attributes a, const Promise *pp)
{
    Rlist *dep;

    switch (a.service.service_policy)
    {
    case SERVICE_POLICY_START:
        break;

    case SERVICE_POLICY_STOP:
    case SERVICE_POLICY_DISABLE:
    case SERVICE_POLICY_RESTART:
    case SERVICE_POLICY_RELOAD:
        if (strcmp(a.service.service_autostart_policy, "none") != 0)
        {
            Log(LOG_LEVEL_ERR,
                "!! Autostart policy of service promiser '%s' needs to be 'none' when service policy is not 'start', but is '%s'",
                  pp->promiser, a.service.service_autostart_policy);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }
        break;

    default:
        Log(LOG_LEVEL_ERR, "Invalid service policy for service '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    for (dep = a.service.service_depend; dep != NULL; dep = dep->next)
    {
        if (strcmp(pp->promiser, RlistScalarValue(dep)) == 0)
        {
            Log(LOG_LEVEL_ERR, "Service promiser '%s' has itself as dependency", pp->promiser);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }
    }

    if (a.service.service_type == NULL)
    {
        Log(LOG_LEVEL_ERR, "Service type for service '%s' is not known", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

#ifdef __MINGW32__

    if (strcmp(a.service.service_type, "windows") != 0)
    {
        Log(LOG_LEVEL_ERR, "Service type for promiser '%s' must be 'windows' on this system, but is '%s'",
              pp->promiser, a.service.service_type);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

#endif /* __MINGW32__ */

    return true;
}

/*****************************************************************************/

static void SetServiceDefaults(Attributes *a)
{
    if (a->service.service_autostart_policy == NULL)
    {
        a->service.service_autostart_policy = "none";
    }

    if (a->service.service_depend_chain == NULL)
    {
        a->service.service_depend_chain = "ignore";
    }

// default service type to "windows" on windows platforms
#ifdef __MINGW32__
    if (a->service.service_type == NULL)
    {
        a->service.service_type = "windows";
    }
#else
    if (a->service.service_type == NULL)
    {
        a->service.service_type = "bundle";
    }
#endif /* __MINGW32__ */
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static PromiseResult VerifyServices(EvalContext *ctx, Attributes a, const Promise *pp)
{
    CfLock thislock;

    thislock = AcquireLock(ctx, pp->promiser, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser", pp->promiser, CF_DATA_TYPE_STRING, "source=promise");
    PromiseBanner(pp);

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (strcmp(a.service.service_type, "windows") == 0)
    {
        result = PromiseResultUpdate(result, VerifyWindowsService(ctx, a, pp));
    }
    else
    {
        result = PromiseResultUpdate(result, DoVerifyServices(ctx, a, pp));
    }

    EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser");
    YieldCurrentLock(thislock);

    return result;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static PromiseResult DoVerifyServices(EvalContext *ctx, Attributes a, const Promise *pp)
{
    FnCall *service_bundle = PromiseGetConstraintAsRval(pp, "service_bundle", RVAL_TYPE_FNCALL);
    if (!service_bundle)
    {
        service_bundle = PromiseGetConstraintAsRval(pp, "service_bundle", RVAL_TYPE_SCALAR);
    }

    assert(service_bundle);

    switch (a.service.service_policy)
    {
    case SERVICE_POLICY_START:
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "service_policy", "start", CF_DATA_TYPE_STRING, "source=promise");
        break;

    case SERVICE_POLICY_RESTART:
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "service_policy", "restart", CF_DATA_TYPE_STRING, "source=promise");
        break;

    case SERVICE_POLICY_RELOAD:
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "service_policy", "reload", CF_DATA_TYPE_STRING, "source=promise");
        break;
        
    case SERVICE_POLICY_STOP:
    case SERVICE_POLICY_DISABLE:
    default:
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "service_policy", "stop", CF_DATA_TYPE_STRING, "source=promise");
        break;
    }

    const Bundle *bp = PolicyGetBundle(PolicyFromPromise(pp), NULL, "agent", service_bundle->name);
    if (!bp)
    {
        bp = PolicyGetBundle(PolicyFromPromise(pp), NULL, "common", service_bundle->name);
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!bp)
    {
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a, "Service '%s' could not be invoked successfully", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

    if (!DONTDO)
    {
        result = PromiseResultUpdate(result, VerifyMethod(ctx, "service_bundle", a, pp));  // Send list of classes to set privately?
    }

    return result;
}

