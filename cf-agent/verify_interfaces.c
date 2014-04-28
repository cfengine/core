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
#include <files_lib.h>
#include <files_interfaces.h>
#include <pipes.h>
#include <item_lib.h>
#include <expand.h>
#include <network_services.h>
#include <files_operators.h>
#include <communication.h>
#include <ip_address.h>

#define CF_DEBIAN_IFCONF "/etc/network/interfaces"
#define CF_DEBIAN_VLAN_FILE "/proc/net/vlan/config"
#define CF_DEBIAN_VLAN_COMMAND "/sbin/vconfig"
#define CF_DEBIAN_IP_COMM "/sbin/ip"
#define CF_DEBIAN_BRCTL "/usr/sbin/brctl"
#define CF_DEBIAN_IFQUERY "/usr/sbin/ifquery"
#define CF_DEBIAN_IFUP "/usr/sbin/ifup"
#define CF_DEBIAN_ETHTOOL "/usr/bin/ethtool"

bool INTERFACE_WANTS_NATIVE = false;

static int InterfaceSanityCheck(EvalContext *ctx, Attributes a,  const Promise *pp);
static void AssessInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static void AssessDebianInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static void AssessDebianTaggedVlan(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static int GetVlanInfo(Item **list, const Promise *pp, const char *interface);
static int GetInterfaceInformation(LinkState **list, const Promise *pp, const Rlist *filter);
static void DeleteInterfaceInfo(LinkState *interfaces);
static int GetBridgeInfo(Bridges **list, const Promise *pp);
static int NewTaggedVLAN(int vlan_id, char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static int DeleteTaggedVLAN(int vlan_id, char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static void DeleteBridgeInfo(Bridges *bridges);
static void AssessIPv4Config(char *promiser, PromiseResult *result, EvalContext *ctx, Rlist *addresses, const Attributes *a, const Promise *pp);
static void AssessIPv6Config(char *promiser, PromiseResult *result, EvalContext *ctx, Rlist *addresses, const Attributes *a, const Promise *pp);
static void AssessBridge(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp);
static void AssessLACPBond(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp);
static void AssessDeviceAlias(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp);
static Rlist *IPV4Addresses(LinkState *ifs, char *interface);
static Rlist *IPV6Addresses(LinkState *ifs, char *interface);
static int CheckBridgeNative(char *promiser, PromiseResult *result, const Promise *pp);
static void CheckInterfaceOptions(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp);
static void GetInterfaceOptions(Promise *pp, int *full, int *speed, int *autoneg);
static int CheckSetBondMode(char *promiser, int mode, PromiseResult *result, const Promise *pp);
static int InterfaceDown(char *interface, PromiseResult *result, const Promise *pp);
static int InterfaceUp(char *interface, PromiseResult *result, const Promise *pp);

/****************************************************************************/

PromiseResult VerifyInterfacePromise(EvalContext *ctx, const Promise *pp)
{
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Attributes a = GetInterfaceAttributes(ctx, pp);

    if (!IsDefinedClass(ctx, pp->classes))
    {
        return PROMISE_RESULT_SKIPPED;
    }

    if (!InterfaceSanityCheck(ctx, a, pp))
    {
        return PROMISE_RESULT_FAIL;
    }

    PromiseBanner(pp);

    if (!IsPrivileged())
    {
        Log(LOG_LEVEL_INFO, "Interface %s cannot be configured without root privilege", pp->promiser);
        return PROMISE_RESULT_SKIPPED;
    }

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

static int InterfaceSanityCheck(EvalContext *ctx, Attributes a,  const Promise *pp)
{
    if (a.interface.delete && a.interface.state && (strcmp(a.interface.state, "up") == 0))
    {
        Log(LOG_LEVEL_ERR, "Cannot delete an interface if it is configured to be up in promise '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (a.havebridge && (a.haveipv4 || a.haveipv6))
    {
        Log(LOG_LEVEL_ERR, "Can't set IP address on bridge virtual interface for 'interfaces' promise '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (a.havebridge && (a.havetvlan || a.haveuvlan))
    {
        Log(LOG_LEVEL_ERR, "Bridge virtual interace cannot have vlans in 'interfaces' promise '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (a.havebond && a.havebridge)
    {
        Log(LOG_LEVEL_ERR, "Can't be both a bond and a bridge in 'interfaces' promise '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (a.havebond && a.interface.bonding == CF_NOINT)
    {
        Log(LOG_LEVEL_ERR, "Must set bonding mode in 'interfaces' promise '%s', e.g. \'802.3ad\'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (PromiseGetConstraintAsBoolean(ctx, "spanning_tree", pp) && !a.havebridge)
    {
        Log(LOG_LEVEL_ERR, "Spanning tree on non-bridge for 'interfaces' promise '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (a.havetvlan + a.haveuvlan + a.havebridge + a.havebond > 1)
    {
        Log(LOG_LEVEL_ERR, "'interfaces' promise for '%s' cannot contain vlan, bridge, aggregate/bond at same time", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (a.havelinkstate)
    {
        if (a.interface.autoneg && a.interface.duplex)
        {
            Log(LOG_LEVEL_ERR, "Interface '%s' cannot promise speed/duplex and auto-negotiation at same time", pp->promiser);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }

        if ((a.interface.speed != CF_NOINT || a.interface.duplex != NULL) && (a.interface.speed == CF_NOINT || a.interface.duplex == NULL))
        {
            Log(LOG_LEVEL_ERR, "Interface '%s' should promise both speed and duplex if either is promised", pp->promiser);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }
    }

    for (Rlist *rp = a.interface.v4_addresses; rp != NULL; rp=rp->next)
    {
        char test[CF_MAXVARSIZE];
        if (strlen(rp->val.item) > CF_MAX_IP_LEN)
        {
            Log(LOG_LEVEL_ERR, "Interface '%s' has improper IPv4 address %s (CIDR format expected)", pp->promiser, (char *)rp->val.item);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }

        strcpy(test, rp->val.item);
        ToLowerStrInplace(test);

        Buffer *buf = BufferNewFrom(test, strlen(test));
        IPAddress *ip1 = IPAddressNew(buf);
        BufferDestroy(buf);
        if (ip1 == NULL)
        {
            Log(LOG_LEVEL_ERR, "Interface '%s' has improper IPv4 address (CIDR format expected)", pp->promiser);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }
    }

    for (Rlist *rp = a.interface.v6_addresses; rp != NULL; rp=rp->next)
    {
        char test[CF_MAXVARSIZE];

        if (strlen(rp->val.item) > CF_MAX_IP_LEN)
        {
            Log(LOG_LEVEL_ERR, "Interface '%s' has improper IPv6 address %s (CIDR format expected) (len = %d/%d)", pp->promiser, (char *)rp->val.item,strlen(rp->val.item),CF_MAX_IP_LEN);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }

        strcpy(test, rp->val.item);
        ToLowerStrInplace(test);

        Buffer *buf = BufferNewFrom(test, strlen(test));
        IPAddress *ip1 = IPAddressNew(buf);
        BufferDestroy(buf);

        if (ip1 == NULL)
        {
            Log(LOG_LEVEL_ERR, "Interface '%s' has improper IPv6 address %s (CIDR format expected)", pp->promiser, (char *)rp->val.item);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }
    }

    return true;
}

/****************************************************************************/

void AssessInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    if (IsDefinedClass(ctx,"linux"))
    {
        AssessDebianInterfacePromise(promiser, result, ctx, a, pp);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "'interfaces' promises are not yet supported on this platform");
        *result = PROMISE_RESULT_INTERRUPTED;
        return;
    }

    if (a->interface.manager && ((strcmp(a->interface.manager, "native") == 0) || (strcmp(a->interface.manager, "nativefirst") == 0)))
    {
        // Signal to the DeleteType post processor WriteNativeInterfacesFile that we are going native!

        INTERFACE_WANTS_NATIVE = true;
    }
}

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

static void AssessDebianInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    LinkState *netinterfaces = NULL;
    Rlist *filter = NULL;
    char cmd[CF_BUFSIZE];

    if (a->havebond)
    {
        filter = a->interface.bond_interfaces;
    }

    if (a->havebridge)
    {
        filter = a->interface.bridge_interfaces;
    }

    if (!GetInterfaceInformation(&netinterfaces, pp, filter))
    {
        Log(LOG_LEVEL_ERR, "Unable to read the vlans - cannot keep interface promises");
        return;
    }

    if (a->interface.delete)
    {
        Log(LOG_LEVEL_INFO, "Shutting down and removing bridge interface %s", promiser);

        snprintf(cmd, CF_BUFSIZE, "%s link set down dev %s", CF_DEBIAN_IP_COMM, promiser);

        if ((ExecCommand(cmd, result, pp) != 0))
        {
            Log(LOG_LEVEL_VERBOSE, "Parent bridge interface %s could not be shutdown", promiser);
            *result = PROMISE_RESULT_FAIL;
            return;
        }

        snprintf(cmd, CF_BUFSIZE, "%s link delete dev %s", CF_DEBIAN_IP_COMM, promiser);

        if ((ExecCommand(cmd, result, pp) != 0))
        {
            Log(LOG_LEVEL_VERBOSE, "Virtual interface %s could not be removed", promiser);
            *result = PROMISE_RESULT_FAIL;
            return;
        }

        DeleteInterfaceInfo(netinterfaces);
        return;
    }

    if (a->interface.state && strcmp(a->interface.state, "down") == 0)
    {
        snprintf(cmd, CF_BUFSIZE, "%s link set dev %s down", CF_DEBIAN_IP_COMM, promiser);

        if (!ExecCommand(cmd, result, pp))
        {
            *result = PROMISE_RESULT_FAIL;
        }

        DeleteInterfaceInfo(netinterfaces);
        return;
    }

    if (a->haveipv4)
    {
        AssessIPv4Config(promiser, result, ctx, IPV4Addresses(netinterfaces, promiser), a, pp);
    }

    if (a->haveipv6)
    {
        AssessIPv6Config(promiser, result, ctx, IPV6Addresses(netinterfaces, promiser), a, pp);
    }

    if (a->havetvlan)
    {
        AssessDebianTaggedVlan(promiser, result, ctx, a, pp);
    }

    if (a->haveuvlan)
    {
        AssessDeviceAlias(promiser, result, ctx, netinterfaces, a, pp);
    }

    if (a->havebridge)
    {
        AssessBridge(promiser, result, ctx, netinterfaces, a, pp);
    }

    if (a->havebond)
    {
        AssessLACPBond(promiser, result, ctx, netinterfaces, a, pp);
    }

    CheckInterfaceOptions(promiser, result, ctx, netinterfaces, a, pp);

    if (a->interface.state && strcmp(a->interface.state, "up") == 0)
    {
        InterfaceUp(promiser, result, pp);
    }

    DeleteInterfaceInfo(netinterfaces);

    LinkStateOSPF *ospfp = xcalloc(sizeof(LinkStateOSPF), 1);

    if (a->havelinkservices && QueryOSPFInterfaceState(ctx, a, pp, ospfp))
    {
        KeepOSPFInterfacePromises(ctx, a, pp, result, ospfp);
    }

    free(ospfp);
}

/****************************************************************************/

static void AssessDebianTaggedVlan(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    Item *ip;
    Rlist *rp;
    int vlan_id = 0, found;
    Item *vlans = NULL;          /* GLOBAL_X */

    // Look through the labelled VLANs to see if they are on this interface

    if (!GetVlanInfo(&vlans, pp, promiser))
    {
        Log(LOG_LEVEL_ERR, "Unable to read the vlans - cannot keep interface promises");
        return;
    }

    for (rp = a->interface.tagged_vlans; rp != NULL; rp = rp->next)
    {
        found = false;
        vlan_id = atoi((char *)rp->val.item);

        if (vlan_id == 1)
        {
            Log(LOG_LEVEL_ERR, "VLAN 1 may not be tagged/access as per 802.1D standard");
            *result = PROMISE_RESULT_FAIL;
            continue;
        }

        if (vlan_id == 0)
        {
            char vlan_lookup[CF_MAXVARSIZE];
            DataType type = CF_DATA_TYPE_NONE;

            // Non-numeric alias (like JunOS) have to be looked up in VLAN_ALIASES[]
            snprintf(vlan_lookup, CF_MAXVARSIZE, "VLAN_ALIASES[%s]", (char *)rp->val.item);

            VarRef *ref = VarRefParse(vlan_lookup);
            const void *value = EvalContextVariableGet(ctx, ref, &type);
            VarRefDestroy(ref);

            if (DataTypeToRvalType(type) == RVAL_TYPE_SCALAR)
            {
                vlan_id = atoi((char *)value);
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Variable %s is not defined in `interfaces' promise", vlan_lookup);
                PromiseRef(LOG_LEVEL_ERR, pp);
                *result = PROMISE_RESULT_INTERRUPTED;
                return;
            }
        }

        for (ip = vlans; ip != NULL; ip = ip->next)
        {
            if (ip->counter == vlan_id)
            {
                DeleteItem(&vlans, ip);
                found = true;
            }
        }

        if (found)
        {
            continue;
        }

        if (!NewTaggedVLAN(vlan_id, promiser, result, ctx, a, pp))
        {
            // Something's wrong...
            return;
        }
    }

    // Anything remaining needs to be removed

    for (ip = vlans; ip != NULL; ip=ip->next)
    {
        DeleteTaggedVLAN(vlan_id, promiser, result, ctx, a, pp);
    }

    DeleteItemList(vlans);
}

/****************************************************************************/

static int NewTaggedVLAN(int vlan_id, char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    char cmd[CF_BUFSIZE];
    int ret;

    Log(LOG_LEVEL_VERBOSE, "Did not find tagged VLAN %d on %s (i.e. virtual device %s.%d)", vlan_id, promiser, promiser, vlan_id);

    snprintf(cmd, CF_BUFSIZE, "%s add %s %d", CF_DEBIAN_VLAN_COMMAND, pp->promiser, vlan_id);

// ip link add link eth0 name eth0.10 type vlan id 10

    if ((ret = ExecCommand(cmd, result, pp)))
    {
        Log(LOG_LEVEL_INFO, "Tagging VLAN %d on %s for 'interfaces' promise", vlan_id, promiser);
    }

    snprintf(cmd, CF_BUFSIZE, "%s.%d", pp->promiser, vlan_id);
    InterfaceUp(cmd, result, pp);

    return ret;
}

/****************************************************************************/

static int DeleteTaggedVLAN(int vlan_id, char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    char cmd[CF_BUFSIZE];
    int ret;

    Log(LOG_LEVEL_VERBOSE, "Attempting to remove VLAN %d on %s (i.e. virtual device %s.%d)", vlan_id, promiser, promiser, vlan_id);

    snprintf(cmd, CF_BUFSIZE, "%s rem %s.%d", CF_DEBIAN_VLAN_COMMAND, promiser, vlan_id);

    if ((ret = ExecCommand(cmd, result, pp)))
    {
        Log(LOG_LEVEL_INFO, "Purged VLAN %d on %s for 'interfaces' promise", vlan_id, promiser);
    }

    return ret;
}

/****************************************************************************/

static void AssessIPv4Config(char *promiser, PromiseResult *result, EvalContext *ctx,  Rlist *addresses, const Attributes *a, const Promise *pp)
{
    Rlist *rp, *rpa;
    char cmd[CF_BUFSIZE];

    for (rp = a->interface.v4_addresses; rp != NULL; rp = rp->next)
    {
        if (!IPAddressInList(addresses, rp->val.item))
        {
            *result = PROMISE_RESULT_CHANGE;

            // Broadcast is now assumed "ones"

            snprintf(cmd, CF_BUFSIZE, "%s addr add %s broadcast + dev %s", CF_DEBIAN_IP_COMM, (char *)rp->val.item, promiser);

            Log(LOG_LEVEL_VERBOSE, "Adding ipv4 %s to %s", (char *)rp->val.item, promiser);

            if (!ExecCommand(cmd, result, pp))
            {
                *result = PROMISE_RESULT_FAIL;
                return;
            }
        }
    }

    if (a->interface.purge)
    {
        for (rpa = addresses; rpa != NULL; rpa=rpa->next)
        {
            if (!IPAddressInList(a->interface.v4_addresses, rpa->val.item))
            {
                *result = PROMISE_RESULT_CHANGE;

                Log(LOG_LEVEL_VERBOSE, "Purging ipv4 %s from %s", (char *)rpa->val.item, promiser);
                snprintf(cmd, CF_BUFSIZE, "%s addr del %s dev %s", CF_DEBIAN_IP_COMM, (char *)rpa->val.item, promiser);

                if (!ExecCommand(cmd, result, pp))
                {
                    *result = PROMISE_RESULT_FAIL;
                }
            }
        }
    }
}

/****************************************************************************/

static void AssessIPv6Config(char *promiser, PromiseResult *result, EvalContext *ctx, Rlist *addresses, const Attributes *a, const Promise *pp)
{
    Rlist *rp, *rpa;
    char cmd[CF_BUFSIZE];
    char b[CF_MAX_IP_LEN], c[CF_MAX_IP_LEN];

    for (rp = a->interface.v6_addresses; rp != NULL; rp = rp->next)
    {
        if (!IPAddressInList(addresses, rp->val.item))
        {
            *result = PROMISE_RESULT_CHANGE;

            snprintf(cmd, CF_BUFSIZE, "%s -6 addr add %s dev %s", CF_DEBIAN_IP_COMM, (char *)rp->val.item, promiser);

            Log(LOG_LEVEL_VERBOSE, "Adding ipv6 %s to %s", (char *)rp->val.item, promiser);

            if (!ExecCommand(cmd, result, pp))
            {
                *result = PROMISE_RESULT_FAIL;
                return;
            }
        }
    }

    if (a->interface.purge)
    {
        for (rpa = addresses; rpa != NULL; rpa=rpa->next)
        {
            if (!IPAddressInList(a->interface.v6_addresses, rpa->val.item))
            {
                *result = PROMISE_RESULT_CHANGE;

                Log(LOG_LEVEL_VERBOSE, "Purging ipv6 %s from %s", (char *)rpa->val.item, promiser);
                snprintf(cmd, CF_BUFSIZE, "%s -6 addr del %s dev %s", CF_DEBIAN_IP_COMM, (char *)rpa->val.item, promiser);

                if (!ExecCommand(cmd, result, pp))
                {
                    *result = PROMISE_RESULT_FAIL;
                }
            }
        }
    }
}

/****************************************************************************/

static void CheckInterfaceOptions(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)

{ LinkState *lsp;
    char comm[CF_BUFSIZE];

    for (lsp = ifs; lsp != NULL; lsp=lsp->next)
    {
        if (strcmp(lsp->name, promiser) == 0)
        {
            break;
        }
    }

    if (lsp != NULL)
    {
        if (a->interface.mtu != 0 && a->interface.mtu != lsp->mtu)
        {
            snprintf(comm, CF_BUFSIZE, "%s link set dev %s mtu %d", CF_DEBIAN_IP_COMM, promiser, lsp->mtu);
            if ((ExecCommand(comm, result, pp) == 0))
            {
                *result = PROMISE_RESULT_FAIL;
                Log(LOG_LEVEL_ERR, "Unable to set MTU on interface %s", promiser);
                return;
            }
        }
    }

    if (a->interface.autoneg || a->interface.speed)
    {
        int full = -1, speed = -1, autoneg = -1;
        int fix = false;

        GetInterfaceOptions(pp, &full, &speed, &autoneg);

        if (a->interface.autoneg && !autoneg)
        {
            snprintf(comm, CF_BUFSIZE, "%s %s autoneg on", CF_DEBIAN_ETHTOOL, promiser);
            fix = true;
        }
        else
        {
            if (full && a->interface.duplex && strcmp(a->interface.duplex, "full") != 0)
            {
                fix = true;
            }

            if (a->interface.speed != CF_NOINT && a->interface.speed != speed)
            {
                fix = true;
            }

            snprintf(comm, CF_BUFSIZE, "%s %s speed %d duplex %s autoneg off",
                     CF_DEBIAN_ETHTOOL, promiser, a->interface.speed, a->interface.duplex);
        }

        if (fix)
        {
            if ((ExecCommand(comm, result, pp) != 0))
            {
                Log(LOG_LEVEL_INFO, "Unable to set interface options in promise by %s", promiser);
                *result = PROMISE_RESULT_FAIL;
            }
        }
    }
}

/****************************************************************************/

static void AssessBridge(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)
{
    Bridges *bridges = NULL, *bp;
    Rlist *rp;
    char comm[CF_BUFSIZE];
    bool have_bridge_interface = false;
    bool pre_req = true;

    if ((strcmp(a->interface.manager, "native") == 0) || (strcmp(a->interface.manager, "nativefirst") == 0))
    {
        if (CheckBridgeNative(promiser, result, pp))
        {
            return;
        }
        else if (strcmp(a->interface.manager, "nativefirst") != 0)
        {
            Log(LOG_LEVEL_ERR, "Unable to configure interface with native tools - CFEngine cannot keep interface promises");
            *result = PROMISE_RESULT_FAIL;
            return;
        }
    }

    if (!GetBridgeInfo(&bridges, pp))
    {
        Log(LOG_LEVEL_ERR, "Unable to read the vlans - cannot keep interface promises");
        *result = PROMISE_RESULT_FAIL;
        return;
    }

    // If the dependencies are not up, we should interrupt and wait for convergence
    // CFEngine doesn't have to recurse like ifup since you always deal with a complete policy
    // You need a separate promise for the interfaces to set addresses anyway

    // Warn if there is not a separate promise for the dependencies, but some interfaces names are implicit
    // due to VLAN etc

    Rlist *all_members = RlistCopy(a->interface.bridge_interfaces);
    LinkState *lsp;

    for (lsp = ifs; lsp != NULL; lsp=lsp->next) // Long list
    {
        for (rp = all_members; rp != NULL; rp=rp->next)
        {
            if (strcmp(lsp->name, (char *)rp->val.item) == 0)
            {
                if (!lsp->up)
                {
                    // Try to bring them up, with or without config
                    snprintf(comm, CF_BUFSIZE, "%s link set up dev %s", CF_DEBIAN_IP_COMM, lsp->name);
                    if ((ExecCommand(comm, result, pp) != 0))
                    {
                        Log(LOG_LEVEL_VERBOSE, "Couldn't bring up member interface %s yet", lsp->name);
                        pre_req = false;
                    }
                }

                *(char *)(rp->val.item) = '\0';  // Mark this ok by blanking
            }
        }
    }

    for (rp = all_members; rp != NULL; rp=rp->next)
    {
        if (*(char *)(rp->val.item) != '\0')
        {
            snprintf(comm, CF_BUFSIZE, "%s link set up dev %s", CF_DEBIAN_IP_COMM,  (char *)(rp->val.item));
            if ((ExecCommand(comm, result, pp) != 0))
            {
                Log(LOG_LEVEL_VERBOSE, "Couldn't bring up member %s of interface %s yet", (char *)(rp->val.item), promiser);
                pre_req = false;
            }
        }
    }

    RlistDestroy(all_members);

    if (!pre_req)
    {
        *result = PROMISE_RESULT_FAIL;
        Log(LOG_LEVEL_INFO, "Member interfaces for %s were not all available, try bridging later", promiser);
        return;
    }

    // Now put the pieces together

    for (bp = bridges; bp != NULL; bp = bp->next)
    {
        if (strcmp(bp->name, promiser) == 0)
        {
            have_bridge_interface = true;
            Log(LOG_LEVEL_VERBOSE, "Parent bridge interface %s exists", promiser);
            break;
        }
    }

    if (!have_bridge_interface)
    {
        if (a->interface.delete)
        {
            // All is ok
            return;
        }

        Log(LOG_LEVEL_INFO, "Adding bridge interface %s", promiser);

        snprintf(comm, CF_BUFSIZE, "%s addbr %s", CF_DEBIAN_BRCTL, promiser);

        if ((ExecCommand(comm, result, pp) != 0))
        {
            Log(LOG_LEVEL_VERBOSE, "Parent bridge interface %s could not be added", promiser);
            *result = PROMISE_RESULT_FAIL;
            return;
        }
    }

    if (a->interface.delete)
    {
        Log(LOG_LEVEL_INFO, "Shutting down and removing bridge interface %s", promiser);

        snprintf(comm, CF_BUFSIZE, "%s link set down dev %s", CF_DEBIAN_IP_COMM, promiser);

        if ((ExecCommand(comm, result, pp) != 0))
        {
            Log(LOG_LEVEL_VERBOSE, "Parent bridge interface %s could not be shutdown", promiser);
            *result = PROMISE_RESULT_FAIL;
            return;
        }

        snprintf(comm, CF_BUFSIZE, "%s delbr %s", CF_DEBIAN_BRCTL, promiser);

        if ((ExecCommand(comm, result, pp) != 0))
        {
            Log(LOG_LEVEL_VERBOSE, "Parent bridge interface %s could not be removed", promiser);
            *result = PROMISE_RESULT_FAIL;
            return;
        }

        return;
    }

    for (rp = a->interface.bridge_interfaces; rp != NULL; rp = rp->next)
    {
        snprintf(comm, CF_BUFSIZE, "%s addif %s %s", CF_DEBIAN_BRCTL, promiser, (char *)rp->val.item);

        if ((ExecCommand(comm, result, pp) != 0))
        {
            Log(LOG_LEVEL_VERBOSE, "Memeber for bridge %s could not be added", promiser);
            *result = PROMISE_RESULT_FAIL;
            return;
        }
    }

    /* will be replaced with
       snprintf(cmd, CF_BUFSIZE, "%s link add name %s type bridge", CF_DEBIAN_IP_COMM, promiser);
       snprintf(cmd, CF_BUFSIZE, "%s link set dev %s master", CF_DEBIAN_IP_COMM, (char *)rp->val.item, promiser);
    */

    DeleteBridgeInfo(bridges);
}

/****************************************************************************/

static int CheckBridgeNative(char *promiser, PromiseResult *result, const Promise *pp)
{
    char comm[CF_BUFSIZE];

    snprintf(comm, CF_BUFSIZE, "%s --check %s --with-depends", CF_DEBIAN_IFQUERY, promiser);

    if ((ExecCommand(comm, result, pp) == 0))
    {
        // Promise ostensibly kept
        return true;
    }

    snprintf(comm, CF_BUFSIZE, "%s %s --with-depends", CF_DEBIAN_IFUP, promiser);

    if ((ExecCommand(comm, result, pp) == 0))
    {
        return true;
    }

    Log(LOG_LEVEL_INFO, "Bridge interfaces missing in %s", promiser);
    *result = PROMISE_RESULT_FAIL;
    return false;
}

/****************************************************************************/

static void AssessLACPBond(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)

{
    Rlist *rp;
    LinkState *lsp;
    char cmd[CF_BUFSIZE];
    int got_master = 0, got_children = 0, all_children = 0;

    // $ /sbin/modprobe bonding

    for (rp = a->interface.bond_interfaces; rp != NULL; rp=rp->next)
    {
        all_children++;
    }

    for (lsp = ifs; lsp != NULL; lsp=lsp->next) // Long list
    {
        bool orphan = true;

        if (strcmp (lsp->name, promiser) == 0 && lsp->is_parent)
        {
            got_master++;
            continue;
        }

        for (rp = a->interface.bond_interfaces; rp != NULL; rp=rp->next)
        {
            if (strcmp(lsp->name, (char *)rp->val.item) == 0)
            {
                if (lsp->parent && (strcmp(lsp->parent, promiser) == 0))
                {
                    orphan = false;
                    got_children++;
                    break;
                }
            }
        }

        if (orphan && lsp->parent && (strcmp(lsp->parent, promiser) == 0))
        {
            snprintf(cmd, CF_BUFSIZE, "%s link set %s nomaster", CF_DEBIAN_IP_COMM, lsp->name);

            Log(LOG_LEVEL_VERBOSE, "Freeing superfluous bonded interface %s from master %s", lsp->name, promiser);
            if ((ExecCommand(cmd, result, pp) != 0))
            {
                Log(LOG_LEVEL_INFO, "Freeing bond child %s for 'interfaces' promise %s failed", lsp->name, promiser);
                *result = PROMISE_RESULT_FAIL;
            }
        }
    }

    if (strcmp(a->interface.state, "down") == 0 || a->interface.delete)
    {
        if (!got_master)
        {
            // Good, we're done
            return;
        }
        else
        {
            Log(LOG_LEVEL_INFO, "Shutting down and removing aggregate/bond interface %s", promiser);

            if (InterfaceDown(promiser, result, pp))
            {
                return;
            }

            if (a->interface.delete)
            {
                // All members slaves are freed when we will the master
                snprintf(cmd, CF_BUFSIZE, "%s link delete dev %s", CF_DEBIAN_IP_COMM, promiser);

                if ((ExecCommand(cmd, result, pp) != 0))
                {
                    Log(LOG_LEVEL_VERBOSE, "Aggregate interface %s could not be removed", promiser);
                    *result = PROMISE_RESULT_FAIL;
                }
            }
            return;
        }
    }

    if (got_children == all_children)
    {
        Log(LOG_LEVEL_VERBOSE, "All children accounted for on aggregate interface %s", promiser);

        CheckSetBondMode(promiser, a->interface.bonding, result, pp);
        return;
    }

    if (!got_master && got_children)
    {
        // If master/slave not set, bring down children in current config

        for (rp = a->interface.bond_interfaces; rp != NULL; rp=rp->next)
        {
            snprintf(cmd, CF_BUFSIZE, "%s link set %s down", CF_DEBIAN_IP_COMM, (char *)rp->val.item);

            if ((ExecCommand(cmd, result, pp) != 0))
            {
                Log(LOG_LEVEL_INFO, "Bond interface child %s for 'interfaces' promise %s is busy", (char *)rp->val.item, promiser);
                *result = PROMISE_RESULT_FAIL;
            }

            snprintf(cmd, CF_BUFSIZE, "%s addr flush dev %s ", CF_DEBIAN_IP_COMM, (char *)rp->val.item);

            if ((ExecCommand(cmd, result, pp) != 0))
            {
                Log(LOG_LEVEL_INFO, "Bond interface child %s for 'interfaces' promise %s will not reset", (char *)rp->val.item, promiser);
                *result = PROMISE_RESULT_FAIL;
            }
        }
    }

    if (!got_master)
    {
        snprintf(cmd, CF_BUFSIZE, "%s link add %s type bond", CF_DEBIAN_IP_COMM, promiser);

        if ((ExecCommand(cmd, result, pp) != 0))
        {
            Log(LOG_LEVEL_INFO, "Bond interface master %s promise failed", promiser);
            *result = PROMISE_RESULT_FAIL;
            return;
        }
    }

    if (CheckSetBondMode(promiser, a->interface.bonding, result, pp))
    {
        return;
    }

    // Re-add children/slave/subordinates

    for (rp = a->interface.bond_interfaces; rp != NULL; rp=rp->next)
    {
        snprintf(cmd, CF_BUFSIZE, "%s link set %s master %s", CF_DEBIAN_IP_COMM, (char *)rp->val.item, promiser);
        printf("Try to bond slave: %s\n", cmd);
        if ((ExecCommand(cmd, result, pp) != 0))
        {
            Log(LOG_LEVEL_INFO, "Bond interface child %s for 'interfaces' promise %s failed", (char *)rp->val.item, promiser);
            *result = PROMISE_RESULT_FAIL;
        }
    }

    // Interface comes up later
}

/****************************************************************************/

static void AssessDeviceAlias(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)
{
    char cmd[CF_BUFSIZE], interface[CF_SMALLBUF];
    int vlan_id;
    LinkState *lsp;

    vlan_id = atoi((char *)a->interface.untagged_vlan);

    if (vlan_id == 0)
    {
        char vlan_lookup[CF_MAXVARSIZE];
        DataType type = CF_DATA_TYPE_NONE;

        // Non-numeric alias (like JunOS) have to be looked up in VLAN_ALIASES[]
        snprintf(vlan_lookup, CF_MAXVARSIZE, "VLAN_ALIASES[%s]", (char *)a->interface.untagged_vlan);

        VarRef *ref = VarRefParse(vlan_lookup);
        const void *value = EvalContextVariableGet(ctx, ref, &type);
        VarRefDestroy(ref);

        if (DataTypeToRvalType(type) == RVAL_TYPE_SCALAR)
        {
            vlan_id = atoi((char *)value);
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Variable %s is not defined in `interfaces' promise", vlan_lookup);
            PromiseRef(LOG_LEVEL_ERR, pp);
            *result = PROMISE_RESULT_INTERRUPTED;
            return;
        }
    }

    snprintf(interface, CF_SMALLBUF, "%s:%d", promiser, vlan_id);

    for (lsp = ifs; lsp != NULL; lsp = lsp->next)
    {
        if (strcmp(lsp->name, interface) == 0)
        {
            return;
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Did not find untagged VLAN %d on %s (i.e. virtual device %s)", vlan_id, promiser, interface);

    snprintf(cmd, CF_BUFSIZE, "%s link set dev %s up", CF_DEBIAN_IP_COMM, interface);

    if ((ExecCommand(cmd, result, pp) != 0))
    {
        Log(LOG_LEVEL_INFO, "Untagged VLAN %d/interface alias on %s for 'interfaces' promise failed", vlan_id, promiser);
        return;
    }

    InterfaceUp(interface, result, pp);

}

/****************************************************************************/

static Rlist *IPV4Addresses(LinkState *ifs, char *interface)
{
    LinkState *lsp;

    for (lsp = ifs; lsp != NULL; lsp = lsp->next)
    {
        if (strcmp(lsp->name, interface) == 0)
        {
            return lsp->v4_addresses;
        }
    }

    return NULL;
}

/****************************************************************************/

static Rlist *IPV6Addresses(LinkState *ifs, char *interface)
{
    LinkState *lsp;

    for (lsp = ifs; lsp != NULL; lsp = lsp->next)
    {
        if (strcmp(lsp->name, interface) == 0)
        {
            return lsp->v6_addresses;
        }
    }

    return NULL;
}

/****************************************************************************/
/* EXEC                                                                     */
/****************************************************************************/

int ExecCommand(char *cmd, PromiseResult *result, const Promise *pp)
{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;

    if (DONTDO)
    {
        Log(LOG_LEVEL_VERBOSE, "Need to execute command '%s' for net/interface configuration", cmd);
        return true;
    }

    if ((pfp = cf_popen(cmd, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", cmd);
        PromiseRef(LOG_LEVEL_ERR, pp);
        *result = PROMISE_RESULT_INTERRUPTED;
        return false;
    }

    char *line = xmalloc(line_size);

    while (!feof(pfp))
    {
        *line = '\0';
        CfReadLine(&line, &line_size, pfp);

        if (feof(pfp))
        {
            break;
        }

        if (strncmp("ERROR:", line, 6) == 0 || strstr(line, "error") == 0)
        {
            Log(LOG_LEVEL_ERR, "Interface returned an error: %s", line);
            *result = PROMISE_RESULT_FAIL;
            break;
        }
        else
        {
            *result = PROMISE_RESULT_CHANGE;
        }
    }

    free(line);
    cf_pclose(pfp);
    return true;
}

/****************************************************************************/

void WriteNativeInterfacesFile()
{
    Item *reverse_output = NULL, *output;
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    char comm[CF_BUFSIZE];
    char *line = xmalloc(line_size);

    if (!INTERFACE_WANTS_NATIVE)
    {
        return;
    }

    //ifquery --running -a > /etc/network/interfaces

    snprintf(comm, CF_BUFSIZE, "%s --running -a", CF_DEBIAN_IFQUERY);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", CF_DEBIAN_IFQUERY);
        free(line);
        return;
    }

    while (!feof(pfp))
    {
        CfReadLine(&line, &line_size, pfp);

        if (feof(pfp))
        {
            break;
        }

        PrependItem(&reverse_output, line, NULL);
    }

    output = ReverseItemList(reverse_output);

    Attributes a = { {0} };
    a.edits.backup = BACKUP_OPTION_TIMESTAMP;

    SaveItemListAsFile(output, CF_DEBIAN_IFCONF, a, NewLineMode_Unix);
    free(line);
}

/****************************************************************************/
/* INFO                                                                     */
/****************************************************************************/

static int GetVlanInfo(Item **list, const Promise *pp, const char *interface)
{
    FILE *fp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char ifname[CF_SMALLBUF];
    char ifparent[CF_SMALLBUF];

// kernel: modprobe 8021q must be done before running this

/*
  host$ sudo more /proc/net/vlan/config
  VLAN Dev name    | VLAN ID
  Name-Type: VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD
  eth0.34        | 34  | eth0
  eth0.35        | 35  | eth0
  eth0.36        | 36  | eth0
  eth0.37        | 37  | eth0
*/

    if ((fp = safe_fopen(CF_DEBIAN_VLAN_FILE, "r")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to open '%s' - try modprobe 8021q to install driver", CF_DEBIAN_VLAN_FILE);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    // Skip two headers
    CfReadLine(&line, &line_size, fp);
    CfReadLine(&line, &line_size, fp);

    while (!feof(fp))
    {
        int id = CF_NOINT;
        CfReadLine(&line, &line_size, fp);
        sscanf(line, "%s | %d | %s", ifname, &id, ifparent);

        if (strcmp(ifparent, interface) == 0)
        {
            PrependFullItem(list, ifname, NULL, id, 0);
        }
    }

    fclose(fp);
    free(line);
    return true;
}

/****************************************************************************/

static int GetInterfaceInformation(LinkState **list, const Promise *pp, const Rlist *filter)
{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char indent[CF_SMALLBUF];
    char hw_addr[CF_MAX_IP_LEN];
    char v4_addr[CF_MAX_IP_LEN];
    char v6_addr[CF_MAX_IP_LEN];
    char if_name[CF_SMALLBUF];
    char endline[CF_BUFSIZE];
    char comm[CF_BUFSIZE];
    char *sp;
    int mtu = CF_NOINT;
    LinkState *entry = NULL;

    snprintf(comm, CF_BUFSIZE, "%s addr", CF_DEBIAN_IP_COMM);

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

        if (isdigit(*line))
        {
            sscanf(line, "%*d: %32[^@ ]", if_name);

            if (if_name[strlen(if_name)-1] == ':')
            {
                if_name[strlen(if_name)-1] = '\0';
            }

            // Only keep interfaces in filter or promiser to avoid unnecessary iteration

            if (filter && (strcmp(if_name, pp->promiser) != 0))
            {
                if (!RlistIsInListOfRegex(filter, if_name))
                {
                    continue;
                }
            }

            entry = xcalloc(sizeof(LinkState), 1);
            entry->next = *list;
            *list = entry;
            entry->name = xstrdup(if_name);

            sscanf(line, "%*[^>]> mtu %d %[^\n]", &mtu, endline);

            if (strstr(endline, "state UP")) // The intended link state is in <..>, the actual is after "state"
            {
                entry->up = true;
            }
            else
            {
                entry->up = false;
            }

            entry->mtu = mtu;

            if (strstr(line, "SLAVE"))
            {
                if ((sp = strstr(endline, "master"))) // The interface seems to be subordinate to an aggregate
                {
                    char parent_interface[CF_SMALLBUF];

                    parent_interface[0] = '\0';
                    sscanf(sp, "master %s", parent_interface);

                    if (parent_interface[0] != '\0')
                    {
                        entry->parent = strdup(parent_interface);
                    }
                }
            }
            else if (strstr(line, "MASTER"))
            {
                entry->is_parent = true;
            }
        }
        else if (entry != NULL)
        {
            *indent = '\0';
            *hw_addr = '\0';
            *v4_addr = '\0';
            *v6_addr = '\0';

            sscanf(line, "%31s", indent);

            if (strncmp(indent, "inet6", 5) == 0)
            {
                sscanf(line, "%*s %64s", v6_addr);
                RlistPrepend(&(entry->v6_addresses), v6_addr, RVAL_TYPE_SCALAR);
            }
            else if (strncmp(indent, "inet", 4) == 0)
            {
                sscanf(line, "%*s %64s", v4_addr);
                RlistPrepend(&(entry->v4_addresses), v4_addr, RVAL_TYPE_SCALAR);
            }
            else if (strncmp(indent, "link", 4) == 0)
            {
                sscanf(line, "%*s %64s", hw_addr);
                entry->hw_address = xstrdup(hw_addr);
            }
        }
    }

    free(line);
    cf_pclose(pfp);
    return true;
}

/****************************************************************************/

static void GetInterfaceOptions(Promise *pp, int *full, int *speed, int *autoneg)
{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char comm[CF_BUFSIZE];
    char *sp;

    snprintf(comm, CF_BUFSIZE, "%s %s", CF_DEBIAN_ETHTOOL, pp->promiser);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", CF_DEBIAN_ETHTOOL);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return;
    }

    while (!feof(pfp))
    {
        CfReadLine(&line, &line_size, pfp);

        if (feof(pfp))
        {
            break;
        }

        /*Speed: 1000Mb/s
          Duplex: Full
          Speed: Unknown!
          Duplex: Unknown! (255)
          Auto-negotiation: on
        */

        if ((sp = strstr(line, "Speed:")))
        {
            sscanf(sp, "%d", speed);
        }

        if ((sp = strstr(line, "Auto-negotiation:")))
        {
            char result[CF_SMALLBUF] = { 0 };

            sscanf(sp, "%s", result);
            if (strcmp(result, "on") == 0)
            {
                *autoneg = true;
            }
        }

        if ((sp = strstr(line, "Duplex:")))
        {
            char result[CF_SMALLBUF] = { 0 };

            sscanf(sp, "%s", result);
            if (strcmp(result, "full") == 0)
            {
                *full = true;
            }
        }
    }

    free(line);
    cf_pclose(pfp);
    return;
}

/****************************************************************************/

static int GetBridgeInfo(Bridges **list, const Promise *pp)
{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    char comm[CF_BUFSIZE];
    char *line = xmalloc(line_size);
    char name[CF_SMALLBUF], bridge_id[CF_SMALLBUF], stp[CF_SMALLBUF], interface[CF_SMALLBUF];
    Bridges *entry = NULL;
    int n;

    snprintf(comm, CF_BUFSIZE, "%s show", CF_DEBIAN_BRCTL);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", CF_DEBIAN_BRCTL);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    // Skip header
    CfReadLine(&line, &line_size, pfp);

    while (!feof(pfp))
    {
        *line = '\0';

        CfReadLine(&line, &line_size, pfp);

        if (feof(pfp))
        {
            break;
        }

        switch (n = sscanf(line, "%s %s %s %s", name, bridge_id, stp, interface))
        {
        case 4: // new bridge line

            entry = xcalloc(sizeof(LinkState), 1);
            entry->next = *list;
            *list = entry;
            entry->name = xstrdup(name);
            entry->id = xstrdup(bridge_id);
            entry->interfaces = NULL;
            PrependItem(&(entry->interfaces), interface, NULL);

            if (strcmp(stp, "yes") == 0)
            {
                entry->stp = true;
            }

            break;

        case 1: // continuation line

            if (entry)
            {
                PrependItem(&(entry->interfaces), name, NULL);
            }

            break;

        default:
            // Shouldn't happen
            break;
        }

    }

    free(line);
    cf_pclose(pfp);
    return true;
}

/****************************************************************************/

static void DeleteInterfaceInfo(LinkState *interfaces)
{
    LinkState *lp, *next;

    for (lp = interfaces; lp != NULL; lp = next)
    {
        free(lp->name);
        RlistDestroy(lp->v4_addresses);
        RlistDestroy(lp->v6_addresses);
        free(lp->hw_address);
        next = lp->next;
        free((char *)lp);
    }
}

/****************************************************************************/

static void DeleteBridgeInfo(Bridges *bridges)
{
    Bridges *bp, *next;

    for (bp = bridges; bp != NULL; bp = next)
    {
        free(bp->name);
        free(bp->id);
        DeleteItemList(bp->interfaces);
        next = bp->next;
        free((char *)bp);
    }
}

/****************************************************************************/

bool IPAddressInList(Rlist *cidr1, char *cidr2)
{
    Rlist *rp;

    for (rp = cidr1; rp != NULL; rp=rp->next)
    {
        if (CompareCIDR(rp->val.item, cidr2))
        {
            return true;
        }
    }

    return false;
}

/****************************************************************************/

static int CheckSetBondMode(char *promiser, int bmode, PromiseResult *result, const Promise *pp)
{
    FILE *fp;
    int mode_ok = false;
    char cmd[CF_BUFSIZE];

    snprintf(cmd, CF_BUFSIZE, "/sys/class/net/%s/bonding/mode", promiser);

    // Converge("cat mode > /sys/class/net/bond0/bonding/mode")

    if ((fp = safe_fopen(cmd, "r")) != NULL)
    {
        size_t line_size = CF_SMALLBUF;
        char *line = xmalloc(line_size), mode[CF_SMALLBUF];
        snprintf(mode, CF_SMALLBUF, "%u", bmode);
        CfReadLine(&line, &line_size, fp);
        if (strstr(line, mode))
        {
            mode_ok = true;
        }
        free(line);
        fclose(fp);
    }

    if (!mode_ok)
    {
        if (DONTDO)
        {
            Log(LOG_LEVEL_INFO, "Need to set bonding mode %d for interface %s", bmode, promiser);
        }
        else
        {
            Log(LOG_LEVEL_INFO, "Trying to set bonding mode %d for interface %s", bmode, promiser);
            InterfaceDown(promiser, result, pp);

            if ((fp = safe_fopen(cmd, "w")) != NULL)
            {
                fprintf(fp, "%d", bmode);
                fclose(fp);
            }
            else
            {
                Log(LOG_LEVEL_INFO, "Failed to set mode %d for %s promise failed", bmode, promiser);
                *result = PROMISE_RESULT_FAIL;
                return false;
            }
        }
    }

    return true;
}

/****************************************************************************/

static int InterfaceDown(char *interface, PromiseResult *result, const Promise *pp)
{
    char cmd[CF_BUFSIZE];

    Log(LOG_LEVEL_VERBOSE, "Bringing interface %s down", interface);
    snprintf(cmd, CF_BUFSIZE, "%s link set down dev %s", CF_DEBIAN_IP_COMM, interface);

    if ((ExecCommand(cmd, result, pp) != 0))
    {
        Log(LOG_LEVEL_VERBOSE, "Interface %s could not be shut down", interface);
        *result = PROMISE_RESULT_FAIL;
        return false;
    }

    return true;
}

/****************************************************************************/

static int InterfaceUp(char *interface, PromiseResult *result, const Promise *pp)
{
    char cmd[CF_BUFSIZE];

    Log(LOG_LEVEL_VERBOSE, "Bringing interface %s up", interface);

    snprintf(cmd, CF_BUFSIZE, "%s link set dev %s up", CF_DEBIAN_IP_COMM, interface);

    if (!ExecCommand(cmd, result, pp))
    {
        *result = PROMISE_RESULT_FAIL;
        return false;
    }

    return true;
}
