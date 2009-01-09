/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
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
   CfOut(cf_error,"","Logical start time %s ",ctime(&CFSTARTTIME));
   CfOut(cf_error,"","This sub-task started really at %s\n",ctime(&CFINITSTARTTIME));
   fflush(stdout);
   
   if (signum == SIGTERM || signum == SIGINT || signum == SIGHUP || signum == SIGSEGV || signum == SIGKILL|| signum == SIGPIPE)
      {
      struct CfLock best_guess;
      
      best_guess.lock = strdup(CFLOCK);
      best_guess.last = strdup(CFLAST);
      best_guess.log = strdup(CFLOG);
      YieldCurrentLock(best_guess);
      unlink(PIDFILE);
      EndAudit();
      closelog();
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

