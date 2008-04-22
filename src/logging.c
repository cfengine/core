/* 

        Copyright (C) 1994-
        Free Software Foundation, Inc.

   This file is part of GNU cfengine - written and maintained 
   by Mark Burgess, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: logging.c                                                           */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"
#include "cf3.server.h"

/*****************************************************************************/

void CloseAudit()

{ double total;
 char *version,type;
 
total = (double)(PR_KEPT+PR_NOTKEPT+PR_REPAIRED)/100.0;

if (GetVariable(CONTEXTID,"version",(void *)version,&type) != cf_notype)
   {
   }
else
   {
   version = "(not specified)";
   }

if (total == 0)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Outcome of version %s: No checks were scheduled\n",version);
   return;
   }
else
   {   
   snprintf(OUTPUT,CF_BUFSIZE,"Outcome of version %s: Promises observed to be kept %.0f%%, Promises repaired %.0f%%, Promises not repaired %.0f\%\n",
            version,
            (double)PR_KEPT/total,
            (double)PR_REPAIRED/total,
            (double)PR_NOTKEPT/total);
   }

CfLog(cfverbose,OUTPUT,"");
AuditLog('y',NULL,0,OUTPUT,CF_REPORT);

if (AUDIT && AUDITDBP)
   {
   AuditLog('y',NULL,0,"Cfagent closing",CF_NOP);
   AUDITDBP->close(AUDITDBP,0);
   }
}
