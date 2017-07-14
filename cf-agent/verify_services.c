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
#include <cf-agent-windows-functions.h>
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

    PromiseBanner(ctx, pp);

    PromiseResult result = PROMISE_RESULT_SKIPPED;
    if (strcmp(a.service.service_type, "windows") == 0)
    {
#ifdef __MINGW32__
        result = PromiseResultUpdate(result, VerifyWindowsService(ctx, a, pp));
#else
        Log(LOG_LEVEL_INFO, "Service type windows not supported on this platform.");
#endif
    }
    else
    {
        result = PromiseResultUpdate(result, DoVerifyServices(ctx, a, pp));
    }

    YieldCurrentLock(thislock);

    return result;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static FnCall *DefaultServiceBundleCall(const Promise *pp, ServicePolicy service_policy)
{
    Rlist *args = NULL;
    FnCall *call = NULL;

    switch (service_policy)
    {
    case SERVICE_POLICY_START:
        RlistAppend(&args, pp->promiser, RVAL_TYPE_SCALAR);
        RlistAppend(&args, "start", RVAL_TYPE_SCALAR);
        break;

    case SERVICE_POLICY_RESTART:
        RlistAppend(&args, pp->promiser, RVAL_TYPE_SCALAR);
        RlistAppend(&args, "restart", RVAL_TYPE_SCALAR);
        break;

    case SERVICE_POLICY_RELOAD:
        RlistAppend(&args, pp->promiser, RVAL_TYPE_SCALAR);
        RlistAppend(&args, "restart", RVAL_TYPE_SCALAR);
        break;

    case SERVICE_POLICY_STOP:
    case SERVICE_POLICY_DISABLE:
    default:
        RlistAppend(&args, pp->promiser, RVAL_TYPE_SCALAR);
        RlistAppend(&args, "stop", RVAL_TYPE_SCALAR);
        break;
    }

    Rval name = DefaultBundleConstraint(pp, "service");

    if (PolicyGetBundle(PolicyFromPromise(pp), PromiseGetBundle(pp)->ns, "agent", (char *)name.item))
    {
        Log(LOG_LEVEL_VERBOSE, "Found service special bundle %s in ns %s\n", (char *)name.item, PromiseGetBundle(pp)->ns);
        call = FnCallNew(name.item, args);
    }
    else
    {
        call = FnCallNew("standard_services", args);
    }

    return call;
}

static PromiseResult DoVerifyServices(EvalContext *ctx, Attributes a, const Promise *pp)
{
    Rval call;
    {
        const Constraint *cp = PromiseGetConstraint(pp, "service_bundle");
        if (cp)
        {
            call = RvalCopy(cp->rval);
        }
        else
        {
            call = (Rval) { DefaultServiceBundleCall(pp, a.service.service_policy), RVAL_TYPE_FNCALL };
        }
    }
    a.havebundle = true;

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

    PromiseResult result = PROMISE_RESULT_NOOP;
    result = PromiseResultUpdate(result, VerifyMethod(ctx, call, a, pp));  // Send list of classes to set privately?

    RvalDestroy(call);

    return result;
}
