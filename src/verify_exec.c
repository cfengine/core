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

#ifdef MINGW
if(a.contain.umask != CF_UNDEFINED)  // TODO: Always true (077 != -1?, compare positive and negative number), make false when umask not set
  {
  CfOut(cf_verbose, "", "contain.umask is ignored on Windows");
  }

if(a.contain.owner != CF_UNDEFINED)
  {
  CfOut(cf_verbose, "", "contain.exec_owner is ignored on Windows");
  }
    
if(a.contain.group != CF_UNDEFINED)
  {
  CfOut(cf_verbose, "", "contain.exec_group is ignored on Windows");
  }
  
if(a.contain.chroot != NULL)
  {
  CfOut(cf_verbose, "", "contain.chroot is ignored on Windows");
  }
  
#else  /* NOT MINGW */
if (a.contain.umask == CF_UNDEFINED)
   {
   a.contain.umask = 077;
   }
#endif  /* NOT MINGW */
   
return true;
}

/*****************************************************************************/

void VerifyExec(struct Attributes a, struct Promise *pp)
    
{ struct CfLock thislock;
  char line[CF_BUFSIZE],eventname[CF_BUFSIZE];
  char comm[20], *sp;
  char execstr[CF_EXPANDSIZE];
  struct timespec start;
  int print, outsourced,count = 0;
  mode_t maskval = 0;
  FILE *pfp;
  int preview = false;
  char cmdOutBuf[CF_BUFSIZE];
  int cmdOutBufPos = 0;
  int lineOutLen;

if (!IsExecutable(GetArg0(pp->promiser)))
   {
   cfPS(cf_error,CF_FAIL,"",pp,a,"%s promises to be executable but isn't\n",pp->promiser);

   if(IsIn(' ', pp->promiser))
     {
     CfOut(cf_verbose, "", "Paths with spaces must be inside escaped quoutes (e.g. \\\"%s\\\")", pp->promiser);
     }

   return;
   }
else
   {
   CfOut(cf_verbose,""," -> Promiser string contains a valid executable (%s) - ok\n",GetArg0(pp->promiser));
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

thislock = AcquireLock(execstr,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
   return;
   }

PromiseBanner(pp);

CfOut(cf_inform,""," -> Executing \'%s\' ...(timeout=%d,owner=%d,group=%d)\n",execstr,a.contain.timeout,a.contain.owner,a.contain.group);

start = BeginMeasure();

if (DONTDO && !a.contain.preview)
   {
   CfOut(cf_error,"","-> Would execute script %s\n",execstr);
   }
else
   {
   CommPrefix(execstr,comm);
   
   if (a.transaction.background)
      {
#ifdef MINGW
      outsourced = true;
#else
      CfOut(cf_verbose,""," -> Backgrounding job %s\n",execstr);
      outsourced = fork();
#endif
      }
   else
      {
      outsourced = false;
      }

   if (outsourced || !a.transaction.background)  // work done here: either by child or non-background parent
      {
      if (a.contain.timeout != 0)
         {
         SetTimeOut(a.contain.timeout);
         }
      
#ifndef MINGW
      CfOut(cf_verbose,""," -> (Setting umask to %o)\n",a.contain.umask);
      maskval = umask(a.contain.umask);
      
      if (a.contain.umask == 0)
         {
         CfOut(cf_verbose,""," !! Programming %s running with umask 0! Use umask= to set\n",execstr);
         }
#endif  /* NOT MINGW */
		 
      if (a.contain.useshell)
         {
         pfp = cf_popen_shsetuid(execstr,"r",a.contain.owner,a.contain.group,a.contain.chdir,a.contain.chroot,a.transaction.background);
         }
      else
         {
         pfp = cf_popensetuid(execstr,"r",a.contain.owner,a.contain.group,a.contain.chdir,a.contain.chroot,a.transaction.background);
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
            cfPS(cf_error,CF_TIMEX,"ferror",pp,a,"Command pipe %s\n",execstr);
            YieldCurrentLock(thislock);
            return;
            }

         CfReadLine(line,CF_BUFSIZE-1,pfp);
         
         if (strstr(line,"cfengine-die"))
            {
            break;
            }
         
         if (ferror(pfp))  /* abortable */
            {
            cfPS(cf_error,CF_TIMEX,"ferror",pp,a,"Command pipe %s\n",execstr);
            YieldCurrentLock(thislock);   
            return;
            }
         
         if (a.contain.preview)
            {
            PreviewProtocolLine(line,execstr);
            }

         if (a.module)
            {
            ModuleProtocol(comm,line,!a.contain.nooutput);
            }
         else if (!a.contain.nooutput && NonEmptyLine(line))
            {

	    lineOutLen = strlen(comm) + strlen(line) + 12;

	    // if buffer is to small for this line, output it directly
	    if(lineOutLen > sizeof(cmdOutBuf))
	      {
	      CfOut(cf_cmdout,"","Q: \"...%s\": %s\n",comm,line);
	      }
	    else
	      {
	      if(cmdOutBufPos + lineOutLen > sizeof(cmdOutBuf))
	        {
		CfOut(cf_cmdout, "", "%s", cmdOutBuf);
		cmdOutBufPos = 0;
		}
	      sprintf(cmdOutBuf + cmdOutBufPos, "Q: \"...%s\": %s\n",comm, line);
       	      cmdOutBufPos += (lineOutLen - 1);
	      }
            count++;
            }
         }
#ifdef MINGW
      if(outsourced)  // only get return value if we waited for command execution
	{
	cf_pclose(pfp);
	}
      else
	{
	cf_pclose_def(pfp,a,pp);
	}
#else  /* NOT MINGW */
      cf_pclose_def(pfp,a,pp);
#endif
      }

   if (count)
      {
      if(cmdOutBufPos)
	{
	CfOut(cf_cmdout, "", "%s", cmdOutBuf);
	}

      cfPS(cf_error,CF_CHG,"",pp,a,"I: Last %d QUOTEed lines were generated by promiser \"%s\"\n",count,execstr);
      }
   
   if (a.contain.timeout != 0)
      {
      alarm(0);
      signal(SIGALRM,SIG_DFL);
      }

   CfOut(cf_inform,""," -> Completed execution of %s\n",execstr);
#ifndef MINGW
   umask(maskval);
#endif
   YieldCurrentLock(thislock);

   snprintf(eventname,CF_BUFSIZE-1,"Exec(%s)",execstr);
   
#ifndef MINGW
   if (a.transaction.background && outsourced)
      {
      CfOut(cf_verbose,""," -> Backgrounded command (%s) is done - exiting\n",execstr);
      exit(0);
      }
#endif  /* NOT MINGW */
   }
}

/*************************************************************/
/* Level                                                     */
/*************************************************************/

void PreviewProtocolLine(char *line, char *comm)

{ int i;
  enum cfreport level = cf_error;
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

CfOut(cf_verbose,"","%s (preview of %s)\n",message,comm);
}
