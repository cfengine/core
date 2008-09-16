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
/* File: agent.c                                                             */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/
/* Agent specific variables                                        */
/*******************************************************************/

enum typesequence
   {
   kp_classes,
   kp_interfaces,
   kp_processes,
   kp_commands,
   kp_files,
   kp_reports,
   kp_none
   };

char *TYPESEQUENCE[] =
   {
   "classes",
   "interfaces",
   "processes",
   "commands",
   "files",
   "reports",
   NULL
   };

int main (int argc,char *argv[]);
void CheckAgentAccess(struct Rlist *list);
void KeepAgentPromise(struct Promise *pp);
void NewTypeContext(enum typesequence type);
void DeleteTypeContext(enum typesequence type);
void ClassBanner(enum typesequence type);
void ParallelFindAndVerifyFilesPromises(struct Promise *pp);

extern struct BodySyntax CFA_CONTROLBODY[];
extern struct Rlist *SERVERLIST;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/
 
 struct option OPTIONS[12] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "dry-run",no_argument,0,'n'},
      { "version",no_argument,0,'V' },
      { "define",required_argument,0,'D' },
      { "negate",required_argument,0,'N' },
      { "no-lock",no_argument,0,'K'},
      { "inform",no_argument,0,'I'},
      { "syntax",no_argument,0,'S'},
      { "diagnostic",no_argument,0,'x'},
      { NULL,0,0,'\0' }
      };


/*******************************************************************/

int main(int argc,char *argv[])

{struct stat sar;

GenericInitialize(argc,argv,"agent");
PromiseManagement("agent");
ThisAgentInit();
KeepPromises();
return 0;
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  struct Item *actionList;
  int optindex = 0;
  int c;
  
while ((c=getopt_long(argc,argv,"d:vnKIf:pD:N:VSx",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          VINPUTFILE[CF_BUFSIZE-1] = '\0';
          MINUSF = true;
          break;

      case 'd': 
          AddClassToHeap("opt_debug");
          switch ((optarg==NULL) ? '3' : *optarg)
             {
             case '1':
                 D1 = true;
                 DEBUG = true;
                 break;
             case '2':
                 D2 = true;
                 DEBUG = true;
                 break;
             case '3':
                 D3 = true;
                 DEBUG = true;
                 VERBOSE = true;
                 break;
             case '4':
                 D4 = true;
                 DEBUG = true;
                 break;
             default:
                 DEBUG = true;
                 break;
             }
          break;
          
      case 'K': IGNORELOCK = true;
          break;
                    
      case 'D': AddMultipleClasses(optarg);
          break;
          
      case 'N': NegateCompoundClass(optarg,&VNEGHEAP);
          break;
          
      case 'I': INFORM = true;
          break;
          
      case 'v': VERBOSE = true;
          break;
          
      case 'n': DONTDO = true;
          IGNORELOCK = true;
          AddClassToHeap("opt_dry_run");
          break;
          
      case 'p': PARSEONLY = true;
          IGNORELOCK = true;
          break;          

      case 'V': Version("Cfengine Agent");
          exit(0);
          
      case 'h': Syntax("Cfengine Agent");
          exit(0);

      case 'x': AgentDiagnostic();
          exit(0);
          
      default:  Syntax("Cfengine Agent");
          exit(1);
          
      }
  }

Debug("Set debugging\n");
}

/*******************************************************************/

void ThisAgentInit()

{
signal(SIGINT,(void*)ExitCleanly);
signal(SIGTERM,(void*)ExitCleanly);
signal(SIGHUP,SIG_IGN);
signal(SIGPIPE,SIG_IGN);
signal(SIGUSR1,HandleSignals);
signal(SIGUSR2,HandleSignals);

/*
  do not set signal(SIGCHLD,SIG_IGN) in agent near
  popen() - or else pclose will fail to return
  status which we need for setting returns
*/
}

/*******************************************************************/

void KeepPromises()

{
BeginAudit();
KeepControlPromises();
KeepPromiseBundles();
EndAudit();
}

/*******************************************************************/
/* Level 2                                                         */
/*******************************************************************/

void KeepControlPromises()
    
{ struct Constraint *cp;
  char rettype;
  void *retval;

for (cp = ControlBodyConstraints(cf_agent); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }

   if (GetVariable("control_agent",cp->lval,&retval,&rettype) == cf_notype)
      {
      CfOut(cf_error,"","Unknown lval %s in agent control body",cp->lval);
      continue;
      }
            
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_maxconnections].lval) == 0)
      {
      CFA_MAXTHREADS = (int)Str2Int(retval);
      Verbose("SET maxconnections = %d\n",CFA_MAXTHREADS);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_agentfacility].lval) == 0)
      {
      SetFacility(retval);
      Verbose("SET Syslog FACILITY = %s\n",retval);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_agentaccess].lval) == 0)
      {
      ACCESSLIST = (struct Rlist *) retval;
      CheckAgentAccess(ACCESSLIST);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_abortclasses].lval) == 0)
      {
      struct Rlist *rp;
      Verbose("SET Abort classes from ...\n");
      
      for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
         {
         if (!IsItemIn(ABORTHEAP,rp->item))
            {
            AppendItem(&ABORTHEAP,rp->item,cp->classes);
            }
         }
      
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_addclasses].lval) == 0)
      {
      struct Rlist *rp;
      Verbose("ADD classes from ...\n");
      
      for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
         {
         AddClassToHeap(rp->item);
         }
      
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_auditing].lval) == 0)
      {
      AUDIT = GetBoolean(retval);
      Verbose("SET auditing = %d\n",AUDIT);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_binarypaddingchar].lval) == 0)
      {
      PADCHAR = *(char *)retval;
      Verbose("SET binarypaddingchar = %c\n",PADCHAR);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_bindtointerface].lval) == 0)
      {
      strncpy(BINDINTERFACE,retval,CF_BUFSIZE-1);
      Verbose("SET bindtointerface = %s\n",BINDINTERFACE);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_checksumpurge].lval) == 0)
      {
      if (GetBoolean(retval))
         {
         Verbose("Purging checksums\n");
         ChecksumPurge();
         }
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_checksumupdates].lval) == 0)
      {
      CHECKSUMUPDATES = GetBoolean(retval);
      Verbose("SET ChecksumUpdates %d\n",CHECKSUMUPDATES);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_compresscommand].lval) == 0)
      {
      COMPRESSCOMMAND = strdup(retval);
      Verbose("SET compresscommand = %s\n",COMPRESSCOMMAND);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_childlibpath].lval) == 0)
      {
      snprintf(OUTPUT,CF_BUFSIZE,"LD_LIBRARY_PATH=%s",retval);
      if (putenv(strdup(OUTPUT)) == 0)
         {
         Verbose("Setting %s\n",OUTPUT);
         }
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_defaultcopytype].lval) == 0)
      {
      DEFAULTCOPYTYPE = *(char *)retval;
      Verbose("SET defaultcopytype = %c\n",DEFAULTCOPYTYPE);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_fsinglecopy].lval) == 0)
      {
      SINGLE_COPY_LIST = (struct Rlist *)retval;
      Verbose("SET file single copy list\n");
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_fautodefine].lval) == 0)
      {
      AUTO_DEFINE_LIST = (struct Rlist *)retval;
      Verbose("SET file auto define list\n");
      continue;
      }
   
   /* Read directly from vars
      
     cfa_deletenonuserfiles,
     cfa_deletenonownerfiles,
     cfa_deletenonusermail,
     cfa_deletenonownermail,
     cfa_warnnonuserfiles,
     cfa_warnnonownerfiles,
     cfa_warnnonusermail,
     cfa_warnnonownermail,
     
   */

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_dryrun].lval) == 0)
      {
      DONTDO = GetBoolean(retval);
      Verbose("SET dryrun = %c\n",DONTDO);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_inform].lval) == 0)
      {
      INFORM = GetBoolean(retval);
      Verbose("SET inform = %c\n",INFORM);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_verbose].lval) == 0)
      {
      VERBOSE = GetBoolean(retval);
      Verbose("SET inform = %c\n",VERBOSE);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_repository].lval) == 0)
      {
      VREPOSITORY = strdup(retval);
      Verbose("SET compresscommand = %s\n",VREPOSITORY);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_skipidentify].lval) == 0)
      {
      SKIPIDENTIFY = GetBoolean(retval);
      Verbose("SET skipidentify = %c\n",SKIPIDENTIFY);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_repchar].lval) == 0)
      {
      REPOSCHAR = *(char *)retval;
      Verbose("SET repchar = %c\n",REPOSCHAR);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_logtidyhomefiles].lval) == 0)
      {
      LOGTIDYHOMEFILES = GetBoolean(retval);
      Verbose("SET logtidyhomefiles = %d\n",LOGTIDYHOMEFILES);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_syslog].lval) == 0)
      {
      LOGGING = GetBoolean(retval);
      Verbose("SET syslog = %c\n",LOGGING);
      continue;
      }
   }
}

/*********************************************************************/

void KeepPromiseBundles()
    
{ struct Bundle *bp;
  struct SubType *sp;
  struct Promise *pp;
  struct Rlist *rp,*params;
  struct FnCall *fp;
  char rettype,*name;
  void *retval;
  int ok = true;
  enum typesequence type;

if (GetVariable("control_common","bundlesequence",&retval,&rettype) == cf_notype)
   {
   CfOut(cf_error,"","No bundlesequence in the common control body");
   exit(1);
   }

for (rp = (struct Rlist *)retval; rp != NULL; rp=rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
          name = (char *)rp->item;
          params = NULL;
          break;
      case CF_FNCALL:
          fp = (struct FnCall *)rp->item;
          name = (char *)fp->name;
          params = (struct Rlist *)fp->args;
          break;
          
      default:
          name = NULL;
          params = NULL;
          CfOut(cf_error,"","Illegal item found in bundlesequence: ");
          ShowRval(stdout,rp->item,rp->type);
          printf(" = %c\n",rp->type);
          ok = false;
          break;
      }
   
   if (!(GetBundle(name,"agent")||(GetBundle(name,"common"))))
      {
      CfOut(cf_error,"","Bundle %s listed in the bundlesequence was not found\n",name);
      ok = false;
      }
   }

if (!ok)
   {
   FatalError("Errors in agent bundles");
   }

/* If all is okay, go ahead and evaluate */

for (rp = (struct Rlist *)retval; rp != NULL; rp=rp->next)
   {
   switch (rp->type)
      {
      case CF_FNCALL:
          fp = (struct FnCall *)rp->item;
          name = (char *)fp->name;
          params = (struct Rlist *)fp->args;
          break;
      default:
          name = (char *)rp->item;
          params = NULL;
          break;
      }
   
   if ((bp = GetBundle(name,"agent")) || (bp = GetBundle(name,"common")))
      {
      BannerBundle(bp,params);
      AugmentScope(bp->name,bp->args,params);
      DeletePrivateClassContext(); // Each time we change bundle      
      }

   SetScope(bp->name);
   
   for (type = 0; TYPESEQUENCE[type] != NULL; type++)
      {
      ClassBanner(type);
      
      if ((sp = GetSubTypeForBundle(TYPESEQUENCE[type],bp)) == NULL)
         {
         continue;      
         }

      BannerSubType(bp->name,sp->name);

      NewTypeContext(type);
      
      for (pp = sp->promiselist; pp != NULL; pp=pp->next)
         {
         ExpandPromise(cf_agent,bp->name,pp,KeepAgentPromise);
         }

      DeleteTypeContext(type);
      }
   }
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

void CheckAgentAccess(struct Rlist *list)

{ char id[CF_MAXVARSIZE];
  struct passwd *pw;
  struct Rlist *rp;
  struct stat sb;
  uid_t uid;
  int access = false;
  
uid = getuid();
  
for (rp  = list; rp != NULL; rp = rp->next)
   {
   if (Str2Uid(rp->item,NULL,NULL) == uid)
      {
      return;
      }
   }

if (VINPUTLIST != NULL)
   {
   for (rp = VINPUTLIST; rp != NULL; rp=rp->next)
      {
      stat(rp->item,&sb);
      
      if (ACCESSLIST)
         {
         for (rp  = ACCESSLIST; rp != NULL; rp = rp->next)
            {
            if (Str2Uid(rp->item,NULL,NULL) == sb.st_uid)
               {
               access = true;
               break;
               }
            }
         
         if (!access)
            {
            CfOut(cf_error,"","File %s is not owned by an authorized user (security exception)",rp->item);
            exit(1);
            }
         }
      else if (IsPrivileged())
         {
         if (sb.st_uid != getuid())
            {
            CfOut(cf_error,"","File %s is not owned by uid %d (security exception)",rp->item,getuid());
            exit(1);
            }
         }
      }
   }

FatalError("You are denied access to run this policy");
}

/*********************************************************************/

void KeepAgentPromise(struct Promise *pp)

{
if (!IsDefinedClass(pp->classes))
   {
   Verbose("\n");
   Verbose(". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   Verbose("Skipping whole next promise, as context %s is not valid\n",pp->classes);
   Verbose(". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   return;
   }

if (pp->done)
   {
   return;
   }

if (strcmp("classes",pp->agentsubtype) == 0)
   {
   KeepClassContextPromise(pp);
   return;
   }

if (strcmp("processes",pp->agentsubtype) == 0)
   {
   VerifyProcessesPromise(pp);
   return;
   }

if (strcmp("files",pp->agentsubtype) == 0)
   {
   if (GetBooleanConstraint("background",pp->conlist))
      {      
      ParallelFindAndVerifyFilesPromises(pp);
      }
   else
      {
      FindAndVerifyFilesPromises(pp);
      }
   return;
   }

if (strcmp("commands",pp->agentsubtype) == 0)
   {
   VerifyExecPromise(pp);
   return;
   }

if (strcmp("reports",pp->agentsubtype) == 0)
   {
   VerifyReportPromise(pp);
   return;
   }
}

/*********************************************************************/
/* Type context                                                      */
/*********************************************************************/

void NewTypeContext(enum typesequence type)

{ int maxconnections,i;
  struct Item *procdata = NULL;
  char *psopts = VPSOPTS[VSYSTEMHARDCLASS];
// get maxconnections

switch(type)
   {
   case kp_files:
       
       /* Prepare shared connection array for non-threaded remote copies */
       
       SERVERLIST = NULL;
       break;

   case kp_processes:
     
       if (!LoadProcessTable(&PROCESSTABLE,psopts))
          {
          CfLog(cferror,"Unable to read the process table\n","");
          AuditLog('y',NULL,0,"Processes inaccessible",CF_FAIL);   
          return;
          }

       break;
   }
}

/*********************************************************************/

void DeleteTypeContext(enum typesequence type)

{ struct Rlist *rp;
  struct ServerItem *svp;
 
switch(type)
   {
   case kp_files:

       /* Cleanup shared connection array for non-threaded remote copies */
       
       for (rp = SERVERLIST; rp != NULL; rp = rp->next)
          {
          svp = (struct ServerItem *)rp->item;
          DeleteAgentConn(svp->conn);
          free(svp->server);
          rp->item = NULL;
          }

       DeleteRlist(SERVERLIST);
       break;

   case kp_processes:

       /* should cleanup proc memory list */

       DeleteItemList(PROCESSTABLE);
       PROCESSTABLE = NULL;
       break;
   }
}

/**************************************************************/

void ClassBanner(enum typesequence type)

{ struct Item *ip;
 
if (type != kp_interfaces)   /* Just parsed all local classes */
   {
   return;
   }

Verbose("\n");
Verbose("     +  Private classes augmented:\n");

for (ip = VADDCLASSES; ip != NULL; ip=ip->next)
   {
   Verbose("     +       %s\n",ip->name);
   }

Verbose("\n");

Verbose("     -  Private classes diminished:\n");

for (ip = VNEGHEAP; ip != NULL; ip=ip->next)
   {
   Verbose("     -       %s\n",ip->name);
   }

Verbose("\n");

Verbose("     ?  Public class context:\n");

for (ip = VHEAP; ip != NULL; ip=ip->next)
   {
   Verbose("     ?       %s\n",ip->name);
   }

Verbose("\n");

}

/**************************************************************/
/* Thread context                                             */
/**************************************************************/

void ParallelFindAndVerifyFilesPromises(struct Promise *pp)
    
{ pid_t child = 1;
  int background = GetBooleanConstraint("background",pp->conlist);

if (background)
   {
   Verbose("Spawning new process...\n");
   child = fork();
   }

if (child || !background)
   {
   FindAndVerifyFilesPromises(pp);
   }
}

