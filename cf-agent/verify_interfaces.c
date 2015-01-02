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

#include <verify_interfaces.h>
#include <attributes.h>
#include <eval_context.h>
#include <ornaments.h>
#include <locks.h>
#include <promises.h>
#include <string_lib.h>
#include <regex.h>
#include <misc_lib.h>
#include <files_lib.h>
#include <files_interfaces.h>
#include <files_names.h>
#include <pipes.h>
#include <item_lib.h>
#include <expand.h>
#include <routing_services.h>
#include <files_operators.h>
#include <communication.h>
#include <ip_address.h>

#define CF_LINUX_IFCONF "/etc/network/interfaces"
#define CF_LINUX_IP_COMM "/sbin/ip"
#define CF_LINUX_BRCTL "/usr/sbin/brctl"
#define CF_LINUX_IFQUERY "/usr/sbin/ifquery"
#define CF_LINUX_IFUP "/usr/sbin/ifup"
#define CF_LINUX_ETHTOOL "/usr/bin/ethtool"

bool INTERFACE_WANTS_NATIVE = false;

static int InterfaceSanityCheck(EvalContext *ctx, Attributes a,  const Promise *pp);
static void AssessInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static void AssessLinuxInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static void AssessVxLAN(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp);
static void AssessLinuxTaggedVlan(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp);
static int GetVlanInfo(Item **list, LinkState *ifs, const char *interface);
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
static void GetInterfaceOptions(const Promise *pp, int *full, int *speed, int *autoneg);
static int CheckSetBondMode(char *promiser, int mode, PromiseResult *result, const Promise *pp);
static int InterfaceDown(char *interface, PromiseResult *result, const Promise *pp);
static int InterfaceUp(char *interface, PromiseResult *result, const Promise *pp);
static int VlanUp(char *interface, int id, PromiseResult *result, const Promise *pp);
static int DeleteInterface(char *interface, PromiseResult *result, const Promise *pp);
static LinkState *MatchInterface(LinkState *fsp, char *interface);
static int GetVxlan(char *promiser, int *vni, char *dev, char *loopback, char *multicast);
static void TunnelAlienRegistration(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static void BridgeAlien(char *loopback, char *mac,int vni, PromiseResult *result, const Promise *pp);

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

    PromiseBanner(ctx, pp);

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

        if (ip1 == NULL || ip1->type != IP_ADDRESS_TYPE_IPV4)
        {
            Log(LOG_LEVEL_ERR, "Interface '%s' has improper IPv4 address (CIDR format expected)", pp->promiser);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }

        IPAddressDestroy(&ip1);
    }

    for (Rlist *rp = a.interface.v6_addresses; rp != NULL; rp=rp->next)
    {
        char test[CF_MAXVARSIZE];

        if (strlen(rp->val.item) > CF_MAX_IP_LEN)
        {
            Log(LOG_LEVEL_ERR, "Interface '%s' has improper IPv6 address %s (CIDR format expected) (len = %d/%d)", pp->promiser, (char *)rp->val.item,(int)strlen(rp->val.item),CF_MAX_IP_LEN);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }

        strcpy(test, rp->val.item);
        ToLowerStrInplace(test);

        Buffer *buf = BufferNewFrom(test, strlen(test));
        IPAddress *ip1 = IPAddressNew(buf);
        BufferDestroy(buf);

        if (ip1 == NULL ||  ip1->type != IP_ADDRESS_TYPE_IPV6)
        {
            Log(LOG_LEVEL_ERR, "Interface '%s' has improper IPv6 address %s (CIDR format expected)", pp->promiser, (char *)rp->val.item);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }

        IPAddressDestroy(&ip1);
    }

    if (a.interface.bgp_ttl_security != CF_NOINT ||
        a.interface.bgp_advert_interval != CF_NOINT ||
        a.interface.bgp_next_hop_self ||
        a.interface.bgp_families ||
        a.interface.bgp_maximum_paths != CF_NOINT ||
        a.interface.bgp_ipv6_neighbor_discovery_route_advertisement
        )
    {
        if (a.interface.bgp_neighbour == NULL || a.interface.bgp_remote_as < 1)
        {
            Log(LOG_LEVEL_ERR, "Interface '%s' seems to make a BGP promise, but has no neighbouring AS sessions", pp->promiser);
            PromiseRef(LOG_LEVEL_ERR, pp);
        }
    }

    if (ROUTING_POLICY != NULL) // This is set by body routing_services control
    {
        if (ROUTING_POLICY->bgp_local_as != a.interface.bgp_remote_as)
        {
            // This is ebgp
            if (a.interface.bgp_reflector)
            {
                Log(LOG_LEVEL_ERR, "Interface '%s', route reflectors only apply to iBGP", pp->promiser);
                PromiseRef(LOG_LEVEL_ERR, pp);
            }
        }
    }

    return true;
}

/****************************************************************************/

void AssessInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    if (IsDefinedClass(ctx,"linux"))
    {
        AssessLinuxInterfacePromise(promiser, result, ctx, a, pp);
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

static void AssessLinuxInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    LinkState *netinterfaces = NULL;
    Rlist *filter = NULL;

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
        InterfaceDown(promiser, result, pp);
        DeleteInterface(promiser, result, pp);
        DeleteInterfaceInfo(netinterfaces);
        return;
    }

    if (a->interface.state && strcmp(a->interface.state, "down") == 0)
    {
        InterfaceDown(promiser, result, pp);
        DeleteInterfaceInfo(netinterfaces);
        return;
    }

    if (a->havetunnel)
    {
        AssessVxLAN(promiser, result, ctx, netinterfaces, a, pp);
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
        AssessLinuxTaggedVlan(promiser, result, ctx, netinterfaces, a, pp);
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
        if (a->interface.ospf_area != CF_NOINT)
        {
            KeepOSPFInterfacePromises(ctx, a, pp, result, ospfp);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "No ospf promise by this interface");
        }
    }

    free(ospfp);

    LinkStateBGP *bgpp = xcalloc(sizeof(LinkStateBGP), 1);

    if (a->havelinkservices && QueryBGPInterfaceState(ctx, a, pp, bgpp))
    {
        if (a->interface.bgp_remote_as)
        {
            KeepBGPInterfacePromises(ctx, a, pp, result, bgpp);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "No bgp promise by this interface");
        }
    }

    free(bgpp);
}

/****************************************************************************/

static void AssessVxLAN(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)
{
    LinkState *lsp;
    int vni = CF_NOINT;
    char dev[CF_SMALLBUF];
    char loopback[CF_MAX_IP_LEN] = {0};
    char multicast[CF_MAX_IP_LEN] = {0};
    bool match = false;
    char comm[CF_BUFSIZE], opt[CF_MAXVARSIZE];

    if ((lsp = MatchInterface(ifs, promiser)) != NULL)
    {
        if (a->interface.delete)
        {
            *result = PROMISE_RESULT_CHANGE;
            DeleteInterface(promiser, result, pp);
            return;
        }

        if (!GetVxlan(promiser, &vni, dev, loopback, multicast))
        {
            *result = PROMISE_RESULT_FAIL;
            return;
        }

        match = true;
    }

    if (a->interface.state && strcmp(a->interface.state, "down") == 0 && lsp->up)
    {
        InterfaceDown(promiser,result,pp);
        *result = PROMISE_RESULT_CHANGE;
        return;
    }

    if (a->interface.tunnel_multicast_group && strcmp(loopback, a->interface.tunnel_multicast_group) != 0)
    {
        match = false;
    }
    else if (a->interface.tunnel_loopback && strcmp(loopback, a->interface.tunnel_loopback) != 0)
    {
        match = false;
    }
    else if (a->interface.tunnel_interface && strcmp(dev, a->interface.tunnel_interface) != 0)
    {
        match = false;
    }
    else if (vni != a->interface.tunnel_id)
    {
        match = false;
    }

    if (match)
    {
        return;
    }

    if (lsp != NULL)
    {
        // Need to start again to fix settings
        InterfaceDown(promiser, result, pp);
        DeleteInterface(promiser, result, pp);
    }

    snprintf(comm, CF_BUFSIZE, "%s link add %s type vxlan id %d", CF_LINUX_IP_COMM, promiser, a->interface.tunnel_id);

    if (a->interface.tunnel_multicast_group)
    {
        snprintf(opt, CF_MAXVARSIZE, " group %s", a->interface.tunnel_multicast_group);
        if (!JoinComm(comm, opt, CF_BUFSIZE))
        {
            *result = PROMISE_RESULT_FAIL;
            return;
        }
    }

    if (a->interface.tunnel_loopback)
    {
        snprintf(opt, CF_MAXVARSIZE, " local %s", a->interface.tunnel_loopback);
        if (!JoinComm(comm, opt, CF_BUFSIZE))
        {
            *result = PROMISE_RESULT_FAIL;
            return;
        }
    }

    if (a->interface.tunnel_interface)
    {
        snprintf(opt, CF_MAXVARSIZE, " dev %s", a->interface.tunnel_interface);
        if (!JoinComm(comm, opt, CF_BUFSIZE))
        {
            *result = PROMISE_RESULT_FAIL;
            return;
        }
    }

    if (!ExecCommand(comm, result, pp))
    {
        *result = PROMISE_RESULT_FAIL;
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Established VTEP %s for VNI %d", promiser, a->interface.tunnel_id);
    *result = PROMISE_RESULT_CHANGE;

    if (a->interface.tunnel_alien_arp)
    {
        TunnelAlienRegistration(promiser, result, ctx, a, pp);
    }
}

/****************************************************************************/

static void TunnelAlienRegistration(ARG_UNUSED char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    VarRef *ref = VarRefParse(a->interface.tunnel_alien_arp);
    VariableTableIterator *iter = EvalContextVariableTableIteratorNew(ctx, ref->ns, ref->scope, ref->lval);
    Variable *var = NULL;
    char regex[CF_MAXVARSIZE];
    snprintf(regex, CF_MAXVARSIZE, "^(?!%s$).*", a->interface.tunnel_loopback);

    // Based on getvalues() function

    while ((var = VariableTableIteratorNext(iter)))
    {
        if (var->ref->num_indices != 1)
        {
            continue;
        }

        if (!StringMatchFull(regex, var->ref->indices[ref->num_indices]))
        {
            continue;
        }

        switch (var->rval.type)
        {
        case RVAL_TYPE_SCALAR:

            BridgeAlien(var->ref->indices[ref->num_indices], var->rval.item, a->interface.tunnel_id,result, pp);
            break;

        case RVAL_TYPE_LIST:
            for (const Rlist *rp = var->rval.item; rp != NULL; rp = rp->next)
            {
                BridgeAlien(var->ref->indices[ref->num_indices], RlistScalarValue(rp), a->interface.tunnel_id,result, pp);
            }
            break;

        default:
            break;
        }
    }

    VariableTableIteratorDestroy(iter);
    VarRefDestroy(ref);
}

/****************************************************************************/

static void AssessLinuxTaggedVlan(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)
{
    Rlist *rp;
    int vlan_id = 0, found;
    Item *ip, *vlans = NULL;

    // Look through the labelled VLANs to see if they are on this interface

    if (!GetVlanInfo(&vlans, ifs, promiser))
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
                ip->counter = CF_NOINT;
                found = true;
            }

            if (found && (ip->time == false)) // up status in "time" field
            {
                VlanUp(promiser, vlan_id, result, pp);
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

        *result = PROMISE_RESULT_CHANGE;
        VlanUp(promiser, vlan_id, result, pp);
    }

    // Anything remaining needs to be removed

    for (ip = vlans; ip != NULL; ip=ip->next)
    {
        if (ip->counter != CF_NOINT)
        {
            *result = PROMISE_RESULT_CHANGE;
            DeleteTaggedVLAN(vlan_id, promiser, result, ctx, a, pp);
        }
    }

    DeleteItemList(vlans);
}

/****************************************************************************/

static int NewTaggedVLAN(int vlan_id, char *promiser, PromiseResult *result, ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Attributes *a, const Promise *pp)
{
    char cmd[CF_BUFSIZE];
    int ret;

    snprintf(cmd, CF_BUFSIZE, "%s link add link %s name %s.%d type vlan id %d", CF_LINUX_IP_COMM, pp->promiser, pp->promiser, vlan_id, vlan_id);

    if ((ret = ExecCommand(cmd, result, pp)))
    {
        Log(LOG_LEVEL_VERBOSE, "Tagging VLAN %d on %s (i.e. virtual device %s.%d)", vlan_id, promiser, promiser, vlan_id);
    }

    return ret;
}

/****************************************************************************/

static int DeleteTaggedVLAN(int vlan_id, char *promiser, PromiseResult *result, ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Attributes *a, const Promise *pp)
{
    char cmd[CF_BUFSIZE];
    int ret;

    Log(LOG_LEVEL_VERBOSE, "Attempting to remove VLAN %d on %s (i.e. virtual device %s.%d)", vlan_id, promiser, promiser, vlan_id);

    snprintf(cmd, CF_BUFSIZE, "%s link delete %s.%d", CF_LINUX_IP_COMM, promiser, vlan_id);

    if ((ret = ExecCommand(cmd, result, pp)))
    {
        Log(LOG_LEVEL_INFO, "Purged VLAN %d on %s for 'interfaces' promise", vlan_id, promiser);
    }

    return ret;
}

/****************************************************************************/

static void AssessIPv4Config(char *promiser, PromiseResult *result, ARG_UNUSED EvalContext *ctx,  Rlist *addresses, const Attributes *a, const Promise *pp)
{
    Rlist *rp, *rpa;
    char cmd[CF_BUFSIZE];

    for (rp = a->interface.v4_addresses; rp != NULL; rp = rp->next)
    {
        if (!IPAddressInList(addresses, rp->val.item))
        {
            *result = PROMISE_RESULT_CHANGE;

            // Broadcast is now assumed "ones"

            snprintf(cmd, CF_BUFSIZE, "%s addr add %s broadcast + dev %s", CF_LINUX_IP_COMM, (char *)rp->val.item, promiser);

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
                snprintf(cmd, CF_BUFSIZE, "%s addr del %s dev %s", CF_LINUX_IP_COMM, (char *)rpa->val.item, promiser);

                if (!ExecCommand(cmd, result, pp))
                {
                    *result = PROMISE_RESULT_FAIL;
                }
            }
        }
    }
}

/****************************************************************************/

static void AssessIPv6Config(char *promiser, PromiseResult *result, ARG_UNUSED EvalContext *ctx, Rlist *addresses, const Attributes *a, const Promise *pp)
{
    Rlist *rp, *rpa;
    char cmd[CF_BUFSIZE];

    for (rp = a->interface.v6_addresses; rp != NULL; rp = rp->next)
    {
        if (!IPAddressInList(addresses, rp->val.item))
        {
            *result = PROMISE_RESULT_CHANGE;

            snprintf(cmd, CF_BUFSIZE, "%s -6 addr add %s dev %s", CF_LINUX_IP_COMM, (char *)rp->val.item, promiser);

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
                snprintf(cmd, CF_BUFSIZE, "%s -6 addr del %s dev %s", CF_LINUX_IP_COMM, (char *)rpa->val.item, promiser);

                if (!ExecCommand(cmd, result, pp))
                {
                    *result = PROMISE_RESULT_FAIL;
                }
            }
        }
    }
}

/****************************************************************************/

static void CheckInterfaceOptions(char *promiser, PromiseResult *result, ARG_UNUSED EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)

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
            snprintf(comm, CF_BUFSIZE, "%s link set dev %s mtu %d", CF_LINUX_IP_COMM, promiser, lsp->mtu);
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
            snprintf(comm, CF_BUFSIZE, "%s %s autoneg on", CF_LINUX_ETHTOOL, promiser);
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
                     CF_LINUX_ETHTOOL, promiser, a->interface.speed, a->interface.duplex);
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

static void AssessBridge(char *promiser, PromiseResult *result, ARG_UNUSED EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)
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
                    snprintf(comm, CF_BUFSIZE, "%s link set up dev %s", CF_LINUX_IP_COMM, lsp->name);
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
            snprintf(comm, CF_BUFSIZE, "%s link set up dev %s", CF_LINUX_IP_COMM,  (char *)(rp->val.item));
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

        snprintf(comm, CF_BUFSIZE, "%s addbr %s", CF_LINUX_BRCTL, promiser);

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

        snprintf(comm, CF_BUFSIZE, "%s link set down dev %s", CF_LINUX_IP_COMM, promiser);

        if ((ExecCommand(comm, result, pp) != 0))
        {
            Log(LOG_LEVEL_VERBOSE, "Parent bridge interface %s could not be shutdown", promiser);
            *result = PROMISE_RESULT_FAIL;
            return;
        }

        snprintf(comm, CF_BUFSIZE, "%s delbr %s", CF_LINUX_BRCTL, promiser);

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
        snprintf(comm, CF_BUFSIZE, "%s addif %s %s", CF_LINUX_BRCTL, promiser, (char *)rp->val.item);

        if ((ExecCommand(comm, result, pp) != 0))
        {
            Log(LOG_LEVEL_VERBOSE, "Member for bridge %s could not be added", promiser);
            *result = PROMISE_RESULT_FAIL;
            return;
        }
    }

    /* will be replaced with
       snprintf(cmd, CF_BUFSIZE, "%s link add name %s type bridge", CF_LINUX_IP_COMM, promiser);
       snprintf(cmd, CF_BUFSIZE, "%s link set dev %s master", CF_LINUX_IP_COMM, (char *)rp->val.item, promiser);
    */

    DeleteBridgeInfo(bridges);
}

/****************************************************************************/

static int CheckBridgeNative(char *promiser, PromiseResult *result, const Promise *pp)
{
    char comm[CF_BUFSIZE];

    snprintf(comm, CF_BUFSIZE, "%s --check %s --with-depends", CF_LINUX_IFQUERY, promiser);

    if ((ExecCommand(comm, result, pp) == 0))
    {
        // Promise ostensibly kept
        return true;
    }

    snprintf(comm, CF_BUFSIZE, "%s %s --with-depends", CF_LINUX_IFUP, promiser);

    if ((ExecCommand(comm, result, pp) == 0))
    {
        return true;
    }

    Log(LOG_LEVEL_INFO, "Bridge interfaces missing in %s", promiser);
    *result = PROMISE_RESULT_FAIL;
    return false;
}

/****************************************************************************/

static void AssessLACPBond(char *promiser, PromiseResult *result, ARG_UNUSED EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)

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
            snprintf(cmd, CF_BUFSIZE, "%s link set %s nomaster", CF_LINUX_IP_COMM, lsp->name);

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
                snprintf(cmd, CF_BUFSIZE, "%s link delete dev %s", CF_LINUX_IP_COMM, promiser);

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
            snprintf(cmd, CF_BUFSIZE, "%s link set %s down", CF_LINUX_IP_COMM, (char *)rp->val.item);

            if ((ExecCommand(cmd, result, pp) != 0))
            {
                Log(LOG_LEVEL_INFO, "Bond interface child %s for 'interfaces' promise %s is busy", (char *)rp->val.item, promiser);
                *result = PROMISE_RESULT_FAIL;
            }

            snprintf(cmd, CF_BUFSIZE, "%s addr flush dev %s ", CF_LINUX_IP_COMM, (char *)rp->val.item);

            if ((ExecCommand(cmd, result, pp) != 0))
            {
                Log(LOG_LEVEL_INFO, "Bond interface child %s for 'interfaces' promise %s will not reset", (char *)rp->val.item, promiser);
                *result = PROMISE_RESULT_FAIL;
            }
        }
    }

    if (!got_master)
    {
        snprintf(cmd, CF_BUFSIZE, "%s link add %s type bond", CF_LINUX_IP_COMM, promiser);

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
        snprintf(cmd, CF_BUFSIZE, "%s link set %s master %s", CF_LINUX_IP_COMM, (char *)rp->val.item, promiser);
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

    snprintf(cmd, CF_BUFSIZE, "%s link set dev %s up", CF_LINUX_IP_COMM, interface);

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
            printf("CMD: %s\n", cmd);
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

    snprintf(comm, CF_BUFSIZE, "%s --running -a", CF_LINUX_IFQUERY);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", CF_LINUX_IFQUERY);
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

    SaveItemListAsFile(output, CF_LINUX_IFCONF, a, NewLineMode_Unix);
    free(line);
}

/****************************************************************************/
/* INFO                                                                     */
/****************************************************************************/

static int GetVlanInfo(Item **list, LinkState *ifs, const char *interface)
{
    LinkState *lsp;

    for (lsp = ifs; lsp != NULL; lsp=lsp->next)
    {
        if (strchr(lsp->name, '.'))
        {
            int id = CF_NOINT;
            char ifname[CF_SMALLBUF];

            sscanf(lsp->name, "%31[^.].%d", ifname, &id);

            if (strcmp(ifname, (char *)interface) == 0 && (id > 0))
            {
                PrependFullItem(list, ifname, NULL, id, lsp->up);
            }
        }
    }

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

    snprintf(comm, CF_BUFSIZE, "%s addr", CF_LINUX_IP_COMM);

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

static void GetInterfaceOptions(const Promise *pp, int *full, int *speed, int *autoneg)
{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char comm[CF_BUFSIZE];
    char *sp;

    snprintf(comm, CF_BUFSIZE, "%s %s", CF_LINUX_ETHTOOL, pp->promiser);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", CF_LINUX_ETHTOOL);
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

    snprintf(comm, CF_BUFSIZE, "%s show", CF_LINUX_BRCTL);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", CF_LINUX_BRCTL);
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
    snprintf(cmd, CF_BUFSIZE, "%s link set down dev %s", CF_LINUX_IP_COMM, interface);

    if ((ExecCommand(cmd, result, pp) != 0))
    {
        Log(LOG_LEVEL_VERBOSE, "Interface %s could not be shut down", interface);
        *result = PROMISE_RESULT_FAIL;
        return false;
    }

    return true;
}

/****************************************************************************/

static int VlanUp(char *interface, int id, PromiseResult *result, const Promise *pp)
{
    char cmd[CF_BUFSIZE];

    snprintf(cmd, CF_BUFSIZE, "%s.%d", interface, id);
    return InterfaceUp(cmd, result, pp);
}

/****************************************************************************/

static int InterfaceUp(char *interface, PromiseResult *result, const Promise *pp)
{
    char cmd[CF_BUFSIZE];

    Log(LOG_LEVEL_VERBOSE, "Bringing interface %s up", interface);

    snprintf(cmd, CF_BUFSIZE, "%s link set dev %s up", CF_LINUX_IP_COMM, interface);

    if (!ExecCommand(cmd, result, pp))
    {
        *result = PROMISE_RESULT_FAIL;
        return false;
    }

    return true;
}

/****************************************************************************/

static int DeleteInterface(char *interface, PromiseResult *result, const Promise *pp)
{
    char cmd[CF_BUFSIZE];

    Log(LOG_LEVEL_VERBOSE, "Deleting interface %s up", interface);

    snprintf(cmd, CF_BUFSIZE, "%s link delete dev %s", CF_LINUX_IP_COMM, interface);

    if ((ExecCommand(cmd, result, pp) != 0))
    {
        Log(LOG_LEVEL_VERBOSE, "Interface %s could not be removed", interface);
        *result = PROMISE_RESULT_FAIL;
        return false;
    }

    return true;
}

/****************************************************************************/

static LinkState *MatchInterface(LinkState *fsp, char *interface)
{
    LinkState *lsp;

    for (lsp = fsp; lsp != NULL; lsp=lsp->next)
    {
        if (strcmp(fsp->name, interface) == 0)
        {
            return lsp;
        }
    }

    return NULL;
}

/****************************************************************************/

static int GetVxlan(char *promiser, int *vni, char *dev, char *loopback, char *multicast)
{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    char comm[CF_BUFSIZE];
    char *sp, *line = xmalloc(line_size);
    bool got_result = false;

    snprintf(comm, CF_BUFSIZE, "%s -d link show dev %s", CF_LINUX_IP_COMM, promiser);

    //29: vtep2000: <BROADCAST,MULTICAST> mtu 1450 qdisc noop state DOWN mode DEFAULT
    //link/ether 96:07:57:22:d3:85 brd ff:ff:ff:ff:ff:ff
    //vxlan id 2000 group 239.0.0.2 local 172.2.2.2 dev eth0 port 32768 61000 ageing 300

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", CF_LINUX_IP_COMM);
        return false;
    }

    while (!feof(pfp))
    {
        CfReadLine(&line, &line_size, pfp);

        if (feof(pfp))
        {
            break;
        }

        if (strstr(line, "vxlan"))
        {
            got_result = true;
            break;
        }
    }

    fclose(pfp);

    if (!got_result)
    {
        return false;
    }

    if ((sp = strstr(line, "id")) != NULL)
    {
        sscanf(sp, "id %d", vni);
    }

    if ((sp = strstr(line, "local")) != NULL)
    {
        sscanf(sp, "local %s", loopback);
    }

    if ((sp = strstr(line, "group")) != NULL)
    {
        sscanf(sp, "group %s", multicast);
    }

    if ((sp = strstr(line, "dev")) != NULL)
    {
        sscanf(sp, "dev %s", dev);
    }

    free(line);
    return true;
}

/****************************************************************************/

bool JoinComm(char *s, char *ds, size_t size)
{
    if (strlen(s) + strlen(ds) < size)
    {
        strcat(s, ds);
        return true;
    }
    else
    {
        return false;
    }
}

/****************************************************************************/

static void BridgeAlien(char *loopback, char *mac,int vni, PromiseResult *result, const Promise *pp)
{
    struct stat sb;
    char comm[CF_BUFSIZE];

    if (stat("/bin/bridge", &sb) != -1)
    {
        snprintf(comm, CF_BUFSIZE, "/bin/bridge fdb add %s dev %s dst %s\n", mac, pp->promiser, loopback);
    }
    else if (stat("/usr/sbin/bridge", &sb) != -1)
    {
        snprintf(comm, CF_BUFSIZE, "/usr/sbin/bridge fdb add %s dev %s dst %s\n", mac, pp->promiser, loopback);
    }
    else
    {
        *result = PROMISE_RESULT_FAIL;
        return;
    }

    if (!ExecCommand(comm, result, pp))
    {
        *result = PROMISE_RESULT_FAIL;
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Bridge alien MAC address %s on VTEP %s for VNI %d", mac, loopback, vni);

    *result = PROMISE_RESULT_CHANGE;
    return;
}
