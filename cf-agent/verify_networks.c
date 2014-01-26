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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <verify_routing.h>
#include <attributes.h>
#include <eval_context.h>
#include <ornaments.h>
#include <locks.h>
#include <promises.h>
#include <string_lib.h>
#include <misc_lib.h>

static int RouteSanityCheck(Attributes a,  const Promise *pp);
static void AssessRoutingPromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);

/****************************************************************************/

PromiseResult VerifyRoutingPromise(EvalContext *ctx, const Promise *pp)
{
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Attributes a = GetRoutingAttributes(ctx, pp);

    if (!RoutingSanityCheck(a, pp))
    {
        return PROMISE_RESULT_FAIL;
    }

    PromiseBanner(pp);

    snprintf(lockname, CF_BUFSIZE - 1, "route-%s", pp->promiser);

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    AssessRoutingPromise(pp->promiser, &result, ctx, &a, pp);

    switch (result)
    {
    case PROMISE_RESULT_NOOP:
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, a, "Routing promise kept");
        break;
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_TIMEOUT:
    case PROMISE_RESULT_INTERRUPTED:
    case PROMISE_RESULT_WARN:
        cfPS(ctx, LOG_LEVEL_INFO, result, pp, a, "Routing promise not kept");
        break;
    case PROMISE_RESULT_CHANGE:
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Routing promise repaired");
        break;
    default:
        ProgrammingError("Unknown promise result");
        break;
    }


    YieldCurrentLock(thislock);
    return result;
}

/****************************************************************************/

static int RouteSanityCheck(Attributes a,  const Promise *pp)
{
    return true;
}

/****************************************************************************/

void AssessRoutingPromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{

    printf("CONFIG %s\n", promiser);
    printf("");

// ip route replace
    // ip route delete

    /*
      r.relay_networks = PromiseGetConstraintAsList(ctx, "relay_networks", pp);
      r.rip_metric = PromiseGetConstraintAsInt(ctx, "rip_metric", pp);
      r.rip_timeout = PromiseGetConstraintAsInt(ctx, "rip_timeout", pp);
      r.rip_splithorizon = PromiseGetConstraintAsBoolean(ctx, "rip_split_horizon", pp);
      r.rip_passive = PromiseGetConstraintAsBoolean(ctx, "rip_passive", pp);

      r.nat_pool = PromiseGetConstraintAsRval(pp, "nat_pool", RVAL_TYPE_SCALAR);
      r.relay_policy = PromiseGetConstraintAsRval(pp, "relay_policy", RVAL_TYPE_SCALAR);

    */
}
