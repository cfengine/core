/* 
   Copyright (C) 2008 - Mark Burgess

   This file is part of Cfengine 3 - written and maintained by Mark Burgess.
 
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
/* File: pipes.c                                                             */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

extern pid_t *CHILD;
extern int    MAXFD; /* Max number of simultaneous pipes */

/******************************************************************************/

int cf_pclose_def(FILE *pfp,struct Attributes a,struct Promise *pp)

{ int fd, status, wait_result;
  pid_t pid;

Debug("cf_pclose_def(pfp)\n");

if (CHILD == NULL)  /* popen hasn't been called */
   {
   return -1;
   }

fd = fileno(pfp);

if ((pid = CHILD[fd]) == 0)
   {
   return -1;
   }

CHILD[fd] = 0;

if (fclose(pfp) == EOF)
   {
   return -1;
   }

Debug("cfpopen_def - Waiting for process %d\n",pid); 

#ifdef HAVE_WAITPID

while(waitpid(pid,&status,0) < 0)
   {
   if (errno != EINTR)
      {
      cfPS(cf_inform,CF_FAIL,"",pp,a,"Finished script -- failed %s\n",pp->promiser);
      return -1;
      }
   else
      {
      cfPS(cf_inform,CF_INTERPT,"",pp,a,"Script %s -- interrupt\n",pp->promiser);
      }
   }

if (status == 0)
   {
   cfPS(cf_inform,CF_CHG,"",pp,a,"Finished script %s ok\n",pp->promiser);
   }
else
   {
   cfPS(cf_inform,CF_INTERPT,"",pp,a,"Finished script %s -- an error occurred\n",pp->promiser);
   }

return status; 
 
#else

while ((wait_result = wait(&status)) != pid)
   {
   if (wait_result <= 0)
      {
      cfPS(cf_inform,CF_CHG,"",pp,a,"Finished script %s - an error occurred\n",pp->promiser);
      return -1;
      }
   }

if (WIFSIGNALED(status))
   {
   cfPS(cf_inform,CF_FAIL,"",pp,a,"Finished script - failed %s\n",pp->promiser);
   return -1;
   }

if (!WIFEXITED(status))
   {
   cfPS(cf_inform,CF_FAIL,"",pp,a,"Finished script - failed %s\n",pp->promiser);
   return -1;
   }

if (WEXITSTATUS(status) == 0)
   {
   cfPS(cf_inform,CF_CHG,"",pp,a,"Finished script %s\n",pp->promiser);
   }
else
   {
   if (errno != EINTR)
      {
      cfPS(cf_inform,CF_FAIL,"",pp,a,"Finished script - failed %s\n",pp->promiser);
      return -1;
      }
   else
      {
      cfPS(cf_inform,CF_INTERPT,"",pp,a,"Script %s - interrupt\n",pp->promiser);
      }
   }

return (WEXITSTATUS(status));
#endif
}

