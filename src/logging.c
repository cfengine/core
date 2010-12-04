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
/* File: logging.c                                                           */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void BeginAudit()

{ struct Promise dummyp = {0};
  struct Attributes dummyattr = {0};

if (THIS_AGENT_TYPE != cf_agent)
   {
   return;
   }
  
memset(&dummyp,0,sizeof(dummyp));
memset(&dummyattr,0,sizeof(dummyattr));

ClassAuditLog(&dummyp,dummyattr,"Cfagent starting",CF_NOP,"");
}

/*****************************************************************************/

void EndAudit()

{ double total;
  char *sp,rettype,string[CF_BUFSIZE];
  void *retval;
  struct Promise dummyp = {0};
  struct Attributes dummyattr = {0};

if (THIS_AGENT_TYPE != cf_agent)
   {
   return;
   }

memset(&dummyp,0,sizeof(dummyp));
memset(&dummyattr,0,sizeof(dummyattr));

if (BooleanControl("control_agent",CFA_CONTROLBODY[cfa_track_value].lval))
   {
   FILE *fout;
   char name[CF_MAXVARSIZE],datestr[CF_MAXVARSIZE];
   time_t now = time(NULL);
   
   CfOut(cf_inform,""," -> Recording promise valuations");
    
   snprintf(name,CF_MAXVARSIZE,"%s/state/%s",CFWORKDIR,CF_VALUE_LOG);
   snprintf(datestr,CF_MAXVARSIZE,"%s",cf_ctime(&now));
   
   if ((fout = fopen(name,"a")) == NULL)
      {
      CfOut(cf_inform,""," !! Unable to write to the value log %s\n",name);
      return;
      }

   Chop(datestr);
   fprintf(fout,"%s,%.4lf,%.4lf,%.4lf\n",datestr,VAL_KEPT,VAL_REPAIRED,VAL_NOTKEPT);
   TrackValue(datestr,VAL_KEPT,VAL_REPAIRED,VAL_NOTKEPT);   
   fclose(fout);
   }

total = (double)(PR_KEPT+PR_NOTKEPT+PR_REPAIRED)/100.0;

if (GetVariable("control_common","version",&retval,&rettype) != cf_notype)
   {
   sp = (char *)retval;
   }
else
   {
   sp = "(not specified)";
   }

if (total == 0)
   {
   *string = '\0';
   CfOut(cf_verbose,"","Outcome of version %s: No checks were scheduled\n",sp);
   return;
   }
else
   {   
   snprintf(string,CF_BUFSIZE,"Outcome of version %s (%s-%d): Promises observed to be kept %.0f%%, Promises repaired %.0f%%, Promises not repaired %.0f\%%",
            sp,
            THIS_AGENT,
            CFA_BACKGROUND,
            (double)PR_KEPT/total,
            (double)PR_REPAIRED/total,
            (double)PR_NOTKEPT/total);

   CfOut(cf_verbose,"","%s",string);
   PromiseLog(string);
   }

if (strlen(string) > 0)
   {
   ClassAuditLog(&dummyp,dummyattr,string,CF_REPORT,"");
   }

ClassAuditLog(&dummyp,dummyattr,"Cfagent closing",CF_NOP,"");
}

/*****************************************************************************/

void ClassAuditLog(struct Promise *pp,struct Attributes attr,char *str,char status,char *reason)

{ time_t now = time(NULL);
  char date[CF_BUFSIZE],lock[CF_BUFSIZE],key[CF_BUFSIZE],operator[CF_BUFSIZE],id[CF_MAXVARSIZE];
  struct AuditLog newaudit;
  struct Audit *ap = pp->audit;
  struct timespec t;
  double keyval;
  int lineno = pp->lineno;
  char name[CF_BUFSIZE];


Debug("ClassAuditLog(%s)\n",str);

switch(status)
   {
   case CF_CHG:
       
       if (!EDIT_MODEL)
          {
          PR_REPAIRED++;       
          VAL_REPAIRED += attr.transaction.value_repaired;
          }

       AddAllClasses(attr.classes.change,attr.classes.persist,attr.classes.timer);
       DeleteAllClasses(attr.classes.del_change);
       NotePromiseCompliance(pp,0.5,cfn_repaired,reason);
       SummarizeTransaction(attr,pp,attr.transaction.log_repaired);
       break;
       
   case CF_WARN:

       PR_NOTKEPT++;
       VAL_NOTKEPT += attr.transaction.value_notkept;
       NotePromiseCompliance(pp,1.0,cfn_notkept,reason);
       break;
       
   case CF_TIMEX:

       PR_NOTKEPT++;
       VAL_NOTKEPT += attr.transaction.value_notkept;
       AddAllClasses(attr.classes.timeout,attr.classes.persist,attr.classes.timer);
       DeleteAllClasses(attr.classes.del_notkept);
       NotePromiseCompliance(pp,0.0,cfn_notkept,reason);
       SummarizeTransaction(attr,pp,attr.transaction.log_failed);
       break;

   case CF_FAIL:

       PR_NOTKEPT++;
       VAL_NOTKEPT += attr.transaction.value_notkept;
       AddAllClasses(attr.classes.failure,attr.classes.persist,attr.classes.timer);
       DeleteAllClasses(attr.classes.del_notkept);
       NotePromiseCompliance(pp,0.0,cfn_notkept,reason);
       SummarizeTransaction(attr,pp,attr.transaction.log_failed);
       break;
       
   case CF_DENIED:

       PR_NOTKEPT++;
       VAL_NOTKEPT += attr.transaction.value_notkept;
       AddAllClasses(attr.classes.denied,attr.classes.persist,attr.classes.timer);
       DeleteAllClasses(attr.classes.del_notkept);
       NotePromiseCompliance(pp,0.0,cfn_notkept,reason);
       SummarizeTransaction(attr,pp,attr.transaction.log_failed);
       break;
       
   case CF_INTERPT:

       PR_NOTKEPT++;
       VAL_NOTKEPT += attr.transaction.value_notkept;
       AddAllClasses(attr.classes.interrupt,attr.classes.persist,attr.classes.timer);
       DeleteAllClasses(attr.classes.del_notkept);
       NotePromiseCompliance(pp,0.0,cfn_notkept,reason);
       SummarizeTransaction(attr,pp,attr.transaction.log_failed);
       break;

   case CF_UNKNOWN:
   case CF_NOP:

       AddAllClasses(attr.classes.kept,attr.classes.persist,attr.classes.timer);
       DeleteAllClasses(attr.classes.del_kept);
       NotePromiseCompliance(pp,1.0,cfn_nop,reason);
       SummarizeTransaction(attr,pp,attr.transaction.log_kept);              
       PR_KEPT++;
       VAL_KEPT += attr.transaction.value_kept;
       break;
   }

if (!(attr.transaction.audit || AUDIT))
   {
   return;
   }

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_AUDITDB_FILE);
MapName(name);

if (!OpenDB(name,&AUDITDBP))
   {
   return;
   }

if (AUDITDBP == NULL || THIS_AGENT_TYPE != cf_agent)
   {
   return;
   }

snprintf(date,CF_BUFSIZE,"%s",cf_ctime(&now));
Chop(date);

ExtractOperationLock(lock);
snprintf(operator,CF_BUFSIZE-1,"[%s] op %s",date,lock);
strncpy(newaudit.operator,operator,CF_AUDIT_COMMENT-1);

if (clock_gettime(CLOCK_REALTIME,&t) == -1)
   {
   CfOut(cf_verbose,"clock_gettime","Clock gettime failure during audit transaction");
   return;
   }

// Auditing key needs microsecond precision to separate entries

keyval = (double)(t.tv_sec)+(double)(t.tv_nsec)/(double)CF_BILLION;
snprintf(key,CF_BUFSIZE-1,"%lf",keyval);

if (DEBUG)
   {
   AuditStatusMessage(stdout,status);
   }

if (ap != NULL)
   {
   strncpy(newaudit.comment,str,CF_AUDIT_COMMENT-1);
   strncpy(newaudit.filename,ap->filename,CF_AUDIT_COMMENT-1);
   
   if (ap->version == NULL || strlen(ap->version) == 0)
      {
      Debug("Promised in %s bundle %s (unamed version last edited at %s) at/before line %d\n",ap->filename,pp->bundle,ap->date,lineno);
      newaudit.version[0] = '\0';
      }
   else
      {
      Debug("Promised in %s bundle %s (version %s last edited at %s) at/before line %d\n",ap->filename,pp->bundle,ap->version,ap->date,lineno);
      strncpy(newaudit.version,ap->version,CF_AUDIT_VERSION-1);
      }
   
   strncpy(newaudit.date,ap->date,CF_AUDIT_DATE);
   newaudit.lineno = lineno;
   }
else
   {
   strcpy(newaudit.date,date);
   strncpy(newaudit.comment,str,CF_AUDIT_COMMENT-1);
   strcpy(newaudit.filename,"schedule");
   strcpy(newaudit.version,"");
   newaudit.lineno = 0;
   }

newaudit.status = status;

if (AUDITDBP && attr.transaction.audit || AUDITDBP && AUDIT)
   {
   WriteDB(AUDITDBP,key,&newaudit,sizeof(newaudit));
   }

CloseDB(AUDITDBP);
}

/*****************************************************************************/

void AddAllClasses(struct Rlist *list,int persist,enum statepolicy policy)

{ struct Rlist *rp;
  int slot;
  char *string;
 
if (list == NULL)
   {
   return;
   }

for (rp = list; rp != NULL; rp=rp->next)
   {
   if (IsHardClass((char *)rp->item))
      {
      CfOut(cf_error,""," !! You cannot use reserved hard class \"%s\" as post-condition class",CanonifyName(rp->item));
      }

   string = (char *)(rp->item);
   slot = (int)*string;

   if (persist > 0)
      {
      CfOut(cf_verbose,""," ?> defining persistent promise result class %s\n",(char *)CanonifyName(rp->item));
      NewPersistentContext(CanonifyName(rp->item),persist,policy);
      IdempPrependItem(&(VHEAP.list[slot]),CanonifyName((char *)rp->item),NULL);
      }
   else
      {
      CfOut(cf_verbose,""," ?> defining promise result class %s\n",(char *)CanonifyName(rp->item));
      IdempPrependItem(&(VHEAP.list[slot]),CanonifyName((char *)rp->item),NULL);
      }
   }
}

/*****************************************************************************/

void DeleteAllClasses(struct Rlist *list)

{ struct Rlist *rp;
  char *string;
  int slot;
 
if (list == NULL)
   {
   return;
   }

for (rp = list; rp != NULL; rp=rp->next)
   {
   if (!CheckParseClass("class cancellation",(char *)rp->item,CF_IDRANGE))
      {
      return;
      }

   if (IsHardClass((char *)rp->item))
      {
      CfOut(cf_error,""," !! You cannot cancel a reserved hard class \"%s\" in post-condition classes", rp->item);
      }

   string = (char *)(rp->item);
   slot = (int)*string;
          
   CfOut(cf_verbose,""," -> Cancelling class %s\n",string);
   DeletePersistentContext(string);
   DeleteItemLiteral(&(VHEAP.list[slot]),CanonifyName(string));
   DeleteItemLiteral(&(VADDCLASSES.list[slot]),CanonifyName(string));
   AppendItem(&VDELCLASSES,CanonifyName(string),NULL);
   }
}

/************************************************************************/

void ExtractOperationLock(char *op)

{ char *sp, lastch = 'x'; 
  int i = 0, dots = 0;
  int offset = strlen("lock...")+strlen(VUQNAME);

/* Use the global copy of the lock from the main serial thread */
  
for (sp = CFLOCK+offset; *sp != '\0'; sp++)
   {
   switch (*sp)
      {
      case '_':
          if (lastch == '_')
             {
             break;
             }
          else
             {
             op[i] = '/';
             }
          break;

      case '.':
          dots++;
          op[i] = *sp;
          break;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
          dots = 9;
          break;
          
      default:
          op[i] = *sp;
          break;
      }

   lastch = *sp;
   i++;
   
   if (dots > 1)
      {
      break;
      }
   }

op[i] = '\0';
}

/************************************************************************/

void PromiseLog(char *s)

{ char filename[CF_BUFSIZE],start[CF_BUFSIZE],end[CF_BUFSIZE];
  time_t now = time(NULL);
  FILE *fout;

if (s == NULL || strlen(s) ==  0)
   {
   return;
   }
  
snprintf(filename,CF_BUFSIZE,"%s/%s",CFWORKDIR,CF_PROMISE_LOG);
MapName(filename);

if ((fout = fopen(filename,"a")) == NULL)
   {
   CfOut(cf_error,"fopen","Could not open %s",filename);
   return;
   }

fprintf(fout,"%ld,%ld: %s\n",CFSTARTTIME,now,s);
fclose(fout);
}

/************************************************************************/

void FatalError(char *s)
    
{ struct CfLock best_guess;

if (s)
   {
   CfOut(cf_error,"","Fatal cfengine error: %s",s); 
   }

if (strlen(CFLOCK) > 0)
   {
   best_guess.lock = strdup(CFLOCK);
   best_guess.last = strdup(CFLAST);
   best_guess.log = strdup(CFLOG);
   YieldCurrentLock(best_guess);
   }

unlink(PIDFILE);
EndAudit();
GenericDeInitialize();
exit(1);
}

/*****************************************************************************/

void AuditStatusMessage(FILE *fp,char status)

{
switch (status) /* Reminder */
   {
   case CF_CHG:
       fprintf(fp,"made a system correction");
       break;
       
   case CF_WARN:
       fprintf(fp,"promise not kept, no action taken");
       break;
       
   case CF_TIMEX:
       fprintf(fp,"timed out");
       break;

   case CF_FAIL:
       fprintf(fp,"failed to make a correction");
       break;
       
   case CF_DENIED:
       fprintf(fp,"was denied access to an essential resource");
       break;
       
   case CF_INTERPT:
       fprintf(fp,"was interrupted\n");
       break;

   case CF_NOP:
       fprintf(fp,"was applied but performed no required actions");
       break;

   case CF_UNKNOWN:
       fprintf(fp,"was applied but status unknown");
       break;

   case CF_REPORT:
       fprintf(fp,"report");
       break;
   }

}
