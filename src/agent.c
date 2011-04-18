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
   kp_outputs,
   kp_interfaces,
   kp_files,
   kp_packages,
   kp_environments,
   kp_methods,
   kp_processes,
   kp_services,
   kp_commands,
   kp_storage,
   kp_databases,
   kp_reports,
   kp_none
   };

char *TYPESEQUENCE[] =
   {
   "vars",
   "classes",    /* Maelstrom order 2 */
   "outputs",
   "interfaces",
   "files",
   "packages",
   "environments",
   "methods",
   "processes",
   "services",
   "commands",
   "storage",
   "databases",
   "reports",
   NULL
   };

int main (int argc,char *argv[]);
void CheckAgentAccess(struct Rlist *list);
void KeepAgentPromise(struct Promise *pp);
int NewTypeContext(enum typesequence type);
void DeleteTypeContext(enum typesequence type);
void ClassBanner(enum typesequence type);
void ParallelFindAndVerifyFilesPromises(struct Promise *pp);
void SetEnvironment(char *s);

extern struct BodySyntax CFA_CONTROLBODY[];
extern struct Rlist *SERVERLIST;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

 char *ID = "The main Cfengine agent is the instigator of change\n"
            "in the system. In that sense it is the most important\n"
            "part of the Cfengine suite.\n";

 struct option OPTIONS[15] =
      {
      { "bootstrap",no_argument,0,'B' },
      { "bundlesequence",required_argument,0,'b' },
      { "debug",optional_argument,0,'d' },
      { "define",required_argument,0,'D' },
      { "diagnostic",optional_argument,0,'x'},
      { "dry-run",no_argument,0,'n'},
      { "file",required_argument,0,'f'},
      { "help",no_argument,0,'h' },
      { "inform",no_argument,0,'I'},
      { "negate",required_argument,0,'N' },
      { "no-lock",no_argument,0,'K'},
      { "policy-server",required_argument,0,'s' },
      { "verbose",no_argument,0,'v' },
      { "version",no_argument,0,'V' },
      { NULL,0,0,'\0' }
      };

 char *HINTS[15] =
      {
      "Bootstrap/repair a cfengine configuration from failsafe file in the WORKDIR else in current directory",
      "Set or override bundlesequence from command line",
      "Set debugging level 0,1,2",
      "Define a list of comma separated classes to be defined at the start of execution",
      "Do internal diagnostic (developers only) level in optional argument",
      "All talk and no action mode - make no changes, only inform of promises not kept",
      "Specify an alternative input file than the default",      
      "Print the help message",
      "Print basic information about changes made to the system, i.e. promises repaired",
      "Define a list of comma separated classes to be undefined at the start of execution",
      "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
      "Define the server name or IP address of the a policy server (for use with bootstrap)",
      "Output verbose information about the behaviour of the agent",
      "Output the version of the software",
      NULL
      };

/*******************************************************************/

int main(int argc,char *argv[])

{
CheckOpts(argc,argv);
GenericInitialize(argc,argv,"agent");
PromiseManagement("agent");
ThisAgentInit();
KeepPromises();
NoteClassUsage(VHEAP);
#ifdef HAVE_NOVA
Nova_NoteVarUsageDB();
#endif
UpdateLastSeen();
PurgeLocks();
GenericDeInitialize();
return 0;
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
 char arg[CF_BUFSIZE],*sp;
  int optindex = 0;
  int c,alpha = false,v6 = false;

/* Because of the MacOS linker we have to call this from each agent
   individually before Generic Initialize */

POLICY_SERVER[0] = '\0';
  
while ((c=getopt_long(argc,argv,"rd:vnKIf:D:N:Vs:x:MBb:",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          if (optarg == NULL)
             {
             FatalError(" -f used but no argument");
             }

          if (optarg && strlen(optarg) < 5)
             {
             snprintf(arg,CF_MAXVARSIZE," -f used but argument \"%s\" incorrect",optarg);
             FatalError(arg);
             }

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          MINUSF = true;
          break;

      case 'b':
          if (optarg)
             {
             CBUNDLESEQUENCE = SplitStringAsRList(optarg,',');
             }
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
          BOOTSTRAP = true;
          MINUSF = true;
          NewClass("bootstrap_mode");
          break;

      case 's':
	  
	  // temporary assure that network functions are working
   	  OpenNetwork();

          strncpy(POLICY_SERVER,Hostname2IPString(optarg),CF_BUFSIZE-1);

          CloseNetwork();


          for (sp = POLICY_SERVER; *sp != '\0'; sp++)
             {
             if (isalpha(*sp))
                {
                alpha = true;
                }

             if (ispunct(*sp) && *sp != ':' && *sp != '.')
                {
                alpha = true;
                }
             
             if (*sp == ':')
                {
                v6 = true;
                }
             }

          if (alpha && !v6)
             {
             FatalError("Error specifying policy server. The policy server's IP address could not be looked up. Please use the IP address instead if there is no error.");
             }
          
          break;
          
      case 'K':
          IGNORELOCK = true;
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
	 AgentDiagnostic(optarg);
          exit(0);
          
      case 'r':
          SHOWREPORTS = true;
          break;

      default:  Syntax("cf-agent - cfengine's change agent",OPTIONS,HINTS,ID);
          exit(1);          
      }
  }

if (argv[optind] != NULL)
   {
   CfOut(cf_error,"","Unexpected argument with no preceding option: %s\n",argv[optind]);
   FatalError("Aborted");
   }

Debug("Set debugging\n");
}

/*******************************************************************/

void ThisAgentInit()

{ FILE *fp;
  char filename[CF_BUFSIZE];

#ifdef HAVE_SETSID
CfOut(cf_verbose,""," -> Immunizing against parental death");
setsid();
#endif

signal(SIGINT,HandleSignals);
signal(SIGTERM,HandleSignals);
signal(SIGHUP,SIG_IGN);
signal(SIGPIPE,SIG_IGN);
signal(SIGUSR1,HandleSignals);
signal(SIGUSR2,HandleSignals);

CFA_MAXTHREADS = 30;
EDITFILESIZE = 100000;

/*
  do not set signal(SIGCHLD,SIG_IGN) in agent near
  popen() - or else pclose will fail to return
  status which we need for setting returns
*/

snprintf(filename,CF_BUFSIZE,"%s/cfagent.%s.log",CFWORKDIR,VSYSNAME.nodename);
MapName(filename);

if ((fp = fopen(filename,"a")) != NULL)
   {
   fclose(fp);
   }
}

/*******************************************************************/

void KeepPromises()

{ double efficiency;
 
BeginAudit();
KeepControlPromises();
KeepPromiseBundles();
EndAudit();

CfOut(cf_verbose,"","Estimated system complexity as touched objects = %d, for %d promises",CF_NODES,CF_EDGES);

efficiency = 100.0*CF_EDGES/(double)(CF_NODES+CF_EDGES);

NoteEfficiency(efficiency);
}

/*******************************************************************/
/* Level 2                                                         */
/*******************************************************************/

void KeepControlPromises()
    
{ struct Constraint *cp;
  char rettype;
  void *retval;
  struct Rlist *rp;

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

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_checksum_alert_time].lval) == 0)
      {
      CF_PERSISTENCE = (int)Str2Int(retval);
      CfOut(cf_verbose,"","SET checksum_alert_time = %d\n",CF_PERSISTENCE);
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

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_refresh_processes].lval) == 0)
      {
      struct Rlist *rp;

      if (VERBOSE)
         {
         printf("%s> SET refresh_processes when starting: ",VPREFIX);

         for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
            {
            printf(" %s",rp->item);
            PrependItem(&PROCESSREFRESH,rp->item,NULL);
            }

         printf("\n");
         }
      
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_abortclasses].lval) == 0)
      {
      struct Rlist *rp;
      CfOut(cf_verbose,"","SET Abort classes from ...\n");
      
      for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
         {
         char name[CF_MAXVARSIZE] = "";
         strncpy(name, rp->item, CF_MAXVARSIZE - 1);
         CanonifyNameInPlace(name);

         if (!IsItemIn(ABORTHEAP,name))
            {
            AppendItem(&ABORTHEAP,name,cp->classes);
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
         char name[CF_MAXVARSIZE] = "";
         strncpy(name, rp->item, CF_MAXVARSIZE - 1);
         CanonifyNameInPlace(name);

         if (!IsItemIn(ABORTBUNDLEHEAP,name))
            {
            AppendItem(&ABORTBUNDLEHEAP,name,cp->classes);
            }
         }
      
      continue;
      }
   
   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_addclasses].lval) == 0)
      {
      struct Rlist *rp;
      CfOut(cf_verbose,"","-> Add classes ...\n");
      
      for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
         {
         CfOut(cf_verbose,""," -> ... %s\n",rp->item);
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

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_alwaysvalidate].lval) == 0)
      {
      ALWAYS_VALIDATE = GetBoolean(retval);
      CfOut(cf_verbose,"","SET alwaysvalidate = %d\n",ALWAYS_VALIDATE);
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
      snprintf(output,CF_BUFSIZE,"LD_LIBRARY_PATH=%s",(char *)retval);
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
      CfOut(cf_verbose,"","SET repository = %s\n",VREPOSITORY);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_skipidentify].lval) == 0)
      {
      SKIPIDENTIFY = GetBoolean(retval);
      CfOut(cf_verbose,"","SET skipidentify = %d\n",SKIPIDENTIFY);
      continue;
      }

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_suspiciousnames].lval) == 0)
      {

      for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
	{
	PrependItem(&SUSPICIOUSLIST,rp->item,NULL);
	CfOut(cf_verbose,"", "-> Concidering %s as suspicious file", rp->item);
	}

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

   if (strcmp(cp->lval,CFA_CONTROLBODY[cfa_timeout].lval) == 0)
      {
      CONNTIMEOUT = Str2Int(retval);
      CfOut(cf_verbose,"","SET timeout = %d\n",CONNTIMEOUT);
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
      CfOut(cf_verbose,"","SET syslog = %d\n",LOGGING);
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

if (GetVariable("control_common",CFG_CONTROLBODY[cfg_lastseenexpireafter].lval,&retval,&rettype) != cf_notype)
   {
   LASTSEENEXPIREAFTER = Str2Int(retval);
   }

if (GetVariable("control_common",CFG_CONTROLBODY[cfg_fips_mode].lval,&retval,&rettype) != cf_notype)
   {
   FIPS_MODE = GetBoolean(retval);
   CfOut(cf_verbose,"","SET FIPS_MODE = %d\n",FIPS_MODE);
   }

if (GetVariable("control_common",CFG_CONTROLBODY[cfg_syslog_port].lval,&retval,&rettype) != cf_notype)
   {
   SYSLOGPORT = (unsigned short)Str2Int(retval);
   CfOut(cf_verbose,"","SET syslog_port to %d",SYSLOGPORT);
   }

if (GetVariable("control_common",CFG_CONTROLBODY[cfg_syslog_host].lval,&retval,&rettype) != cf_notype)
   {   
   strncpy(SYSLOGHOST,Hostname2IPString(retval),CF_MAXVARSIZE-1);
   CfOut(cf_verbose,"","SET syslog_host to %s",SYSLOGHOST);
   }

#ifdef HAVE_NOVA
Nova_Initialize();
#endif
}

/*********************************************************************/

void KeepPromiseBundles()
    
{ struct Bundle *bp;
  struct Rlist *rp,*params;
  struct FnCall *fp;
  char rettype,*name;
  void *retval;
  int ok = true;

if (CBUNDLESEQUENCE)
   {
   CfOut(cf_inform,""," >> Using command line specified bundlesequence");
   retval = CBUNDLESEQUENCE;
   rettype = CF_LIST;
   }
else if (GetVariable("control_common","bundlesequence",&retval,&rettype) == cf_notype)
   {
   CfOut(cf_error,""," !! !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
   CfOut(cf_error,""," !! No bundlesequence in the common control body");
   CfOut(cf_error,""," !! !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
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

          if (strcmp(name,CF_NULL_VALUE) == 0)
             {
             continue;
             }
          
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

   if (!IGNORE_MISSING_BUNDLES)
      {
      if (!(GetBundle(name,"agent")||(GetBundle(name,"common"))))
         {
         CfOut(cf_error,"","Bundle \"%s\" listed in the bundlesequence was not found\n",name);
         ok = false;
         }
      }
   }

if (!ok)
   {
   FatalError("Errors in agent bundles");
   }

if (VERBOSE || DEBUG)
   {
   printf("%s> -> Bundlesequence => ",VPREFIX);
   ShowRval(stdout,retval,rettype);
   printf("\n");
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
      SetBundleOutputs(bp->name);
      AugmentScope(bp->name,bp->args,params);
      BannerBundle(bp,params);
      THIS_BUNDLE = bp->name;
      DeletePrivateClassContext(); // Each time we change bundle
      ScheduleAgentOperations(bp);
      ResetBundleOutputs(bp->name);
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

if (PROCESSREFRESH == NULL || (PROCESSREFRESH && IsRegexItemIn(PROCESSREFRESH,bp->name)))
   {
   DeleteItemList(PROCESSTABLE);
   PROCESSTABLE = NULL;
   }

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

      if (!NewTypeContext(type))
         {
         continue;
         }

      for (pp = sp->promiselist; pp != NULL; pp=pp->next)
         {
         SaveClassEnvironment();

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

#ifdef MINGW
{
}
#else  /* NOT MINGW */
{
  struct Rlist *rp,*rp2;
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
      cfstat(rp->item,&sb);
      
      if (ACCESSLIST)
         {
         for (rp2  = ACCESSLIST; rp2 != NULL; rp2 = rp2->next)
            {
            if (Str2Uid(rp2->item,NULL,NULL) == sb.st_uid)
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
#endif  /* NOT MINGW */

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

// Record promises examined for efficiency calc

CF_EDGES++;

if (strcmp("vars",pp->agentsubtype) == 0)
   {
   ConvergeVarHashPromise(pp->bundle,pp,true);
   return;
   }

if (strcmp("classes",pp->agentsubtype) == 0)
   {
   KeepClassContextPromise(pp);
   return;
   }

if (strcmp("outputs",pp->agentsubtype) == 0)
   {
   VerifyOutputsPromise(pp);
   return;
   }

SetPromiseOutputs(pp);

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
   if (GetBooleanConstraint("background",pp))
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

if (strcmp("databases",pp->agentsubtype) == 0)
   {
   VerifyDatabasePromises(pp);
   EndMeasurePromise(start,pp);
   return;
   }

if (strcmp("methods",pp->agentsubtype) == 0)
   {
   VerifyMethodsPromise(pp);
   EndMeasurePromise(start,pp);
   return;
   }

if (strcmp("services",pp->agentsubtype) == 0)
   {
   VerifyServicesPromise(pp);
   EndMeasurePromise(start,pp);
   return;
   }

if (strcmp("environments",pp->agentsubtype) == 0)
   {
   VerifyEnvironmentsPromise(pp);
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
   CfOut(cf_inform,"putenv","Failed to set environment %s",s);
   }
}

/*********************************************************************/
/* Type context                                                      */
/*********************************************************************/

int NewTypeContext(enum typesequence type)

{
// get maxconnections

switch(type)
   {
   case kp_environments:
#ifdef HAVE_NOVA
      Nova_NewEnvironmentsContext();
#endif
      break;
       
   case kp_files:

       SERVERLIST = NULL;
       break;

   case kp_processes:
     
       if (!LoadProcessTable(&PROCESSTABLE))
          {
          CfOut(cf_error,"","Unable to read the process table - cannot keep process promises\n","");
          return false;
          }
       break;

   case kp_storage:

#ifndef MINGW  // TODO: Run if implemented on Windows
       if (MOUNTEDFSLIST != NULL)
          {
          DeleteMountInfo(MOUNTEDFSLIST);
          MOUNTEDFSLIST = NULL;
          }
#endif  /* NOT MINGW */
       break;

   case kp_packages:
       INSTALLED_PACKAGE_LISTS = NULL;
       break;
   }

return true;
}

/*********************************************************************/

void DeleteTypeContext(enum typesequence type)

{ struct Rlist *rp;
  struct ServerItem *svp;
  struct Attributes a = {{0}};
 
switch(type)
   {
   case kp_classes:
       HashVariables(THIS_BUNDLE);
       break;

   case kp_environments:
#ifdef HAVE_NOVA
      Nova_DeleteEnvironmentsContext();
#endif
      break;

   case kp_files:

       /* Cleanup shared connection array for non-threaded remote copies */
       
       for (rp = SERVERLIST; rp != NULL; rp = rp->next)
          {
          svp = (struct ServerItem *)rp->item;

          if (svp == NULL)
             {
             continue;
             }
          
          ServerDisconnection(svp->conn);
          
          if (svp->server)
             {
             free(svp->server);
             }

          rp->item = NULL;
          }

       DeleteRlist(SERVERLIST);
       SERVERLIST = NULL;

       break;

   case kp_processes:
       break;

   case kp_storage:
#ifndef MINGW
       CfOut(cf_verbose,""," -> Number of changes observed in %s is %d\n",VFSTAB[VSYSTEMHARDCLASS],FSTAB_EDITS);
       
       if (FSTAB_EDITS && FSTABLIST && !DONTDO)
          {
          if (FSTABLIST)
             {
             SaveItemListAsFile(FSTABLIST,VFSTAB[VSYSTEMHARDCLASS],a,NULL);
             DeleteItemList(FSTABLIST);
             FSTABLIST = NULL;
             }
          FSTAB_EDITS = 0;
          }

       if (!DONTDO && CF_MOUNTALL)
          {
          CfOut(cf_verbose,"","");
          CfOut(cf_verbose,""," -> Mounting all filesystems\n");
          MountAll();
          CfOut(cf_verbose,"","");
          }
#endif  /* NOT MINGW */
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
  int i;
 
if (type != kp_interfaces)   /* Just parsed all local classes */
   {
   return;
   }

CfOut(cf_verbose,"","\n");
CfOut(cf_verbose,"","     +  Private classes augmented:\n");

for (i = 0; i < CF_ALPHABETSIZE; i++)
   {
   for (ip = VADDCLASSES.list[i]; ip != NULL; ip=ip->next)
      {
      CfOut(cf_verbose,"","     +       %s\n",ip->name);
      }
   }

NoteClassUsage(VADDCLASSES);

CfOut(cf_verbose,"","\n");

CfOut(cf_verbose,"","     -  Private classes diminished:\n");

for (ip = VNEGHEAP; ip != NULL; ip=ip->next)
   {
   CfOut(cf_verbose,"","     -       %s\n",ip->name);
   }

CfOut(cf_verbose,"","\n");

Debug("     ?  Public class context:\n");

for (i = 0; i < CF_ALPHABETSIZE; i++)
   {
   for (ip = VHEAP.list[i]; ip != NULL; ip=ip->next)
      {
      Debug("     ?       %s\n",ip->name);
      }
   }

CfOut(cf_verbose,"","\n");
}

/**************************************************************/
/* Thread context                                             */
/**************************************************************/

void ParallelFindAndVerifyFilesPromises(struct Promise *pp)
    
{ pid_t child = 1;
  int background = GetBooleanConstraint("background",pp);

#ifdef MINGW

if (background)
   {
   CfOut(cf_verbose, "", "Background processing of files promises is not supported on Windows");
   }
  
FindAndVerifyFilesPromises(pp);

#else  /* NOT MINGW */

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
 
if (child == 0 || !background)
   {
   FindAndVerifyFilesPromises(pp);
   }
   
#endif  /* NOT MINGW */
}

