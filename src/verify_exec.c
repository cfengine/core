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
/* File: verify_exec.c                                                       */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void VerifyExecPromise(struct Promise *pp)

{ struct Attributes a;

a = GetExecAttributes(pp);
ExecSanityChecks(a,pp);
VerifyExec(a,pp);
DeleteScalar("this","promiser");
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int ExecSanityChecks(struct Attributes a,struct Promise *pp)

{
if (a.contain.nooutput && a.contain.preview)
   {
   CfOut(cf_error,"","no_output and preview are mutually exclusive (broken promise)");
   PromiseRef(cf_error,pp);
   return false;
   }

if (a.contain.umask == CF_UNDEFINED)
   {
   a.contain.umask = 077;
   }

return true;
}

/*****************************************************************************/

void VerifyExec(struct Attributes a, struct Promise *pp)
    
{ struct CfLock thislock;
  char line[CF_BUFSIZE],eventname[CF_BUFSIZE];
  char comm[20], *sp;
  char execstr[CF_EXPANDSIZE];
  struct timespec start;
  int print, outsourced;
  mode_t maskval = 0;
  FILE *pfp;
  int preview = false;

thislock = AcquireLock(pp->promiser,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
   return;
   }

PromiseBanner(pp);

if (!IsExecutable(GetArg0(pp->promiser)))
   {
   cfPS(cf_error,CF_FAIL,"",pp,a,"%s promises to be executable but isn't\n",pp->promiser);
   return;
   }
else
   {
   Verbose(" -> Promiser string contains a valid executable (%s) - ok\n",GetArg0(pp->promiser));
   }

NewScalar("this","promiser",pp->promiser,cf_str);

if (a.args)
   {
   snprintf(execstr,CF_EXPANDSIZE-1,"%s %s",pp->promiser,a.args);
   }
else
   {
   strncpy(execstr,pp->promiser,CF_BUFSIZE);
   }

CfOut(cf_inform,""," -> Executing \'%s\' ...(timeout=%d,owner=%d,group=%d)\n",execstr,a.contain.timeout,a.contain.owner,a.contain.group);

start = BeginMeasure();

if (DONTDO && !a.contain.preview)
   {
   CfOut(cf_error,"","Would execute script %s\n",execstr);
   }
else
   {
   CommPrefix(execstr,comm);
   
   if (a.transaction.background)
      {
      Verbose(" -> Backgrounding job %s\n",execstr);
      outsourced = fork();
      }
   else
      {
      outsourced = false;
      }

   if (outsourced || !a.transaction.background)
      {
      if (a.contain.timeout != 0)
         {
         SetTimeOut(a.contain.timeout);
         }
      
      Verbose(" -> (Setting umask to %o)\n",a.contain.umask);
      maskval = umask(a.contain.umask);
      
      if (a.contain.umask == 0)
         {
         CfOut(cf_verbose,"","Programming %s running with umask 0! Use umask= to set\n",execstr);
         }

      if (a.contain.useshell)
         {
         pfp = cf_popen_shsetuid(execstr,"r",a.contain.owner,a.contain.group,a.contain.chdir,a.contain.chroot);
         }
      else
         {
         // pfp = cf_popensetuid(execstr,"r",a.contain.owner,a.contain.group,a.contain.chdir,a.contain.chroot);
         pfp = cf_popensetuid(execstr,"r",-1,-1,NULL,NULL);         
         }

      if (pfp == NULL)
         {
         cfPS(cf_error,CF_FAIL,"cf_popen",pp,a,"Couldn't open pipe to command %s\n",execstr);
         YieldCurrentLock(thislock);
         return;
         }

      while (!feof(pfp))
         {
         if (ferror(pfp))  /* abortable */
            {
            cfPS(cf_error,CF_TIMEX,"ferror",pp,a,"Shell command pipe %s\n",execstr);
            return;
            }

         ReadLine(line,CF_BUFSIZE-1,pfp);
         
         if (strstr(line,"cfengine-die"))
            {
            break;
            }
         
         if (ferror(pfp))  /* abortable */
            {
            cfPS(cf_error,CF_TIMEX,"ferror",pp,a,"Shell command pipe %s\n",execstr);
            return;
            }
         
         if (a.contain.preview)
            {
            PreviewProtocolLine(line,execstr);
            }
         else 
            {
            if (!a.contain.nooutput && NonEmptyLine(line))
               {
               CfOut(cf_error,"","[%s] %s\n",comm,line);
               }
            }
         }
      
      cf_pclose_def(pfp,a,pp);
      }
   
   if (a.contain.timeout != 0)
      {
      alarm(0);
      signal(SIGALRM,SIG_DFL);
      }

   cfPS(cf_inform,CF_CHG,"",pp,a," -> Completed execution of %s\n",execstr);
   umask(maskval);
   YieldCurrentLock(thislock);

   snprintf(eventname,CF_BUFSIZE-1,"Exec(%s)",execstr);
   EndMeasure(eventname,start);
   
   if (a.transaction.background && outsourced)
      {
      Verbose(" -> Backgrounded command (%s) is done - exiting\n",execstr);
      exit(0);
      }
   }
}

/*************************************************************/
/* Level                                                     */
/*************************************************************/

int IsExecutable(char *file)

{ struct stat sb;
  gid_t grps[NGROUPS];
  int i,n;

if (stat(file,&sb) == -1)
   {
   CfOut(cf_error,"","Proposed executable %s doesn't exist",file);
   return false;
   }
  
if (getuid() == sb.st_uid)
   {
   if (sb.st_mode && 0100)
      {
      return true;
      }
   }
else if (getgid() == sb.st_gid)
   {
   if (sb.st_mode && 0010)
      {
      return true;
      }    
   }
else
   {
   if (sb.st_mode && 0001)
      {
      return true;
      }
   
   if ((n = getgroups(NGROUPS,grps)) > 0)
      {
      for (i = 0; i < n; i++)
         {
         if (grps[i] == sb.st_gid)
            {
            if (sb.st_mode && 0010)
               {
               return true;
               }                 
            }
         }
      }
   }

return false;
}

/*************************************************************/

char *GetArg0(char *execstr)

{ char *sp;
  static char arg[CF_BUFSIZE];
  int i = 0;

for (sp = execstr; *sp != ' ' && *sp != '\0'; sp++)
   {
   i++;
   }

memset(arg,0,20);
strncpy(arg,execstr,i);
return arg;
}

/*************************************************************/

void CommPrefix(char *execstr,char *comm)

{ char *sp;

for (sp = execstr; *sp != ' ' && *sp != '\0'; sp++)
   {
   }

if (sp - 10 >= execstr)
   {
   sp -= 10;   /* copy 15 most relevant characters of command */
   }
else
   {
   sp = execstr;
   }

memset(comm,0,20);
strncpy(comm,sp,15);
}

/*************************************************************/

int NonEmptyLine(char *line)

{ char *sp;
            
for (sp = line; *sp != '\0'; sp++)
   {
   if (!isspace((int)*sp))
      {
      return true;
      }
   }

return false;
}

/*************************************************************/

void PreviewProtocolLine(char *line, char *comm)

{ int i;
  int level = cferror;
  char *message = line;
               
  /*
   * Table matching cfoutputlevel enums to log prefixes.
   */
  
  char *prefixes[] =
      {
          ":silent:",
          ":inform:",
          ":verbose:",
          ":editverbose:",
          ":error:",
          ":logonly:",
      };
  
  int precount = sizeof(prefixes)/sizeof(char *);
  
if (line[0] == ':')
   {
   /*
    * Line begins with colon - see if it matches a log prefix.
    */
   
   for (i = 0; i < precount; i++)
      {
      int prelen = 0;

      prelen = strlen(prefixes[i]);

      if (strncmp(line, prefixes[i], prelen) == 0)
         {
         /*
          * Found log prefix - set logging level, and remove the
          * prefix from the log message.
          */

         level = i;
         message += prelen;
         break;
         }
      }
   }

CfOut(level,"","%s (preview of %s)\n",message,comm);
}
