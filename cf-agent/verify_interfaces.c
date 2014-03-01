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

#define CF_DEBIAN_IFCONF "/etc/network/interfaces"
#define CF_DEBIAN_VLAN_FILE "/proc/net/vlan/config"
#define CF_DEBIAN_VLAN_COMMAND "/sbin/vconfig"
#define CF_DEBIAN_IP_COMM "/sbin/ip"
#define CF_DEBIAN_BRCTL "/usr/sbin/brctl"

static int InterfaceSanityCheck(EvalContext *ctx, Attributes a,  const Promise *pp);
static void AssessInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);

#ifdef OS_LINUX
static void AssessDebianInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static void AssessDebianTaggedVlan(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static int GetVlanInfo(Item **list, const Promise *pp);
static int GetInterfaceInformation(LinkState **list, const Promise *pp);
static int GetBridgeInfo(Bridges **list, const Promise *pp);
static int NewVLAN(int vlan_id, char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static int DeleteVLAN(int vlan_id, char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
int ExecCommand(char *cmd, PromiseResult *result, const Promise *pp);
static void AssessIPv4Config(char *promiser, PromiseResult *result, EvalContext *ctx, Rlist *addresses, const Attributes *a, const Promise *pp);
static void AssessIPv6Config(char *promiser, PromiseResult *result, EvalContext *ctx, Rlist *addresses, const Attributes *a, const Promise *pp);
static void AssessBridge(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp);
static void AssessLACPBond(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp);
static void AssessDeviceAlias(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp);
static Rlist *IPV4Addresses(LinkState *ifs, char *interface);
static Rlist *IPV6Addresses(LinkState *ifs, char *interface);
#endif

/****************************************************************************/

PromiseResult VerifyInterfacePromise(EvalContext *ctx, const Promise *pp)
{
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Attributes a = GetInterfaceAttributes(ctx, pp);

    if (!InterfaceSanityCheck(ctx, a, pp))
    {
        return PROMISE_RESULT_FAIL;
    }

    PromiseBanner(pp);

    if (!IsPrivileged())
    {
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

    if (a.haveaggr && a.havebridge)
    {
        Log(LOG_LEVEL_ERR, "Bonded/aggregate interface is not a bridge in 'interfaces' promise '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (PromiseGetConstraintAsBoolean(ctx, "spanning_tree", pp) && !a.havebridge)
    {
        Log(LOG_LEVEL_ERR, "Spanning tree on non-bridge for 'interfaces' promise '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    return true;
}

/****************************************************************************/

void AssessInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
#ifdef OS_LINUX

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

#endif
}

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

#ifdef OS_LINUX

static void AssessDebianInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{

    // Linux naming INTERFACE:alias.vlan, e.g. eth0:2.1 or eth0.100

    if (a->haveipv4)
    {
        AssessIPv4Config(promiser, result, ctx, IPV4Addresses(NETINTERFACES, promiser), a, pp);
    }

    if (a->haveipv6)
    {
        AssessIPv6Config(promiser, result, ctx, IPV6Addresses(NETINTERFACES, promiser), a, pp);
    }

    if (a->havetvlan)
    {
        AssessDebianTaggedVlan(promiser, result, ctx, a, pp);
    }

    if (a->haveuvlan)
    {
        AssessDeviceAlias(promiser, result, ctx, NETINTERFACES, a, pp);
        //DO UNTAGGED -- just a device ALIAS eth0:n ?
    }
    else if (a->havebridge)
    {
        AssessBridge(promiser, result, ctx, NETINTERFACES, a, pp);
    }
    else if (a->haveaggr)
    {
        AssessLACPBond(promiser, result, ctx, NETINTERFACES, a, pp);
    }

    printf("NOW EDIT %s ??\n", CF_DEBIAN_IFCONF);

}

/****************************************************************************/

static void AssessDebianTaggedVlan(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    Item *ip;
    Rlist *rp;
    int vlan_id = 0;

    // Look through the labelled VLANs to see if they are on this interface

    for (rp = a->interface.tagged_vlans; rp != NULL; rp = rp->next)
    {
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

        for (ip = VLANS; ip != NULL; ip = ip->next)
        {
            if (ip->counter == vlan_id)
            {
                DeleteItem(&VLANS, ip);
            }
        }

        if (!NewVLAN(vlan_id, promiser, result, ctx, a, pp))
        {
            return;
        }

        // Anything remaining needs to be removed

        for (ip = VLANS; ip != NULL; ip=ip->next)
        {
            DeleteVLAN(vlan_id, promiser, result, ctx, a, pp);
        }
    }
}

/****************************************************************************/

static int NewVLAN(int vlan_id, char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    char cmd[CF_BUFSIZE];
    int ret;

    Log(LOG_LEVEL_VERBOSE, "Did not find VLAN %d on %s (i.e. virtual device %s.%d)", vlan_id, promiser, promiser, vlan_id);

    snprintf(cmd, CF_BUFSIZE, "%s add %s %d", CF_DEBIAN_VLAN_COMMAND, pp->promiser, vlan_id);

// ip link add link (parent device eth0) name eth0.1 type vlan id vlanid ...

    if ((ret = ExecCommand(cmd, result, pp)))
    {
        Log(LOG_LEVEL_INFO, "Tagging VLAN %d on %s for 'interfaces' promise", vlan_id, promiser);
    }

    return ret;
}

/****************************************************************************/

static int DeleteVLAN(int vlan_id, char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
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
        if (!RlistKeyIn(addresses, rp->val.item))
        {
            *result = PROMISE_RESULT_CHANGE;

            if (a->interface.v4_broadcast && strlen(a->interface.v4_broadcast) > 0)
            {
                snprintf(cmd, CF_BUFSIZE, "%s addr add %s broadcast %s dev %s", CF_DEBIAN_IP_COMM, (char *)rp->val.item, a->interface.v4_broadcast, promiser);
                a->interface.v4_broadcast[0] = '\0'; // Only need to do this once
            }
            else
            {
                snprintf(cmd, CF_BUFSIZE, "%s addr add %s dev %s", CF_DEBIAN_IP_COMM, (char *)rp->val.item, promiser);
            }

            Log(LOG_LEVEL_VERBOSE, "Adding ipv4 %s to %s", (char *)rp->val.item, promiser);

            if (!ExecCommand(cmd, result, pp))
            {
                printf("FAILED!!\n");
                *result = PROMISE_RESULT_FAIL;
                return;
            }
        }
    }

    if (a->interface.purge)
    {
        for (rpa = addresses; rpa != NULL; rpa=rpa->next)
        {
            if (!RlistKeyIn(a->interface.v4_addresses, rpa->val.item))
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

    for (rp = a->interface.v6_addresses; rp != NULL; rp = rp->next)
    {
        if (!RlistKeyIn(addresses, rp->val.item))
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
            if (!RlistKeyIn(a->interface.v6_addresses, rpa->val.item))
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

static void AssessBridge(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)
{

    /*   brctl addbr vlan1
         brctl addif vlan1 eth0
         brctl addif vlan1 eth1.1
    */
}

/****************************************************************************/

static void AssessLACPBond(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)
{

    printf("BONDING not yet impelemented\n");

/*
  # Untagged interface
  ifconfig eth0 up
  # Tagged interface
  ifconfig eth1 up
  vconfig add eth1 1 set_name_type DEV_PLUS_VID_NO_PAD
  # Bridge them together
  brctl addbr vlan1
  brctl addif vlan1 eth0
  brctl addif vlan1 eth1.1
*/
}

/****************************************************************************/

static void AssessDeviceAlias(char *promiser, PromiseResult *result, EvalContext *ctx, LinkState *ifs, const Attributes *a, const Promise *pp)
{

    printf("ALIASING not yet implemented - untagged VLANS\n");
}






/*
  Routes

  ip route add default via <default gateway IP address>

  # ip link set eth0 up
  # ip link set eth0 down
*/

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

int ExecCommand(char *cmd, PromiseResult *result, const Promise *pp)
{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;

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
            Log(LOG_LEVEL_ERR, "Execution returned an error: %s", line);
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

static int GetVlanInfo(Item **list, const Promise *pp)
{
    FILE *fp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char ifname[CF_SMALLBUF];
    char ifparent[CF_SMALLBUF];

    if ((fp = safe_fopen(CF_DEBIAN_VLAN_FILE, "r")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to open '%s'", CF_DEBIAN_VLAN_FILE);
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
        PrependFullItem(list, ifname, NULL, id, 0);
    }

    fclose(fp);
    free(line);
    return true;
}

/****************************************************************************/

static int GetInterfaceInformation(LinkState **list, const Promise *pp)
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

            entry = xcalloc(sizeof(LinkState), 1);
            entry->next = *list;
            *list = entry;
            entry->name = xstrdup(if_name);

            sscanf(line, "%*[^>]> mtu %d %[^\n]", &mtu, endline);

            if (strstr(endline, "UP>")) // The intended link state is in <..>, the actual is after "state"
            {
                entry->up = true;
            }
            else
            {
                entry->up = false;
            }

            entry->mtu = mtu;
        }
        else
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

void DeleteInterfaceInfo()
{
    LinkState *lp, *next;

    for (lp = NETINTERFACES; lp != NULL; lp = next)
    {
        free(lp->name);
        DeleteRlist(lp->v4_adresses);
        DeleteRlist(lp->v6_adresses);
        free(lp->hw_address);
        next = lp->next;
        free((char *)lp);
    }
}

/****************************************************************************/

void DeleteBridgeInfo()
{
    Bridges *bp, *next;

    for (bp = NETBRIDGES; bp != NULL; bp = bp->next)
    {
        free(bp->name);
        free(bp->id);
        DeleteItemList(bp->interfaces);
        next = bp->next;
        free((char *)bp);
    }
}

#endif
