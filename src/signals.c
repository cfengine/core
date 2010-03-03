/* 
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.

*/

/*****************************************************************************/
/*                                                                           */
/* File: signals.c                                                           */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"
#include "cf3.server.h"


void HandleSignals(int signum)
 
{
if (signum != SIGCHLD)
   {
   CfOut(cf_error,"","Received signal %d (%s) while doing [%s]",signum,SIGNALS[signum],CFLOCK);
   CfOut(cf_error,"","Logical start time %s ",cf_ctime(&CFSTARTTIME));
   CfOut(cf_error,"","This sub-task started really at %s\n",cf_ctime(&CFINITSTARTTIME));
   fflush(stdout);
   
   if (signum == SIGTERM || signum == SIGINT || signum == SIGHUP || signum == SIGSEGV || signum == SIGKILL|| signum == SIGPIPE)
      {
      SelfTerminatePrelude();
      exit(0);
      }
   else if (signum == SIGUSR1)
      {
      DEBUG= true;
      D2= true;
      }
   else if (signum == SIGUSR2)
      {
      DEBUG= false;
      D2= false;
      }
   else /* zombie cleanup - how hard does it have to be? */
      {
      }
   
   /* Reset the signal handler */
   signal(signum,HandleSignals);
   }
}

/*****************************************************************************/

void SelfTerminatePrelude()
{
struct CfLock best_guess;

CfOut(cf_verbose,"","Trying to remove lock - try %s",CFLOCK);
best_guess.lock = strdup(CFLOCK);
best_guess.last = strdup(CFLAST);
best_guess.log = strdup(CFLOG);
YieldCurrentLock(best_guess);
unlink(PIDFILE);
EndAudit();
GenericDeInitialize();
}
