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
          OSPF_ACTIVE->ospf_log_timestamp_precision = (int) IntFromString(value); // 0,6
          Log(LOG_LEVEL_VERBOSE, "Setting the ospf timestamp precision to %d microseconds", OSPF_ACTIVE->ospf_log_timestamp_precision);
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
          OSPF_ACTIVE->ospf_log_file = (char *)value;
          Log(LOG_LEVEL_VERBOSE, "Setting the ospf log fileto %s", (char *)value);
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
   
       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_EXTERNAL_METRIC_TYPE].lval) == 0)
          {
          OSPF_ACTIVE->ospf_external_metric_type = (int) IntFromString(value); // 1 or 2
          Log(LOG_LEVEL_VERBOSE, "Setting external metric type to %d", OSPF_ACTIVE->ospf_external_metric_type);
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
       }
    }
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
