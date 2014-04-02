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

/*****************************************************************************/
/*                                                                           */
/* File: network_services.c                                                  */
/*                                                                           */
/* Created: Tue Apr  1 12:29:21 2014                                         */
/*                                                                           */
/*****************************************************************************/

#define VTYSH_FILENAME "/usr/bin/vtysh"

static void HandleOSPFServiceConfig(EvalContext *ctx, CommonOSPF *ospfp, char *line);
static void HandleOSPFInterfaceConfig(EvalContext *ctx, LinkStateOSPF *ospfp, const char *line, const char *iname);
static char *GetStringAfter(const char *line, const char *prefix);
static int GetIntAfter(const char *line, const char *prefix);
static bool GetMetricIf(const char *line, const char *prefix, int *metric, int *metric_type);

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
    OSPF_ACTIVE = (CommonOSPF *)calloc(sizeof(CommonOSPF), 1);

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
          OSPF_ACTIVE->ospf_log_adjacency_changes = (char *) value;
          Log(LOG_LEVEL_VERBOSE, "Setting ospf_log_adjacency_changes to %s", (const char *)value);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_LOG_TIMESTAMP_PRECISION].lval) == 0)
          {
          OSPF_ACTIVE->log_timestamp_precision = (int) IntFromString(value); // 0,6
          Log(LOG_LEVEL_VERBOSE, "Setting the logging timestamp precision to %d microseconds", OSPF_ACTIVE->log_timestamp_precision);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_ROUTER_ID].lval) == 0)
          {
          OSPF_ACTIVE->ospf_router_id = (char *)value;
          Log(LOG_LEVEL_VERBOSE, "Setting the router-id (trad. \"loopback address\") to %s", (char *)value);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_LOG_FILE].lval) == 0)
          {
          OSPF_ACTIVE->log_file = (char *)value;
          Log(LOG_LEVEL_VERBOSE, "Setting the log file to %s", (char *)value);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE].lval) == 0)
          {
          for (const Rlist *rp = value; rp != NULL; rp = rp->next)
             {
             if (strcmp(rp->val.item, "kernel"))
                {
                OSPF_ACTIVE->ospf_redistribute_kernel = true;
                Log(LOG_LEVEL_VERBOSE, "Setting ospf redistribution from kernel FIB");
                continue;
                }
             if (strcmp(rp->val.item, "connected"))
                {
                OSPF_ACTIVE->ospf_redistribute_connected = true;
                Log(LOG_LEVEL_VERBOSE, "Setting ospf redistribution from connected networks");
                continue;
                }
             if (strcmp(rp->val.item, "static"))
                {
                Log(LOG_LEVEL_VERBOSE, "Setting ospf redistribution from static FIB");
                OSPF_ACTIVE->ospf_redistribute_static = true;
                continue;
                }
             if (strcmp(rp->val.item, "bgp"))
                {
                Log(LOG_LEVEL_VERBOSE, "Setting ospf to allow bgp route injection");
                OSPF_ACTIVE->ospf_redistribute_bgp = true;
                continue;
                }
             }

          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_KERNEL_METRIC].lval) == 0)
          {
          OSPF_ACTIVE->ospf_redistribute_kernel_metric = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric for kernel routes to %d", OSPF_ACTIVE->ospf_redistribute_kernel_metric);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_CONNECTED_METRIC].lval) == 0)
          {
          OSPF_ACTIVE->ospf_redistribute_connected_metric = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric for kernel routes to %d", OSPF_ACTIVE->ospf_redistribute_connected_metric);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_STATIC_METRIC].lval) == 0)
          {
          OSPF_ACTIVE->ospf_redistribute_static_metric = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric for static routes to %d", OSPF_ACTIVE->ospf_redistribute_kernel_metric);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_BGP_METRIC].lval) == 0)
          {
          OSPF_ACTIVE->ospf_redistribute_bgp_metric = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric for bgp routes to %d", OSPF_ACTIVE->ospf_redistribute_kernel_metric);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_KERNEL_METRIC_TYPE].lval) == 0)
          {
          OSPF_ACTIVE->ospf_redistribute_kernel_metric_type = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric-type for kernel routes to %d", OSPF_ACTIVE->ospf_redistribute_kernel_metric_type);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_CONNECTED_METRIC_TYPE].lval) == 0)
          {
          OSPF_ACTIVE->ospf_redistribute_connected_metric_type = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric-type for kernel routes to %d", OSPF_ACTIVE->ospf_redistribute_connected_metric_type);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_STATIC_METRIC_TYPE].lval) == 0)
          {
          OSPF_ACTIVE->ospf_redistribute_static_metric_type = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric-type for static routes to %d", OSPF_ACTIVE->ospf_redistribute_kernel_metric_type);
          continue;
          }

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_REDISTRIBUTE_BGP_METRIC_TYPE].lval) == 0)
          {
          OSPF_ACTIVE->ospf_redistribute_bgp_metric_type = (int) IntFromString(value);
          Log(LOG_LEVEL_VERBOSE, "Setting metric-type for bgp routes to %d", OSPF_ACTIVE->ospf_redistribute_kernel_metric_type);
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

    if ((*line = '!'))
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
 return true;
}

/*****************************************************************************/

int QueryOSPFInterfaceState(EvalContext *ctx, const Promise *pp, LinkStateOSPF *ospfp)

{
 FILE *pfp;
 size_t line_size = CF_BUFSIZE;
 char *line = xmalloc(line_size);
 char comm[CF_BUFSIZE];
 char search[CF_MAXVARSIZE];
 RouterCategory state = CF_RC_INITIAL;

 ospfp->ospf_priority = 1;  // default value seems to be 1, in which case this line does not appear

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

    if ((*line = '!'))
       {
       state = CF_RC_INITIAL;
       continue;
       }

    if (strcmp(line, search) == 0)
       {
       state = CF_RC_INTERFACE;
       continue;
       }

    switch(state)
       {
       case CF_RC_OSPF:
       case CF_RC_INTERFACE:
           HandleOSPFInterfaceConfig(ctx, ospfp, line, pp->promiser);
           break;

       case CF_RC_INITIAL:
       case CF_RC_BGP:
       default:
           break;
       }
    }

 free(line);
 cf_pclose(pfp);
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
 int i, metric = CF_NOINT, metric_type = 2;
 char *sp;

 // These don't really belong in OSPF??

 if ((i = GetIntAfter(line, "log timestamp precision")) != CF_NOINT)
    {
    Log(LOG_LEVEL_VERBOSE, "Found log timestamp precision of %d", i);
    ospfp->log_timestamp_precision = i;
    return;
    }

 if ((sp = GetStringAfter(line, "log file")) == NULL)
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

static void HandleOSPFInterfaceConfig(EvalContext *ctx, LinkStateOSPF *ospfp, const char *line, const char *iname)
{
 int i;
 char *sp;

 // These don't really belong in OSPF??

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
 snprintf(search, CF_MAXVARSIZE, "passive-interface %s", iname);

 if (strstr(line, search)) // Not kept under interface config(!)
    {
    ospfp->ospf_passive_interface = true;
    }

 //   bool ospf_passive_interface;  Handled outside this fn, as it doesn't belong to interfaces in ospf(!)

 if (ospfp->ospf_area != 0)
    {
    snprintf(search, CF_MAXVARSIZE, "area 0.0.0.%d", ospfp->ospf_area);

    if (strstr(line, search))
       {
       if (strstr(line, "stub"))
          {
          ospfp->ospf_area_type = xstrdup("stub"); // stub, nssa etc
          }

       if (strstr(line, "no-summary"))
          {
          ospfp->ospf_abr_summarization = false; // Not "no-summary"
          }
       else
          {
          ospfp->ospf_abr_summarization = true;
          }
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
    }

 return xstrdup(buffer);
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
       *metric = GetIntAfter(line+prefix_length, "metric");

       if (strstr(line+prefix_length, "metric-type"))
          {
          *metric_type = 1;
          }
       }
    }

 return result;
}
