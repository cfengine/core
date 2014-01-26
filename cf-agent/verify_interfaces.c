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

typedef struct LinkState_ LinkState;

struct LinkState_
{
    char *name;
    char *v4_address;
    char *v4_broadcast;
    Rlist *v6_addresses;
    char *hw_address;
    bool multicast;
    bool up;
    int mtu;
    int speed;
    LinkState *next;
};

#define CF_DEBIAN_IFCONF "/etc/network/interfaces"
#define CF_DEBIAN_VLAN_FILE "/proc/net/vlan/config"
#define CF_DEBIAN_VLAN_COMMAND "/sbin/vconfig"
#define CF_DEBIAN_LISTINTERFACES_COMMAND "/bin/ip addr"

static int InterfaceSanityCheck(EvalContext *ctx, Attributes a,  const Promise *pp);
static void AssessInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static void AssessDebianInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static void AssessDebianVlan(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp);
static int GetVlanInfo(Item **list);
static int GetInterfaceInformation(LinkState **list);

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
// if (IsDefinedClass(ctx,"debian"))
    if (true)
    {
        AssessDebianInterfacePromise(promiser, result, ctx, a, pp);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "'interfaces' promises are not yet supported on this platform");
        *result = PROMISE_RESULT_INTERRUPTED;
        return;
    }
}

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

static void AssessDebianInterfacePromise(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{

    LinkState *ifs = NULL;

    if (!GetInterfaceInformation(&ifs))
    {
        *result = PROMISE_RESULT_INTERRUPTED;
        return;
    }

    LinkState *lsp;

    for (lsp = ifs; lsp != NULL; lsp = lsp->next)
    {
        printf("======================\n");
        printf("INTERFACE %s (mtu %d)\n", lsp->name, lsp->mtu);
        printf("V4: %s\n", lsp->v4_address);
        printf("MAC: %s\n", lsp->hw_address);
        for (Rlist *rp = lsp->v6_addresses; rp!=NULL; rp=rp->next)
        {
            printf("V6: %s\n", (char *)rp->val.item);
        }
    }

    printf("======================\n");

    if (a->havetvlan || a->haveuvlan)
    {
        printf("SET VLANs %s\n", pp->promiser);
        AssessDebianVlan(promiser, result, ctx, a, pp);
    }
    else if (a->havebridge)
    {
        printf("BRIDGE %s\n", pp->promiser);
    }
    else if (a->haveaggr)
    {
        printf("BONDED %s\n", pp->promiser);
    }

    if (a->haveipv4)
    {
        printf("SET 4 ADDRESS %s\n", pp->promiser);
    }

    if (a->haveipv6)
    {
        printf("SET 6 ADDRESS %s\n", pp->promiser);
    }

    if (PromiseGetConstraintAsBoolean(ctx, "link_state", pp))
    {
        printf("LINK STAT on %s\n", pp->promiser);
    }

    printf("NOW EDIT %s\n", CF_DEBIAN_IFCONF);
    printf("----------------\n");
}

/****************************************************************************/

static void AssessDebianVlan(char *promiser, PromiseResult *result, EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    Item *vlans = NULL;


    if (!GetVlanInfo(&vlans))
    {
        *result = PROMISE_RESULT_INTERRUPTED;
        return;
    }


// Look for reserved variable
// VLANS[blue] int => "id"

    // Linux naming INTERFACE:alias.vlan, e.g. eth0:2.1 or eth0.100

    // GET INTERFACES

// /sbin/ip -6 addr add 2001:0db8:0:f101::1/64 dev eth0
}

/****************************************************************************/

static int GetVlanInfo(Item **list)
{
    FILE *fp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char ifname[CF_SMALLBUF];
    char ifparent[CF_SMALLBUF];

    if ((fp = safe_fopen(CF_DEBIAN_VLAN_FILE, "r")) == NULL)
    {
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
        printf("GOT %s with id %d\n", ifname, id);
        PrependFullItem(list, ifname, NULL, id, 0);
    }

    fclose(fp);
    free(line);
    return true;
}

/****************************************************************************/

static int GetInterfaceInformation(LinkState **list)
{
    FILE *pp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char indent[CF_SMALLBUF];
    char hw_addr[CF_MAX_IP_LEN];
    char v4_addr[CF_MAX_IP_LEN];
    char v6_addr[CF_MAX_IP_LEN];
    char if_name[CF_SMALLBUF];
    char endline[CF_BUFSIZE];
    int mtu = CF_NOINT;
    LinkState *entry;

    if ((pp = cf_popen(CF_DEBIAN_LISTINTERFACES_COMMAND, "r", true)) == NULL)
    {
        return false;
    }

    while (!feof(pp))
    {
        CfReadLine(&line, &line_size, pp);

        if (feof(pp))
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
            if (strstr(endline, "UP"))
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
                entry->v4_address = xstrdup(v4_addr);
            }
            else if (strncmp(indent, "link", 4) == 0)
            {
                sscanf(line, "%*s %64s", hw_addr);
                entry->hw_address = xstrdup(hw_addr);
            }
        }
    }

    free(line);
    cf_pclose(pp);
    return true;
}
