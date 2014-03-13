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

#include <verify_networks.h>
#include <attributes.h>
#include <eval_context.h>
#include <ornaments.h>
#include <locks.h>
#include <promises.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <files_lib.h>
#include <files_interfaces.h>
#include <pipes.h>
#include <item_lib.h>

#define CF_DEBIAN_IP_COMM "/sbin/ip"

static int NetworkSanityCheck(Attributes a,  const Promise *pp);
static void AssessNetworkingPromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static int GetRouteInfo(FIBState **list, const Promise *pp);
static void AssessStaticRoute(char *promiser, PromiseResult *result, EvalContext *ctx, FIBState *fib, const Attributes *a, const Promise *pp);


static void AssessAdvertiseRoute(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static void AssessLoadBalance(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
int ExecCommand(char *cmd, PromiseResult *result, const Promise *pp);


/****************************************************************************/

PromiseResult VerifyNetworkingPromise(EvalContext *ctx, const Promise *pp)
{
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Attributes a = GetNetworkAttributes(ctx, pp);

    if (!NetworkSanityCheck(a, pp))
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
    AssessNetworkingPromise(pp->promiser, &result, ctx, &a, pp);

    switch (result)
    {
    case PROMISE_RESULT_NOOP:
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, a, "Networking promise kept");
        break;
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_TIMEOUT:
    case PROMISE_RESULT_INTERRUPTED:
    case PROMISE_RESULT_WARN:
        cfPS(ctx, LOG_LEVEL_INFO, result, pp, a, "Networking promise not kept");
        break;
    case PROMISE_RESULT_CHANGE:
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Networking promise repaired");
        break;
    default:
        ProgrammingError("Unknown promise result");
        break;
    }


    YieldCurrentLock(thislock);
    return result;
}

/****************************************************************************/

static int NetworkSanityCheck(Attributes a,  const Promise *pp)
{
    if (strchr(a.networks.gateway_ip, '/'))
    {
        Log(LOG_LEVEL_ERR, "The gateway IP address should not be in CIDR format in 'networks' promise about '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (a.haveroutedto)
    {
        if (strchr(pp->promiser, '/'))
        {
            if (strstr(pp->promiser, ".0/") == NULL)
            {
                Log(LOG_LEVEL_ERR, "Unicast address given? Only network IP addresses may be in CIDR notation in 'networks' promise about '%s'", pp->promiser);
                PromiseRef(LOG_LEVEL_ERR, pp);
                return false;
            }
        }
    }

    return true;
}

/****************************************************************************/

void AssessNetworkingPromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    FIBState *fib = NULL, *fip;

    if (!GetRouteInfo(&fib, pp))
    {
        *result = PROMISE_RESULT_INTERRUPTED;
        return;
    }

    if (a->haveroutedto)
    {
        AssessStaticRoute(promiser, result, ctx, fib, a, pp);
    }
#ifdef OS_LINUX
    else if (a->haveadvertisedby)
    {
        AssessAdvertiseRoute(promiser, result, ctx, a, pp);
    }

    else if (a->havebalance)
    {
        AssessLoadBalance(promiser, result, ctx, a, pp);
    }
#endif
}

/*************************************************************************/

static int GetRouteInfo(FIBState **list, const Promise *pp)
{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char comm[CF_BUFSIZE];
    char network[CF_MAX_IP_LEN], gateway[CF_MAX_IP_LEN], device[CF_MAX_IP_LEN];
    FIBState *entry = NULL;

    snprintf(comm, CF_BUFSIZE, "%s route", CF_DEBIAN_IP_COMM);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", CF_DEBIAN_IP_COMM);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    while (!feof(pfp))
    {
        CfReadLine(&line, &line_size, pfp);

        if (feof(pfp))
        {
            break;
        }

        /*
          default via 192.168.1.1 dev wlan0  proto static
          10.6.0.0/24 via 10.6.0.25 dev tun0  proto static
          10.6.0.25 dev tun0  proto kernel  scope link  src 10.6.0.26
        */

        sscanf(line, "%31s via %31s dev %31s", network, gateway, device);
        entry = xcalloc(sizeof(FIBState), 1);
        entry->next = *list;
        *list = entry;
        entry->network = xstrdup(network);
        entry->gateway = xstrdup(gateway);
        entry->device = xstrdup(device);
    }

    free(line);
    cf_pclose(pfp);

    return true;
}

/*************************************************************************/

static void AssessStaticRoute(char *promiser, PromiseResult *result, EvalContext *ctx, FIBState *fib, const Attributes *a, const Promise *pp)
{
    FIBState *fip;
    char cmd[CF_BUFSIZE];

    for (fip = fib; fip != NULL; fip = fip->next)
    {
        if (strcmp(fip->network, promiser) == 0)
        {
            if (strcmp(a->networks.gateway_ip, fip->gateway) == 0)
            {
                if (strcmp(a->networks.gateway_interface, fip->device) == 0)
                {
                    if (a->networks.delete_route)
                    {
                        snprintf(cmd, CF_BUFSIZE, "%s route del %s via %s dev %s", CF_DEBIAN_IP_COMM, promiser, a->networks.gateway_ip, a->networks.gateway_interface);
                        Log(LOG_LEVEL_VERBOSE, "Deleting static route for %s via %s on %s", promiser, a->networks.gateway_ip, a->networks.gateway_interface);
                    }
                    else
                    {
                        Log(LOG_LEVEL_VERBOSE, "Static route for %s via %s / %s is ok", promiser, a->networks.gateway_ip, a->networks.gateway_interface);
                        return;
                    }
                }
                else
                {
                    snprintf(cmd, CF_BUFSIZE, "%s route replace %s via %s dev %s", CF_DEBIAN_IP_COMM, promiser, a->networks.gateway_ip, a->networks.gateway_interface);
                    Log(LOG_LEVEL_VERBOSE, "Adding static route for %s via %s on %s", promiser, a->networks.gateway_ip, a->networks.gateway_interface);
                }
            }
        }
        else
        {
            snprintf(cmd, CF_BUFSIZE, "%s route replace %s via %s dev %s", CF_DEBIAN_IP_COMM, promiser, a->networks.gateway_ip, a->networks.gateway_interface);
            Log(LOG_LEVEL_VERBOSE, "Adding static route for %s via %s on %s", promiser, a->networks.gateway_ip, a->networks.gateway_interface);
        }

        // The semantics of route replace allow us to add or change in a single convergent command.

        if (!ExecCommand(cmd, result, pp))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to change routing");
            *result = PROMISE_RESULT_FAIL;
            return;
        }
    }
}


/*************************************************************************/

static void AssessAdvertiseRoute(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    printf("Route advertisement not yet implemented\n");
}

/*************************************************************************/

static void AssessLoadBalance(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    printf("Load balancing not yet implemented\n");
}

