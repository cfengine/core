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

#include <verify_interfaces.h>
#include <attributes.h>
#include <eval_context.h>
#include <ornaments.h>
#include <locks.h>
#include <promises.h>
#include <string_lib.h>
#include <misc_lib.h>

static int InterfaceSanityCheck(Attributes a,  const Promise *pp);
static void AssessInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);

/****************************************************************************/

PromiseResult VerifyInterfacePromise(EvalContext *ctx, const Promise *pp)
{
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Attributes a = GetInterfaceAttributes(ctx, pp);

    if (!InterfaceSanityCheck(a, pp))
    {
        return PROMISE_RESULT_FAIL;
    }

    PromiseBanner(pp);

    snprintf(lockname, CF_BUFSIZE - 1, "interface-%s", pp->promiser);

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    AssessInterfacePromise(pp->promiser, &result, ctx, &a, pp);

    switch (result)
    {
    case PROMISE_RESULT_NOOP:
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, a, "Interface promise kept");
        break;
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_TIMEOUT:
    case PROMISE_RESULT_INTERRUPTED:
    case PROMISE_RESULT_WARN:
        cfPS(ctx, LOG_LEVEL_INFO, result, pp, a, "Interface promise not kept");
        break;
    case PROMISE_RESULT_CHANGE:
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Interface promise repaired");
        break;
    default:
        ProgrammingError("Unknown promise result");
        break;
    }


    YieldCurrentLock(thislock);
    return result;
}

/****************************************************************************/

static int InterfaceSanityCheck(Attributes a,  const Promise *pp)
{
    return true;
}

/****************************************************************************/

void AssessInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{

// Look for reserved variable
// VLANS[blue] int => "id"

    printf("CONFIG %s\n", promiser);
    printf("");

    if (IsDefinedClass(ctx,"debian"))
    {

    }

    // JunOS, Arista, CiscoIOS ?

    /*
      i.tagged_vlans = PromiseGetConstraintAsList(ctx, "tagged_vlans", pp);
      i.untagged_vlan = PromiseGetConstraintAsRval(pp, "untagged_vlan", RVAL_TYPE_SCALAR);
      i.v4_address = PromiseGetConstraintAsRval(pp, "ipv4_address", RVAL_TYPE_SCALAR);
      i.v6_address = PromiseGetConstraintAsRval(pp, "ipv6_address", RVAL_TYPE_SCALAR);
      i.duplex = PromiseGetConstraintAsRval(pp, "duplex", RVAL_TYPE_SCALAR);
      i.state = PromiseGetConstraintAsRval(pp, "state", RVAL_TYPE_SCALAR);
      i.aggregate = PromiseGetConstraintAsList(ctx, "aggregate", pp);
      i.state = PromiseGetConstraintAsRval(pp, "state", RVAL_TYPE_SCALAR);
      i.spanning = PromiseGetConstraintAsRval(pp, "spanning", RVAL_TYPE_SCALAR);
      i.bonding = PromiseGetConstraintAsBoolean(ctx, "bonding", pp);
      i.mtu = PromiseGetConstraintAsInt(ctx, "mtu", pp);
      i.speed = PromiseGetConstraintAsInt(ctx, "speed", pp);
      i.min_bonding = PromiseGetConstraintAsInt(ctx, "min_bonding", pp);
    */
}
