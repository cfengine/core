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

       if (strcmp(cp->lval, OSPF_CONTROLBODY[OSPF_CONTROL_HELLO_INTERVAL].lval) == 0)
          {
          printf("\n");
          Log(LOG_LEVEL_VERBOSE, "Setting maxconnections to %d", CFA_MAXTHREADS);
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
