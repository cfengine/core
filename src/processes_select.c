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
/* File: processes_select.c                                                  */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/***************************************************************************/

int SelectProcess(char *procentry,char **names,int *start,int *end,struct Attributes a,struct Promise *pp)

{ struct Item *proc_attr = NULL;
 int result = true,s,e;
  char *criteria = NULL;
  char *column[CF_PROCCOLS];
  struct Rlist *rp;
    
Debug("SelectProcess(%s)\n",procentry);

if (!a.haveselect)
   {
   return true;
   }

if (!SplitProcLine(procentry,names,start,end,column))
   {
   return false;
   }

for (rp = a.process_select.owner; rp != NULL; rp = rp->next)
   {
   if (SelectProcRegexMatch("USER","UID",(char *)rp->item,names,column))
      {
      PrependItem(&proc_attr,"process_owner","");
      break;
      }
   }

if (SelectProcRangeMatch("PID","PID",a.process_select.min_pid,a.process_select.max_pid,names,column))
   {
   PrependItem(&proc_attr,"pid","");
   }

if (SelectProcRangeMatch("PPID","PPID",a.process_select.min_ppid,a.process_select.max_ppid,names,column))
   {
   PrependItem(&proc_attr,"ppid","");
   }

if (SelectProcRangeMatch("PGID","PGID",a.process_select.min_pgid,a.process_select.max_pgid,names,column))
   {
   PrependItem(&proc_attr,"pgid","");
   }

if (SelectProcRangeMatch("SZ","VSZ",a.process_select.min_vsize,a.process_select.max_vsize,names,column))
   {
   PrependItem(&proc_attr,"vsize","");
   }

if (SelectProcRangeMatch("RSS","RSS",a.process_select.min_rsize,a.process_select.max_rsize,names,column))
   {
   PrependItem(&proc_attr,"rsize","");
   }

if (SelectProcTimeCounterRangeMatch("TIME","TIME",a.process_select.min_ttime,a.process_select.max_ttime,names,column))
   {
   PrependItem(&proc_attr,"ttime","");
   }

if (SelectProcTimeAbsRangeMatch("STIME","START",a.process_select.min_stime,a.process_select.max_stime,names,column))
   {
   PrependItem(&proc_attr,"stime","");
   }

if (SelectProcRangeMatch("NI","PRI",a.process_select.min_pri,a.process_select.max_pri,names,column))
   {
   PrependItem(&proc_attr,"priority","");
   }

if (SelectProcRangeMatch("NLWP","NLWP",a.process_select.min_thread,a.process_select.max_thread,names,column))
   {
   PrependItem(&proc_attr,"threads","");
   }

if (SelectProcRegexMatch("S","STAT",a.process_select.status,names,column))
   {
   PrependItem(&proc_attr,"status","");
   }

if (SelectProcRegexMatch("CMD","COMMAND",a.process_select.command,names,column))
   {
   PrependItem(&proc_attr,"command","");
   }

if (SelectProcRegexMatch("TTY","TTY",a.process_select.tty,names,column))
   {
   PrependItem(&proc_attr,"tty","");
   }

if (result = EvaluateORString(a.process_select.process_result,proc_attr,0))
   {
   //ClassesFromString(fp->defines);
   }

DeleteItemList(proc_attr);

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

return result; 
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

int SelectProcRangeMatch(char *name1,char *name2,int min,int max,char **names,char **line)

{ int i;
  long value;
 
for (i = 0; names[i] != NULL; i++)
   {
   if ((strcmp(names[i],name1) == 0) || (strcmp(names[i],name2) == 0))
      {
      value = Str2Int(line[i]);

      if (value == CF_NOINT)
         {
         CfOut(cf_inform,"","Failed to extract a valid integer from %s => \"%s\" in process list\n",name1[i],line[i]);
         return false;
         }
      
      if (min < value && value < max)
         {
         return true;
         }
      else
         {   
         return false;
         }
      }
   }

return false; 
}

/***************************************************************************/

int SelectProcTimeCounterRangeMatch(char *name1,char *name2,time_t min,time_t max,char **names,char **line)

{ int i;
  time_t value;
 
for (i = 0; names[i] != NULL; i++)
   {
   if ((strcmp(names[i],name1) == 0) || (strcmp(names[i],name2) == 0))
      {
      value = (time_t) TimeCounter2Int(line[i]);
               
      if (value == CF_NOINT)
         {
         CfOut(cf_inform,"","Failed to extract a valid integer from %s => \"%s\" in process list\n",name1[i],line[i]);
         return false;
         }
      
      if (min < value && value < max)
         {
         CfOut(cf_verbose,"","Selection filter matched %s/%s = %s in [%ld,%ld]\n",name1,name2,line[i],min,max);
         return true;
         }
      else
         {   
         return false;
         }
      }
   }

return false; 
}

/***************************************************************************/

int SelectProcTimeAbsRangeMatch(char *name1,char *name2,time_t min,time_t max,char **names,char **line)

{ int i;
  time_t value;
 
for (i = 0; names[i] != NULL; i++)
   {
   if ((strcmp(names[i],name1) == 0) || (strcmp(names[i],name2) == 0))
      {
      value = (time_t)TimeAbs2Int(line[i]);
               
      if (value == CF_NOINT)
         {
         CfOut(cf_inform,"","Failed to extract a valid integer from %s => \"%s\" in process list\n",name1[i],line[i]);
         return false;
         }
      
      if (min < value && value < max)
         {
         CfOut(cf_verbose,"","Selection filter matched %s/%s = %s in [%ld,%ld]\n",name1,name2,line[i],min,max);
         return true;
         }
      else
         {
         return false;
         }
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
 
for (i = 0; names[i] != NULL; i++)
   {
   if ((strcmp(names[i],name1) == 0) || (strcmp(names[i],name2) == 0))
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
   }

return false; 
}

/*******************************************************************/

int SplitProcLine(char *proc,char **names,int *start,int *end,char **line)

{ int i,s,e;

Debug("SplitProcLine(%s)\n",proc); 

if (proc == NULL || strlen(proc) == 0)
   {
   return false;
   }

for (i = 0; i < CF_PROCCOLS; i++)
   {
   line[i] = NULL;
   }
 
for (i = 0; names[i] != NULL; i++)
   {
   for (s = start[i]; (s >= 0) && !isspace((int)*(proc+s)); s--)
      {
      }

   if (s < 0)
      {
      s = 0;
      }
   
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
      line[i] = (char *)malloc(e-s+2);
      memset(line[i],0,(e-s+2));
      strncpy(line[i],(char *)(proc+s),(e-s+1));
      }
   else
      {
      line[i] = (char *)malloc(1);
      line[i][0] = '\0';
      }
   }

return true;
}
