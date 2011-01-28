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
/* File: unix.c                                                              */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifndef MINGW

/* newly created, used in timeout.c and transaction.c */

int Unix_GracefulTerminate(pid_t pid)

{ int res;
 
if ((res = kill(pid,SIGINT)) == -1)
   {
   sleep(1);
   res = 0;
   
   if ((res = kill(pid,SIGTERM)) == -1)
      {   
      sleep(5);
      res = 0;
      
      if ((res = kill(pid,SIGKILL)) == -1)
         {
         sleep(1);
         }
      }
   }

return (res == 0);
}

/*************************************************************/

int Unix_GetCurrentUserName(char *userName, int userNameLen)
{
struct passwd *user_ptr;

memset(userName, 0, userNameLen);
user_ptr = getpwuid(getuid());

if(user_ptr == NULL)
  {
  CfOut(cf_error,"getpwuid","Could not get user name of current process, using \"UNKNOWN\"");
  strncpy(userName, "UNKNOWN", userNameLen - 1);
  return false;
  }

strncpy(userName, user_ptr->pw_name, userNameLen - 1);
return true;
}

/*************************************************************/

char *Unix_GetErrorStr(void)
{
return strerror(errno);
}

/*************************************************************/

/* from exec_tools.c */

int Unix_IsExecutable(char *file)

{ struct stat sb;
  gid_t grps[NGROUPS];
  int i,n;

if (cfstat(file,&sb) == -1)
   {
   CfOut(cf_error,"","Proposed executable file \"%s\" doesn't exist",file);
   return false;
   }

if (sb.st_mode & 02)
   {
   CfOut(cf_error,""," !! SECURITY ALERT: promised executable \"%s\" is world writable! ",file);
   CfOut(cf_error,""," !! SECURITY ALERT: cfengine will not execute this - requires human inspection");
   return false;
   }

if (getuid() == sb.st_uid)
   {
   if (sb.st_mode & 0100)
      {
      return true;
      }
   }
else if (getgid() == sb.st_gid)
   {
   if (sb.st_mode & 0010)
      {
      return true;
      }    
   }
else
   {
   if (sb.st_mode & 0001)
      {
      return true;
      }
   
   if ((n = getgroups(NGROUPS,grps)) > 0)
      {
      for (i = 0; i < n; i++)
         {
         if (grps[i] == sb.st_gid)
            {
            if (sb.st_mode & 0010)
               {
               return true;
               }                 
            }
         }
      }
   }

return false;
}

/*******************************************************************/

/* from exec_tools.c */

int Unix_ShellCommandReturnsZero(char *comm,int useshell)

{ int status, i, argc = 0;
  pid_t pid;
  char arg[CF_MAXSHELLARGS][CF_BUFSIZE];
  char **argv;
  char esc_command[CF_BUFSIZE];

if (!useshell)
   {
   /* Build argument array */

   for (i = 0; i < CF_MAXSHELLARGS; i++)
      {
      memset (arg[i],0,CF_BUFSIZE);
      }

   argc = ArgSplitCommand(comm,arg);

   if (argc == -1)
      {
      CfOut(cf_error,"","Too many arguments in %s\n",comm);
      return false;
      }
   }

if ((pid = fork()) < 0)
   {
   FatalError("Failed to fork new process");
   }
else if (pid == 0)                     /* child */
   {
   ALARM_PID = -1;

   if (useshell)
      {
      strncpy(esc_command,WinEscapeCommand(comm),CF_BUFSIZE-1);

      if (execl("/bin/sh","sh","-c",esc_command,NULL) == -1)
         {
         CfOut(cf_error,"execl","Command %s failed",esc_command);
         exit(1);
         }
      }
   else
      {      
      argv = (char **) malloc((argc+1)*sizeof(char *));

      if (argv == NULL)
         {
         FatalError("Out of memory");
         }

      for (i = 0; i < argc; i++)
         {
         argv[i] = arg[i];
         }

      argv[i] = (char *) NULL;

      if (execv(arg[0],argv) == -1)
         {
         CfOut(cf_error,"execv","Command %s failed (%d args)",argv[0],argc - 1);
         exit(1);
         }

      free((char *)argv);
      }
   }
else                                    /* parent */
   {
   pid_t wait_result;
   ALARM_PID = pid;

#ifdef HAVE_WAITPID
   
   while(waitpid(pid,&status,0) < 0)
      {
      if (errno != EINTR)
         {
         return -1;
         }
      }

   return (WEXITSTATUS(status) == 0);
   
#else
   
   while ((wait_result = wait(&status)) != pid)
      {
      if (wait_result <= 0)
         {
         CfOut(cf_inform,"wait"," !! Wait for child failed\n");
         return false;
         }
      }
   
   if (WIFSIGNALED(status))
      {
      return false;
      }
   
   if (! WIFEXITED(status))
      {
      return false;
      }
   
   return (WEXITSTATUS(status) == 0);
#endif
   }

return false;
}

/**********************************************************************************/

/* from verify_processes.c */

int Unix_DoAllSignals(struct Item *siglist,struct Attributes a,struct Promise *pp)

{ struct Item *ip;
  struct Rlist *rp;
  pid_t pid;
  int killed = false;

Debug("DoSignals(%s)\n",pp->promiser);
  
if (siglist == NULL)
   {
   return 0;
   }

if (a.signals == NULL)
   {
   CfOut(cf_verbose,""," -> No signals to send for %s\n",pp->promiser);
   return 0;
   }

for (ip = siglist; ip != NULL; ip=ip->next)
   {
   pid = ip->counter;
   
   for (rp = a.signals; rp != NULL; rp=rp->next)
      {
      int signal = Signal2Int(rp->item);
      
      if (!DONTDO)
         {         
         if (signal == SIGKILL || signal == SIGTERM)
            {
            killed = true;
            }
         
         if (kill((pid_t)pid,signal) < 0)
            {
            cfPS(cf_verbose,CF_FAIL,"kill",pp,a," !! Couldn't send promised signal \'%s\' (%d) to pid %d (might be dead)\n",rp->item,signal,pid);
            }
         else
            {
            cfPS(cf_inform,CF_CHG,"",pp,a," -> Signalled \'%s\' (%d) to observed process match \'%s\'\n",rp->item,signal,ip->name);
            }
         }
      else
         {
         CfOut(cf_error,""," -> Need to keep signal promise \'%s\' in process entry %s",rp->item,ip->name);
         }
      }
   }

return killed;
}

/*******************************************************************/

/* from verify_processes.c */

int Unix_LoadProcessTable(struct Item **procdata,char *psopts)

{ FILE *prp;
  char pscomm[CF_MAXLINKSIZE], vbuff[CF_BUFSIZE], *sp;
  struct Item *rootprocs = NULL;
  struct Item *otherprocs = NULL;

snprintf(pscomm,CF_MAXLINKSIZE,"%s %s",VPSCOMM[VSYSTEMHARDCLASS],psopts);

CfOut(cf_verbose,"","Observe process table with %s\n",pscomm); 
  
if ((prp = cf_popen(pscomm,"r")) == NULL)
   {
   CfOut(cf_error,"popen","Couldn't open the process list with command %s\n",pscomm);
   return false;
   }

while (!feof(prp))
   {
   memset(vbuff,0,CF_BUFSIZE);
   CfReadLine(vbuff,CF_BUFSIZE,prp);

   for (sp = vbuff+strlen(vbuff)-1; sp > vbuff && isspace(*sp); sp--)
      {
      *sp = '\0';
      }
   
   if (ForeignZone(vbuff))
      {
      continue;
      }

   AppendItem(procdata,vbuff,"");
   }

cf_pclose(prp);

/* Now save the data */

snprintf(vbuff,CF_MAXVARSIZE,"%s/state/cf_procs",CFWORKDIR);
RawSaveItemList(*procdata,vbuff);

CopyList(&rootprocs,*procdata);
CopyList(&otherprocs,*procdata);

while (DeleteItemNotContaining(&rootprocs,"root"))
   {
   }

while (DeleteItemContaining(&otherprocs,"root"))
   {
   }

if (otherprocs)
   {
   PrependItem(&rootprocs,otherprocs->name,NULL);
   }

snprintf(vbuff,CF_MAXVARSIZE,"%s/state/cf_rootprocs",CFWORKDIR);
RawSaveItemList(rootprocs,vbuff);
DeleteItemList(rootprocs);

snprintf(vbuff,CF_MAXVARSIZE,"%s/state/cf_otherprocs",CFWORKDIR);
RawSaveItemList(otherprocs,vbuff);
DeleteItemList(otherprocs);

return true;
}

/*********************************************************************/

/* from files_operators.c */

void Unix_CreateEmptyFile(char *name)

{ int tempfd;

if (unlink(name) == -1)
   {
   Debug("Pre-existing object %s could not be removed or was not there\n",name);
   }

if ((tempfd = open(name, O_CREAT|O_EXCL|O_WRONLY,0600)) < 0)
   {
   CfOut(cf_error,"open","Couldn't open a file %s\n",name);  
   }

close(tempfd);
}

#endif  /* NOT MINGW */
