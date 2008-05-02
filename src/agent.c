/* 

        Copyright (C) 1994-
        Free Software Foundation, Inc.

   This file is part of GNU cfengine - written and maintained 
   by Mark Burgess, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
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

int main (int argc,char *argv[]);
void CheckAgentAccess(struct Rlist *list);
void KeepAgentPromise(struct Promise *pp);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

  /* GNU STUFF FOR LATER #include "getopt.h" */
 
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

extern struct BodySyntax CFA_CONTROLBODY[];

/*******************************************************************/

int main(int argc,char *argv[])

{
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
  
while ((c=getopt_long(argc,argv,"d:vnIf:pD:N:VSx",OPTIONS,&optindex)) != EOF)
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

      case 'x': SelfDiagnostic();
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
signal(SIGCHLD,SIG_IGN);
signal(SIGUSR1,HandleSignals);
signal(SIGUSR2,HandleSignals);

}

/*******************************************************************/

void KeepPromises()

{
KeepControlPromises();
KeepPromiseBundles();
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
      snprintf(OUTPUT,CF_BUFSIZE,"Unknown lval %s in agent control body",cp->lval);
      CfLog(cferror,OUTPUT,"");
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
      struct Rlist *rp;
      Verbose("Checking accesss ...\n");
      
      CheckAgentAccess((struct Rlist *) retval);
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
  int ok = true,i;
  static char *typesequence[] = { "interfaces", "processes", "files", "executions", NULL };

if (GetVariable("control_common","bundlesequence",&retval,&rettype) == cf_notype)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"No bundlesequence in the common control body");
   CfLog(cferror,OUTPUT,"");
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
          snprintf(OUTPUT,CF_BUFSIZE,"Illegal item found in bundlesequence: ");
          CfLog(cferror,OUTPUT,"");
          ShowRval(stdout,rp->item,rp->type);
          printf(" = %c\n",rp->type);
          ok = false;
          break;
      }
   
   if (!GetBundle(name,"agent"))
      {
      snprintf(OUTPUT,CF_BUFSIZE,"Bundle %s listed in the bundlesequence was not found\n",name);
      CfLog(cferror,OUTPUT,"");
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
   
   if ((bp = GetBundle(name,"agent")) == NULL)
      {
      FatalError("Software error in finding bundle - shouldn't happen");
      }

   BannerBundle(bp,params);
   AugmentScope(bp->name,bp->args,params);
            
   for (i = 0;  typesequence[i] != NULL; i++)
      {
      if ((sp = GetSubTypeForBundle(typesequence[i],bp)) == NULL)
         {
         continue;      
         }

      BannerSubType(bp->name,sp->name);
      
      for (pp = sp->promiselist; pp != NULL; pp=pp->next)
         {
         /* Dial up the generic promise expansion with a callback */  

         ExpandPromise(cf_agent,bp->name,pp,KeepAgentPromise);
         }         
      }

   DeleteFromScope(bp->name,bp->args);
   }
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

void CheckAgentAccess(struct Rlist *list)

{ char id[CF_MAXVARSIZE];
  struct passwd *pw;
  struct Rlist *rp;
  uid_t uid;
  
uid = getuid();
  
for (rp  = list; rp != NULL; rp = rp->next)
   {
   if (isalpha((int)*(char *)(rp->item)))
      {
      if ((pw = getpwnam(rp->item)) == NULL)
         {
         Verbose("Unknown user on this system %s\n",rp->item);
         return;
         }
      
      if (pw->pw_uid == uid)
         {
         return;
         }
      }
   else
      {
      if (atoi(rp->item) == uid)
         {
         return;
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
   Verbose("Skipping whole promise, as context %s is not valid\n",pp->classes);
   return;
   }

if (strcmp("files",pp->agentsubtype) == 0)
   {
   VerifyFilesPromise(pp);
   return;
   }

}
