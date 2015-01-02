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
#include <misc_lib.h>
#include <files_lib.h>
#include <files_interfaces.h>
#include <pipes.h>
#include <item_lib.h>
#include <expand.h>
#include <routing_services.h>
#include <mod_common.h>
#include <conversion.h>
#include <addr_lib.h>
#include <communication.h>
#include <string_lib.h>

/*****************************************************************************/
/*                                                                           */
/* File: routing_services.c                                                  */
/*                                                                           */
/* Created: Tue Apr  1 12:29:21 2014                                         */
/*                                                                           */
/*****************************************************************************/

CommonRouting *ROUTING_ACTIVE = NULL;
CommonRouting *ROUTING_POLICY = NULL;

#define VTYSH_FILENAME "/usr/bin/vtysh"

static void HandleOSPFServiceConfig(EvalContext *ctx, CommonRouting *ospfp, char *line);
static void HandleOSPFInterfaceConfig(EvalContext *ctx, LinkStateOSPF *ospfp, const char *line, const Attributes *a, const Promise *pp);
static void HandleBGPServiceConfig(EvalContext *ctx, CommonRouting *bgpp, char *line);
static void HandleBGPInterfaceConfig(EvalContext *ctx, LinkStateBGP *bgpp, const char *line, const Attributes *a, const Promise *pp, char *family);
static char *GetStringAfter(const char *line, const char *prefix);
static int GetIntAfter(const char *line, const char *prefix);
static bool GetMetricIf(const char *line, const char *prefix, int *metric, int *metric_type);
static int ExecRouteCommand(char *cmd);
static BGPNeighbour *GetPeer(char *id, LinkStateBGP *bgpp);
static BGPNeighbour *IsNeighbourIn(BGPNeighbour *list, char *name);

/*****************************************************************************/
/* OSPF                                                                      */
/*****************************************************************************/

CommonRouting *NewRoutingState()
{
    return (CommonRouting *)calloc(sizeof(CommonRouting), 1);
}

/*****************************************************************************/

void DeleteRoutingState(CommonRouting *state)
{
    if (state->log_file)
    {
        free(state->log_file);
    }

    if (state->ospf_log_adjacency_changes)
    {
        free(state->ospf_log_adjacency_changes);
    }

    if (state->ospf_router_id)
    {
        free(state->ospf_router_id);
    }

    free(state);
}

/*****************************************************************************/

void InitializeRoutingServices(const Policy *policy, EvalContext *ctx)

{
    if (!HaveRoutingService(ctx))
    {
        return;
    }

    // Look for the control body for ospf
    Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_ROUTING);

    if (constraints)
    {
        ROUTING_POLICY = (CommonRouting *)calloc(sizeof(CommonRouting), 1);

        for (size_t i = 0; i < SeqLength(constraints); i++)
        {
            Constraint *cp = SeqAt(constraints, i);

            if (!IsDefinedClass(ctx, cp->classes))
            {
                continue;
            }

            VarRef *ref = VarRefParseFromScope(cp->lval, "control_routing_services");
            const void *value = EvalContextVariableGet(ctx, ref, NULL);
            VarRefDestroy(ref);

            if (!value)
            {
                Log(LOG_LEVEL_ERR, "Unknown lval '%s' in routing_services control body", cp->lval);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[ROUTING_CONTROL_LOG_FILE].lval) == 0)
            {
                ROUTING_POLICY->log_file = (char *)value;
                Log(LOG_LEVEL_VERBOSE, "Setting the log file to %s", (char *)value);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[OSPF_CONTROL_LOG_ADJACENCY_CHANGES].lval) == 0)
            {
                ROUTING_POLICY->ospf_log_adjacency_changes = (char *) value;
                Log(LOG_LEVEL_VERBOSE, "Setting ospf_log_adjacency_changes to %s", (const char *)value);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[OSPF_CONTROL_LOG_TIMESTAMP_PRECISION].lval) == 0)
            {
                ROUTING_POLICY->log_timestamp_precision = (int) IntFromString(value); // 0,6
                Log(LOG_LEVEL_VERBOSE, "Setting the logging timestamp precision to %d microseconds", ROUTING_POLICY->log_timestamp_precision);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[OSPF_CONTROL_ROUTER_ID].lval) == 0)
            {
                ROUTING_POLICY->ospf_router_id = (char *)value;
                Log(LOG_LEVEL_VERBOSE, "Setting the router-id (trad. \"loopback address\") to %s", (char *)value);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE].lval) == 0)
            {
                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (strcmp(rp->val.item, "kernel"))
                    {
                        ROUTING_POLICY->ospf_redistribute_kernel = true;
                        Log(LOG_LEVEL_VERBOSE, "Setting ospf redistribution from kernel FIB");
                        continue;
                    }
                    if (strcmp(rp->val.item, "connected"))
                    {
                        ROUTING_POLICY->ospf_redistribute_connected = true;
                        Log(LOG_LEVEL_VERBOSE, "Setting ospf redistribution from connected networks");
                        continue;
                    }
                    if (strcmp(rp->val.item, "static"))
                    {
                        Log(LOG_LEVEL_VERBOSE, "Setting ospf redistribution from static FIB");
                        ROUTING_POLICY->ospf_redistribute_static = true;
                        continue;
                    }
                    if (strcmp(rp->val.item, "bgp"))
                    {
                        Log(LOG_LEVEL_VERBOSE, "Setting ospf to allow bgp route injection");
                        ROUTING_POLICY->ospf_redistribute_bgp = true;
                        continue;
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_KERNEL_METRIC].lval) == 0)
            {
                ROUTING_POLICY->ospf_redistribute_kernel_metric = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting ospf metric for kernel routes to %d", ROUTING_POLICY->ospf_redistribute_kernel_metric);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_CONNECTED_METRIC].lval) == 0)
            {
                ROUTING_POLICY->ospf_redistribute_connected_metric = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting ospf metric for kernel routes to %d", ROUTING_POLICY->ospf_redistribute_connected_metric);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_STATIC_METRIC].lval) == 0)
            {
                ROUTING_POLICY->ospf_redistribute_static_metric = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting ospf metric for static routes to %d", ROUTING_POLICY->ospf_redistribute_kernel_metric);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_BGP_METRIC].lval) == 0)
            {
                ROUTING_POLICY->ospf_redistribute_bgp_metric = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting ospf metric for bgp routes to %d", ROUTING_POLICY->ospf_redistribute_kernel_metric);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_METRIC_TYPE].lval) == 0)
            {
                ROUTING_POLICY->ospf_redistribute_external_metric_type = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting ospf metric-type for redistributed routes to %d", ROUTING_POLICY->ospf_redistribute_external_metric_type);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[BGP_LOCAL_AS].lval) == 0)
            {
                ROUTING_POLICY->bgp_local_as = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting BGP AS number to %d", ROUTING_POLICY->bgp_local_as);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[BGP_ROUTER_ID].lval) == 0)
            {
                ROUTING_POLICY->bgp_router_id = (char *)value;
                Log(LOG_LEVEL_VERBOSE, "Setting BGP router ID to %s", ROUTING_POLICY->bgp_router_id);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[BGP_GRACEFUL_RESTART].lval) == 0)
            {
                ROUTING_POLICY->bgp_graceful_restart = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting graceful-restart = %d", ROUTING_POLICY->bgp_graceful_restart);
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[BGP_LOG_NEIGHBOR_CHANGES].lval) == 0)
            {
                ROUTING_POLICY->bgp_log_neighbor_changes = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting BGP log neighbour changes");
                continue;
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[BGP_REDISTRIBUTE].lval) == 0)
            {
                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (strcmp(rp->val.item, "kernel"))
                    {
                        ROUTING_POLICY->bgp_redistribute_kernel = true;
                        Log(LOG_LEVEL_VERBOSE, "Setting bgp redistribution from kernel FIB");
                        continue;
                    }
                    if (strcmp(rp->val.item, "connected"))
                    {
                        ROUTING_POLICY->bgp_redistribute_connected = true;
                        Log(LOG_LEVEL_VERBOSE, "Setting bgp redistribution from connected networks");
                        continue;
                    }
                    if (strcmp(rp->val.item, "static"))
                    {
                        Log(LOG_LEVEL_VERBOSE, "Setting bgp redistribution from static FIB");
                        ROUTING_POLICY->bgp_redistribute_static = true;
                        continue;
                    }
                    if (strcmp(rp->val.item, "ospf"))
                    {
                        Log(LOG_LEVEL_VERBOSE, "Setting bgp to allow ospf route injection");
                        ROUTING_POLICY->bgp_redistribute_ospf = true;
                        continue;
                    }
                }
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[BGP_V4_NETWORKS].lval) == 0)
            {
                ROUTING_POLICY->bgp_advertisable_v4_networks = (Rlist *)value;

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (IsIPV4NetworkAddress(rp->val.item))
                    {
                        Log(LOG_LEVEL_VERBOSE, "Setting BGP ipv4 network advertisement for: %s", (char *)rp->val.item);
                    }
                    else
                    {
                        Log(LOG_LEVEL_ERR, "BGP network advertisement %s is not a valid ipv4 network address", (char *)rp->val.item);
                    }
                }
            }

            if (strcmp(cp->lval, ROUTING_CONTROLBODY[BGP_V6_NETWORKS].lval) == 0)
            {
                ROUTING_POLICY->bgp_advertisable_v6_networks = (Rlist *)value;

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    ToLowerStrInplace(rp->val.item);

                    if (IsIPV6NetworkAddress(rp->val.item))
                    {
                        Log(LOG_LEVEL_VERBOSE, "Setting BGP ipv6 network advertisement for: %s", (char *)rp->val.item);
                    }
                    else
                    {
                        Log(LOG_LEVEL_ERR, "BGP network advertisement %s is not a valid ipv6 network address", (char *)rp->val.item);
                    }
                }
            }
        }
    }
}

/*****************************************************************************/

int QueryRoutingServiceState(EvalContext *ctx, CommonRouting *routingp)

{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char comm[CF_BUFSIZE];
    RouterCategory state = CF_RC_INITIAL;

    snprintf(comm, CF_BUFSIZE, "%s -c \"show running-config\"", VTYSH_FILENAME);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", VTYSH_FILENAME);
        return false;
    }

    while (!feof(pfp))
    {
        CfReadLine(&line, &line_size, pfp);

        if (feof(pfp))
        {
            break;
        }

        if (*line == '!')
        {
            state = CF_RC_INITIAL;
            continue;
        }

        if (strncmp(line, "router ospf", strlen("router ospf")) == 0)
        {
            state = CF_RC_OSPF;
            continue;
        }

        if (strncmp(line, "router bgp", strlen("router bgp")) == 0 || strncmp(line, " address-family", strlen(" address-family")) == 0)
        {
            state = CF_RC_BGP;
        }

        switch(state)
        {
        case CF_RC_INITIAL:
        case CF_RC_OSPF:
            HandleOSPFServiceConfig(ctx, routingp, line);
            break;

        case CF_RC_INTERFACE:
        case CF_RC_BGP:
            HandleBGPServiceConfig(ctx, routingp, line);
        default:
            break;
        }
    }

    free(line);
    cf_pclose(pfp);

    if (routingp->log_file)
    {
        Log(LOG_LEVEL_VERBOSE, "Routing log file: %s", routingp->log_file);
    }

    if (routingp->password)
    {
        Log(LOG_LEVEL_VERBOSE, "Routing service password: %s, enabled = %d", routingp->password, routingp->enable_password);
    }

    Log(LOG_LEVEL_VERBOSE, "Routing log timestamp precision: %d microseconds", routingp->log_timestamp_precision);

    if (routingp->ospf_log_adjacency_changes)
    {
        Log(LOG_LEVEL_VERBOSE, "OSPF log adjacency change logging: %s ", routingp->ospf_log_adjacency_changes);
    }

    if (routingp->ospf_router_id)
    {
        Log(LOG_LEVEL_VERBOSE, "OSPF router-id: %s ", routingp->ospf_router_id);
    }

    if (routingp->ospf_redistribute_kernel)
    {
        Log(LOG_LEVEL_VERBOSE,"OSPF redistributing kernel routes: %d with metric %d (type %d)",
            routingp->ospf_redistribute_kernel,routingp->ospf_redistribute_kernel_metric,routingp->ospf_redistribute_external_metric_type);
    }

    if (routingp->ospf_redistribute_connected)
    {
        Log(LOG_LEVEL_VERBOSE,"OSPF redistributing connected networks: %d with metric %d (type %d)",
            routingp->ospf_redistribute_connected, routingp->ospf_redistribute_connected_metric, routingp->ospf_redistribute_external_metric_type);
    }

    if (routingp->ospf_redistribute_static)
    {
        Log(LOG_LEVEL_VERBOSE,"OSPF redistributing static routes: %d with metric %d (type %d)",
            routingp->ospf_redistribute_static, routingp->ospf_redistribute_static_metric, routingp->ospf_redistribute_external_metric_type);
    }

    if (routingp->ospf_redistribute_bgp)
    {
        Log(LOG_LEVEL_VERBOSE,"OSPF redistributing bgp: %d with metric %d (type %d)",
            routingp->ospf_redistribute_bgp, routingp->ospf_redistribute_bgp_metric, routingp->ospf_redistribute_external_metric_type);
    }

    if (routingp->bgp_router_id)
    {
        Log(LOG_LEVEL_VERBOSE, "Host currently has bgp router_id: %s", routingp->bgp_router_id);
    }

    if (routingp->bgp_redistribute_kernel)
    {
        Log(LOG_LEVEL_VERBOSE, "Host is currently redistributing kernel routes");
    }

    if (routingp->bgp_redistribute_static)
    {
        Log(LOG_LEVEL_VERBOSE, "Host is currently redistributing static routes");
    }

    if (routingp->bgp_redistribute_connected)
    {
        Log(LOG_LEVEL_VERBOSE, "Host is currently redistributing connected routes");
    }

    if (routingp->bgp_redistribute_ospf)
    {
        Log(LOG_LEVEL_VERBOSE, "Host is currently redistributing ospf routes");
    }

    for (Rlist *rp = routingp->bgp_advertisable_v4_networks; rp != NULL; rp=rp->next)
    {
        Log(LOG_LEVEL_VERBOSE, "Host is currently advertising ipv4 network %s", (char *)rp->val.item);
    }

    for (Rlist *rp = routingp->bgp_advertisable_v6_networks; rp != NULL; rp=rp->next)
    {
        Log(LOG_LEVEL_VERBOSE, "Host is currently advertising ipv6 network %s", (char *)rp->val.item);
    }

    return true;
}

/*****************************************************************************/

void KeepOSPFLinkServiceControlPromises(CommonRouting *policy, CommonRouting *state)
{
    char comm[CF_BUFSIZE];

    if (policy == NULL || state == NULL)
    {
        Log(LOG_LEVEL_VERBOSE,"OSPF not promised, skipping setup");
        return;
    }

    // Log file

    if (policy->log_file)
    {
        if (state->log_file == NULL || strcmp(policy->log_file, state->log_file) == 0)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"log file %s\"", VTYSH_FILENAME, policy->log_file);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to set keep promised OSPF log file: %s", policy->log_file);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: log file to: %s", policy->log_file);
            }
        }
    }

    // Timestamp precision

    if (policy->log_timestamp_precision != state->log_timestamp_precision)
    {
        snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"log timestamp precision %d\"", VTYSH_FILENAME, policy->log_timestamp_precision);

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: log timestamp precision %d microseconds", policy->log_timestamp_precision);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise log timestamp precision: %d microseconds", policy->log_timestamp_precision);
        }
    }

    // Adjacency change logging

    if (policy->ospf_log_adjacency_changes)
    {
        if (state->ospf_log_adjacency_changes == NULL || strcmp(policy->ospf_log_adjacency_changes, state->ospf_log_adjacency_changes) != 0)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"log-adjacency-changes %s\"", VTYSH_FILENAME, policy->ospf_log_adjacency_changes);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: log adjacency change logging %s ", policy->ospf_log_adjacency_changes);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: log adjacency change logging %s ", policy->ospf_log_adjacency_changes);
            }
        }
    }

    // Router id / "loopback"

    if (policy->ospf_router_id)
    {
        if (state->ospf_router_id == NULL || strcmp(policy->ospf_router_id, state->ospf_router_id) != 0)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"router-id %s\"", VTYSH_FILENAME, policy->ospf_router_id);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: router-id is %s", policy->ospf_router_id);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: router-id is %s", policy->ospf_router_id);
            }
        }
    }
    else if (state->ospf_router_id)
    {
        snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"no router-id\"", VTYSH_FILENAME);

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: no router-id");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: removed router-id");
        }
    }

    // Route redistribution
    // kernel

    if (policy->ospf_redistribute_kernel)
    {
        if (!state->ospf_redistribute_kernel || (policy->ospf_redistribute_kernel_metric != state->ospf_redistribute_kernel_metric
                                                 && policy->ospf_redistribute_kernel_metric_type != policy->ospf_redistribute_external_metric_type))
        {
            if (policy->ospf_redistribute_kernel_metric && policy->ospf_redistribute_external_metric_type)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute kernel metric %d metric-type %d\"",
                         VTYSH_FILENAME,
                         policy->ospf_redistribute_kernel_metric,
                         policy->ospf_redistribute_external_metric_type);
            }
            else if (policy->ospf_redistribute_kernel_metric)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute kernel metric %d\"",
                         VTYSH_FILENAME,
                         policy->ospf_redistribute_kernel_metric);
            }
            else
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute kernel\"", VTYSH_FILENAME);
            }

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: redistribute kernel");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: redistribute kernel");
            }
        }
    }
    else if (state->ospf_redistribute_kernel)
    {
        snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"no redistribute kernel\"", VTYSH_FILENAME);

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: no redistribute kernel");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: no redistribute kernel");
        }
    }

    // connected

    if (policy->ospf_redistribute_connected)
    {
        if (!policy->ospf_redistribute_connected || (policy->ospf_redistribute_connected_metric != state->ospf_redistribute_connected_metric
                                                     && policy->ospf_redistribute_connected_metric_type != policy->ospf_redistribute_external_metric_type))
        {
            if (policy->ospf_redistribute_connected_metric && policy->ospf_redistribute_external_metric_type)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute connected metric %d metric-type %d\"",
                         VTYSH_FILENAME,
                         policy->ospf_redistribute_connected_metric,
                         policy->ospf_redistribute_external_metric_type);
            }
            else if (policy->ospf_redistribute_connected_metric)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute connected metric %d\"",
                         VTYSH_FILENAME,
                         policy->ospf_redistribute_connected_metric);
            }
            else
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute connected\"", VTYSH_FILENAME);
            }

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: redistribute connected");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: redistribute connected");
            }
        }
    }
    else if (state->ospf_redistribute_connected)
    {
        snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"no redistribute connected\"", VTYSH_FILENAME);

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: no redistribute connected");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: no redistribute connected");
        }
    }

    // static

    if (policy->ospf_redistribute_static)
    {
        if (!state->ospf_redistribute_static || (policy->ospf_redistribute_static_metric != state->ospf_redistribute_static_metric
                                                 && policy->ospf_redistribute_static_metric_type != policy->ospf_redistribute_external_metric_type))
        {
            if (policy->ospf_redistribute_static_metric && policy->ospf_redistribute_external_metric_type)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute static metric %d metric-type %d\"",
                         VTYSH_FILENAME,
                         policy->ospf_redistribute_static_metric,
                         policy->ospf_redistribute_external_metric_type);
            }
            else if (policy->ospf_redistribute_static_metric)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute static metric %d\"",
                         VTYSH_FILENAME,
                         policy->ospf_redistribute_static_metric);
            }
            else
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute static\"", VTYSH_FILENAME);
            }

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: redistribute static");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: redistribute static");
            }
        }
    }
    else if (state->ospf_redistribute_static)
    {
        snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"no redistribute static\"", VTYSH_FILENAME);

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: no redistribute static");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: no redistribute static");
        }
    }

    // bgp

    if (policy->ospf_redistribute_bgp)
    {
        if (!state->ospf_redistribute_bgp || (policy->ospf_redistribute_bgp_metric != state->ospf_redistribute_bgp_metric
                                              && policy->ospf_redistribute_bgp_metric_type != policy->ospf_redistribute_external_metric_type))
        {
            if (policy->ospf_redistribute_bgp_metric && policy->ospf_redistribute_external_metric_type)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute bgp metric %d metric-type %d\"",
                         VTYSH_FILENAME,
                         policy->ospf_redistribute_bgp_metric,
                         policy->ospf_redistribute_external_metric_type);
            }
            else if (policy->ospf_redistribute_bgp_metric)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute bgp metric %d\"",
                         VTYSH_FILENAME,
                         policy->ospf_redistribute_bgp_metric);
            }
            else
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"redistribute bgp\"", VTYSH_FILENAME);
            }

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: redistribute bgp");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: redistribute bgp");
            }
        }
    }
    else if (state->ospf_redistribute_bgp)
    {
        snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"no redistribute bgp\"", VTYSH_FILENAME);

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep OSPF promise: no redistribute bgp");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: no redistribute bgp");
        }
    }
}

/*****************************************************************************/

int QueryOSPFInterfaceState(EvalContext *ctx, const Attributes *a, const Promise *pp, LinkStateOSPF *ospfp)

{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char comm[CF_BUFSIZE];
    char search[CF_MAXVARSIZE];
    RouterCategory state = CF_RC_INITIAL;

    ospfp->ospf_priority = 1;  // default value seems to be 1, in which case this line does not appear
    ospfp->ospf_area = CF_NOINT;
    ospfp->ospf_area_type = 'n'; // normal, non-stub

    snprintf(comm, CF_BUFSIZE, "%s -c \"show running-config\"", VTYSH_FILENAME);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", VTYSH_FILENAME);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    snprintf(search, CF_MAXVARSIZE, "interface %s", pp->promiser);

    while (!feof(pfp))
    {
        CfReadLine(&line, &line_size, pfp);

        if (feof(pfp))
        {
            break;
        }

        if (*line == '!')
        {
            state = CF_RC_INITIAL;
            continue;
        }

        if (strcmp(line, search) == 0)
        {
            state = CF_RC_INTERFACE;
            continue;
        }

        if (strncmp(line, "router ospf", strlen("router ospf")) == 0)
        {
            state = CF_RC_OSPF;
            continue;
        }

        switch(state)
        {
        case CF_RC_OSPF:
        case CF_RC_INTERFACE:
            HandleOSPFInterfaceConfig(ctx, ospfp, line, a, pp);
            break;

        case CF_RC_INITIAL:
        case CF_RC_BGP:
            break;
        default:
            break;
        }
    }

    free(line);
    cf_pclose(pfp);

    Log(LOG_LEVEL_VERBOSE, "Interface %s currently has ospf_hello_interval: %d", pp->promiser, ospfp->ospf_hello_interval);
    Log(LOG_LEVEL_VERBOSE, "Interface %s currently has ospf_priority: %d", pp->promiser, ospfp->ospf_priority);
    Log(LOG_LEVEL_VERBOSE, "Interface %s currently has ospf_link_type: %s", pp->promiser, ospfp->ospf_link_type);
    Log(LOG_LEVEL_VERBOSE, "Interface %s currently has ospf_authentication_digest: %s", pp->promiser, ospfp->ospf_authentication_digest);
    Log(LOG_LEVEL_VERBOSE, "Interface %s currently has ospf_passive_interface: %d", pp->promiser, ospfp->ospf_passive_interface);
    Log(LOG_LEVEL_VERBOSE, "Interface %s currently has ospf_abr_summarization: %d", pp->promiser, ospfp->ospf_abr_summarization);
    Log(LOG_LEVEL_VERBOSE, "Interface %s currently points to ospf_area: %d", pp->promiser, ospfp->ospf_area);
    Log(LOG_LEVEL_VERBOSE, "Interface %s currently points to ospf_area_type: %c", pp->promiser, ospfp->ospf_area_type);

    return true;
}

/*****************************************************************************/

void KeepOSPFInterfacePromises(ARG_UNUSED EvalContext *ctx, const Attributes *a, const Promise *pp, PromiseResult *result, LinkStateOSPF *ospfp)

{
    char comm[CF_BUFSIZE];

    // String

    if (a->interface.ospf_link_type)
    {
        if (strcmp(a->interface.ospf_link_type, ospfp->ospf_link_type) != 0)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"interface %s\" -c \"ospf network %s\"",
                     VTYSH_FILENAME, pp->promiser, a->interface.ospf_link_type);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to set keep promised OSPF link_type: %s", a->interface.ospf_link_type);
                *result = PROMISE_RESULT_FAIL;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: link_type %s", a->interface.ospf_link_type);
                *result = PROMISE_RESULT_CHANGE;
            }
        }
    }

    if (a->interface.ospf_authentication_digest)
    {
        if (ospfp->ospf_authentication_digest == NULL || strcmp(a->interface.ospf_authentication_digest, ospfp->ospf_authentication_digest) != 0)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"interface %s\" -c \"ip ospf authentication message-digest\"",
                     VTYSH_FILENAME, pp->promiser);

            if (!ExecRouteCommand(comm))
            {
                // Ignore?
            }

            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"interface %s\" -c \"ip ospf message-digest-key 1 md5 %s\"",
                     VTYSH_FILENAME, pp->promiser, a->interface.ospf_authentication_digest);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to set keep promised OSPF link_type: %s", a->interface.ospf_link_type);
                *result = PROMISE_RESULT_FAIL;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: link_type %s", a->interface.ospf_link_type);
                *result = PROMISE_RESULT_CHANGE;
            }
        }
    }

    // Int

    if (a->interface.ospf_hello_interval > 0)
    {
        if (a->interface.ospf_hello_interval == ospfp->ospf_hello_interval)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"interface %s\" -c \"ip ospf hello-interval %d\"",
                     VTYSH_FILENAME, pp->promiser, a->interface.ospf_hello_interval);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to set keep promised OSPF hello-interval: %d", a->interface.ospf_hello_interval);
                *result = PROMISE_RESULT_FAIL;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: hello-interval %d", a->interface.ospf_hello_interval);
                *result = PROMISE_RESULT_CHANGE;
            }
        }
    }

    if (a->interface.ospf_priority > 0)
    {
        if (a->interface.ospf_priority == ospfp->ospf_priority)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"interface %s\" -c \"ip ospf priority %d\"",
                     VTYSH_FILENAME, pp->promiser, a->interface.ospf_priority);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to set keep promised OSPF priority: %d", a->interface.ospf_priority);
                *result = PROMISE_RESULT_FAIL;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: priority%d", a->interface.ospf_priority);
                *result = PROMISE_RESULT_CHANGE;
            }
        }
    }

    if (a->interface.ospf_area != ospfp->ospf_area || a->interface.ospf_area_type != ospfp->ospf_area_type
        || a->interface.ospf_abr_summarization != ospfp->ospf_abr_summarization)
    {
        snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"interface %s\" -c \"ip ospf area 0.0.0.%d\"",
                 VTYSH_FILENAME, pp->promiser, a->interface.ospf_area);

        if (a->interface.ospf_area_type == 's')
        {
            strcat(comm, " stub");
        }

        if (!a->interface.ospf_abr_summarization)
        {
            strcat(comm, " no-usmmary");
        }

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep promised OSPF area: %d", a->interface.ospf_priority);
            *result = PROMISE_RESULT_FAIL;
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise: area %d", a->interface.ospf_priority);
            *result = PROMISE_RESULT_CHANGE;
        }
    }

    // Boolean

    if (a->interface.ospf_passive_interface != ospfp->ospf_passive_interface)
    {
        if (a->interface.ospf_passive_interface)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"passive %s\"",
                     VTYSH_FILENAME, pp->promiser);
        }
        else
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router ospf\" -c \"no passive %s\"",
                     VTYSH_FILENAME, pp->promiser);
        }

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep promised OSPF passive interface");
            *result = PROMISE_RESULT_FAIL;
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept OSPF promise for passive interface");
            *result = PROMISE_RESULT_CHANGE;
        }
    }
}

/*****************************************************************************/

bool HaveRoutingService(ARG_UNUSED EvalContext *ctx)

{ struct stat sb;

    if (stat(VTYSH_FILENAME, &sb) == -1)
    {
        return false;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Quagga link services interface found at %s", VTYSH_FILENAME);
        return true;
    }

    return false;
}

/*****************************************************************************/
/* BGP                                                                       */
/*****************************************************************************/

void KeepBGPInterfacePromises(ARG_UNUSED EvalContext *ctx, const Attributes *a, const Promise *pp, PromiseResult *result, LinkStateBGP *bgpp)

{ BGPNeighbour *bp;
    char comm[CF_BUFSIZE];
    bool am_ibgp = false;
    bool am_unnumbered = false;
    Item *address_families = NULL;

    if (bgpp && !ROUTING_POLICY)
    {
        Log(LOG_LEVEL_ERR, "Found BGP link service promises for interface %s, but no routing services control body", pp->promiser);
    }

    if (!ROUTING_POLICY)
    {
        Log(LOG_LEVEL_VERBOSE,"Skipping BGP interface promises as routing services control body is missing\n");
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Verifying BGP link service promises for interface %s", pp->promiser);
    Log(LOG_LEVEL_VERBOSE, "Interface %s belongs to ASN %d", pp->promiser, bgpp->bgp_local_as);

    if (ROUTING_POLICY->bgp_local_as == a->interface.bgp_remote_as)
    {
        Log(LOG_LEVEL_VERBOSE,"Interface connection connects us to our own AS %d (iBGP)\n", a->interface.bgp_remote_as);
        am_ibgp = true;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE,"Interface connection connects us to remote AS %d\n", a->interface.bgp_remote_as);
    }

    if (a->interface.bgp_neighbour && !(IsIPV4Address(a->interface.bgp_neighbour) || IsIPV6Address(a->interface.bgp_neighbour)))
    {
        Log(LOG_LEVEL_VERBOSE, "(The peer %s seems to be reached by an unnumbered interface)\n", a->interface.bgp_neighbour);
        am_unnumbered = true;
    }

    if ((bp = GetPeer(a->interface.bgp_neighbour, bgpp)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Software error in BGP handling looking for %s", a->interface.bgp_neighbour);
        *result = PROMISE_RESULT_FAIL;
        return;
    }

    if ((bp->bgp_remote_as != 0) && (bp->bgp_remote_as !=  a->interface.bgp_remote_as))
    {
        snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"no neighbor %s remote-as %d\"",
                 VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, a->interface.bgp_neighbour, bp->bgp_remote_as);

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to remove incorrect BGP session %s ASN %d", a->interface.bgp_neighbour, bp->bgp_remote_as);
            *result = PROMISE_RESULT_FAIL;
            return;
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Removed existing BGP session %s ASN %d", a->interface.bgp_neighbour, bp->bgp_remote_as);
        }
    }

    if (bp->bgp_remote_as !=  a->interface.bgp_remote_as)
    {
        snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"neighbor %s remote-as %d\"",
                 VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, a->interface.bgp_neighbour, a->interface.bgp_remote_as);

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to establish BGP session %s ASN %d", a->interface.bgp_neighbour, a->interface.bgp_remote_as);
            *result = PROMISE_RESULT_FAIL;
            return;
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Established BGP session %s ASN %d", a->interface.bgp_neighbour, a->interface.bgp_remote_as);
        }
    }

    // Now interface family specific commands
    // Transduce address families for this router implementation

    for (Rlist *rp = a->interface.bgp_families; rp != NULL; rp=rp->next)
    {
        if (strcmp(rp->val.item, "ipv4_unicast") == 0)
        {
            PrependItem(&address_families, "ipv4 unicast", NULL);
        }

        if (strcmp(rp->val.item, "ipv6_unicast") == 0)
        {
            PrependItem(&address_families, "ipv6", NULL);
        }
    }

    // The cleansing

    for (Item *ip = bgpp->bgp_advertise_families; ip != NULL; ip=ip->next)
    {
        if (!IsItemIn(address_families, ip->name))
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"   -c \"address-family %s\" -c \"no neighbor %s remote-as %d activate\"",
                     VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name, a->interface.bgp_neighbour, a->interface.bgp_remote_as);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to deactivate %s in BGP session %s ASN %d", ip->name, a->interface.bgp_neighbour, a->interface.bgp_remote_as);
                *result = PROMISE_RESULT_FAIL;
                return;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Established BGP session %s ASN %d", a->interface.bgp_neighbour, a->interface.bgp_remote_as);
            }
        }
    }

    // Activation

    for (Item *ip = address_families; ip != NULL; ip=ip->next)
    {
        if (!IsNeighbourIn(bgpp->bgp_peers,a->interface.bgp_neighbour))
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"   -c \"address-family %s\" -c \"neighbor %s remote-as %d\"",
                     VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name,  a->interface.bgp_neighbour, a->interface.bgp_remote_as);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to add BGP session %s ASN %d on family %s", a->interface.bgp_neighbour, a->interface.bgp_remote_as, ip->name);
                *result = PROMISE_RESULT_FAIL;
                continue;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Added BGP session %s ASN %d on family %s", a->interface.bgp_neighbour, a->interface.bgp_remote_as, ip->name);
            }
        }

        // Must activate before setting any params - ipv4 only reports NOT active

        if (!IsItemIn(bgpp->bgp_advertise_families, ip->name))
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"   -c \"address-family %s\" -c \"neighbor %s activate\"",
                     VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name,  a->interface.bgp_neighbour);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to activate BGP session %s ASN %d on family %s", a->interface.bgp_neighbour, a->interface.bgp_remote_as, ip->name);
                *result = PROMISE_RESULT_FAIL;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Activated BGP session %s ASN %d on family %s", a->interface.bgp_neighbour, a->interface.bgp_remote_as, ip->name);
            }
        }

        // Route reflector

        if (am_ibgp)
        {
            if (bp->bgp_reflector != a->interface.bgp_reflector)
            {
                if (bp->bgp_reflector)
                {
                    snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"no neighbor %s route-reflector-client\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name, a->interface.bgp_neighbour);
                }
                else
                {
                    snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"neighbor %s route-reflector-client\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name, a->interface.bgp_neighbour);
                }

                if (!ExecRouteCommand(comm))
                {
                    Log(LOG_LEVEL_VERBOSE, "Failed establish BGP route reflector for %s over %s", a->interface.bgp_neighbour, ip->name);
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE, "Established BGP route reflector for %s over %s", a->interface.bgp_neighbour, ip->name);
                }
            }
        }

        // ttl-security

        if (!am_unnumbered)
        {
            if (bp->bgp_ttl_security != a->interface.bgp_ttl_security)
            {
                if (a->interface.bgp_ttl_security)
                {
                    snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"neighbor %s ttl-security hops %d\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name, a->interface.bgp_neighbour, a->interface.bgp_ttl_security);
                }
                else
                {
                    snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"no neighbor %s ttl-security hops %d\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name, a->interface.bgp_neighbour, a->interface.bgp_ttl_security);
                }
            }

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed establish BGP ttl-security hops %d for %s over %s",  a->interface.bgp_ttl_security, a->interface.bgp_neighbour, ip->name);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Established BGP ttl-security hops %d for %s over %s",  a->interface.bgp_ttl_security, a->interface.bgp_neighbour, ip->name);
            }
        }

        // advertisement_interval

        if (bp->bgp_advert_interval != a->interface.bgp_advert_interval)
        {
            if (a->interface.bgp_advert_interval)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"neighbor %s advertisement-interval %d\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name, a->interface.bgp_neighbour, a->interface.bgp_advert_interval);
            }
            else
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"no neighbor %s advertisement-interval\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name, a->interface.bgp_neighbour);
            }

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to set BGP advertisement interval");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Set BGP advertisement interval for %s to %d", a->interface.bgp_neighbour, a->interface.bgp_advert_interval);
            }
        }

        if (bp->bgp_next_hop_self != a->interface.bgp_next_hop_self)
        {
            if (a->interface.bgp_next_hop_self)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"neighbor %s next-hop-self\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name, a->interface.bgp_neighbour);
            }
            else
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"no neighbor %s next-hop-self\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name, a->interface.bgp_neighbour);
            }

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to set BGP next-hop-self");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Set BGP next-hop-self for %s to %d", a->interface.bgp_neighbour, a->interface.bgp_advert_interval);
            }
        }

        // maximum paths

        if (bgpp->bgp_maximum_paths_internal != a->interface.bgp_maximum_paths)
        {
            if (a->interface.bgp_maximum_paths == 0)
            {
                if (am_ibgp)
                {
                    snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"maximum-paths ibgp\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name);
                }
                else
                {
                    snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"maximum-paths\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name);
                }
            }
            else
            {
                if (am_ibgp)
                {
                    snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"maximum-paths ibgp %d\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name, a->interface.bgp_maximum_paths);
                }
                else
                {
                    snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"  -c \"address-family %s\" -c \"maximum-paths %d\"", VTYSH_FILENAME, ROUTING_POLICY->bgp_local_as, ip->name, a->interface.bgp_maximum_paths);
                }
            }

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to set BGP maximum-paths");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Set BGP maximum-paths for %s to %d", a->interface.bgp_neighbour, a->interface.bgp_maximum_paths);
            }
        }
    }

    DeleteItemList(address_families);

    if (bgpp->bgp_ipv6_neighbor_discovery_route_advertisement && a->interface.bgp_ipv6_neighbor_discovery_route_advertisement)
    {
        if (strcmp(bgpp->bgp_ipv6_neighbor_discovery_route_advertisement, a->interface.bgp_ipv6_neighbor_discovery_route_advertisement) != 0)
        {
            if (strcmp(a->interface.bgp_ipv6_neighbor_discovery_route_advertisement, "suppress") == 0)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"interface %s\" -c \"ipv6 nd suppress-ra\"", VTYSH_FILENAME, pp->promiser);
            }
            else
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"interface %s\" -c \"no ipv6 nd suppress-ra\"", VTYSH_FILENAME, pp->promiser);
            }

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to set BGP neighbour discovery route suppression");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Set BGP neighbour discovery route suppression %s", pp->promiser);
            }

        }
    }
}

/*****************************************************************************/

void KeepBGPLinkServiceControlPromises(CommonRouting *policy, CommonRouting *state)
{
    char comm[CF_BUFSIZE];

    if (policy == NULL || state == NULL)
    {
        Log(LOG_LEVEL_VERBOSE,"BGP not promised, skipping setup");
        return;
    }

    if (policy->bgp_local_as == 0 && state->bgp_local_as == 0)
    {
        Log(LOG_LEVEL_VERBOSE,"No BGP AS is defined for this host, skipping setup");
        return;
    }

    // Log file

    if (policy->log_file)
    {
        if (state->log_file == NULL || strcmp(policy->log_file, state->log_file) == 0)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"log file %s\"", VTYSH_FILENAME, policy->log_file);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to set keep promised routing service log file: %s", policy->log_file);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept routing service promise: log file to: %s", policy->log_file);
            }
        }
    }

    if (policy->password != state->password)
    {
        if ((state->password == NULL) || (policy->password && strcmp(policy->password, state->password) == 0))
        {
            if (policy->password == NULL || strcmp(policy->password, "none") == 0|| strcmp(policy->password, "disabled") == 0)
            {
                policy->enable_password = false;
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"no enable password\"", VTYSH_FILENAME);
            }
            else
            {
                policy->enable_password = true;
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"password %s\"", VTYSH_FILENAME, policy->password);
            }

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to set keep promised routing service password");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept routing service promise: password set");
            }

            if (policy->enable_password)
            {
                snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"enable password %s\"", VTYSH_FILENAME, policy->password);

                if (!ExecRouteCommand(comm))
                {
                    Log(LOG_LEVEL_VERBOSE, "Failed to set keep routing service password promise");
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE, "Kept routing service promise: password set to %s", policy->password);
                }
            }
        }
    }

    // Adjacency change logging

    if (policy->bgp_local_as != state->bgp_local_as)
    {
        // Remove existing AS
        snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"no router bgp %d\"", VTYSH_FILENAME, state->bgp_local_as);

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise to remove existing AS %d ", state->bgp_local_as);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept BGP promise - removed current AS %d", state->bgp_local_as);
        }

        if (policy->bgp_local_as == 0)
        {
            // We're done - deactivate AS
            return;
        }

        // Add back correct AS

        snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\"", VTYSH_FILENAME, policy->bgp_local_as);

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: establish ASN %d on this router", policy->bgp_local_as);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: establish ASN %d on this router", policy->bgp_local_as);
        }
    }

    if (policy->bgp_graceful_restart != state->bgp_graceful_restart)
    {
        if (state->bgp_graceful_restart)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"no bgp graceful-restart\"", VTYSH_FILENAME, policy->bgp_local_as);
        }
        else
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"bgp graceful-restart\"", VTYSH_FILENAME, policy->bgp_local_as);
        }

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: set graceful-restart to %d", policy->bgp_graceful_restart);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: neighbor change logging");
        }
    }

    if (policy->bgp_log_neighbor_changes != state->bgp_log_neighbor_changes)
    {
        if (state->bgp_log_neighbor_changes)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"no bgp log-neighbor-changes\"", VTYSH_FILENAME, policy->bgp_local_as);
        }
        else
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"bgp log-neighbor-changes\"", VTYSH_FILENAME, policy->bgp_local_as);
        }

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: log neighbor change logging");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: neighbor change logging");
        }
    }

    // Router id / "loopback"

    if (policy->bgp_router_id)
    {
        if (state->bgp_router_id == NULL || strcmp(policy->bgp_router_id, state->bgp_router_id) != 0)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"router-id %s\"", VTYSH_FILENAME, policy->bgp_local_as, policy->bgp_router_id);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: router-id is %s", policy->bgp_router_id);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: router-id is %s", policy->bgp_router_id);
            }
        }
    }
    else
    {
        if (state->bgp_router_id && strcmp(state->bgp_router_id, "0.0.0.0") == 0) // empty state is 0.0.0.0 always present
        {
        }
        else
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"no bgp router-id\"", VTYSH_FILENAME, policy->bgp_local_as);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: no router-id");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: removed/reset router-id");
            }
        }
    }

    // Route redistribution
    // kernel

    if (policy->bgp_redistribute_kernel != state->bgp_redistribute_kernel)
    {
        if (state->bgp_redistribute_kernel)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgpp %d \" -c \"no redistribute kernel\"", VTYSH_FILENAME, policy->bgp_local_as);
        }
        else
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"redistribute kernel\"", VTYSH_FILENAME, policy->bgp_local_as);
        }

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: on redistribute kernel");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: on redistribute kernel");
        }
    }

    // connected

    if (policy->bgp_redistribute_connected != state->bgp_redistribute_connected)
    {
        if (state->bgp_redistribute_connected)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgpp %d \" -c \"no redistribute connected\"", VTYSH_FILENAME, policy->bgp_local_as);
        }
        else
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"redistribute connected\"", VTYSH_FILENAME, policy->bgp_local_as);
        }

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: on redistribute connected");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: on redistribute connected");
        }
    }

    // static

    if (policy->bgp_redistribute_static != state->bgp_redistribute_static)
    {
        if (state->bgp_redistribute_static)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgpp %d \" -c \"no redistribute static\"", VTYSH_FILENAME, policy->bgp_local_as);
        }
        else
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"redistribute static\"", VTYSH_FILENAME, policy->bgp_local_as);
        }

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: on redistribute static");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: on redistribute static");
        }
    }

    // ospf

    if (policy->bgp_redistribute_ospf != state->bgp_redistribute_ospf)
    {
        if (state->bgp_redistribute_ospf)
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgpp %d \" -c \"no redistribute ospf\"", VTYSH_FILENAME, policy->bgp_local_as);
        }
        else
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"redistribute ospf\"", VTYSH_FILENAME, policy->bgp_local_as);
        }

        if (!ExecRouteCommand(comm))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: on redistribute ospf");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: on redistribute ospf");
        }
    }

    // Now handle manually advertised networks - will there be transient behavioural issues?

    for (Rlist *rp = policy->bgp_advertisable_v4_networks; rp != NULL; rp=rp->next)
    {
        if (!RlistKeyIn(state->bgp_advertisable_v4_networks, rp->val.item))
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"address-family ipv4 unicast\" -c \"network %s\"", VTYSH_FILENAME, policy->bgp_local_as, (char *)rp->val.item);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: add ipv4 network %s", (char *)rp->val.item);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: added ipv4 network %s", (char *)rp->val.item);
            }
        }
    }

    // Purge others not in policy

    for (Rlist *rp = state->bgp_advertisable_v4_networks; rp != NULL; rp=rp->next)
    {
        if (!RlistKeyIn(policy->bgp_advertisable_v4_networks, rp->val.item))
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"address-family ipv4 unicast\" -c \"no network %s\"", VTYSH_FILENAME, policy->bgp_local_as, (char *)rp->val.item);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: purge ipv4 network");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: purged ipv4 network %s", (char *)rp->val.item);
            }
        }
    }

    // v6

    for (Rlist *rp = policy->bgp_advertisable_v6_networks; rp != NULL; rp=rp->next)
    {
        if (!IPAddressInList(state->bgp_advertisable_v6_networks, rp->val.item))
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"address-family ipv6\" -c \"network %s\"", VTYSH_FILENAME, policy->bgp_local_as, (char *)rp->val.item);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: add ipv6 network");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: added ipv6 network %s", (char *)rp->val.item);
            }
        }
    }

    // Purge others not in policy

    for (Rlist *rp = state->bgp_advertisable_v6_networks; rp != NULL; rp=rp->next)
    {
        if (!IPAddressInList(policy->bgp_advertisable_v6_networks, rp->val.item))
        {
            snprintf(comm, CF_BUFSIZE, "%s -c \"configure terminal\" -c \"router bgp %d\" -c \"address-family ipv6\" -c \"no network %s\"", VTYSH_FILENAME, policy->bgp_local_as, (char *)rp->val.item);

            if (!ExecRouteCommand(comm))
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to keep BGP promise: purge ipv6 network");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Kept BGP promise: purged ipv6 network %s", (char *)rp->val.item);
            }
        }
    }
}

/*****************************************************************************/

int QueryBGPInterfaceState(EvalContext *ctx, const Attributes *a, const Promise *pp, LinkStateBGP *bgpp)
{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    char *family = xstrdup("ipv4 unicast");
    char search[CF_MAXVARSIZE];
    char comm[CF_BUFSIZE];
    RouterCategory state = CF_RC_INITIAL;

    IdempPrependItem(&(bgpp->bgp_advertise_families), family, NULL);

    snprintf(search, CF_MAXVARSIZE, "interface %s", pp->promiser);
    snprintf(comm, CF_BUFSIZE, "%s -c \"show running-config\"", VTYSH_FILENAME);

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", VTYSH_FILENAME);
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

        if (*line == '!')
        {
            state = CF_RC_INITIAL;
            continue;
        }

        if (strncmp(line, "router bgp", strlen("router bgp")) == 0)
        {
            state = CF_RC_BGP;
        }

        if (strcmp(line, search) == 0)
        {
            state = CF_RC_INTERFACE;
        }

        if (strncmp(line, " address-family", strlen(" address-family")) == 0)
        {
            free(family);
            family = xstrdup(line + strlen(" address-family "));
            state = CF_RC_BGP;
        }

        switch(state)
        {
        case CF_RC_OSPF:
            break;

        case CF_RC_INTERFACE:
            if (strcmp(line, " ipv6 nd suppress-ra") == 0)
            {
                bgpp->bgp_ipv6_neighbor_discovery_route_advertisement = "suppress";
                Log(LOG_LEVEL_VERBOSE, "Found neighbour discovery route advertisement suppression for interface %s\n", pp->promiser);
            }

            if (strcmp(line, " no ipv6 nd suppress-ra") == 0)
            {
                bgpp->bgp_ipv6_neighbor_discovery_route_advertisement = "allow";
                Log(LOG_LEVEL_VERBOSE, "Found neighbour discovery route advertisement suppression for interface %s\n", pp->promiser);
            }

            break;

        case CF_RC_INITIAL:
        case CF_RC_BGP:
            HandleBGPInterfaceConfig(ctx, bgpp, line, a, pp, family);
            break;
        default:
            break;
        }
    }

    free(line);
    cf_pclose(pfp);
    free(family);
    return true;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

static void HandleOSPFServiceConfig(EvalContext *ctx, CommonRouting *ospfp, char *line)
{
    int i, metric = 0, metric_type = 2;
    char *sp;

    // These don't really belong in OSPF??

    if ((i = GetIntAfter(line, "log timestamp precision")) != CF_NOINT)
    {
        Log(LOG_LEVEL_VERBOSE, "Found log timestamp precision of %d", i);
        ospfp->log_timestamp_precision = i;
        return;
    }

    if ((sp = GetStringAfter(line, "log file")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Log file is %s", sp);
        ospfp->log_file = sp;
        return;
    }

    if ((sp = GetStringAfter(line, "password")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Password is %s", sp);
        ospfp->password = sp;
        return;
    }

    if ((sp = GetStringAfter(line, "enable password")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Password is %s", sp);
        ospfp->password = sp;
        ospfp->enable_password = true;
        return;
    }

    // ospf

    if ((sp = GetStringAfter(line, " log-adjacency-changes")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Log adjacency setting is: %s", sp);
        ospfp->ospf_log_adjacency_changes = sp;
        return;
    }

    if ((sp = GetStringAfter(line, " ospf router-id")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Router-id (aka loopback) %s", sp);
        ospfp->ospf_router_id = sp;
        return;
    }

    if (GetMetricIf(line, " redistribute kernel", &metric, &metric_type))
    {
        ospfp->ospf_redistribute_kernel = true;
        if (metric != CF_NOINT)
        {
            ospfp->ospf_redistribute_kernel_metric = metric;
            ospfp->ospf_redistribute_kernel_metric_type = metric_type;
        }

        Log(LOG_LEVEL_VERBOSE, "Discovered redistribution of kernel routes with metric %d (type %d)", metric, metric_type);
        return;
    }

    if (GetMetricIf(line, " redistribute connected", &metric, &metric_type))
    {
        ospfp->ospf_redistribute_connected = true;
        if (metric != CF_NOINT)
        {
            ospfp->ospf_redistribute_connected_metric = metric;
            ospfp->ospf_redistribute_connected_metric_type = metric_type;
        }

        Log(LOG_LEVEL_VERBOSE, "Discovered redistribution of connected networks with metric %d (type %d)", metric, metric_type);
        return;
    }

    if (GetMetricIf(line, " redistribute static", &metric, &metric_type))
    {
        ospfp->ospf_redistribute_static = true;
        if (metric != CF_NOINT)
        {
            ospfp->ospf_redistribute_static_metric = metric;
            ospfp->ospf_redistribute_static_metric_type = metric_type;
        }

        Log(LOG_LEVEL_VERBOSE, "Discovered redistribution of static routes with metric %d (type %d)", metric, metric_type);
        return;
    }

    if (GetMetricIf(line, " redistribute bgp", &metric, &metric_type))
    {
        ospfp->ospf_redistribute_bgp = true;
        if (metric != CF_NOINT)
        {
            ospfp->ospf_redistribute_bgp_metric = metric;
            ospfp->ospf_redistribute_bgp_metric_type = metric_type;
        }

        Log(LOG_LEVEL_VERBOSE, "Discovered redistribution of bgp routes with metric %d (type %d)", metric, metric_type);
        return;
    }

    // Try to infer the ospf area

    if ((sp = GetStringAfter(line, " network")) != NULL)
    {
        char network[CF_MAXVARSIZE] = { 0 };
        char buffer[CF_SMALLBUF];
        int area = CF_NOINT;

        sscanf(sp, "%s", network);

        VarRef *ref = VarRefParseFromScope("ip_addresses", "sys");
        DataType value_type = CF_DATA_TYPE_NONE;
        Rlist *iplist= (Rlist *)EvalContextVariableGet(ctx, ref, &value_type);
        VarRefDestroy(ref);

        for (Rlist *rp = iplist; rp != NULL; rp=rp->next)
        {
            if (FuzzySetMatch(network, (char *)(rp->val.item)) == 0)
            {
                sscanf(line + strlen("   network") + strlen(network), "area 0.0.0.%d", &area);
                Log(LOG_LEVEL_VERBOSE, "Interface address %s seems to be in area %d (inferred from obsolete network area state)", (char *)(rp->val.item), area);
            }
        }
        free(sp);
        if (area != CF_NOINT)
        {
            snprintf(buffer, CF_SMALLBUF, "ospf_area_%d", area);
            EvalContextClassPutHard(ctx, buffer, "inventory,attribute_name=none,source=agent");
            EvalContextHeapPersistentSave(ctx, buffer, CF_PERSISTENCE, CONTEXT_STATE_POLICY_PRESERVE, "");
        }
        return;
    }
}

/*****************************************************************************/

static void HandleOSPFInterfaceConfig(EvalContext *ctx, LinkStateOSPF *ospfp, const char *line, const Attributes *a, const Promise *pp)
{
    int i;
    char *sp;

    if ((i = GetIntAfter(line, " ip ospf hello-interval")) != CF_NOINT)
    {
        Log(LOG_LEVEL_VERBOSE, "Discovered hello-interval %d", i);
        ospfp->ospf_hello_interval = i;
        return;
    }

    if ((i = GetIntAfter(line, " ip ospf priority")) != CF_NOINT)
    {
        Log(LOG_LEVEL_VERBOSE, "Router priority on this interface is %d", i);
        ospfp->ospf_priority = i;
        return;
    }

    if ((sp = GetStringAfter(line, " ip ospf network")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Link type is %s", sp);
        ospfp->ospf_link_type = sp;
        return;
    }

    if ((sp = GetStringAfter(line, " ip ospf message-digest-key 1 md5")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Authentication digest %s", sp);
        ospfp->ospf_authentication_digest = sp;
        return;
    }

    if ((sp = GetStringAfter(line, " ip ospf area")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "OSPF Area %s", sp);
        sscanf(sp, "%*d.%*d.%*d.%d", &(ospfp->ospf_area));
        free(sp);
        char buffer[CF_SMALLBUF];
        if (ospfp->ospf_area != CF_NOINT)
        {
            snprintf(buffer, CF_SMALLBUF, "ospf_area_%d", ospfp->ospf_area);
            EvalContextClassPutHard(ctx, buffer, "inventory,attribute_name=none,source=agent");
            EvalContextHeapPersistentSave(ctx, buffer, CF_PERSISTENCE, CONTEXT_STATE_POLICY_PRESERVE, "");
        }
        return;
    }

    char search[CF_MAXVARSIZE];
    snprintf(search, CF_MAXVARSIZE, " passive-interface %s", pp->promiser);

    if (strstr(line, search)) // Not kept under interface config(!)
    {
        ospfp->ospf_passive_interface = true;
    }

    // If we can't get the area directly, look for "network <addr> area 0.0.0.n"

    if ((ospfp->ospf_area == CF_NOINT) && ((sp = GetStringAfter(line, " network")) != NULL))
    {
        char network[CF_MAXVARSIZE] = { 0 };
        char address[CF_MAX_IP_LEN] = { 0 };
        char buffer[CF_SMALLBUF];
        int area = CF_NOINT;

        sscanf(sp, "%s", network);

        for (Rlist *rp = a->interface.v4_addresses; rp != NULL; rp = rp->next)
        {
            sscanf((char *)(rp->val.item), "%[^/\n]", address);

            if (FuzzySetMatch(network, address) == 0)
            {
                sscanf(line + strlen("   network") + strlen(network), "area 0.0.0.%d", &area);
                Log(LOG_LEVEL_VERBOSE, "Interface %s seems to be in area %d (inferred from obsolete network area state)", pp->promiser, area);
                ospfp->ospf_area = area;
            }
        }
        free(sp);
        if (ospfp->ospf_area != CF_NOINT)
        {
            snprintf(buffer, CF_SMALLBUF, "ospf_area_%d", ospfp->ospf_area);
            EvalContextClassPutHard(ctx, buffer, "inventory,attribute_name=none,source=agent");
            EvalContextHeapPersistentSave(ctx, buffer, CF_PERSISTENCE, CONTEXT_STATE_POLICY_PRESERVE, "");
        }
        return;
    }

    if (ospfp->ospf_area != CF_NOINT)
    {
        snprintf(search, CF_MAXVARSIZE, " area 0.0.0.%d", ospfp->ospf_area);

        if ((sp = GetStringAfter(line, search)) != NULL)
        {
            if (strstr(sp, "stub"))
            {
                ospfp->ospf_area_type = 's'; // stub, nssa etc
            }

            if (strstr(line, "no-summary"))
            {
                ospfp->ospf_abr_summarization = false; // Not "no-summary"
            }
            else
            {
                ospfp->ospf_abr_summarization = true;
            }

            free(sp);
        }
    }
}

/*****************************************************************************/

static void HandleBGPServiceConfig(ARG_UNUSED EvalContext *ctx, CommonRouting *bgpp, char *line)
{
    int i;
    char *sp;

    // These don't really belong in BGP or OSPF, but either can set them??

    if ((i = GetIntAfter(line, "log timestamp precision")) != CF_NOINT)
    {
        Log(LOG_LEVEL_VERBOSE, "Found log timestamp precision of %d", i);
        bgpp->log_timestamp_precision = i;
        return;
    }

    if ((sp = GetStringAfter(line, "log file")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Log file is %s", sp);
        bgpp->log_file = sp;
        return;
    }

    if ((sp = GetStringAfter(line, "password")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Password is %s", sp);
        bgpp->password = sp;
        return;
    }

    if ((sp = GetStringAfter(line, "enable password")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Password is %s", sp);
        bgpp->password = sp;
        bgpp->enable_password = true;
        return;
    }

    // bgp

    if ((sp = GetStringAfter(line, " bgp router-id")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "BGP Router ID is %s", sp);
        bgpp->bgp_router_id = sp;
        return;
    }

    if ((i = GetIntAfter(line, "router bgp")) != CF_NOINT)
    {
        Log(LOG_LEVEL_VERBOSE, "Discovered BGP ASN %d", i);
        bgpp->bgp_local_as = i;
        return;
    }

    if (strcmp(line, " bgp graceful-restart") == 0)
    {
        bgpp->bgp_graceful_restart = true;
        Log(LOG_LEVEL_VERBOSE,"Found graceful-restart, global setting\n");
    }

    if ((sp = GetStringAfter(line, "log file")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Log file is %s", sp);
        bgpp->log_file = sp;
        return;
    }

    if ((sp = GetStringAfter(line, " network")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Adding network %s", sp);

        if (IsIPV6Address(sp))
        {
            RlistPrependScalarIdemp(&(bgpp->bgp_advertisable_v6_networks), sp);
        }
        else
        {
            RlistPrependScalarIdemp(&(bgpp->bgp_advertisable_v4_networks), sp);
        }
        return;
    }

    if ((sp = GetStringAfter(line, " redistribute")) != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Adding redistribution source %s", sp);

        if (strcmp(sp, "kernel") == 0)
        {
            bgpp->bgp_redistribute_kernel = true;
        }

        if (strcmp(sp, "static") == 0)
        {
            bgpp->bgp_redistribute_static = true;
        }

        if (strcmp(sp, "ospf") == 0)
        {
            bgpp->bgp_redistribute_ospf = true;
        }

        if (strcmp(sp, "connected") == 0)
        {
            bgpp->bgp_redistribute_connected = true;
        }

        return;
    }
}

/*****************************************************************************/

static void HandleBGPInterfaceConfig(ARG_UNUSED EvalContext *ctx, LinkStateBGP *bgpp, const char *line, ARG_UNUSED const Attributes *a, ARG_UNUSED const Promise *pp, char *family)
{
    int i;
    BGPNeighbour *bp;
    int as;
    char peer_id[CF_MAX_IP_LEN];

    if ((i = GetIntAfter(line, "router bgp")) != CF_NOINT)
    {
        Log(LOG_LEVEL_VERBOSE, "Discovered BGP local ASN %d", i);
        bgpp->bgp_local_as = i;
        return;
    }

    if (strncmp(line, " neighbor", strlen(" neighbor")) == 0)
    {
        as = CF_NOINT;
        peer_id[0] = '\0';

        sscanf(line+strlen(" neighbor"), "%63s", peer_id);

        const char *args = line + strlen(" neighbor") + strlen(peer_id) + 2;

        bp = GetPeer(peer_id, bgpp);

        if (strstr(args, "remote-as"))
        {
            sscanf(args,"remote-as %d", &as);
            bp->bgp_remote_as = as;

            if (as == bgpp->bgp_local_as)
            {
                bgpp->have_ibgp_peers = true;
                Log(LOG_LEVEL_VERBOSE,"Found AS %d for ibgp peer %s\n", as, peer_id);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE,"Found AS %d for ebgp peer %s\n", as, peer_id);
                bgpp->have_ebgp_peers = true;
            }

            return;
        }

        if (sscanf(args, "ebgp-multihop %d", &(bp->bgp_multihop)))
        {
            Log(LOG_LEVEL_VERBOSE,"Found ebgp-multihop %d for peer %s\n", bp->bgp_multihop, peer_id);
            return;
        }

        if (sscanf(args, "ttl-security %d", &(bp->bgp_ttl_security)))
        {
            Log(LOG_LEVEL_VERBOSE,"Found ttl-security %d for peer %s\n", bp->bgp_ttl_security, peer_id);
            return;
        }

        if (strcmp(args, "activate") == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Found peer %s activation for family %s\n", peer_id, family);
            IdempPrependItem(&(bgpp->bgp_advertise_families), family, NULL);
            return;
        }

        if (strcmp(args, "soft-reconfiguration inbound") == 0)
        {
            Log(LOG_LEVEL_VERBOSE,"Found inbound soft reconfiguration for peer %s\n", peer_id);
            bp->bgp_soft_inbound = true;
            return;
        }

        if (strcmp(args, "next-hop-self") == 0)
        {
            Log(LOG_LEVEL_VERBOSE,"Perceived next-hop-self for peer %s\n", peer_id);
            bp->bgp_next_hop_self = true;
            return;
        }

        if (strcmp(args, "route-reflector-client") == 0)
        {
            Log(LOG_LEVEL_VERBOSE,"Route-reflector role detected for peer %s\n", peer_id);
            bp->bgp_reflector = true;
            return;
        }

        if (sscanf(args, "advertisement-interval %d", &(bp->bgp_advert_interval)))
        {
            Log(LOG_LEVEL_VERBOSE,"Found advertisement interval %d for peer %s\n", bp->bgp_advert_interval, peer_id);
            return;
        }
    }

    if (strncmp(line, " no neighbor", strlen(" no neighbor")) == 0)
    {
        as = CF_NOINT;
        peer_id[0] = '\0';

        sscanf(line+strlen(" no neighbor"), "%63s", peer_id);

        const char *args = line + strlen(" no neighbor") + strlen(peer_id) + 2;

        bp = GetPeer(peer_id, bgpp);

        if (strcmp(args, "activate") == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Found peer %s de-activation for family %s\n", peer_id, family);
            DeleteItem(&(bgpp->bgp_advertise_families), ReturnItemIn(bgpp->bgp_advertise_families,family));
            return;
        }
    }

    if (sscanf(line, " maximum-paths %d", &(bgpp->bgp_maximum_paths_external)))
    {
        Log(LOG_LEVEL_VERBOSE,"Found maximum-paths %d for peer %s\n", bgpp->bgp_maximum_paths_external, peer_id);
        return;
    }

    if (sscanf(line, " maximum-paths ibgp %d", &(bgpp->bgp_maximum_paths_internal)))
    {
        Log(LOG_LEVEL_VERBOSE,"Found maximum-paths ibgp %d for peer %s\n", bgpp->bgp_maximum_paths_internal, peer_id);
        return;
    }
}

/*****************************************************************************/
/* tools                                                                     */
/*****************************************************************************/

static BGPNeighbour *GetPeer(char *id, LinkStateBGP *bgpp)
{
    BGPNeighbour *bp;

    if (strlen(id) == 0)
    {
        return NULL;
    }

    if ((bp = IsNeighbourIn(bgpp->bgp_peers, id)))
    {
        return bp;
    }

    bp = xcalloc(sizeof(BGPNeighbour), 1);
    bp->next = bgpp->bgp_peers;
    bgpp->bgp_peers = bp;
    bp->bgp_neighbour = xstrdup(id);
    return bp;
}

/*****************************************************************************/

BGPNeighbour *IsNeighbourIn(BGPNeighbour *list, char *name)
{
    for (BGPNeighbour *bp = list;  bp != NULL; bp=bp->next)
    {
        if (strcmp(name, bp->bgp_neighbour) == 0)
        {
            return bp;
        }
    }

    return NULL;
}

/*****************************************************************************/

static int GetIntAfter(const char *line, const char *prefix)
{
    int prefix_length = strlen(prefix);
    int intval = CF_NOINT;

    if (strncmp(line, prefix, prefix_length) == 0)
    {
        sscanf(line+prefix_length, "%d", &intval);
    }

    return intval;
}

/*****************************************************************************/

static char *GetStringAfter(const char *line, const char *prefix)
{
    int prefix_length = strlen(prefix);
    char buffer[CF_MAXVARSIZE];

    buffer[0] = '\0';

    if (strncmp(line, prefix, prefix_length) == 0)
    {
        sscanf(line+prefix_length, "%128s", buffer);
        return xstrdup(buffer);
    }

    return NULL;
}

/*****************************************************************************/

static bool GetMetricIf(const char *line, const char *prefix, int *metric, int *metric_type)
{
    int prefix_length = strlen(prefix);
    bool result = false;

    // parse e.g.: redistribute connected metric 4 metric-type 1

    if (strncmp(line, prefix, prefix_length) == 0)
    {
        result = true;

        if (strstr(line+prefix_length, "metric"))
        {
            *metric = GetIntAfter(line+prefix_length, " metric");

            if (strstr(line+prefix_length, "metric-type"))
            {
                *metric_type = 1;
            }
        }
    }

    return result;
}


/****************************************************************************/

static int ExecRouteCommand(char *cmd)
{
    FILE *pfp;
    size_t line_size = CF_BUFSIZE;
    int ret = true;

    if (DONTDO)
    {
        Log(LOG_LEVEL_VERBOSE, "Need to execute command '%s' for interface routing configuration", cmd);
        return true;
    }

    if ((pfp = cf_popen(cmd, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to execute '%s'", cmd);
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

        // VTYSH errors:
        // There is already same network statement
        // Can't find specified network area configuration
        // % Unknown command.

        if (strlen(line) > 0)
        {
            //Log(LOG_LEVEL_ERR, "%s", line);
            ret = false;
            break;
        }
    }

    free(line);
    cf_pclose(pfp);
    return ret;
}
