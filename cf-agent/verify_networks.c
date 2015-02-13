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

#include <verify_networks.h>
#include <verify_interfaces.h>
#include <attributes.h>
#include <eval_context.h>
#include <ornaments.h>
#include <locks.h>
#include <promises.h>
#include <misc_lib.h>
#include <files_lib.h>
#include <files_interfaces.h>
#include <pipes.h>
#include <item_lib.h>
#include <ip_address.h>
#include <string_lib.h>
#include <routing_services.h>

#define CF_LINUX_IP_COMM "/sbin/ip"

static int NetworkSanityCheck(Attributes a,  const Promise *pp);
static int ArpSanityCheck(Attributes a,  const Promise *pp);
static void AssessRoutePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static void AssessArpPromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static int GetRouteInfo(FIBState **list, const Promise *pp);
static int GetArpInfo(ARPState **list, const Promise *pp);
static void AssessStaticRoute(char *promiser, PromiseResult *result, EvalContext *ctx, FIBState *fib, const Attributes *a, const Promise *pp);
static void DeleteARPState(ARPState *arptable);

/****************************************************************************/

PromiseResult VerifyRoutePromise(EvalContext *ctx, const Promise *pp)
{
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Attributes a = GetNetworkAttributes(ctx, pp);

    if (!NetworkSanityCheck(a, pp))
    {
        return PROMISE_RESULT_FAIL;
    }

    PromiseBanner(ctx, pp);

    snprintf(lockname, CF_BUFSIZE - 1, "route-%s", pp->promiser);

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    AssessRoutePromise(pp->promiser, &result, ctx, &a, pp);

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

void AssessRoutePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    FIBState *fib = NULL;

    if (!GetRouteInfo(&fib, pp))
    {
        *result = PROMISE_RESULT_INTERRUPTED;
        return;
    }

    if (a->haveroutedto)
    {
        AssessStaticRoute(promiser, result, ctx, fib, a, pp);
    }
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

    snprintf(comm, CF_BUFSIZE, "%s route", CF_LINUX_IP_COMM);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", CF_LINUX_IP_COMM);
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

static void AssessStaticRoute(char *promiser, PromiseResult *result, ARG_UNUSED EvalContext *ctx, FIBState *fib, const Attributes *a, const Promise *pp)
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
                        snprintf(cmd, CF_BUFSIZE, "%s route del %s via %s dev %s", CF_LINUX_IP_COMM, promiser, a->networks.gateway_ip, a->networks.gateway_interface);
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
                    snprintf(cmd, CF_BUFSIZE, "%s route replace %s via %s dev %s", CF_LINUX_IP_COMM, promiser, a->networks.gateway_ip, a->networks.gateway_interface);
                    Log(LOG_LEVEL_VERBOSE, "Adding static route for %s via %s on %s", promiser, a->networks.gateway_ip, a->networks.gateway_interface);
                }
            }
        }
        else
        {
            snprintf(cmd, CF_BUFSIZE, "%s route replace %s via %s dev %s", CF_LINUX_IP_COMM, promiser, a->networks.gateway_ip, a->networks.gateway_interface);
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


/****************************************************************************/
/* ARP                                                                      */
/****************************************************************************/

PromiseResult VerifyArpPromise(EvalContext *ctx, const Promise *pp)
{
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Attributes a = GetArpAttributes(ctx, pp);

    if (!ArpSanityCheck(a, pp))
    {
        return PROMISE_RESULT_FAIL;
    }

    PromiseBanner(ctx, pp);

    snprintf(lockname, CF_BUFSIZE - 1, "arp-%s", pp->promiser);

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    AssessArpPromise(pp->promiser, &result, ctx, &a, pp);

    switch (result)
    {
    case PROMISE_RESULT_NOOP:
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, a, "Arp promise kept");
        break;
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_TIMEOUT:
    case PROMISE_RESULT_INTERRUPTED:
    case PROMISE_RESULT_WARN:
        cfPS(ctx, LOG_LEVEL_INFO, result, pp, a, "Arp promise not kept");
        break;
    case PROMISE_RESULT_CHANGE:
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Arp promise repaired");
        break;
    default:
        ProgrammingError("Unknown promise result");
        break;
    }


    YieldCurrentLock(thislock);
    return result;
}

/****************************************************************************/

static void AssessArpPromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    ARPState *arptable = NULL;
    char comm[CF_BUFSIZE];
    char ivar[CF_SMALLBUF];
    const char *choice = a->arp.interface;

    if (!GetArpInfo(&arptable, pp))
    {
        *result = PROMISE_RESULT_INTERRUPTED;
        return;
    }

    DataType type = CF_DATA_TYPE_NONE;
    xsnprintf(ivar, sizeof(ivar), "sys.interface");
    VarRef *ref = VarRefParse(ivar);
    const void *default_interface = EvalContextVariableGet(ctx, ref, &type);
    VarRefDestroy(ref);

    if (a->arp.interface == NULL && default_interface == NULL)
    {
        Log(LOG_LEVEL_ERR, "Cannot determine interface for addresses promise");
        *result = PROMISE_RESULT_FAIL;
        return;
    }

    if (a->arp.interface == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "No interface specified so defaulting to system discovered %s", (char *)default_interface);
        choice = default_interface;
    }

    for (ARPState *ap = arptable; ap != NULL; ap=ap->next)
    {
        if (strcmp(ap->ip, promiser) == 0)
        {
            if (strcmp(ap->mac, a->arp.link_address) == 0 &&
                strcmp(ap->device, choice) == 0 &&
                strcmp(ap->state, "PERMANENT") == 0)
            {
                // All ok
                return;
            }
            else
            {
                break;
            }
        }
    }

    if (a->arp.delete_link)
    {
        Log(LOG_LEVEL_VERBOSE, "Deleting neighbour address resolution for %s", promiser);
        snprintf(comm, CF_BUFSIZE, "%s neighbour delete %s dev %s", CF_LINUX_IP_COMM, promiser, choice);

        if (!ExecCommand(comm, result, pp))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to change address bindings");
            *result = PROMISE_RESULT_FAIL;
        }

        return;
    }

    if (a->arp.link_address)
    {
        Log(LOG_LEVEL_VERBOSE, "Adding neighbour address resolution for %s", promiser);
        printf("Addinging neighbour address resolution for %s\n", promiser);
        snprintf(comm, CF_BUFSIZE, "%s neighbour add %s lladdr %s dev %s nud permanent", CF_LINUX_IP_COMM, promiser, a->arp.link_address, choice);

        if (!ExecCommand(comm, result, pp))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to change address bindings");
            *result = PROMISE_RESULT_FAIL;
            return;
        }
    }

    DeleteARPState(arptable);
}

/****************************************************************************/

static int ArpSanityCheck(Attributes a,  const Promise *pp)
{
    char *sp;

    if ((sp = strchr(pp->promiser, '/')))
    {
        Log(LOG_LEVEL_ERR, "IP address should not include a mask `%s' in promise '%s'", sp, pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (a.arp.delete_link && a.arp.link_address)
    {
        Log(LOG_LEVEL_ERR, "Conflicting promise attributes in promise '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    return true;
}

/****************************************************************************/

static int GetArpInfo(ARPState **list, const Promise *pp)
{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char comm[CF_BUFSIZE];
    char ip[CF_MAX_IP_LEN + 1], mac[CF_MAX_IP_LEN], device[CF_SMALLBUF], state[CF_SMALLBUF];
    ARPState *entry = NULL;

    snprintf(comm, CF_BUFSIZE, "%s neighbour show", CF_LINUX_IP_COMM);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", CF_LINUX_IP_COMM);
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
          192.168.1.1 dev wlan0 lladdr 00:1d:7e:28:22:c6 REACHABLE

        */

        sscanf(line, "%64s dev %31s lladdr %31s %31s", ip, device, mac, state);
        entry = xcalloc(sizeof(FIBState), 1);
        entry->next = *list;
        *list = entry;
        entry->ip = xstrdup(ip);
        entry->mac = xstrdup(mac);
        entry->device = xstrdup(device);
        entry->state = xstrdup(state);
    }

    free(line);
    cf_pclose(pfp);

    return true;
}

/****************************************************************************/

static void DeleteARPState(ARPState *arptable)
{
    ARPState *ap, *next;

    for (ap = arptable; ap != NULL; ap = next)
    {
        free(ap->ip);
        free(ap->mac);
        free(ap->state);
        free(ap->device);
        next = ap->next;
        free((char *)ap);
    }
}
