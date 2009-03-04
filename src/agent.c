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
   kp_vars,
   kp_classes,
   kp_interfaces,
   kp_processes,
   kp_storage,
   kp_packages,
   kp_commands,
   kp_methods,
   kp_files,
   kp_reports,
   kp_none
   };

char *TYPESEQUENCE[] =
   {
   "vars",
   "classes",
   "interfaces",
   "processes",
   "storage",
   "packages",
   "commands",
   "methods",
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
void SetEnvironment(char *s);

extern struct BodySyntax CFA_CONTROLBODY[];
extern struct Rlist *SERVERLIST;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

 char *ID = "The main cfengine agent is the instigator of change\n"
            "in the system. In that sense it is the most important\n"
            "part of the cfengine suite.\n";

 struct option OPTIONS[13] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "dry-run",no_argument,0,'n'},
      { "version",no_argument,0,'V' },
      { "bootstrap",no_argument,0,'B' },
      { "file",required_argument,0,'f'},
      { "define",required_argument,0,'D' },
      { "negate",required_argument,0,'N' },
      { "no-lock",no_argument,0,'K'},
      { "inform",no_argument,0,'I'},
      { "diagnostic",no_argument,0,'x'},
      { NULL,0,0,'\0' }
      };

 char *HINTS[13] =
      {
      "Print the help message",
      "Set debugging level 0,1,2",
      "Output verbose information about the behaviour of the agent",
      "All talk and no action mode - make no changes, only inform of promises not kept",
      "Output the version of the software",
      "Bootstrap/repair a cfengine configuration from failsafe file in the current directory",
      "Specify an alternative input file than the default",
      "Define a list of comma separated classes to be defined at the start of execution",
      "Define a list of comma separated classes to be undefined at the start of execution",
      "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
      "Print basic information about changes made to the system, i.e. promises repaired",
      "Activate internal diagnostics (developers only)",
      NULL
      };

/*******************************************************************/

int main(int argc,char *argv[])

{ struct stat sar;

CheckOpts(argc,argv);
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

/* Because of the MacOS linker we have to call this from each agent
   individually before Generic Initialize */
  
while ((c=getopt_long(argc,argv,"rd:vnKIf:D:N:VSxMB",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          MINUSF = true;
          break;

      case 'd': 
          NewClass("opt_debug");
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
             default:
                 DEBUG = true;
                 break;
             }
          break;

      case 'B':
          strncpy(VINPUTFILE,"./failsafe.cf",CF_BUFSIZE-1);
          BOOTSTRAP = true;
          MINUSF = true;
          break;
          
      case 'K': IGNORELOCK = true;
          break;
                    
      case 'D': NewClassesFromString(optarg);
          break;
          
      case 'N': NegateClassesFromString(optarg,&VNEGHEAP);
          break;
          
      case 'I': INFORM = true;
          break;
          
      case 'v': VERBOSE = true;
          break;
          
      case 'n': DONTDO = true;
          IGNORELOCK = true;
          NewClass("opt_dry_run");
          break;
          
      case 'V':
          Version("cf-agent");
          exit(0);
          
      case 'h':
          Syntax("cf-agent - cfengine's change agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'M':
          ManPage("cf-agent - cfengine's change agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'x':
          AgentDiagnostic();
          exit(0);

      case 'r':
          SHOWREPORTS = true;
          break;

      default:  Syntax("cf-agent - cfengine's change agent",OPTIONS,HINTS,ID);
          exit(1);
          
      }
  }

Debug("Set debugging\n");
}

/*******************************************************************/

void ThisAgentInit()

{ FILE *fp;
  char filename[CF_BUFSIZE];
 
signal(SIGINT,HandleSignals);
signal(SIGTERM,HandleSignals);
signal(SIGHUP,SIG_IGN);
signal(SIGPIPE,SIG_IGN);
signal(SIGUSR1,HandleSignals);
signal(SIGUSR2,HandleSignals);

CFA_MAXTHREADS = 30;
EDITFILESIZE = 100000;

snprintf(filename,CF_BUFSIZE,"%s/cfagent.%s.log",CFWORKDIR,VSYSNAME.nodename);

if ((fp = fopen(filename,"a")) == NULL)
   {
   CfOut(cf_error,"fopen","Unable to create a writable log %s\n",filename);
   }
else
   {
   fclose(fp);
   }

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

   if (GetVariable("control_common",cp->lval,&retval,&rettype) != cf_notype)
      {
      /* Already handled in generic_agent */
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
      CfOut(cf_verbose,"","SET maxconnections = %d\n",CFA_MAXTHREADS);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_agentfacility].lval) == 0)
      {
      SetFacility(retval);
      CfOut(cf_verbose,"","SET Syslog FACILITY = %s\n",retval);
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
      CfOut(cf_verbose,"","SET Abort classes from ...\n");
      
      for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
         {
         if (!IsItemIn(ABORTHEAP,rp->item))
            {
            AppendItem(&ABORTHEAP,rp->item,cp->classes);
            }
         }
      
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_abortbundleclasses].lval) == 0)
      {
      struct Rlist *rp;
      CfOut(cf_verbose,"","SET Abort bundle classes from ...\n");
      
      for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
         {
         if (!IsItemIn(ABORTBUNDLEHEAP,rp->item))
            {
            AppendItem(&ABORTBUNDLEHEAP,rp->item,cp->classes);
            }
         }
      
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_addclasses].lval) == 0)
      {
      struct Rlist *rp;
      CfOut(cf_verbose,"","ADD classes from ...\n");
      
      for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
         {
         NewClass(rp->item);
         }
      
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_auditing].lval) == 0)
      {
      AUDIT = GetBoolean(retval);
      CfOut(cf_verbose,"","SET auditing = %d\n",AUDIT);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_secureinput].lval) == 0)
      {
      CFPARANOID = GetBoolean(retval);
      CfOut(cf_verbose,"","SET secure input = %d\n",CFPARANOID);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_binarypaddingchar].lval) == 0)
      {
      PADCHAR = *(char *)retval;
      CfOut(cf_verbose,"","SET binarypaddingchar = %c\n",PADCHAR);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_bindtointerface].lval) == 0)
      {
      strncpy(BINDINTERFACE,retval,CF_BUFSIZE-1);
      CfOut(cf_verbose,"","SET bindtointerface = %s\n",BINDINTERFACE);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_hashupdates].lval) == 0)
      {
      CHECKSUMUPDATES = GetBoolean(retval);
      CfOut(cf_verbose,"","SET ChecksumUpdates %d\n",CHECKSUMUPDATES);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_exclamation].lval) == 0)
      {
      EXCLAIM = GetBoolean(retval);
      CfOut(cf_verbose,"","SET exclamation %d\n",EXCLAIM);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_childlibpath].lval) == 0)
      {
      char output[CF_BUFSIZE];
      snprintf(output,CF_BUFSIZE,"LD_LIBRARY_PATH=%s",retval);
      if (putenv(strdup(output)) == 0)
         {
         CfOut(cf_verbose,"","Setting %s\n",output);
         }
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_defaultcopytype].lval) == 0)
      {
      DEFAULT_COPYTYPE = (char *)retval;
      CfOut(cf_verbose,"","SET defaultcopytype = %c\n",DEFAULT_COPYTYPE);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_fsinglecopy].lval) == 0)
      {
      SINGLE_COPY_LIST = (struct Rlist *)retval;
      CfOut(cf_verbose,"","SET file single copy list\n");
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_fautodefine].lval) == 0)
      {
      AUTO_DEFINE_LIST = (struct Rlist *)retval;
      CfOut(cf_verbose,"","SET file auto define list\n");
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_dryrun].lval) == 0)
      {
      DONTDO = GetBoolean(retval);
      CfOut(cf_verbose,"","SET dryrun = %c\n",DONTDO);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_inform].lval) == 0)
      {
      INFORM = GetBoolean(retval);
      CfOut(cf_verbose,"","SET inform = %c\n",INFORM);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_verbose].lval) == 0)
      {
      VERBOSE = GetBoolean(retval);
      CfOut(cf_verbose,"","SET inform = %c\n",VERBOSE);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_repository].lval) == 0)
      {
      VREPOSITORY = strdup(retval);
      CfOut(cf_verbose,"","SET compresscommand = %s\n",VREPOSITORY);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_skipidentify].lval) == 0)
      {
      SKIPIDENTIFY = GetBoolean(retval);
      CfOut(cf_verbose,"","SET skipidentify = %c\n",SKIPIDENTIFY);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_repchar].lval) == 0)
      {
      REPOSCHAR = *(char *)retval;
      CfOut(cf_verbose,"","SET repchar = %c\n",REPOSCHAR);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_mountfilesystems].lval) == 0)
      {
      CF_MOUNTALL = GetBoolean(retval);
      CfOut(cf_verbose,"","SET mountfilesystems = %d\n",CF_MOUNTALL);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_editfilesize].lval) == 0)
      {
      EDITFILESIZE = Str2Int(retval);
      CfOut(cf_verbose,"","SET EDITFILESIZE = %d\n",EDITFILESIZE);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_ifelapsed].lval) == 0)
      {
      VIFELAPSED = Str2Int(retval);
      CfOut(cf_verbose,"","SET ifelapsed = %d\n",VIFELAPSED);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_expireafter].lval) == 0)
      {
      VEXPIREAFTER = Str2Int(retval);
      CfOut(cf_verbose,"","SET ifelapsed = %d\n",VEXPIREAFTER);
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_max_children].lval) == 0)
      {
      CFA_BACKGROUND_LIMIT = Str2Int(retval);
      CfOut(cf_verbose,"","SET MAX_CHILDREN = %d\n",CFA_BACKGROUND_LIMIT);
      if (CFA_BACKGROUND_LIMIT > 10)
         {
         CfOut(cf_error,"","Silly value for max_children in agent control promise (%d > 10)",CFA_BACKGROUND_LIMIT);
         CFA_BACKGROUND_LIMIT = 1;
         }
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_syslog].lval) == 0)
      {
      LOGGING = GetBoolean(retval);
      CfOut(cf_verbose,"","SET syslog = %c\n",LOGGING);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_environment].lval) == 0)
      {
      struct Rlist *rp;
      CfOut(cf_verbose,"","SET environment variables from ...\n");
      
      for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
         {
         SetEnvironment(rp->item);
         }
      
      continue;
      }
   }

if (GetVariable("control_common","CFG_CONTROLBODY[cfg_lastseenexpireafter]",&retval,&rettype) != cf_notype)
   {
   LASTSEENEXPIREAFTER = Str2Int(retval);
   }
}

/*********************************************************************/

void KeepPromiseBundles()
    
{ struct Bundle *bp;
  struct Rlist *rp,*params;
  struct FnCall *fp;
  char rettype,*name;
  void *retval;
  int ok = true;

if (GetVariable("control_common","bundlesequence",&retval,&rettype) == cf_notype)
   {
   CfOut(cf_error,"","No bundlesequence in the common control body");
   exit(1);
   }

if (rettype != CF_LIST)
   {
   FatalError("Promised bundlesequence was not a list");
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
      AugmentScope(bp->name,bp->args,params);
      BannerBundle(bp,params);
      ScheduleAgentOperations(bp);
      }
   }
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

int ScheduleAgentOperations(struct Bundle *bp)

{ struct SubType *sp;
  struct Promise *pp;
  enum typesequence type;
  int pass;

DeletePrivateClassContext(); // Each time we change bundle
  
for (pass = 1; pass < CF_DONEPASSES; pass++)
   {
   for (type = 0; TYPESEQUENCE[type] != NULL; type++)
      {
      ClassBanner(type);
      
      if ((sp = GetSubTypeForBundle(TYPESEQUENCE[type],bp)) == NULL)
         {
         continue;      
         }

      BannerSubType(bp->name,sp->name,pass);
      SetScope(bp->name);
      
      NewTypeContext(type);

      for (pp = sp->promiselist; pp != NULL; pp=pp->next)
         {
         ExpandPromise(cf_agent,bp->name,pp,KeepAgentPromise);

         if (Abort())
            {
            DeleteTypeContext(type);
            return false;
            }
         }

      DeleteTypeContext(type);      
      }
   }

return true;
}

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
      else if (CFPARANOID && IsPrivileged())
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

{ char *sp = NULL;
  struct timespec start = BeginMeasure();

if (!IsDefinedClass(pp->classes))
   {
   CfOut(cf_verbose,"","\n");
   CfOut(cf_verbose,"",". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   CfOut(cf_verbose,"","Skipping whole next promise (%s), as context %s is not relevant\n",pp->promiser,pp->classes);
   CfOut(cf_verbose,"",". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   return;
   }
 
if (pp->done)
   {
   return;
   }

if (VarClassExcluded(pp,&sp))
   {
   CfOut(cf_verbose,"","\n");
   CfOut(cf_verbose,"",". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   CfOut(cf_verbose,"","Skipping whole next promise (%s), as var-context %s is not relevant\n",pp->promiser,sp);
   CfOut(cf_verbose,"",". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   return;
   }

if (strcmp("classes",pp->agentsubtype) == 0)
   {
   KeepClassContextPromise(pp);
   return;
   }

if (strcmp("interfaces",pp->agentsubtype) == 0)
   {
   VerifyInterfacesPromise(pp);
   return;
   }

if (strcmp("processes",pp->agentsubtype) == 0)
   {
   VerifyProcessesPromise(pp);
   return;
   }

if (strcmp("storage",pp->agentsubtype) == 0)
   {
   FindAndVerifyStoragePromises(pp);
   EndMeasurePromise(start,pp);
   return;
   }

if (strcmp("packages",pp->agentsubtype) == 0)
   {
   VerifyPackagesPromise(pp);
   EndMeasurePromise(start,pp);
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
   
   EndMeasurePromise(start,pp);
   return;
   }
  
if (strcmp("commands",pp->agentsubtype) == 0)
   {
   VerifyExecPromise(pp);
   EndMeasurePromise(start,pp);
   return;
   }

if (strcmp("methods",pp->agentsubtype) == 0)
   {
   VerifyMethodsPromise(pp);
   EndMeasurePromise(start,pp);
   return;
   }

if (strcmp("reports",pp->agentsubtype) == 0)
   {
   VerifyReportPromise(pp);
   return;
   }
}

/*********************************************************************/

void SetEnvironment(char *s)

{
if (putenv(s) != 0)
   {
   CfOut(cf_inform,"putenv","Failed to set environement %s",s);
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
          struct Promise dummyp;
          struct Attributes dummyattr;
          memset(&dummyp,0,sizeof(dummyp));
          memset(&dummyattr,0,sizeof(dummyattr));
          dummyattr.transaction.audit = true;
          dummyattr.transaction.log_level = cf_inform;
          cfPS(cf_error,CF_FAIL,"",&dummyp,dummyattr,"Unable to read the process table\n","");
          return;
          }
       break;

   case kp_storage:

       if (MOUNTEDFSLIST != NULL)
          {
          DeleteMountInfo(MOUNTEDFSLIST);
          MOUNTEDFSLIST = NULL;
          }

       break;

   case kp_packages:
       INSTALLED_PACKAGE_LISTS = NULL;
       break;
   }
}

/*********************************************************************/

void DeleteTypeContext(enum typesequence type)

{ struct Rlist *rp;
  struct ServerItem *svp;
  struct Attributes a;
 
switch(type)
   {
   case kp_files:

       /* Cleanup shared connection array for non-threaded remote copies */
       
       for (rp = SERVERLIST; rp != NULL; rp = rp->next)
          {
          svp = (struct ServerItem *)rp->item;
          ServerDisconnection(svp->conn);
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

   case kp_storage:

       CfOut(cf_verbose,""," -> Number of changes observed in %s is %d\n",VFSTAB[VSYSTEMHARDCLASS],FSTAB_EDITS);
       
       if (FSTAB_EDITS && FSTABLIST && !DONTDO)
          {
          SaveItemListAsFile(FSTABLIST,VFSTAB[VSYSTEMHARDCLASS],a,NULL);
          DeleteItemList(FSTABLIST);
          FSTABLIST = NULL;
          FSTAB_EDITS = 0;
          }

       if (!DONTDO && CF_MOUNTALL)
          {
          CfOut(cf_verbose,"","");
          CfOut(cf_verbose,""," -> Mounting all filesystems\n");
          MountAll();
          CfOut(cf_verbose,"","");
          }

       break;

   case kp_packages:

       if (!DONTDO && PACKAGE_SCHEDULE)
          {
          ExecutePackageSchedule(PACKAGE_SCHEDULE);
          }

       DeletePackageManagers(INSTALLED_PACKAGE_LISTS);
       DeletePackageManagers(PACKAGE_SCHEDULE);
       INSTALLED_PACKAGE_LISTS = NULL;
       PACKAGE_SCHEDULE = NULL;
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

CfOut(cf_verbose,"","\n");
CfOut(cf_verbose,"","     +  Private classes augmented:\n");

for (ip = VADDCLASSES; ip != NULL; ip=ip->next)
   {
   CfOut(cf_verbose,"","     +       %s\n",ip->name);
   }

CfOut(cf_verbose,"","\n");

CfOut(cf_verbose,"","     -  Private classes diminished:\n");

for (ip = VNEGHEAP; ip != NULL; ip=ip->next)
   {
   CfOut(cf_verbose,"","     -       %s\n",ip->name);
   }

CfOut(cf_verbose,"","\n");

Debug("     ?  Public class context:\n");

for (ip = VHEAP; ip != NULL; ip=ip->next)
   {
   Debug("     ?       %s\n",ip->name);
   }

CfOut(cf_verbose,"","\n");

}

/**************************************************************/
/* Thread context                                             */
/**************************************************************/

void ParallelFindAndVerifyFilesPromises(struct Promise *pp)
    
{ pid_t child = 1;
  int background = GetBooleanConstraint("background",pp->conlist);

if (background && (CFA_BACKGROUND < CFA_BACKGROUND_LIMIT))
   {
   CFA_BACKGROUND++;
   CfOut(cf_verbose,"","Spawning new process...\n");
   child = fork();

   if (child == 0)
      {
      ALARM_PID = -1;
      AM_BACKGROUND_PROCESS = true;
      }
   else
      {
      AM_BACKGROUND_PROCESS = false;
      }
   }
else if (CFA_BACKGROUND >= CFA_BACKGROUND_LIMIT)
   {
   CfOut(cf_verbose,""," !> Promised parallel execution promised but exceeded the max number of promised background tasks, so serializing");
   }

if (child || !background)
   {
   FindAndVerifyFilesPromises(pp);
   }
}

