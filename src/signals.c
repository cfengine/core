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
   snprintf(OUTPUT,CF_BUFSIZE*2,"Received signal %d (%s) while doing [%s]",signum,SIGNALS[signum],CFLOCK);
   Chop(OUTPUT);
   CfLog(cferror,OUTPUT,"");
   snprintf(OUTPUT,CF_BUFSIZE*2,"Logical start time %s ",ctime(&CFSTARTTIME));
   Chop(OUTPUT);
   CfLog(cferror,OUTPUT,"");
   snprintf(OUTPUT,CF_BUFSIZE*2,"This sub-task started really at %s\n",ctime(&CFINITSTARTTIME));
   CfLog(cferror,OUTPUT,"");
   fflush(stdout);
   
   if (signum == SIGTERM || signum == SIGINT || signum == SIGHUP || signum == SIGSEGV || signum == SIGKILL|| signum == SIGPIPE)
      {
      unlink(PIDFILE);
      ReleaseCurrentLock();
      Close3AuditLog();
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
   signal(signum,HandleSignal);
   }
}
