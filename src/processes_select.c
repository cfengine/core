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
/* File: processes_select.c                                                  */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/***************************************************************************/

int SelectProcess(char *procentry,char **names,int *start,int *end,struct Attributes a,struct Promise *pp)

{ struct AlphaList proc_attr;
 int result = true,i;
  char *column[CF_PROCCOLS];
  struct Rlist *rp;

Debug("SelectProcess(%s)\n",procentry);

InitAlphaList(&proc_attr);

if (!a.haveselect)
   {
   return true;
   }

if (!SplitProcLine(procentry,names,start,end,column))
   {
   return false;
   }

if (DEBUG)
   {
   for (i = 0; names[i] != NULL; i++)
      {
      printf("COL[%s] = \"%s\"\n",names[i],column[i]);
      }
   }

for (rp = a.process_select.owner; rp != NULL; rp = rp->next)
   {
   if (SelectProcRegexMatch("USER","UID",(char *)rp->item,names,column))
      {
      PrependAlphaList(&proc_attr,"process_owner");
      break;
      }
   }

if (SelectProcRangeMatch("PID","PID",a.process_select.min_pid,a.process_select.max_pid,names,column))
   {
   PrependAlphaList(&proc_attr,"pid");
   }

if (SelectProcRangeMatch("PPID","PPID",a.process_select.min_ppid,a.process_select.max_ppid,names,column))
   {
   PrependAlphaList(&proc_attr,"ppid");
   }

if (SelectProcRangeMatch("PGID","PGID",a.process_select.min_pgid,a.process_select.max_pgid,names,column))
   {
   PrependAlphaList(&proc_attr,"pgid");
   }

if (SelectProcRangeMatch("VSZ","SZ",a.process_select.min_vsize,a.process_select.max_vsize,names,column))
   {
   PrependAlphaList(&proc_attr,"vsize");
   }

if (SelectProcRangeMatch("RSS","RSS",a.process_select.min_rsize,a.process_select.max_rsize,names,column))
   {
   PrependAlphaList(&proc_attr,"rsize");
   }

if (SelectProcTimeCounterRangeMatch("TIME","TIME",a.process_select.min_ttime,a.process_select.max_ttime,names,column))
   {
   PrependAlphaList(&proc_attr,"ttime");
   }

if (SelectProcTimeAbsRangeMatch("STIME","START",a.process_select.min_stime,a.process_select.max_stime,names,column))
   {
   PrependAlphaList(&proc_attr,"stime");
   }

if (SelectProcRangeMatch("NI","PRI",a.process_select.min_pri,a.process_select.max_pri,names,column))
   {
   PrependAlphaList(&proc_attr,"priority");
   }

if (SelectProcRangeMatch("NLWP","NLWP",a.process_select.min_thread,a.process_select.max_thread,names,column))
   {
   PrependAlphaList(&proc_attr,"threads");
   }

if (SelectProcRegexMatch("S","STAT",a.process_select.status,names,column))
   {
   PrependAlphaList(&proc_attr,"status");
   }

if (SelectProcRegexMatch("CMD","COMMAND",a.process_select.command,names,column))
   {
   PrependAlphaList(&proc_attr,"command");
   }

if (SelectProcRegexMatch("TTY","TTY",a.process_select.tty,names,column))
   {
   PrependAlphaList(&proc_attr,"tty");
   }

if ((result = EvalProcessResult(a.process_select.process_result,&proc_attr)))
   {
   //ClassesFromString(fp->defines);
   }

DeleteAlphaList(&proc_attr);

if (result)
   {
   if (a.transaction.action == cfa_warn)
      {
      CfOut(cf_error,""," !! Matched: %s\n",procentry);
      }
   else
      {
      CfOut(cf_inform,""," !! Matched: %s\n",procentry);
      }
   }

for (i = 0; column[i] != NULL; i++)
   {
   free(column[i]);
   }

return result; 
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

int SelectProcRangeMatch(char *name1,char *name2,int min,int max,char **names,char **line)

{ int i;
  long value;

if (min == CF_NOINT || max == CF_NOINT)
   {
   return false;
   }

if ((i = GetProcColumnIndex(name1,name2,names)) != -1)
   {
   value = Str2Int(line[i]);
   
   if (value == CF_NOINT)
      {
      CfOut(cf_inform,"","Failed to extract a valid integer from %s => \"%s\" in process list\n",names[i],line[i]);
      return false;
      }
   
   if (min <= value && value <= max)
      {
      return true;
      }
   else
      {   
      return false;
      }
   }

return false; 
}

/***************************************************************************/

int SelectProcTimeCounterRangeMatch(char *name1,char *name2,time_t min,time_t max,char **names,char **line)

{ int i;
  time_t value;

if (min == CF_NOINT || max == CF_NOINT)
   {
   return false;
   }

if ((i = GetProcColumnIndex(name1,name2,names)) != -1)
   {
   value = (time_t) TimeCounter2Int(line[i]);
   
   if (value == CF_NOINT)
      {
      CfOut(cf_inform,"","Failed to extract a valid integer from %s => \"%s\" in process list\n",name1[i],line[i]);
      return false;
      }

   if (min <= value && value <= max)
      {
      CfOut(cf_verbose,"","Selection filter matched counter range %s/%s = %s in [%ld,%ld] (= %ld secs)\n",name1,name2,line[i],min,max,value);
      return true;
      }
   else
      {   
      Debug("Selection filter REJECTED counter range %s/%s = %s in [%ld,%ld] (= %ld secs)\n",name1,name2,line[i],min,max,value);
      return false;
      }
   }

return false; 
}

/***************************************************************************/

int SelectProcTimeAbsRangeMatch(char *name1,char *name2,time_t min,time_t max,char **names,char **line)

{ int i;
  time_t value;

if (min == CF_NOINT || max == CF_NOINT)
   {
   return false;
   }

if ((i = GetProcColumnIndex(name1,name2,names)) != -1)
   {
   value = (time_t)TimeAbs2Int(line[i]);
   
   if (value == CF_NOINT)
      {
      CfOut(cf_inform,"","Failed to extract a valid integer from %s => \"%s\" in process list\n",name1[i],line[i]);
      return false;
      }
   
   if (min <= value && value <= max)
      {
      CfOut(cf_verbose,"","Selection filter matched absolute %s/%s = %s in [%ld,%ld]\n",name1,name2,line[i],min,max);
      return true;
      }
   else
      {
      return false;
      }
   }

return false; 
}

/***************************************************************************/

int SelectProcRegexMatch(char *name1,char *name2,char *regex,char **names,char **line)

{ int i;

if (regex == NULL)
   {
   return false;
   }

if ((i = GetProcColumnIndex(name1,name2,names)) != -1)
   {
   if (FullTextMatch(regex,line[i]))
      {
      return true;
      }
   else
      {   
      return false;
      }
   }

return false; 
}

/*******************************************************************/

int SplitProcLine(char *proc,char **names,int *start,int *end,char **line)

{ int i,s,e;

char *sp = NULL;
char cols1[CF_PROCCOLS][CF_SMALLBUF] = { "" };
char cols2[CF_PROCCOLS][CF_SMALLBUF] = { "" };

Debug("SplitProcLine(%s)\n",proc); 

if (proc == NULL || strlen(proc) == 0)
   {
   return false;
   }

memset(line, 0, sizeof(char *) * CF_PROCCOLS);

// First try looking at all the separable items

sp = proc;

for (i = 0; i < CF_PROCCOLS && names[i] != NULL; i++)
   {
   while(*sp == ' ')
      {
      sp++;
      }

   if (strcmp(names[i],"CMD") == 0 || strcmp(names[i],"COMMAND") == 0)
      {
      sscanf(sp,"%127[^\n]",cols1[i]);
      sp += strlen(cols1[i]);
      }
   else
      {
      sscanf(sp,"%127s",cols1[i]);
      sp += strlen(cols1[i]);
      }
   
   // Some ps stimes may contain spaces, e.g. "Jan 25"
   if (strcmp(names[i],"STIME") == 0 && strlen(cols1[i]) == 3)
      {
      char s[CF_SMALLBUF] = {0};
      sscanf(sp,"%127s",s);
      strcat(cols1[i]," ");
      strcat(cols1[i],s);
      sp += strlen(s)+1;
      }
   }

// Now try looking at columne alignment

for (i = 0; i < CF_PROCCOLS && names[i] != NULL; i++)
   {
   // Start from the header/column tab marker and count backwards until we find 0 or space
   for (s = start[i]; (s >= 0) && !isspace((int)*(proc+s)); s--)
      {
      }

   if (s < 0)
      {
      s = 0;
      }

   // Make sure to strip off leading spaces
   while (isspace((int)proc[s]))
      {
      s++;
      }

   if (strcmp(names[i],"CMD") == 0 || strcmp(names[i],"COMMAND") == 0)
      {
      e = strlen(proc);
      }
   else
      {
      for (e = end[i]; (e <= end[i]+10) && !isspace((int)*(proc+e)); e++)
         {
         }
      
      while (isspace((int)proc[e]))
         {
         if (e > 0)
            {
            e--;
            }
         }
      }
   
   if (s <= e)
      {
      strncpy(cols2[i],(char *)(proc+s),MIN(CF_SMALLBUF-1,(e-s+1)));
      }
   else
      {
      cols2[i][0] = '\0';
      }

   Chop(cols2[i]);

   if (strcmp(cols2[i],cols1[i]) != 0)
      {
      CfOut(cf_inform,""," !! Unacceptable model uncertainty examining processes");
      }

   line[i] = strdup(cols1[i]);
   }

return true;
}

/*******************************************************************/

int GetProcColumnIndex(char *name1,char *name2,char **names)

{ int i;
 
for (i = 0; names[i] != NULL; i++)
   {
   if ((strcmp(names[i],name1) == 0) || (strcmp(names[i],name2) == 0))
      {
      return i;
      }
   }

CfOut(cf_verbose,""," INFO - process column %s/%s was not supported on this system",name1,name2);
return -1;
}
