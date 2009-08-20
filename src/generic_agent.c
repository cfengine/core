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
/* File: generic_agent.c                                                     */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

extern FILE *yyin;
extern char *CFH[][2];
extern void CheckOpts(int argc,char **argv);

/*****************************************************************************/

void GenericInitialize(int argc,char **argv,char *agents)

{ enum cfagenttype ag = Agent2Type(agents);
  char vbuff[CF_BUFSIZE];
  int ok;

InitializeGA(argc,argv);

SetReferenceTime(true);
SetStartTime(false);
SetSignals();

if (EnterpriseExpiry("3","July","21009"))
   {
   CfOut(cf_error,"","Cfengine - autonomous configuration engine. This enterprise trial has expired.\n");
   exit(1);
   }
   
if (!NOHARDCLASSES)
   {
   NewScope("const");
   NewScope("match");
   NewScope("mon");
   SetNewScope("sys");
   GetNameInfo3();
   
   if (ag != cf_know)
      {
      GetInterfaceInfo3();
      FindV6InterfaceInfo();
      Get3Environment();
      OSClasses();
      }
   }

LoadPersistentContext();
LoadSystemConstants();
   
strcpy(THIS_AGENT,CF_AGENTTYPES[ag]); 
NewClass(CanonifyName(THIS_AGENT));
THIS_AGENT_TYPE = ag;

snprintf(vbuff,CF_BUFSIZE,"control_%s",THIS_AGENT);

SetNewScope(vbuff);
NewScope("this");
NewScope("match");

if (BOOTSTRAP)
   {
   SetPolicyServer(POLICY_SERVER);
   CheckAutoBootstrap();
   }

ok = BOOTSTRAP || CheckPromises(ag);

if (ok)
   {
   ReadPromises(ag,agents);
   }
else
   {
   CfOut(cf_error,"","cf-agent was not able to get confirmation of promises from cf-promises, so going to failsafe\n");
   snprintf(VINPUTFILE,CF_BUFSIZE-1,"failsafe.cf");
   ReadPromises(ag,agents);
   }

if (SHOWREPORTS || ERRORCOUNT)
   {
   CompilationReport(VINPUTFILE);
   }

XML = 0;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int CheckPromises(enum cfagenttype ag)

{ char cmd[CF_BUFSIZE],path[CF_BUFSIZE];
  struct stat sb;
 
if ((ag != cf_agent) && (ag != cf_executor))
   {
   return true;
   }

snprintf(cmd,CF_BUFSIZE-1,"%s%cbin%ccf-promises",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR);

if (stat(cmd,&sb) == -1)
   {
   CfOut(cf_error,"","cf-promises needs to be installed in %s/bin for pre-validation of full configuration",CFWORKDIR);
   return false;
   }

/* If we are cf-agent, check syntax before attempting to run */

if ((*VINPUTFILE == '.') || IsAbsoluteFileName(VINPUTFILE))
   {
   snprintf(cmd,CF_BUFSIZE-1,"%s%cbin%ccf-promises -f %s",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR,VINPUTFILE);
   }
else
   {
   snprintf(cmd,CF_BUFSIZE-1,"%s%cbin%ccf-promises -f %s%cinputs%c%s",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR,CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR,VINPUTFILE);
   }

/* Check if reloading policy will succeed */
 
if (ShellCommandReturnsZero(cmd,true))
   {
   return true;
   }
else
   {
   return false;
   }
}

/*****************************************************************************/

void ReadPromises(enum cfagenttype ag,char *agents)

{ char *v,rettype;
  void *retval;
  char vbuff[CF_BUFSIZE];
  struct Constraint *cp;
 
if (ag == cf_keygen)
   {
   return;
   }

/* Parse the files*/

Cf3ParseFiles();

/* Now import some web variables that are set in cf-know/control for the report options */

strncpy(STYLESHEET,"/cf_enterprise.css",CF_BUFSIZE-1);

for (cp = ControlBodyConstraints(cf_know); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_query_engine].lval) == 0)
      {
      strncpy(WEBDRIVER,cp->rval,CF_MAXVARSIZE);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_htmlbanner].lval) == 0)
      {
      strncpy(BANNER,cp->rval,2*CF_BUFSIZE-1);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_htmlfooter].lval) == 0)
      {
      strncpy(FOOTER,cp->rval,CF_BUFSIZE-1);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_stylesheet].lval) == 0)
      {
      strncpy(STYLESHEET,cp->rval,CF_MAXVARSIZE);
      continue;
      }

   }

/* Make the compilation reports*/

OpenReports(agents);

SetAuditVersion();

if (GetVariable("control_common","version",&retval,&rettype) != cf_notype)
   {
   v = (char *)retval;
   }
else
   {
   v = "not specified";
   }


snprintf(vbuff,CF_BUFSIZE-1,"<h1>Expanded promises for %s</h1>",agents);
CfHtmlHeader(FREPORT_HTML,vbuff,STYLESHEET,WEBDRIVER,BANNER);

fprintf(FREPORT_TXT,"Expanded promise list for %s component\n\n",agents);

ShowContext();
fprintf(FREPORT_HTML,"%s",CFH[cfx_promise][cfb]);

VerifyPromises(cf_common);

fprintf(FREPORT_HTML,"%s",CFH[cfx_promise][cfe]);

if (ag != cf_common)
   {
   ShowScopedVariables();
   }

CfHtmlFooter(FREPORT_HTML,FOOTER);

CloseReports(agents);
}

/*****************************************************************************/

void Cf3OpenLog()

{
openlog(VPREFIX,LOG_PID|LOG_NOWAIT|LOG_ODELAY,FACILITY);
}

/*****************************************************************************/

void PromiseManagement(char *agent)

{ enum cfagenttype ag = Agent2Type(agent);

switch (ag)
   {
   case cf_common:
       break;
       
   case cf_agent:
       break;
       
   case cf_server:
       break;

   case cf_monitor:
       break;
   }
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

void InitializeGA(int argc,char *argv[])

{ char *sp;
  int i,j,seed,force = false;
  struct stat statbuf,sb;
  unsigned char s[16],vbuff[CF_BUFSIZE];
  char ebuff[CF_EXPANDSIZE];

#ifdef NT
if (stat("/cygdrive",&statbuf) == 0)
   {
   FILE_SEPARATOR = '/';
   strcpy(FILE_SEPARATOR_STR,"/");
   }
else
   {
   FILE_SEPARATOR = '\\';
   strcpy(FILE_SEPARATOR_STR,"\\");
   }
#else
FILE_SEPARATOR = '/';
strcpy(FILE_SEPARATOR_STR,"/");
#endif

NewClass("any");

strcpy(VPREFIX,GetConsolePrefix());

if (VERBOSE)
   {
   NewClass("verbose_mode");
   }

if (INFORM)
   {
   NewClass("inform_mode");
   }

if (DEBUG)
   {
   NewClass("debug_mode");
   }

CfOut(cf_verbose,"","Cfengine - autonomous configuration engine - commence self-diagnostic prelude\n");  
CfOut(cf_verbose,"","------------------------------------------------------------------------\n");

/* Define trusted directories */

#ifndef NT
if (getuid() > 0)
   {
   strncpy(CFWORKDIR,GetHome(getuid()),CF_BUFSIZE-10);
   strcat(CFWORKDIR,"/.cfagent");
   
   if (strlen(CFWORKDIR) > CF_BUFSIZE/2)
      {
      FatalError("Suspicious looking home directory. The path is too long and will lead to problems.");
      }
   }
else
   {
   strcpy(CFWORKDIR,WORKDIR);
   }

#else
strcpy(CFWORKDIR,MapName(WORKDIR));
#endif

if (!LOOKUP) /* cf-know should not do this in lookup mode */
   {
   CfOut(cf_verbose,"","Work directory is %s\n",CFWORKDIR);

   snprintf(HASHDB,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_CHKDB);

   snprintf(vbuff,CF_BUFSIZE,"%s/inputs/update.conf",CFWORKDIR);
   MakeParentDirectory(vbuff,force);
   snprintf(vbuff,CF_BUFSIZE,"%s/bin/cf-agent -D from_cfexecd",CFWORKDIR);
   MakeParentDirectory(vbuff,force);
   snprintf(vbuff,CF_BUFSIZE,"%s/outputs/spooled_reports",CFWORKDIR);
   MakeParentDirectory(vbuff,force);
   snprintf(vbuff,CF_BUFSIZE,"%s/lastseen/intermittencies",CFWORKDIR);
   MakeParentDirectory(vbuff,force);
   snprintf(vbuff,CF_BUFSIZE,"%s/reports/various",CFWORKDIR);
   MakeParentDirectory(vbuff,force);
   
   snprintf(vbuff,CF_BUFSIZE,"%s/inputs",CFWORKDIR);

   if (stat(vbuff,&sb) == -1)
      {
      FatalError(" !!! No access to workspace");
      }
   else
      {
      chmod(vbuff,sb.st_mode | 0700);
      }
   
   snprintf(vbuff,CF_BUFSIZE,"%s/outputs",CFWORKDIR);

   if (stat(vbuff,&sb) == -1)
      {
      FatalError(" !!! No access to workspace");
      }
   else
      {
      chmod(vbuff,sb.st_mode | 0700);
      }
   
   sprintf(ebuff,"%s/state/cf_procs",CFWORKDIR);
   MakeParentDirectory(ebuff,force);
   
   if (stat(ebuff,&statbuf) == -1)
      {
      CreateEmptyFile(ebuff);
      }
   
   sprintf(ebuff,"%s/state/cf_rootprocs",CFWORKDIR);
   
   if (stat(ebuff,&statbuf) == -1)
      {
      CreateEmptyFile(ebuff);
      }
   
   sprintf(ebuff,"%s/state/cf_otherprocs",CFWORKDIR);
   
   if (stat(ebuff,&statbuf) == -1)
      {
      CreateEmptyFile(ebuff);
      }
   }

/* Init crypto stuff */

OpenSSL_add_all_algorithms();
OpenSSL_add_all_digests();
ERR_load_crypto_strings();
CheckWorkingDirectories();
RandomSeed();

RAND_bytes(s,16);
s[15] = '\0';
seed = ElfHash(s);
srand48((long)seed);  

LoadSecretKeys();

/* CheckOpts(argc,argv); - MacOS can't handle this back reference */

if (!MINUSF)
   {
   snprintf(VINPUTFILE,CF_BUFSIZE-1,"promises.cf");
   }

AUDITDBP = NULL;

DetermineCfenginePort();

VIFELAPSED = 1;
VEXPIREAFTER = 1;

setlinebuf(stdout);

if (BOOTSTRAP)
   {
   snprintf(vbuff,CF_BUFSIZE,"%s/inputs/failsafe.cf",CFWORKDIR);
   
   if (!IsEnterprise() && stat(vbuff,&statbuf) == -1)
      {
      CfOut(cf_inform,"","Didn't find established file %s, so looking for one in current directory\n",vbuff);
      strncpy(VINPUTFILE,"./failsafe.cf",CF_BUFSIZE-1);
      }
   else
      {
      CfOut(cf_inform,"","Found an established failsafe file %s, so using it.\n",vbuff);
      strncpy(VINPUTFILE,vbuff,CF_BUFSIZE-1);
      }   
   }

}

/*******************************************************************/

void Cf3ParseFiles()

{ struct Rlist *rp,*sl;

PARSING = true;

if ((PROMISETIME = time((time_t *)NULL)) == -1)
   {
   printf("Couldn't read system clock\n");
   }

Cf3ParseFile(VINPUTFILE);

// Expand any lists in this list now

HashVariables();

if (VINPUTLIST != NULL)
   {
   for (rp = VINPUTLIST; rp != NULL; rp=rp->next)
      {
      if (rp->type != CF_SCALAR)
         {
         CfOut(cf_error,"","Non file object %s in list\n",(char *)rp->item);
         }
      else
         {
         struct Rval returnval = EvaluateFinalRval("sys",rp->item,rp->type,true,NULL);

         switch (returnval.rtype)
            {
            case CF_SCALAR:
                Cf3ParseFile((char *)returnval.item);
                break;
                
            case CF_LIST:
                for (sl = (struct Rlist *)returnval.item; sl != NULL; sl=sl->next)
                   {
                   Cf3ParseFile((char *)sl->item);
                   }
                break;
            }
         }
      }
   }

HashVariables();
PARSING = false;
}

/*******************************************************************/

int NewPromiseProposals()

{ struct Rlist *rp,*sl;
  struct stat sb;
  int result = false;

if (stat(InputLocation(VINPUTFILE),&sb) == -1)
   {
   CfOut(cf_error,"stat","There is no readable input file at %s",VINPUTFILE);
   return false;
   }

if (sb.st_mtime > PROMISETIME)
   {
   return true;
   }
          
if (VINPUTLIST != NULL)
   {
   for (rp = VINPUTLIST; rp != NULL; rp=rp->next)
      {
      if (rp->type != CF_SCALAR)
         {
         CfOut(cf_error,"","Non file object %s in list\n",(char *)rp->item);
         }
      else
         {
         struct Rval returnval = EvaluateFinalRval("sys",rp->item,rp->type,true,NULL);

         switch (returnval.rtype)
            {
            case CF_SCALAR:

                if (stat(InputLocation((char *)returnval.item),&sb) == -1)
                   {
                   CfOut(cf_error,"stat","There are no readable promise proposals at %s",(char *)returnval.item);
                   break;
                   }

                if (sb.st_mtime > PROMISETIME)
                   {
                   result = true;                   
                   }
                
                break;
                
            case CF_LIST:

                for (sl = (struct Rlist *)returnval.item; sl != NULL; sl=sl->next)
                   {
                   if (stat(InputLocation((char *)sl->item),&sb) == -1)
                      {
                      CfOut(cf_error,"stat","There are no readable promise proposals at %s",(char *)sl->item);
                      break;
                      }

                   if (sb.st_mtime > PROMISETIME)
                      {
                      result = true;
                      break;
                      }
                   }
                
                break;
            }
         
         DeleteRvalItem(returnval.item,returnval.rtype);

         if (result)
            {
            break;
            }
         }
      }
   }

return result;
}

/*******************************************************************/

void OpenReports(char *agents)

{ char name[CF_BUFSIZE];

if (SHOWREPORTS)
   {
   snprintf(name,CF_BUFSIZE,"%s/reports/promise_output_%s.txt",CFWORKDIR,agents);

   if ((FREPORT_TXT = fopen(name,"w")) == NULL)
      {
      CfOut(cf_error,"fopen","Cannot open output file %s",name);
      FREPORT_TXT = fopen("/dev/null","w");
      }
   
   snprintf(name,CF_BUFSIZE,"%s/reports/promise_output_%s.html",CFWORKDIR,agents);

   if ((FREPORT_HTML = fopen(name,"w")) == NULL)
      {
      CfOut(cf_error,"fopen","Cannot open output file %s",name);
      FREPORT_HTML = fopen("/dev/null","w");
      }

   snprintf(name,CF_BUFSIZE,"%s/promise_knowledge.cf",CFWORKDIR);
   
   if ((FKNOW = fopen(name,"w")) == NULL)
      {
      CfOut(cf_error,"fopen","Cannot open output file %s",name);
      FKNOW = fopen("/dev/null","w");
      }
   }
else
   {
   snprintf(name,CF_BUFSIZE,"/dev/null");
   if ((FREPORT_TXT = fopen(name,"w")) == NULL)
      {
      char vbuff[CF_BUFSIZE];
      snprintf(vbuff,CF_BUFSIZE,"Cannot open output file %s",name);
      FatalError(vbuff);
      }
   
   if ((FREPORT_HTML = fopen(name,"w")) == NULL)
      {
      char vbuff[CF_BUFSIZE];
      snprintf(vbuff,CF_BUFSIZE,"Cannot open output file %s",name);
      FatalError(vbuff);
      }

   if ((FKNOW = fopen(name,"w")) == NULL)
      {
      char vbuff[CF_BUFSIZE];
      snprintf(vbuff,CF_BUFSIZE,"Cannot open output file %s",name);
      FatalError(vbuff);
      }
   }

if (!(FKNOW && FREPORT_HTML && FREPORT_TXT))
   {
   FatalError("Unable to continue as /dev/null is unwritable");
   }

fprintf(FKNOW,"bundle knowledge CfengineSiteConfiguration\n{\n");
ShowTopicRepresentation(FKNOW);
}

/*******************************************************************/

void CloseReports(char *agents)

{ char name[CF_BUFSIZE];
 
if (SHOWREPORTS)
   {
   CfOut(cf_error,"","Wrote compilation report %s/reports/promise_output_%s.txt",CFWORKDIR,agents);
   CfOut(cf_error,"","Wrote compilation report %s/reports/promise_output_%s.html",CFWORKDIR,agents);
   CfOut(cf_error,"","Wrote knowledge map %s/promise_knowledge.cf",CFWORKDIR,agents);
   }

fprintf(FKNOW,"}\n");
fclose(FKNOW); 
fclose(FREPORT_HTML);
fclose(FREPORT_TXT);
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

void Cf3ParseFile(char *filename)

{ FILE *save_yyin = yyin;
  struct stat statbuf;
  struct Rlist *rp;
  int access = false;
  char wfilename[CF_BUFSIZE];


strncpy(wfilename,InputLocation(filename),CF_BUFSIZE);

if (stat(wfilename,&statbuf) == -1)
   {
   CfOut(cf_error,"stat","Can't stat file \"%s\" for parsing\n",wfilename);
   exit(1);
   }

if (statbuf.st_mode & (S_IWGRP | S_IWOTH))
   {
#ifndef NT
   CfOut(cf_error,"","File %s (owner %d) is writable by others (security exception)",wfilename,statbuf.st_uid);
   exit(1);
#endif
   }

Debug("+++++++++++++++++++++++++++++++++++++++++++++++\n");
CfOut(cf_verbose,"","  > Parsing file %s\n",wfilename);
Debug("+++++++++++++++++++++++++++++++++++++++++++++++\n");

PrependAuditFile(wfilename);
 
if ((yyin = fopen(wfilename,"r")) == NULL)      /* Open root file */
   {
   printf("Can't open file %s for parsing\n",wfilename);
   exit (1);
   }
 
P.line_no = 1;
P.line_pos = 1;
P.list_nesting = 0;
P.arg_nesting = 0;
P.filename = strdup(wfilename);

P.currentid = NULL;
P.currentstring = NULL;
P.currenttype = NULL;
P.currentclasses = NULL;   
P.currentRlist = NULL;
P.currentpromise = NULL;
P.promiser = NULL;

while (!feof(yyin))
   {
   yyparse();
   
   if (ferror(yyin))  /* abortable */
      {
      perror("cfengine");
      exit(1);
      }
   }

fclose (yyin);
}

/*******************************************************************/

struct Constraint *ControlBodyConstraints(enum cfagenttype agent)

{ struct Body *body;
  char scope[CF_BUFSIZE];

for (body = BODIES; body != NULL; body = body->next)
   {
   if (strcmp(body->type,CF_AGENTTYPES[agent]) == 0)
      {
      if (strcmp(body->name,"control") == 0)
         {
         Debug("%s body for type %s\n",body->name,body->type);
         return body->conlist;
         }
      }
   }

return NULL;
}

/*******************************************************************/

void SetFacility(char *retval)

{
if (strcmp(retval,"LOG_USER") == 0)
   {
   FACILITY = LOG_USER;
   }
else if (strcmp(retval,"LOG_DAEMON") == 0)
   {
   FACILITY = LOG_DAEMON;
   }
else if (strcmp(retval,"LOG_LOCAL0") == 0)
   {
   FACILITY = LOG_LOCAL0;
   }
else if (strcmp(retval,"LOG_LOCAL1") == 0)
   {
   FACILITY = LOG_LOCAL1;
   }
else if (strcmp(retval,"LOG_LOCAL2") == 0)
   {
   FACILITY = LOG_LOCAL2;
   }
else if (strcmp(retval,"LOG_LOCAL3") == 0)
   {
   FACILITY = LOG_LOCAL3;
   }
else if (strcmp(retval,"LOG_LOCAL4") == 0)
   {
   FACILITY = LOG_LOCAL4;
   }
else if (strcmp(retval,"LOG_LOCAL5") == 0)
   {
   FACILITY = LOG_LOCAL5;
   }
else if (strcmp(retval,"LOG_LOCAL6") == 0)
   {
   FACILITY = LOG_LOCAL6;
   }   
else if (strcmp(retval,"LOG_LOCAL7") == 0)
   {
   FACILITY = LOG_LOCAL7;
   }

closelog();
Cf3OpenLog();
}

/**************************************************************/

struct Bundle *GetBundle(char *name,char *agent)

{ struct Bundle *bp;
 
for (bp = BUNDLES; bp != NULL; bp = bp->next) /* get schedule */
   {
   if (strcmp(bp->name,name) == 0)
      {
      if (agent)
         {
         if ((strcmp(bp->type,agent) == 0) || (strcmp(bp->type,"common") == 0))
            {
            return bp;
            }
         else
            {
            CfOut(cf_verbose,"","The bundle called %s is not of type %s\n",name,agent);
            }
         }
      else
         {
         return bp;
         }
      }
   }

return NULL;
}

/**************************************************************/

struct SubType *GetSubTypeForBundle(char *type,struct Bundle *bp)

{ struct SubType *sp;

if (bp == NULL)
   {
   return NULL;
   }
 
for (sp = bp->subtypes; sp != NULL; sp=sp->next)
   {
   if (strcmp(type,sp->name)== 0)
      {
      return sp;
      }
   }

return NULL;
}

/**************************************************************/

void BannerBundle(struct Bundle *bp,struct Rlist *params)

{
CfOut(cf_verbose,"","\n");
CfOut(cf_verbose,"","*****************************************************************\n");

if (VERBOSE || DEBUG)
   {
   printf("%s BUNDLE %s",VPREFIX,bp->name);
   }

if (params && (VERBOSE||DEBUG))
   {
   printf("(");
   ShowRlist(stdout,params);
   printf(" )\n");
   }
else
   {
   if (VERBOSE||DEBUG) printf("\n");
   }

CfOut(cf_verbose,"","*****************************************************************\n");
CfOut(cf_verbose,"","\n");
}

/**************************************************************/

void BannerSubBundle(struct Bundle *bp,struct Rlist *params)

{
CfOut(cf_verbose,"","\n");
CfOut(cf_verbose,"","      * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");

if (VERBOSE || DEBUG)
   {
   printf("%s       BUNDLE %s",VPREFIX,bp->name);
   }

if (params && (VERBOSE||DEBUG))
   {
   printf("(");
   ShowRlist(stdout,params);
   printf(" )\n");
   }
else
   {
   if (VERBOSE||DEBUG) printf("\n");
   }
CfOut(cf_verbose,"","      * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");
CfOut(cf_verbose,"","\n");
}

/**************************************************************/

void PromiseBanner(struct Promise *pp)

{ char *sp,handle[CF_MAXVARSIZE];

if ((sp = GetConstraint("handle",pp->conlist,CF_SCALAR)) || (sp = PromiseID(pp)))
   {
   strncpy(handle,sp,CF_MAXVARSIZE-1);
   }
else
   {
   strcpy(handle,"(enterprise only)");
   }
 
CfOut(cf_verbose,"","\n");
CfOut(cf_verbose,"","    .........................................................\n");

if (VERBOSE||DEBUG)
   {
   printf ("%s     Promise handle: %s\n",VPREFIX,handle);
   printf ("%s     Promise made by: %s",VPREFIX,pp->promiser);
   }

if (pp->promisee)
   {
   if (VERBOSE)
      {
      printf(" -> ");
      ShowRval(stdout,pp->promisee,pp->petype);
      }
   }

if (VERBOSE)
   {
   printf("\n");
   }

if (pp->ref)
   {
   CfOut(cf_verbose,"","\n");
   CfOut(cf_verbose,"","    Comment:  %s\n",pp->ref);
   }
   
CfOut(cf_verbose,"","    .........................................................\n");
CfOut(cf_verbose,"","\n");
}


/*********************************************************************/

void CheckWorkingDirectories()

{ struct stat statbuf;
  int result;
  char *sp,vbuff[CF_BUFSIZE];
  char output[CF_BUFSIZE];

Debug("CheckWorkingDirectories()\n");

if (uname(&VSYSNAME) == -1)
   {
   perror("uname ");
   FatalError("Uname couldn't get kernel name info!!\n");
   }
 
snprintf(LOGFILE,CF_BUFSIZE,"%s/cfagent.%s.log",CFWORKDIR,VSYSNAME.nodename);
VSETUIDLOG = strdup(LOGFILE); 
 
snprintf(vbuff,CF_BUFSIZE,"%s/.",CFWORKDIR);
MakeParentDirectory(vbuff,false);

CfOut(cf_verbose,"","Making sure that locks are private...\n"); 

if (chown(CFWORKDIR,getuid(),getgid()) == -1)
   {
   CfOut(cf_error,"chown","Unable to set owner on %s to %d.%d",CFWORKDIR,getuid(),getgid());
   }
 
if (stat(CFWORKDIR,&statbuf) != -1)
   {
   /* change permissions go-w */
   chmod(CFWORKDIR,(mode_t)(statbuf.st_mode & ~022));
   }

snprintf(vbuff,CF_BUFSIZE,"%s/state/.",CFWORKDIR);
MakeParentDirectory(vbuff,false);

snprintf(CFPRIVKEYFILE,CF_BUFSIZE,"%s/ppkeys/localhost.priv",CFWORKDIR);
snprintf(CFPUBKEYFILE,CF_BUFSIZE,"%s/ppkeys/localhost.pub",CFWORKDIR);

CfOut(cf_verbose,"","Checking integrity of the state database\n");
snprintf(vbuff,CF_BUFSIZE,"%s/state",CFWORKDIR);

if (stat(vbuff,&statbuf) == -1)
   {
   snprintf(vbuff,CF_BUFSIZE,"%s/state/.",CFWORKDIR);
   MakeParentDirectory(vbuff,false);
   
   if (chown(vbuff,getuid(),getgid()) == -1)
      {
      CfOut(cf_error,"chown","Unable to set owner on %s to %d.%d",vbuff,getuid(),getgid());
      }

   chmod(vbuff,(mode_t)0755);
   }
else 
   {
   if (statbuf.st_mode & 022)
      {
      CfOut(cf_error,"","UNTRUSTED: State directory %s (mode %o) was not private!\n",CFWORKDIR,statbuf.st_mode & 0777);
      }
   }

CfOut(cf_verbose,"","Checking integrity of the module directory\n"); 

snprintf(vbuff,CF_BUFSIZE,"%s/modules",CFWORKDIR);

if (stat(vbuff,&statbuf) == -1)
   {
   snprintf(vbuff,CF_BUFSIZE,"%s/modules/.",CFWORKDIR);
   MakeParentDirectory(vbuff,false);
   
   if (chown(vbuff,getuid(),getgid()) == -1)
      {
      CfOut(cf_error,"chown","Unable to set owner on %s to %d.%d",vbuff,getuid(),getgid());
      }

   chmod(vbuff,(mode_t)0700);
   }
else 
   {
   if (statbuf.st_mode & 022)
      {
      CfOut(cf_error,"","UNTRUSTED: Module directory %s (mode %o) was not private!\n",CFWORKDIR,statbuf.st_mode & 0777);
      }
   }

CfOut(cf_verbose,"","Checking integrity of the input data for RPC\n"); 

snprintf(vbuff,CF_BUFSIZE,"%s/rpc_in",CFWORKDIR);

if (stat(vbuff,&statbuf) == -1)
   {
   snprintf(vbuff,CF_BUFSIZE,"%s/rpc_in/.",CFWORKDIR);
   MakeParentDirectory(vbuff,false);
   
   if (chown(vbuff,getuid(),getgid()) == -1)
      {
      CfOut(cf_error,"chown","Unable to set owner on %s to %d.%d",vbuff,getuid(),getgid());
      }

   chmod(vbuff,(mode_t)0700);
   }
else 
   {
   if (statbuf.st_mode & 077)
      {
      snprintf(output,CF_BUFSIZE-1,"UNTRUSTED: RPC input directory %s was not private! (%o)\n",vbuff,statbuf.st_mode & 0777);
      FatalError(output);
      }
   }

CfOut(cf_verbose,"","Checking integrity of the output data for RPC\n"); 

snprintf(vbuff,CF_BUFSIZE,"%s/rpc_out",CFWORKDIR);

if (stat(vbuff,&statbuf) == -1)
   {
   snprintf(vbuff,CF_BUFSIZE,"%s/rpc_out/.",CFWORKDIR);
   MakeParentDirectory(vbuff,false);

   if (chown(vbuff,getuid(),getgid()) == -1)
      {
      CfOut(cf_error,"chown","Unable to set owner on %s to %d.%d",vbuff,getuid(),getgid());
      }

   chmod(vbuff,(mode_t)0700);   
   }
else
   {
   if (statbuf.st_mode & 077)
      {
      
      snprintf(output,CF_BUFSIZE-1,"UNTRUSTED: RPC output directory %s was not private! (%o)\n",vbuff,statbuf.st_mode & 0777);
      FatalError(output);
      }
   }
 
CfOut(cf_verbose,"","Checking integrity of the PKI directory\n");

snprintf(vbuff,CF_BUFSIZE,"%s/ppkeys",CFWORKDIR);
    
if (stat(vbuff,&statbuf) == -1)
   {
   snprintf(vbuff,CF_BUFSIZE,"%s/ppkeys/.",CFWORKDIR);
   MakeParentDirectory(vbuff,false);

   chmod(vbuff,(mode_t)0700); /* Keys must be immutable to others */
   }
else
   {
   if (statbuf.st_mode & 077)
      {
      snprintf(output,CF_BUFSIZE-1,"UNTRUSTED: Private key directory %s/ppkeys (mode %o) was not private!\n",CFWORKDIR,statbuf.st_mode & 0777);
      FatalError(output);
      }
   }
}

/*******************************************************************/
/* Level 2                                                         */
/*******************************************************************/

char *InputLocation(char *filename)

{ static char wfilename[CF_BUFSIZE], path[CF_BUFSIZE];

if (MINUSF && (filename != VINPUTFILE) && (*VINPUTFILE == '.' || IsAbsoluteFileName(VINPUTFILE)) && !IsAbsoluteFileName(filename))
   {
   /* If -f assume included relative files are in same directory */
   strncpy(path,VINPUTFILE,CF_BUFSIZE-1);
   ChopLastNode(path);
   snprintf(wfilename,CF_BUFSIZE-1,"%s/%s",path,filename);
   }
else if ((*filename == '.') || IsAbsoluteFileName(filename))
   {
   strncpy(wfilename,filename,CF_BUFSIZE-1);
   }
else
   {
   snprintf(wfilename,CF_BUFSIZE-1,"%s/inputs/%s",CFWORKDIR,filename);
   }

return MapName(wfilename);
}

/*******************************************************************/

void CompilationReport(char *fname)

{ char filename[CF_BUFSIZE],output[CF_BUFSIZE];

snprintf(filename,CF_BUFSIZE-1,"%s.txt",fname);
printf("Summarizing promises as text to %s\n",filename);

if ((FREPORT_TXT = fopen(filename,"w")) == NULL)
   {
   snprintf(output,CF_BUFSIZE,"Could not write output log to %s",filename);
   FatalError(output);
   }

snprintf(filename,CF_BUFSIZE-1,"%s.html",fname);
printf("Summarizing promises as html to %s\n",filename);

if ((FREPORT_HTML = fopen(filename,"w")) == NULL)
   {
   snprintf(output,CF_BUFSIZE,"Could not write output log to %s",filename);
   FatalError(output);
   }

if ((FKNOW = fopen("/dev/null","w")) == NULL)
   {
   FatalError("/dev/null failed");
   }

ShowPromises(BUNDLES,BODIES);

fclose(FREPORT_HTML);
fclose(FREPORT_TXT);
fclose(FKNOW);
}

/*******************************************************************/

void VerifyPromises(enum cfagenttype agent)

{ struct Bundle *bp,*bundles;
  struct SubType *sp;
  struct Promise *pp;
  struct Body *bdp;
  struct Scope *ptr;
  struct Rlist *rp,*params;
  struct FnCall *fp;
  char buf[CF_BUFSIZE], *scope;
  char rettype,*name;

Debug("\n\nVerifyPromises()\n");

if (REQUIRE_COMMENTS == CF_UNDEFINED)
   {
   for (bdp = BODIES; bdp != NULL; bdp = bdp->next) /* get schedule */
      {
      if ((strcmp(bdp->name,"control") == 0) && (strcmp(bdp->type,"common") == 0))
         {
         REQUIRE_COMMENTS = GetBooleanConstraint("require_comments",bdp->conlist);
         break;
         }
      }
   }

for (rp = BODYPARTS; rp != NULL; rp=rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
          if (!IsBody(BODIES,(char *)rp->item))
             {
             CfOut(cf_error,"","Undeclared promise body \"%s()\" was referenced in a promise\n",(char *)rp->item);
             ERRORCOUNT++;
             }
          break;

      case CF_FNCALL:
          fp = (struct FnCall *)rp->item;

          if (!IsBody(BODIES,fp->name))
             {
             CfOut(cf_error,"","Undeclared promise body \"%s()\" was referenced in a promise\n",fp->name);
             ERRORCOUNT++;
             }
          break;
      }
   }

/* Check for undefined subbundles */

for (rp = SUBBUNDLES; rp != NULL; rp=rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
          if (!IsBundle(BUNDLES,(char *)rp->item))
             {
             CfOut(cf_error,"","Undeclared promise bundle \"%s()\" was referenced in a promise\n",(char *)rp->item);
             ERRORCOUNT++;
             }
          break;

      case CF_FNCALL:
          fp = (struct FnCall *)rp->item;

          if (!IsBundle(BUNDLES,fp->name))
             {
             CfOut(cf_error,"","Undeclared promise bundle \"%s()\" was referenced in a promise\n",fp->name);
             ERRORCOUNT++;
             }
          break;
      }
   }

/* Now look once through ALL the bundles themselves */

for (bp = BUNDLES; bp != NULL; bp = bp->next) /* get schedule */
   {
   scope = bp->name;
   THIS_BUNDLE = bp->name;

   for (sp = bp->subtypes; sp != NULL; sp = sp->next) /* get schedule */
      {
      if (strcmp(sp->name,"classes") == 0)
         {
         /* these should not be evaluated here */
         continue;
         }

      for (pp = sp->promiselist; pp != NULL; pp=pp->next)
         {
         ExpandPromise(agent,scope,pp,NULL);
         }
      }
   }


HashVariables();
HashControls();

/* Now look once through the sequences bundles themselves */

if (BadBundleSequence(agent))
   {
   FatalError("Errors in promise bundles");
   }
}

/********************************************************************/

void PrependAuditFile(char *file)

{ struct stat statbuf;;

if ((AUDITPTR = (struct Audit *)malloc(sizeof(struct Audit))) == NULL)
   {
   FatalError("Memory allocation failure in PrependAuditFile");
   }

if (stat(file,&statbuf) == -1)
   {
   /* shouldn't happen */
   return;
   }

HashFile(file,AUDITPTR->digest,cf_md5);   

AUDITPTR->next = VAUDIT;
AUDITPTR->filename = strdup(file);
AUDITPTR->date = strdup(ctime(&statbuf.st_mtime));
Chop(AUDITPTR->date);
AUDITPTR->version = NULL;
VAUDIT = AUDITPTR;
}


/*******************************************************************/
/* Level 3                                                         */
/*******************************************************************/

void CheckVariablePromises(char *scope,struct Promise *varlist)

{ struct Promise *pp;
  int allow_redefine = false;

Debug("CheckVariablePromises()\n");
  
for (pp = varlist; pp != NULL; pp=pp->next)
   {
   ConvergeVarHashPromise(scope,pp,allow_redefine);
   }
}

/*******************************************************************/

void CheckCommonClassPromises(struct Promise *classlist)

{ struct Promise *pp;

CfOut(cf_verbose,""," -> Checking common class promises...\n");

for (pp = classlist; pp != NULL; pp=pp->next)
   {
   KeepClassContextPromise(pp);
   }
}

/*******************************************************************/

void CheckBundleParameters(char *scope,struct Rlist *args)

{ struct Rlist *rp;
  struct Rval retval;
  char *lval,rettype;;
 
for (rp = args; rp != NULL; rp = rp->next)
   {
   lval = (char *)rp->item;
   
   if (GetVariable(scope,lval,(void *)&retval,&rettype) != cf_notype)
      {
      CfOut(cf_error,"","Variable and bundle parameter %s collide",lval);
      FatalError("Aborting");
      }
   }
}

/*******************************************************************/

void CheckControlPromises(char *scope,char *agent,struct Constraint *controllist)

{ struct Constraint *cp;
  struct SubTypeSyntax *sp;
  struct BodySyntax *bp = NULL;
  char *lval;
  void *rval = NULL;
  int i = 0,override = true;
  struct Rval returnval;
  char rettype;
  void *retval;

Debug("CheckControlPromises(%s)\n",agent);

for (i = 0; CF_ALL_BODIES[i].bs != NULL; i++)
   {
   bp = CF_ALL_BODIES[i].bs;

   if (strcmp(agent,CF_ALL_BODIES[i].btype) == 0)
      {
      break;
      }
   }

if (bp == NULL)
   {
   FatalError("Unknown agent");
   }

for (cp = controllist; cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }
   
   if (strcmp(cp->lval,CFG_CONTROLBODY[cfg_bundlesequence].lval) == 0)
      {
      returnval = ExpandPrivateRval(CONTEXTID,cp->rval,cp->type);
      }
   else
      {
      returnval = EvaluateFinalRval(CONTEXTID,cp->rval,cp->type,true,NULL);
      }

   if (!AddVariableHash(scope,cp->lval,returnval.item,returnval.rtype,GetControlDatatype(cp->lval,bp),cp->audit->filename,cp->lineno))
      {
      CfOut(cf_error,"","Rule from %s at/before line %d\n",cp->audit->filename,cp->lineno);
      }

   if (strcmp(cp->lval,CFG_CONTROLBODY[cfg_output_prefix].lval) == 0)
      {
      strncpy(VPREFIX,returnval.item,CF_MAXVARSIZE);
      }

   if (strcmp(cp->lval,CFG_CONTROLBODY[cfg_domain].lval) == 0)
      {
      strcpy(VDOMAIN,cp->rval);
      CfOut(cf_verbose,"","SET domain = %s\n",VDOMAIN);
      DeleteScalar("sys","domain");
      DeleteScalar("sys","fqhost");
      snprintf(VFQNAME,CF_MAXVARSIZE,"%s.%s",VUQNAME,VDOMAIN);
      NewScalar("sys","fqhost",VFQNAME,cf_str);
      NewScalar("sys","domain",VDOMAIN,cf_str);
      DeleteClass("undefined_domain");
      NewClass(CanonifyName(VDOMAIN));
      continue;
      }

   DeleteRvalItem(returnval.item,returnval.rtype);
   }
}

/*******************************************************************/

void SetAuditVersion()

{ void *rval;
  char rtype = 'x';

  /* In addition, each bundle can have its own version */
 
switch (GetVariable("control_common","cfinputs_version",&rval,&rtype))
   {
   case cf_str:
       if (rtype != CF_SCALAR)
          {
          yyerror("non-scalar version string");
          }
       AUDITPTR->version = strdup((char *)rval);
       break;

   default:
       AUDITPTR->version = strdup("no specified version");
       break;
   }
}

/*******************************************************************/

void Syntax(char *component,struct option options[],char *hints[],char *id)

{ int i;

printf("\n\n%s\n\n",component); 

printf("SYNOPSIS:\n\n   program [options]\n\nDESCRIPTION:\n\n%s\n",id);
printf("Command line options:\n\n");

for (i=0; options[i].name != NULL; i++)
   {
   if (options[i].has_arg)
      {
      printf("--%-12s, -%c value - %s\n",options[i].name,(char)options[i].val,hints[i]);
      }
   else
      {
      printf("--%-12s, -%-7c - %s\n",options[i].name,(char)options[i].val,hints[i]);
      }
   }


printf("\nBug reports: bug-cfengine@cfengine.org, ");
printf("Community help: help-cfengine@cfengine.org\n");
printf("Community info: http://www.cfengine.org, ");
printf("Support services: http://www.cfengine.com\n\n");
printf("This software is (C) 2008 Cfengine AS.\n");
}

/*******************************************************************/

void ManPage(char *component,struct option options[],char *hints[],char *id)

{ int i;

printf(".TH %s 8 \"Maintenance Commands\"\n",GetArg0(component));
printf(".SH NAME\n%s\n\n",component);

printf(".SH SYNOPSIS:\n\n %s [options]\n\n.SH DESCRIPTION:\n\n%s\n",GetArg0(component),id);

printf(".B cfengine\n"
       "is a self-healing configuration and change management based system. You can think of"
       ".B cfengine\n"
       "as a very high level language, much higher level than Perl or shell. A"
       "single statement is called a promise, and compliance can result in many hundreds of files"
       "being created, or the permissions of many hundreds of"
       "files being set. The idea of "
       ".B cfengine\n"
       "is to create a one or more sets of configuration files which will"
       "classify and describe the setup of every host in a network.\n");

printf(".SH COMMAND LINE OPTIONS:\n");

for (i=0; options[i].name != NULL; i++)
   {
   if (options[i].has_arg)
      {
      printf(".IP \"--%s, -%c\" value\n%s\n",options[i].name,(char)options[i].val,hints[i]);
      }
   else
      {
      printf(".IP \"--%s, -%c\"\n%s\n",options[i].name,(char)options[i].val,hints[i]);
      }
   }

printf(".SH AUTHOR\n"
       "Mark Burgess and Cfengine AS\n"
       ".SH INFORMATION\n");

printf("\nBug reports: bug-cfengine@cfengine.org\n");
printf(".pp\nCommunity help: help-cfengine@cfengine.org\n");
printf(".pp\nCommunity info: http://www.cfengine.org\n");
printf(".pp\nSupport services: http://www.cfengine.com\n");
printf(".pp\nThis software is (C) 2008- Cfengine AS.\n");
}

/*******************************************************************/

void Version(char *component)

{
printf("This comprises %s core community version %s - %s%s\n",component,VERSION,CF3COPYRIGHT,VYEAR);
EnterpriseVersion();
}

/********************************************************************/

void WritePID(char *filename)

{ FILE *fp;

snprintf(PIDFILE,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,filename);

if ((fp = fopen(PIDFILE,"w")) == NULL)
   {
   CfOut(cf_inform,"fopen","Could not write to PID file %s\n",filename);
   return;
   }

fprintf(fp,"%d\n",getpid());

fclose(fp);
}

/*******************************************************************/

void HashVariables()

{ struct Bundle *bp,*bundles;
  struct SubType *sp;
  struct Scope *ptr;

CfOut(cf_verbose,"","Initiate variable convergence...\n");
  
for (bp = BUNDLES; bp != NULL; bp = bp->next) /* get schedule */
   {
   SetNewScope(bp->name);
   THIS_BUNDLE = bp->name;

   for (sp = bp->subtypes; sp != NULL; sp = sp->next) /* get schedule */
      {      
      if (strcmp(sp->name,"vars") == 0)
         {
         CheckVariablePromises(bp->name,sp->promiselist);
         }

      // We must also set global classes here?
      
      if (strcmp(bp->type,"common") == 0&&  strcmp(sp->name,"classes") == 0)
         {
         CheckCommonClassPromises(sp->promiselist);
         }
      }

   CheckBundleParameters(bp->name,bp->args);
   }
}

/*******************************************************************/

void HashControls()

{ struct Body *bdp;
  char buf[CF_BUFSIZE];

/* Only control bodies need to be hashed like variables */

CfOut(cf_verbose,"","Initiate control variable convergence...\n");
  
for (bdp = BODIES; bdp != NULL; bdp = bdp->next) /* get schedule */
   {
   if (strcmp(bdp->name,"control") == 0)
      {
      snprintf(buf,CF_BUFSIZE,"%s_%s",bdp->name,bdp->type);
      SetNewScope(buf);
      CheckControlPromises(buf,bdp->type,bdp->conlist);
      }
   }
}

/*******************************************************************/

void UnHashVariables()

{ struct Bundle *bp,*bundles;

for (bp = BUNDLES; bp != NULL; bp = bp->next) /* get schedule */
   {
   DeleteScope(bp->name);
   }
}

/********************************************************************/

int BadBundleSequence(enum cfagenttype agent)

{ struct Rlist *rp,*params;
  char rettype,*name;
  void *retval;
  int ok = true;
  struct FnCall *fp;

if (THIS_AGENT_TYPE != cf_agent && THIS_AGENT_TYPE != cf_know && THIS_AGENT_TYPE != cf_common)
   {
   return false;
   }

if (CBUNDLESEQUENCE)
   {
   return false;
   }

if (GetVariable("control_common","bundlesequence",&retval,&rettype) == cf_notype)
   {
   CfOut(cf_error,""," !!! No bundlesequence in the common control body");

   if (agent == cf_common)
      {
      return false;
      }
   else
      {
      return true;
      }
   }

if (rettype != CF_LIST)
   {
   FatalError("Promised bundlesequence was not a list");
   }

if (agent == cf_agent || agent == cf_common)
   {
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
      
      if (!GetBundle(name,NULL))
         {
         CfOut(cf_error,"","Bundle %s listed in the bundlesequence is not a defined bundle\n",name);
         ok = false;
         }
      }
   
   if (!ok)
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
