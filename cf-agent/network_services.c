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
#include <mod_common.h>
#include <conversion.h>
#include <addr_lib.h>

/*****************************************************************************/
/*                                                                           */
/* File: network_services.c                                                  */
/*                                                                           */
/* Created: Tue Apr  1 12:29:21 2014                                         */
/*                                                                           */
/*****************************************************************************/

#define VTYSH_FILENAME "/usr/bin/vtysh"

static void HandleOSPFServiceConfig(EvalContext *ctx, CommonOSPF *ospfp, char *line);
static void HandleOSPFInterfaceConfig(EvalContext *ctx, LinkStateOSPF *ospfp, const char *line, const Attributes *a, const Promise *pp);
static char *GetStringAfter(const char *line, const char *prefix);
static int GetIntAfter(const char *line, const char *prefix);
static bool GetMetricIf(const char *line, const char *prefix, int *metric, int *metric_type);

/*****************************************************************************/

CommonOSPF *NewOSPFState()
{
return (CommonOSPF *)calloc(sizeof(CommonOSPF), 1);
}

/*****************************************************************************/

void DeleteOSPFState(CommonOSPF *state)
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
   (state->ospf_router_id);
   }

free(state);
}

/*****************************************************************************/

void InitializeOSPF(const Policy *policy, EvalContext *ctx)

{
 if (!HaveOSPFService(ctx))
    {
    return;
    }

 // Look for the control body for ospf
 Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_OSPF);

 if (constraints)
    {
    OSPF_POLICY = (CommonOSPF *)calloc(sizeof(CommonOSPF), 1);

    for (size_t i = 0; i < SeqLength(constraints); i++)
       {
       Constraint *cp = SeqAt(constraints, i);

       if (!IsDefinedClass(ctx, cp->classes))
          {
          continue;
          }

       VarRef *ref = VarRefParseFromScope(cp->lval, "control_ospf");
       const void *value = EvalContextVariableGet(ctx, ref, NULL);
       VarRefDestroy(ref);

       if (!value)
          {
          Log(LOG_LEVEL_ERR, "Unknown lval '%s' in ospf control body", cp->lval);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_LOG_ADJACENCY_CHANGES].lval) == 0)
          {
          OSPF_POLICY->ospf_log_adjacency_changes = (char *) value;
          Log(LOG_LEVEL_VERBOSE, "Setting ospf_log_adjacency_changes to %s", (const char *)value);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_LOG_TIMESTAMP_PRECISION].lval) == 0)
          {
          OSPF_POLICY->log_timestamp_precision = (int) IntFromString(value); // 0,6
          Log(LOG_LEVEL_VERBOSE, "Setting the logging timestamp precision to %d microseconds", OSPF_POLICY->log_timestamp_precision);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_ROUTER_ID].lval) == 0)
          {
          OSPF_POLICY->ospf_router_id = (char *)value;
          Log(LOG_LEVEL_VERBOSE, "Setting the router-id (trad. \"loopback address\") to %s", (char *)value);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_LOG_FILE].lval) == 0)
          {
          OSPF_POLICY->log_file = (char *)value;
          Log(LOG_LEVEL_VERBOSE, "Setting the log file to %s", (char *)value);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE].lval) == 0)
          {
          for (const Rlist *rp = value; rp != NULL; rp = rp->next)
             {
             if (strcmp(rp->val.item, "kernel"))
                {
                OSPF_POLICY->ospf_redistribute_kernel = true;
                Log(LOG_LEVEL_VERBOSE, "Setting ospf redistribution from kernel FIB");
                continue;
                }
             if (strcmp(rp->val.item, "connected"))
                {
                OSPF_POLICY->ospf_redistribute_connected = true;
                Log(LOG_LEVEL_VERBOSE, "Setting ospf redistribution from connected networks");
                continue;
                }
             if (strcmp(rp->val.item, "static"))
                {
                Log(LOG_LEVEL_VERBOSE, "Setting ospf redistribution from static FIB");
                OSPF_POLICY->ospf_redistribute_static = true;
                continue;
                }
             if (strcmp(rp->val.item, "bgp"))
                {
                Log(LOG_LEVEL_VERBOSE, "Setting ospf to allow bgp route injection");
                OSPF_POLICY->ospf_redistribute_bgp = true;
                continue;
                }
             }

          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_KERNEL_METRIC].lval) == 0)
          {
          OSPF_POLICY->ospf_redistribute_kernel_metric = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric for kernel routes to %d", OSPF_POLICY->ospf_redistribute_kernel_metric);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_CONNECTED_METRIC].lval) == 0)
          {
          OSPF_POLICY->ospf_redistribute_connected_metric = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric for kernel routes to %d", OSPF_POLICY->ospf_redistribute_connected_metric);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_STATIC_METRIC].lval) == 0)
          {
          OSPF_POLICY->ospf_redistribute_static_metric = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric for static routes to %d", OSPF_POLICY->ospf_redistribute_kernel_metric);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_BGP_METRIC].lval) == 0)
          {
          OSPF_POLICY->ospf_redistribute_bgp_metric = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric for bgp routes to %d", OSPF_POLICY->ospf_redistribute_kernel_metric);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_KERNEL_METRIC_TYPE].lval) == 0)
          {
          OSPF_POLICY->ospf_redistribute_kernel_metric_type = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric-type for kernel routes to %d", OSPF_POLICY->ospf_redistribute_kernel_metric_type);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_CONNECTED_METRIC_TYPE].lval) == 0)
          {
          OSPF_POLICY->ospf_redistribute_connected_metric_type = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric-type for kernel routes to %d", OSPF_POLICY->ospf_redistribute_connected_metric_type);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_STATIC_METRIC_TYPE].lval) == 0)
          {
          OSPF_POLICY->ospf_redistribute_static_metric_type = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric-type for static routes to %d", OSPF_POLICY->ospf_redistribute_kernel_metric_type);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_BGP_METRIC_TYPE].lval) == 0)
          {
          OSPF_POLICY->ospf_redistribute_bgp_metric_type = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric-type for bgp routes to %d", OSPF_POLICY->ospf_redistribute_kernel_metric_type);
          continue;
          }
       }
    }
}

/*****************************************************************************/

int QueryOSPFServiceState(EvalContext *ctx, CommonOSPF *ospfp)

{
 FILE *pfp;
 size_t line_size = CF_BUFSIZE;
 char *line = xmalloc(line_size);
 char comm[CF_BUFSIZE];
 RouterCategory state = CF_RC_INITIAL;

    printf("xOSPF redistributing kernel routes: %d with metric %d (type %d)\n",
       ospfp->ospf_redistribute_kernel,ospfp->ospf_redistribute_kernel_metric,ospfp->ospf_redistribute_kernel_metric_type);
   printf("xOSPF redistributing connected networks: %d with metric %d (type %d)\n",
       ospfp->ospf_redistribute_connected, ospfp->ospf_redistribute_connected_metric, ospfp->ospf_redistribute_connected_metric_type);
   printf("xOSPF redistributing static routes: %d with metric %d (type %d)\n",
       ospfp->ospf_redistribute_static, ospfp->ospf_redistribute_static_metric, ospfp->ospf_redistribute_static_metric_type);
   printf("xOSPF redistributing bgp: %d with metric %d (type %d)\n",
       ospfp->ospf_redistribute_bgp, ospfp->ospf_redistribute_bgp_metric, ospfp->ospf_redistribute_bgp_metric_type);

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

    switch(state)
       {
       case CF_RC_INITIAL:
       case CF_RC_OSPF:
           HandleOSPFServiceConfig(ctx, ospfp, line);
           break;

       case CF_RC_INTERFACE:
       case CF_RC_BGP:
       default:
           break;
       }
    }

 free(line);
 cf_pclose(pfp);

   if (ospfp->log_file)
      {
      Log(LOG_LEVEL_VERBOSE, "OSPF log file: %s", ospfp->log_file);
      }

   Log(LOG_LEVEL_VERBOSE, "OSPF log timestamp precision: %d microseconds", ospfp->log_timestamp_precision);

   if (ospfp->ospf_log_adjacency_changes)
      {
      Log(LOG_LEVEL_VERBOSE, "OSPF log adjacency change logging: %s ", ospfp->ospf_log_adjacency_changes);
      }

   if (ospfp->ospf_router_id)
      {
      Log(LOG_LEVEL_VERBOSE, "OSPF router-id: %s ", ospfp->ospf_router_id);
      }

   //Log(LOG_LEVEL_VERBOSE,
   printf("OSPF redistributing kernel routes: %d with metric %d (type %d)\n",
       ospfp->ospf_redistribute_kernel,ospfp->ospf_redistribute_kernel_metric,ospfp->ospf_redistribute_kernel_metric_type);
   printf("OSPF redistributing connected networks: %d with metric %d (type %d)\n",
       ospfp->ospf_redistribute_connected, ospfp->ospf_redistribute_connected_metric, ospfp->ospf_redistribute_connected_metric_type);
   printf("OSPF redistributing static routes: %d with metric %d (type %d)\n",
       ospfp->ospf_redistribute_static, ospfp->ospf_redistribute_static_metric, ospfp->ospf_redistribute_static_metric_type);
   printf("OSPF redistributing bgp: %d with metric %d (type %d)\n",
       ospfp->ospf_redistribute_bgp, ospfp->ospf_redistribute_bgp_metric, ospfp->ospf_redistribute_bgp_metric_type);

 return true;
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

bool HaveOSPFService(EvalContext *ctx)

{ struct stat sb;

 if (IsDefinedClass(ctx, "cumulus"))
    {
    // Might want to check the port instead

    if (stat(VTYSH_FILENAME, &sb) == -1)
       {
       return false;
       }
    else
       {
       Log(LOG_LEVEL_VERBOSE, "Quagga link services interface found at %s", VTYSH_FILENAME);
       return true;
       }
    }

 return false;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

static void HandleOSPFServiceConfig(EvalContext *ctx, CommonOSPF *ospfp, char *line)
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
    printf("XXXXXXXXXXXXXXXx\n");
    ospfp->ospf_redistribute_bgp = true;
    if (metric != CF_NOINT)
       {
       ospfp->ospf_redistribute_bgp_metric = metric;
       ospfp->ospf_redistribute_bgp_metric_type = metric_type;
       }

    Log(LOG_LEVEL_VERBOSE, "Discovered redistribution of bgp routes with metric %d (type %d)", metric, metric_type);
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
    Log(LOG_LEVEL_VERBOSE, "Authentication digest %s", sp);
    sscanf(sp, "%*d.%*d.%*d.%d", &(ospfp->ospf_area));
    free(sp);
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
    return;
    }

 //  infer the area type

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
/* tools                                                                     */
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
