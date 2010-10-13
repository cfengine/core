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
/* File: verify_processes.c                                                  */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void VerifyProcessesPromise(struct Promise *pp)

{ struct Attributes a;

a = GetProcessAttributes(pp);
ProcessSanityChecks(a,pp);

VerifyProcesses(a,pp);
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int ProcessSanityChecks(struct Attributes a,struct Promise *pp)

{ int promised_zero, promised_any, ret = true;

if (a.signals != NULL && a.process_stop != NULL)
   {
   CfOut(cf_error,"","Promise constraint conflicts - %s processes cannot be signalled if they are stopped",pp->promiser);
   PromiseRef(cf_error,pp);
   ret = false;
   }

promised_zero = (a.process_count.min_range == 0 && a.process_count.max_range == 0);
promised_any = (a.process_count.min_range == CF_NOINT);


if (a.restart_class)
   {
   if (IsStringIn(a.signals,"term") || IsStringIn(a.signals,"kill"))
      {
      CfOut(cf_inform,""," -> (warning) Promise %s kills then restarts - never strictly converges",pp->promiser);
      PromiseRef(cf_inform,pp);
      }   
   }

if (promised_zero && a.restart_class)
   {
   CfOut(cf_error,"","Promise constraint conflicts - %s processes cannot have zero count if restarted",pp->promiser);
   PromiseRef(cf_error,pp);
   ret = false;
   }

if (a.haveselect && !a.process_select.process_result)
   {
   CfOut(cf_error,""," !! Process select constraint body promised no result (check body definition)");
   PromiseRef(cf_error,pp);
   return false;   
   }

return ret;
}

/*****************************************************************************/

void VerifyProcesses(struct Attributes a, struct Promise *pp)

{ struct CfLock thislock;
 char lockname[CF_BUFSIZE];

if (a.restart_class)
   {
   snprintf(lockname,CF_BUFSIZE-1,"proc-%s-%s",pp->promiser,a.restart_class);
   }
else
   {
   snprintf(lockname,CF_BUFSIZE-1,"proc-%s-norestart",pp->promiser);
   }
 
thislock = AcquireLock(lockname,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
   return;
   }

NewScalar("this","promiser",pp->promiser,cf_str);
PromiseBanner(pp);
VerifyProcessOp(PROCESSTABLE,a,pp);
DeleteScalar("this","promiser");

YieldCurrentLock(thislock);
}


/*******************************************************************/

int LoadProcessTable(struct Item **procdata,char *psopts)

{ 
#ifdef MINGW
return NovaWin_LoadProcessTable(procdata);
#else
return Unix_LoadProcessTable(procdata,psopts);
#endif
}

/*******************************************************************/

void VerifyProcessOp(struct Item *procdata,struct Attributes a,struct Promise *pp)

{ char line[CF_BUFSIZE];
  int matches = 0,do_signals = true,out_of_range,killed = 0,need_to_restart = true;
  mode_t maskval;
  struct stat statbuf;
  struct Item *killlist = NULL;

Debug("VerifyProcessOp\n");

matches = FindPidMatches(procdata,&killlist,a,pp); 

/* promise based on number of matches */

if (a.process_count.min_range != CF_NOINT) /* if a range is specified */
   {
   if (matches < a.process_count.min_range || matches > a.process_count.max_range)
      {
      cfPS(cf_error,CF_CHG,"",pp,a," !! Process count for \'%s\' was out of promised range (%d found)\n",pp->promiser,matches);
      AddEphemeralClasses(a.process_count.out_of_range_define);
      out_of_range = true;
      }
   else
      {
      AddEphemeralClasses(a.process_count.in_range_define);
      cfPS(cf_verbose,CF_NOP,"",pp,a," -> Process promise for %s is kept",pp->promiser);
      out_of_range = false;
      }
   }
else
   {
   out_of_range = true;
   }

if (!out_of_range)
   {
   return;
   }


if (a.transaction.action == cfa_warn)
   {
   do_signals = false;
   }
else
   {
   do_signals = true;
   }

/* signal/kill promises for existing matches */

if (do_signals && matches > 0)
   {
   if (a.process_stop != NULL)
      {
      if (IsExecutable(GetArg0(a.process_stop)))
         {
         ShellCommandReturnsZero(a.process_stop,false);
         }
      else
         {
         cfPS(cf_verbose,CF_FAIL,"",pp,a,"Process promise to stop %s could not be kept because %s the stop operator failed",pp->promiser,a.process_stop);
         DeleteItemList(killlist);
         return;
         }
      }
   else
      {
      killed = DoAllSignals(killlist,a,pp);
      }
   }

/* delegated promise to restart killed or non-existent entries */

need_to_restart = (a.restart_class != NULL) && (killed || matches == 0);

DeleteItemList(killlist);
 
if (!need_to_restart)
   {
   cfPS(cf_verbose,CF_NOP,"",pp,a," -- Matches in range for %s - process count promise kept\n",pp->promiser);
   return;
   }
else
   {
   if (a.transaction.action == cfa_warn)
      {
      cfPS(cf_error,CF_WARN,"",pp,a," -- Need to keep restart promise for %s, but only a warning is promised",pp->promiser);
      }
   else 
      {
      cfPS(cf_inform,CF_CHG,"",pp,a," -> Making a one-time restart promise for %s",pp->promiser);
      NewBundleClass(a.restart_class,pp->bundle);
      }
   }
}

/**********************************************************************************/

int FindPidMatches(struct Item *procdata,struct Item **killlist,struct Attributes a,struct Promise *pp)

{ struct Item *ip, *ip2;
  char *sp,saveuid[16];
  int pid=-1,ret,matches=0,got,i,one_space,s,e,promised_zero;
  pid_t cfengine_pid = getpid();
  char *names[CF_PROCCOLS];      /* ps headers */
  int start[CF_PROCCOLS];
  int end[CF_PROCCOLS];
  struct CfRegEx rex;

if (procdata == NULL)
   {
   return 0;
   }
  
GetProcessColumnNames(procdata->name,(char **)names,start,end); 

for (ip = procdata->next; ip != NULL; ip=ip->next)
   {
   CF_NODES++;
   
   if (BlockTextMatch(pp->promiser,ip->name,&s,&e))
      {
      if (!SelectProcess(ip->name,names,start,end,a,pp))
         {
         continue;
         }

      pid = ExtractPid(ip->name,names,start,end);

      if (pid == -1)
         {
         CfOut(cf_verbose,"","Unable to extract pid while looking for %s\n",pp->promiser);
         continue;
         }
      
      Debug("Found matching pid %d\n",pid);
      
      matches++;
      
      if (pid == 1)
         {
         if ((RlistLen(a.signals) == 1) && IsStringIn(a.signals,"hup"))
            {
            CfOut(cf_verbose,"","(Okay to send only HUP to init)\n");
            }
         else
            {
            continue;
            }
         }

      if (pid < 4 && a.signals)
         {
         CfOut(cf_verbose,"","Will not signal or restart processes 0,1,2,3 (occurred while looking for %s)\n",pp->promiser);
         continue;
         }

      promised_zero = a.process_count.min_range == 0 && a.process_count.max_range == 0;
      
      if (a.transaction.action == cfa_warn && promised_zero)
         {
         CfOut(cf_error,"","Process alert: %s\n",procdata->name); /* legend */
         CfOut(cf_error,"","Process alert: %s\n",ip->name);
         continue;
         }
      
      if (pid == cfengine_pid)
         {
         CfOut(cf_verbose,"","cf-agent will not kill itself!\n");
         continue;
         }
      
      snprintf(saveuid,15,"%d",(int)pid);
      PrependItem(killlist,saveuid,"");
      SetItemListCounter(*killlist,saveuid,pid);
      }
   }

// Free up allocated memory

for (i = 0; i < CF_PROCCOLS; i++)
   {
   if (names[i] != NULL)
      {
      free(names[i]);
      }
   }

return matches;
}

/**********************************************************************************/

int DoAllSignals(struct Item *siglist,struct Attributes a,struct Promise *pp)

{
#ifdef MINGW
return NovaWin_DoAllSignals(siglist,a,pp);
#else
return Unix_DoAllSignals(siglist,a,pp);
#endif
}

/**********************************************************************************/
/* Level                                                                          */
/**********************************************************************************/

int ExtractPid(char *psentry,char **names,int *start,int *end)

{ char *sp;
 int col,pid = -1,offset = 0;

for (col = 0; col < CF_PROCCOLS; col++)
   {
   if (strcmp(names[col],"PID") == 0)
      {
      if (col > 0)
         {
         offset=end[col-1];
         }
      break;
      }
   }

for (sp = psentry+offset; *sp != '\0'; sp++) /* if first field contains alpha, skip */
   {
   /* If start with alphanum then skip it till the first space */
   
   if (isalnum((int)*sp))
      {
      while ((*sp != ' ') && (*sp != '\0'))
         {
         sp++;
         }
      }
   
   while ((*sp == ' ') && (*sp == '\t'))
      {
      sp++;
      }

   sscanf(sp,"%d",&pid);
   
   if (pid != -1)
      {
      break;
      }
   }

return pid;
}

/**********************************************************************************/

void GetProcessColumnNames(char *proc,char **names,int *start,int *end)

{ char *sp,title[16];
  int col,offset = 0;
  
for (col = 0; col < CF_PROCCOLS; col++)
   {
   start[col] = end[col] = -1;
   names[col] = NULL;
   }

col = 0; 
 
for (sp = proc; *sp != '\0'; sp++)
   {
   offset = sp - proc;

   if (isspace((int)*sp))
      {
      if (start[col] != -1)
         {
         Debug("End of %s is %d\n",title,offset-1);
         end[col++] = offset - 1;
         if (col > CF_PROCCOLS - 1)
            {
            CfOut(cf_error,"","Column overflow in process table");
            break;
            }
         }
      continue;
      }
   
   else if (start[col] == -1)
      {
      start[col] = offset;
      sscanf(sp,"%15s",title);
      Debug("Start of %s is %d\n",title,offset);
      names[col] = strdup(title);
      Debug("Col[%d]=%s\n",col,names[col]);
      }
   }

if (end[col] == -1)
   {
   Debug("End of %s is %d\n",title,offset);
   end[col] = offset;
   }
}

/**********************************************************************************/

int GracefulTerminate(pid_t pid)

{
#ifdef MINGW
return NovaWin_GracefulTerminate(pid);
#else
return Unix_GracefulTerminate(pid);
#endif
}

