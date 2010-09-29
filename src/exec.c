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
/* File: exec.c                                                              */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int main (int argc,char *argv[]);

#ifdef NT
#include <process.h>
#endif

/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

int  NO_FORK = false;
int  ONCE = false;
char MAILTO[CF_BUFSIZE];
char MAILFROM[CF_BUFSIZE];
char EXECCOMMAND[CF_BUFSIZE];
char VMAILSERVER[CF_BUFSIZE];
struct Item *SCHEDULE = NULL;

pid_t MYTWIN = 0;
int MAXLINES = 30;
int SPLAYTIME = 0;
const int INF_LINES = -2;
int NOSPLAY = false;
int NOWINSERVICE = false;
int THREADS = 0;

extern struct BodySyntax CFEX_CONTROLBODY[];

/*******************************************************************/

void StartServer(int argc,char **argv);
int ScheduleRun(void);
static char *timestamp(time_t stamp, char *buf, size_t len);
void *LocalExec(void *scheduled_run);
int FileChecksum(char *filename,unsigned char digest[EVP_MAX_MD_SIZE+1],char type);
int CompareResult(char *filename,char *prev_file);
void MailResult(char *file,char *to);
int Dialogue(int sd,char *s);
void Apoptosis(void);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

 char *ID = "The executor daemon is a scheduler and wrapper for\n"
            "execution of cf-agent. It collects the output of the\n"
            "agent and can email it to a specified address. It can\n"
            "splay the start time of executions across the network\n"
            "and work as a class-based clock for scheduling.";
 
 struct option OPTIONS[15] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "dry-run",no_argument,0,'n'},
      { "version",no_argument,0,'V' },
      { "file",required_argument,0,'f'},
      { "define",required_argument,0,'D' },
      { "negate",required_argument,0,'N' },
      { "no-lock",no_argument,0,'K'},
      { "inform",no_argument,0,'I'},
      { "diagnostic",no_argument,0,'x'},
      { "no-fork",no_argument,0,'F' },
      { "no-winsrv",no_argument,0,'W' },
      { "ld-library-path",required_argument,0,'L'},
      { NULL,0,0,'\0' }
      };

 char *HINTS[15] =
      {
      "Print the help message",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "All talk and no action mode - make no changes, only inform of promises not kept",
      "Output the version of the software",
      "Specify an alternative input file than the default",
      "Define a list of comma separated classes to be defined at the start of execution",
      "Define a list of comma separated classes to be undefined at the start of execution",
      "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
      "Print basic information about changes made to the system, i.e. promises repaired",
      "Activate internal diagnostics (developers only)",
      "Run as a foreground processes (do not fork)",
      "Do not run as a service on windows - use this when running from a command shell (Cfengine Nova only)",
      "Set the internal value of LD_LIBRARY_PATH for child processes",
      NULL
      };

/*****************************************************************************/

int main(int argc,char *argv[])

{
CheckOpts(argc,argv);
GenericInitialize(argc,argv,"executor");
ThisAgentInit();
KeepPromises();

#ifdef MINGW
if(NOWINSERVICE)
  {
  StartServer(argc,argv);
  }
else
  {
  NovaWin_StartExecService();
  }
#else  /* NOT MINGW */

StartServer(argc,argv);

#endif  /* NOT MINGW */

return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  char arg[CF_BUFSIZE];
  struct Item *actionList;
  int optindex = 0;
  int c;
  char ld_library_path[CF_BUFSIZE];

while ((c=getopt_long(argc,argv,"d:vnKIf:D:N:VxL:hFV1gMW",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          if (optarg && strlen(optarg) < 5)
             {
             snprintf(arg,CF_MAXVARSIZE," -f used but argument \"%s\" incorrect",optarg);
             FatalError(arg);
             }

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          VINPUTFILE[CF_BUFSIZE-1] = '\0';
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
          
      case 'K': IGNORELOCK = true;
          break;
                    
      case 'D': NewClassesFromString(optarg);
          break;
          
      case 'N':
          NegateClassesFromString(optarg,&VNEGHEAP);
          break;
          
      case 'I': INFORM = true;
          break;
          
      case 'v':
          VERBOSE = true;
          NO_FORK = true;
          break;
	  
          
      case 'n': DONTDO = true;
          IGNORELOCK = true;
          NewClass("opt_dry_run");
          break;
          
      case 'q': NOSPLAY = true;
          break;
          
      case 'L': 
          snprintf(ld_library_path,CF_BUFSIZE-1,"LD_LIBRARY_PATH=%s",optarg);
          if (putenv(strdup(ld_library_path)) != 0)
             {
             }
          break;

      case 'W':
    	  NOWINSERVICE = true;
          break;
		  
      case 'F':
          ONCE = true;
          NO_FORK = true;
          break;

      case 'V': Version("cf-execd");
          exit(0);
          
      case 'h': Syntax("cf-execd - cfengine's execution agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'M': ManPage("cf-execd - cfengine's execution agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'x': SelfDiagnostic();
          exit(0);
          
      default: Syntax("cf-execd - cfengine's execution agent",OPTIONS,HINTS,ID);
          exit(1);
          
      }
   }

if (argv[optind] != NULL)
   {
   CfOut(cf_error,"","Unexpected argument with no preceding option: %s\n",argv[optind]);
   }
}

/*****************************************************************************/

void ThisAgentInit()

{ char vbuff[CF_BUFSIZE];

umask(077);
LOGGING = true;
MAILTO[0] = '\0';
MAILFROM[0] = '\0';
VMAILSERVER[0] = '\0';
EXECCOMMAND[0] = '\0';

if (SCHEDULE == NULL)
   {
   AppendItem(&SCHEDULE,"Min00",NULL);
   AppendItem(&SCHEDULE,"Min05",NULL);
   AppendItem(&SCHEDULE,"Min10",NULL);
   AppendItem(&SCHEDULE,"Min15",NULL);
   AppendItem(&SCHEDULE,"Min20",NULL);
   AppendItem(&SCHEDULE,"Min25",NULL);   
   AppendItem(&SCHEDULE,"Min30",NULL);
   AppendItem(&SCHEDULE,"Min35",NULL);
   AppendItem(&SCHEDULE,"Min40",NULL);
   AppendItem(&SCHEDULE,"Min45",NULL);
   AppendItem(&SCHEDULE,"Min50",NULL);
   AppendItem(&SCHEDULE,"Min55",NULL);
   }
}

/*****************************************************************************/

void KeepPromises()

{ struct Constraint *cp;
  char rettype,splay[CF_BUFSIZE];
  void *retval;

for (cp = ControlBodyConstraints(cf_executor); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }
   
   if (GetVariable("control_executor",cp->lval,&retval,&rettype) == cf_notype)
      {
      CfOut(cf_error,"","Unknown lval %s in exec control body",cp->lval);
      continue;
      }
   
   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_mailfrom].lval) == 0)
      {
      strcpy(MAILFROM,retval);
      Debug("mailfrom = %s\n",MAILFROM);
      }
   
   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_mailto].lval) == 0)
      {
      strcpy(MAILTO,retval);
      Debug("mailto = %s\n",MAILTO);
      }
   
   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_smtpserver].lval) == 0)
      {
      strcpy(VMAILSERVER,retval);
      Debug("smtpserver = %s\n",VMAILSERVER);
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_execcommand].lval) == 0)
      {
      strcpy(EXECCOMMAND,retval);
      Debug("exec_command = %s\n",EXECCOMMAND);
      }
   
   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_executorfacility].lval) == 0)
      {
      SetFacility(retval);
      CfOut(cf_verbose,"","SET Syslog FACILITY = %s\n",retval);
      continue;
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_mailmaxlines].lval) == 0)
      {
      MAXLINES = Str2Int(retval);
      Debug("maxlines = %d\n",MAXLINES);
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_splaytime].lval) == 0)
      {
      int hash,time = Str2Int(retval);
      snprintf(splay,CF_BUFSIZE,"%s+%s+%d",VFQNAME,VIPADDRESS,getuid());
      hash = Hash(splay);
      SPLAYTIME = (int)(time*60*hash/CF_HASHTABLESIZE);
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_schedule].lval) == 0)
      {
      struct Rlist *rp;
      Debug("schedule ...\n");
      DeleteItemList(SCHEDULE);
      SCHEDULE = NULL;
      
      for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
         {
         if (!IsItemIn(SCHEDULE,rp->item))
            {
            AppendItem(&SCHEDULE,rp->item,NULL);
            }
         }
      }
   }
}

/*****************************************************************************/

void StartServer(int argc,char **argv)

{ int pid,time_to_run = false;
  time_t now = time(NULL);
  struct Promise *pp = NewPromise("exec_cfengine","the executor agent"); 
  struct Attributes dummyattr;
  struct CfLock thislock;

Banner("Starting executor");
memset(&dummyattr,0,sizeof(dummyattr));

dummyattr.restart_class = "nonce";
dummyattr.transaction.ifelapsed = CF_EXEC_IFELAPSED;
dummyattr.transaction.expireafter = CF_EXEC_EXPIREAFTER;

if (!ONCE)
   {
   thislock = AcquireLock(pp->promiser,VUQNAME,CFSTARTTIME,dummyattr,pp);

   if (thislock.lock == NULL)
      {
      DeletePromise(pp);
      return;
      }
   }

Apoptosis();

#ifdef MINGW

if (!NO_FORK)
  {
  CfOut(cf_verbose, "", "Windows does not support starting processes in the background - starting in foreground");
  }

#else  /* NOT MINGW */

if ((!NO_FORK) && (fork() != 0))
   {
   CfOut(cf_inform,"","cf-execd starting %.24s\n",cf_ctime(&now));
   exit(0);
   }

if (!NO_FORK)
   {
   ActAsDaemon(0);
   }
   
#endif  /* NOT MINGW */

WritePID("cf-execd.pid");
signal(SIGINT,HandleSignals);
signal(SIGTERM,HandleSignals);
signal(SIGHUP,SIG_IGN);
signal(SIGPIPE,SIG_IGN);
signal(SIGUSR1,HandleSignals);
signal(SIGUSR2,HandleSignals);
 
umask(077);

if (ONCE)
   {
   CfOut(cf_verbose,"","Sleeping for splaytime %d seconds\n\n",SPLAYTIME);
   sleep(SPLAYTIME);
   LocalExec((void *)0);
   }
else
   { char **nargv;
     int i;
#ifdef HAVE_PTHREAD_H
     pthread_t tid;
#endif

#if defined NT && !(defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD) 
   /*
    * Append --once option to our arguments for spawned monitor process.
    */

   nargv = malloc(sizeof(char *) * (argc+2));
     
   for (i = 0; i < argc; i++)
      {
      nargv[i] = argv[i];
      }
   
   nargv[i++] = strdup("-FK");
   nargv[i++] = NULL;
#endif
   
   while (true)
      {
      time_to_run = ScheduleRun();

      if (time_to_run)
         {
         CfOut(cf_verbose,"","Sleeping for splaytime %d seconds\n\n",SPLAYTIME);
         sleep(SPLAYTIME);

#if defined NT && !(defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD) 
         /*
          * Spawn a separate process - spawn will work if the cfexecd binary
          * has changed (where cygwin's fork() would fail).
          */
         
         Debug("Spawning %s\n", nargv[0]);

         pid = _spawnvp((int)_P_NOWAIT,(char *)(nargv[0]),(char **)nargv);

         if (pid < 1)
            {
            CfOut(cf_error,"_spawnvp","Can't spawn run");
            }
#endif
         
#if (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
         
         pthread_attr_init(&PTHREADDEFAULTS);
         pthread_attr_setdetachstate(&PTHREADDEFAULTS,PTHREAD_CREATE_DETACHED);
         
#ifdef HAVE_PTHREAD_ATTR_SETSTACKSIZE
         pthread_attr_setstacksize(&PTHREADDEFAULTS,(size_t)2048*1024);
#endif

         if (pthread_create(&tid,&PTHREADDEFAULTS,LocalExec,(void *)1) != 0)
            {
            CfOut(cf_inform,"pthread_create","Can't create thread!");
            LocalExec((void *)1);
            }

         ThreadLock(cft_system);
         pthread_attr_destroy(&PTHREADDEFAULTS);
         ThreadUnlock(cft_system);
#else
         LocalExec((void *)1);  
#endif
         }
      }
   }

if (!ONCE)
   {
   YieldCurrentLock(thislock);
   }
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

void Apoptosis()

{ struct Promise pp;
  struct Rlist *signals = NULL, *owners = NULL;
  char mypid[32],pidrange[32];
  char *psopts = GetProcessOptions();
  static char promiserBuf[CF_SMALLBUF];

if (ONCE || VSYSTEMHARDCLASS == cfnt)
   {
   /* Otherwise we'll just kill off long jobs */
   return;
   }
  
CfOut(cf_verbose,""," !! Programmed pruning of the scheduler cluster");

#ifdef MINGW
snprintf(promiserBuf, sizeof(promiserBuf), "cf-execd");	  // using '\' causes regexp problems
#else
snprintf(promiserBuf, sizeof(promiserBuf), "%s/bin/cf-execd", CFWORKDIR);
#endif

pp.promiser = promiserBuf;
pp.promisee = "cfengine";
pp.classes = "any";
pp.petype = CF_SCALAR;
pp.lineno = 0;
pp.audit = NULL;
pp.conlist = NULL;

pp.bundletype = "agent";
pp.bundle = "exec_apoptosis";
pp.ref = "Programmed death";
pp.agentsubtype = "processes";
pp.done = false;
pp.next = NULL;
pp.cache = NULL;
pp.inode_cache = NULL;
pp.this_server = NULL;
pp.donep = &(pp.done);
pp.conn = NULL;

GetCurrentUserName(mypid,31);

PrependRlist(&signals,"term",CF_SCALAR);
PrependRlist(&owners,mypid,CF_SCALAR);

AppendConstraint(&(pp.conlist),"signals",signals,CF_LIST,"any",false);
AppendConstraint(&(pp.conlist),"process_select",strdup("true"),CF_SCALAR,"any",false);
AppendConstraint(&(pp.conlist),"process_owner",owners,CF_LIST,"any",false);
AppendConstraint(&(pp.conlist),"ifelapsed",strdup("0"),CF_SCALAR,"any",false);
AppendConstraint(&(pp.conlist),"process_count",strdup("true"),CF_SCALAR,"any",false);
AppendConstraint(&(pp.conlist),"match_range",strdup("0,2"),CF_SCALAR,"any",false);
AppendConstraint(&(pp.conlist),"process_result",strdup("process_owner.process_count"),CF_SCALAR,"any",false);

CfOut(cf_verbose,""," -> Looking for cf-execd processes owned by %s",mypid);

if (LoadProcessTable(&PROCESSTABLE,psopts))
   {
   VerifyProcessesPromise(&pp);   
   }

DeleteItemList(PROCESSTABLE);

if (pp.conlist)
   {
   DeleteConstraintList(pp.conlist);
   }

CfOut(cf_verbose,""," !! Pruning complete");
}

/*****************************************************************************/

int ScheduleRun()

{ time_t now;
  char timekey[64];
  struct Item *ip;

CfOut(cf_verbose,"","Sleeping...\n");
sleep(CFPULSETIME);                /* 1 Minute resolution is enough */ 
now = time(NULL);

// recheck license (in case of license updates or expiry)

if (EnterpriseExpiry(LIC_DAY,LIC_MONTH,LIC_YEAR,LIC_COMPANY)) 
  {
  CfOut(cf_error,"","Cfengine - autonomous configuration engine. This enterprise license is invalid.\n");
  exit(1);
  }

ThreadLock(cft_system);
DeleteItemList(VHEAP);
VHEAP = NULL;
DeleteItemList(VADDCLASSES);
VADDCLASSES = NULL;
DeleteItemList(IPADDRESSES);
IPADDRESSES = NULL;
DeleteScope("this");
DeleteScope("mon");
DeleteScope("sys");
CfGetInterfaceInfo(cf_executor);
Get3Environment();
OSClasses();
SetReferenceTime(true);
snprintf(timekey,63,"%s",cf_ctime(&now)); 
AddTimeClass(timekey); 
ThreadUnlock(cft_system);

for (ip = SCHEDULE; ip != NULL; ip = ip->next)
   {
   CfOut(cf_verbose,"","Checking schedule %s...\n",ip->name);

   if (IsDefinedClass(ip->name))
      {
      CfOut(cf_verbose,"","Waking up the agent at %s ~ %s \n",timekey,ip->name);
      return true;
      }
   }

return false;
}

/*************************************************************************/

static char *timestamp(time_t stamp, char *buf, size_t len)

{ struct tm *ltime;
 
ltime = localtime(&stamp);
snprintf(buf, len, "%04d-%02d-%02d--%02d-%02d-%02d",
         ltime->tm_year+1900,
         ltime->tm_mon+1,
         ltime->tm_mday,
         ltime->tm_hour,
         ltime->tm_min,
         ltime->tm_sec);
return buf;
}

/**************************************************************/

void *LocalExec(void *scheduled_run)

{ FILE *pp; 
  char line[CF_BUFSIZE],filename[CF_BUFSIZE],*sp;
  char cmd[CF_BUFSIZE],esc_command[CF_BUFSIZE];
  int print,count = 0;
  void *threadName;
  time_t starttime = time(NULL);
  FILE *fp;
#ifdef HAVE_PTHREAD_SIGMASK
  sigset_t sigmask;

sigemptyset(&sigmask);
pthread_sigmask(SIG_BLOCK,&sigmask,NULL); 
#endif

#ifdef HAVE_PTHREAD
threadName = ThreadUniqueName(pthread_self());
#else
threadName = NULL;
#endif
 
CfOut(cf_verbose,"","------------------------------------------------------------------\n\n");
CfOut(cf_verbose,"","  LocalExec(%sscheduled) at %s\n", scheduled_run ? "" : "not ", cf_ctime(&starttime));
CfOut(cf_verbose,"","------------------------------------------------------------------\n"); 

/* Need to make sure we have LD_LIBRARY_PATH here or children will die  */

if (strlen(EXECCOMMAND) > 0)
   {
   strncpy(cmd,EXECCOMMAND,CF_BUFSIZE-1);

   if (!strstr(EXECCOMMAND,"-Dfrom_cfexecd"))
      {
      strcat(EXECCOMMAND," -Dfrom_cfexecd");
      }
   }
else
   {
   struct stat sb;
   int twin_exists = false;
      
   // twin is bin-twin\cf-agent.exe on windows, bin/cf-twin on Unix

   if (VSYSTEMHARDCLASS == mingw || VSYSTEMHARDCLASS == cfnt)
      {
      snprintf(cmd,CF_BUFSIZE-1,"%s/bin-twin/cf-agent.exe",CFWORKDIR);
      MapName(cmd);

      if (stat(cmd,&sb) == 0)
         {
         twin_exists = true;
         }
      
      if (twin_exists && IsExecutable(cmd))
	 {
         snprintf(cmd,CF_BUFSIZE-1,"\"%s/bin-twin/cf-agent.exe\" -f failsafe.cf && \"%s/bin/cf-agent.exe%s\" -Dfrom_cfexecd%s",
                  CFWORKDIR,
                  CFWORKDIR,
                  NOSPLAY ? " -q" : "",
                  scheduled_run ? ":scheduled_run" : "");      
	 }
      else
	 {
         snprintf(cmd,CF_BUFSIZE-1,"\"%s/bin/cf-agent.exe\" -f failsafe.cf && \"%s/bin/cf-agent.exe%s\" -Dfrom_cfexecd%s",
                  CFWORKDIR,
                  CFWORKDIR,
                  NOSPLAY ? " -q" : "",
                  scheduled_run ? ":scheduled_run" : "");      
	 }
      }
   else
      {
      snprintf(cmd,CF_BUFSIZE-1,"%s/bin/cf-twin",CFWORKDIR);

      if (stat(cmd,&sb) == 0)
         {
         twin_exists = true;
         }
      
      if (twin_exists && IsExecutable(cmd))
	 {
         snprintf(cmd,CF_BUFSIZE-1,"\"%s/bin/cf-twin\" -f failsafe.cf && \"%s/bin/cf-agent%s\" -Dfrom_cfexecd%s",
                  CFWORKDIR,
                  CFWORKDIR,
                  NOSPLAY ? " -q" : "",
                  scheduled_run ? ":scheduled_run" : "");      
	 }
      else
	 {
         snprintf(cmd,CF_BUFSIZE-1,"\"%s/bin/cf-agent\" -f failsafe.cf && \"%s/bin/cf-agent%s\" -Dfrom_cfexecd%s",
                  CFWORKDIR,
                  CFWORKDIR,
                  NOSPLAY ? " -q" : "",
                  scheduled_run ? ":scheduled_run" : "");      
	 }
      }   
   }

strncpy(esc_command,MapName(cmd),CF_BUFSIZE-1);
   
snprintf(line,CF_BUFSIZE-1,"_%d_%s",starttime,CanonifyName(cf_ctime(&starttime)));
snprintf(filename,CF_BUFSIZE-1,"%s/outputs/cf_%s_%s_%x",CFWORKDIR,CanonifyName(VFQNAME),line,threadName);
MapName(filename);

/* What if no more processes? Could sacrifice and exec() - but we need a sentinel */

if ((fp = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","!! Couldn't open \"%s\" - aborting exec\n",filename);
   return NULL;
   }

CfOut(cf_verbose,""," -> Command => %s\n",cmd);

if ((pp = cf_popen_sh(esc_command,"r")) == NULL)
   {
   CfOut(cf_error,"cf_popen","!! Couldn't open pipe to command \"%s\"\n",cmd);
   fclose(fp);
   return NULL;
   }

CfOut(cf_verbose,""," -> Command is executing...%s\n",esc_command);

while (!feof(pp) && CfReadLine(line,CF_BUFSIZE,pp))
   {
   if (ferror(pp))
      {
      fflush(pp);
      break;
      }  
   
   print = false;
      
   for (sp = line; *sp != '\0'; sp++)
      {
      if (!isspace((int)*sp))
         {
         print = true;
         break;
         }
      }
   
   if (print)
      {
      fprintf(fp,"%s\n",line);
      count++;
      
      /* If we can't send mail, log to syslog */
      
      if (strlen(MAILTO) == 0)
         {
         strncat(line,"\n",CF_BUFSIZE-1-strlen(line));
         if ((strchr(line,'\n')) == NULL)
            {
            line[CF_BUFSIZE-2] = '\n';
            }
         
         CfOut(cf_inform,"",line);
         }
      
      line[0] = '\0';
      }
   }
 
cf_pclose(pp);
Debug("Closing fp\n");
fclose(fp);

if (ONCE)
   {
   Cf3CloseLog();
   }

CfOut(cf_verbose,""," -> Command is complete\n",cmd);

if (count)
   {
   CfOut(cf_verbose,""," -> Mailing result\n",cmd);
   MailResult(filename,MAILTO);
   }
else
   {
   CfOut(cf_verbose,""," -> No output\n",cmd);
   unlink(filename);
   }

return NULL; 
}

/******************************************************************************/
/* Level 4                                                                    */
/******************************************************************************/

int FileChecksum(char *filename,unsigned char digest[EVP_MAX_MD_SIZE+1],char type)

{ FILE *file;
  EVP_MD_CTX context;
  int len, md_len;
  unsigned char buffer[1024];
  const EVP_MD *md = NULL;

Debug2("FileChecksum(%c,%s)\n",type,filename);

if ((file = fopen(filename,"rb")) == NULL)
   {
   printf ("%s can't be opened\n", filename);
   }
else
   {
   switch (type)
      {
      case 's': md = EVP_get_digestbyname("sha");
           break;
      case 'm': md = EVP_get_digestbyname("md5");
         break;
      default: FatalError("Software failure in ChecksumFile");
      }

   if (!md)
      {
      return 0;
      }
   
   EVP_DigestInit(&context,md);

   while (len = fread(buffer,1,1024,file))
      {
      EVP_DigestUpdate(&context,buffer,len);
      }

   EVP_DigestFinal(&context,digest,&md_len);
   fclose (file);
   return(md_len);
   }

return 0; 
}

/*******************************************************************/

int CompareResult(char *filename,char *prev_file)

{ int i;
  char digest1[EVP_MAX_MD_SIZE+1];
  char digest2[EVP_MAX_MD_SIZE+1];
  int  md_len1, md_len2;
  FILE *fp;
  int rtn = 0;

CfOut(cf_verbose,"","Comparing files  %s with %s\n", prev_file, filename);

if ((fp=fopen(prev_file,"r")) != NULL)
   {
   fclose(fp);

   md_len1 = FileChecksum(prev_file, digest1, 'm');
   md_len2 = FileChecksum(filename,  digest2, 'm');

   if (md_len1 != md_len2)
      {
      rtn = 1;
      }
   else
      {
      for (i = 0; i < md_len1; i++)
         {
         if (digest1[i] != digest2[i])
            {
            rtn = 1;
            break;
            }
         }
      }
   }
else
   {
   /* no previous file */
   rtn = 1;
   }

if (!ThreadLock(cft_count))
   {
   exit(1);
   }

/* replace old file with new*/   

unlink(prev_file);

 if(!LinkOrCopy(filename,prev_file,true))
   {
     CfOut(cf_inform,"","Could symlink or copy %s to %s",filename,prev_file);
     rtn = 1;
   }

ThreadUnlock(cft_count);
return(rtn);
}

/***********************************************************************/

void MailResult(char *file,char *to)

{ int sd, sent, count = 0, anomaly = false;
 char domain[256], prev_file[CF_BUFSIZE],vbuff[CF_BUFSIZE];
  struct hostent *hp;
  struct sockaddr_in raddr;
  struct servent *server;
  struct stat statbuf;
  time_t now = time(NULL);
  FILE *fp;

if ((strlen(VMAILSERVER) == 0) || (strlen(to) == 0))
   {
   /* Syslog should have done this */
   return;
   }

CfOut(cf_verbose,"","Mail result...\n");

if (cfstat(file,&statbuf) == -1)
   {
   return;
   }

snprintf(prev_file,CF_BUFSIZE-1,"%s/outputs/previous",CFWORKDIR);
MapName(prev_file);

if (statbuf.st_size == 0)
   {
   unlink(file);
   Debug("Nothing to report in %s\n",file);
   return;
   }

if (CompareResult(file,prev_file) == 0) 
   {
   CfOut(cf_verbose,"","Previous output is the same as current so do not mail it\n");
   return;
   }

if (MAXLINES == 0)
   {
   Debug("Not mailing: EmailMaxLines was zero\n");
   return;
   }
 
Debug("Mailing results of (%s) to (%s)\n",file,to);
 
if (strlen(to) == 0)
   {
   return;
   }

/* Check first for anomalies - for subject header */
 
if ((fp = fopen(file,"r")) == NULL)
   {
   CfOut(cf_inform,"fopen","Couldn't open file %s",file);
   return;
   }

while (!feof(fp))
   {
   vbuff[0] = '\0';
   fgets(vbuff,CF_BUFSIZE,fp);

   if (strstr(vbuff,"entropy"))
      {
      anomaly = true;
      break;
      }
   }

fclose(fp);
 
if ((fp = fopen(file,"r")) == NULL)
   {
   CfOut(cf_inform,"fopen","Couldn't open file %s",file);
   return;
   }
 
Debug("Looking up hostname %s\n\n",VMAILSERVER);

if ((hp = gethostbyname(VMAILSERVER)) == NULL)
   {
   printf("Unknown host: %s\n", VMAILSERVER);
   printf("Make sure that fully qualified names can be looked up at your site.\n");
   fclose(fp);
   return;
   }

if ((server = getservbyname("smtp","tcp")) == NULL)
   {
   CfOut(cf_inform,"getservbyname","Unable to lookup smtp service");
   fclose(fp);
   return;
   }

memset(&raddr,0,sizeof(raddr));

raddr.sin_port = (unsigned int) server->s_port;
raddr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
raddr.sin_family = AF_INET;  

Debug("Connecting...\n");

if ((sd = socket(AF_INET,SOCK_STREAM,0)) == -1)
   {
   CfOut(cf_inform,"socket","Couldn't open a socket");
   fclose(fp);
   return;
   }
   
if (connect(sd,(void *) &raddr,sizeof(raddr)) == -1)
   {
   CfOut(cf_inform,"connect","Couldn't connect to host %s\n",VMAILSERVER);
   fclose(fp);
   cf_closesocket(sd);
   return;
   }

/* read greeting */
 
if (!Dialogue(sd,NULL))
   {
   goto mail_err;
   }
 
sprintf(vbuff,"HELO %s\r\n",VFQNAME); 
Debug("%s",vbuff);

if (!Dialogue(sd,vbuff))
   {
   goto mail_err;
   }

 if (strlen(MAILFROM) == 0)
    {
    sprintf(vbuff,"MAIL FROM: <cfengine@%s>\r\n",VFQNAME);
    Debug("%s",vbuff);
    }
 else
    {
    sprintf(vbuff,"MAIL FROM: <%s>\r\n",MAILFROM);
    Debug("%s",vbuff);    
    }

if (!Dialogue(sd,vbuff))
   {
   goto mail_err;
   }
 
sprintf(vbuff,"RCPT TO: <%s>\r\n",to);
Debug("%s",vbuff);

if (!Dialogue(sd,vbuff))
   {
   goto mail_err;
   }

if (!Dialogue(sd,"DATA\r\n"))
   {
   goto mail_err;
   }

if (anomaly)
   {
   sprintf(vbuff,"Subject: %s **!! [%s/%s]\r\n",MailSubject(),VFQNAME,VIPADDRESS);
   Debug("%s",vbuff);
   }
else
   {
   sprintf(vbuff,"Subject: %s [%s/%s]\r\n",MailSubject(),VFQNAME,VIPADDRESS);
   Debug("%s",vbuff);
   }
 
sent = send(sd,vbuff,strlen(vbuff),0);

#if defined LINUX || defined NETBSD || defined FREEBSD || defined OPENBSD
strftime(vbuff,CF_BUFSIZE,"Date: %a, %d %b %Y %H:%M:%S %z\r\n",localtime(&now));
sent=send(sd,vbuff,strlen(vbuff),0);
#endif

 if (strlen(MAILFROM) == 0)
    {
    sprintf(vbuff,"From: cfengine@%s\r\n",VFQNAME);
    Debug("%s",vbuff);
    }
 else
    {
    sprintf(vbuff,"From: %s\r\n",MAILFROM);
    Debug("%s",vbuff);    
    }
 
sent = send(sd,vbuff,strlen(vbuff),0);

sprintf(vbuff,"To: %s\r\n\r\n",to); 
Debug("%s",vbuff);
sent = send(sd,vbuff,strlen(vbuff),0);

while(!feof(fp))
   {
   vbuff[0] = '\0';
   fgets(vbuff,CF_BUFSIZE,fp);
   Debug("%s",vbuff);
   
   if (strlen(vbuff) > 0)
      {
      vbuff[strlen(vbuff)-1] = '\r';
      strcat(vbuff, "\n");
      count++;
      sent = send(sd,vbuff,strlen(vbuff),0);
      }
   
   if ((MAXLINES != INF_LINES) && (count > MAXLINES))
      {
      sprintf(vbuff,"\r\n[Mail truncated by cfengine. File is at %s on %s]\r\n",file,VFQNAME);
      sent = send(sd,vbuff,strlen(vbuff),0);
      break;
      }
   } 

if (!Dialogue(sd,".\r\n"))
   {
   Debug("mail_err\n");
   goto mail_err;
   }
 
Dialogue(sd,"QUIT\r\n");
Debug("Done sending mail\n");
fclose(fp);
cf_closesocket(sd);
return;
 
mail_err: 

fclose(fp);
cf_closesocket(sd); 
CfOut(cf_log,"","Cannot mail to %s.", to);
}

/******************************************************************/
/* Level 5                                                        */
/******************************************************************/

int Dialogue(int sd,char *s)

{ int sent;
  char ch,f = '\0';
  int charpos,rfclinetype = ' ';

if ((s != NULL) && (*s != '\0'))
   {
   sent = send(sd,s,strlen(s),0);
   Debug("SENT(%d)->%s",sent,s);
   }
else
   {
   Debug("Nothing to send .. waiting for opening\n");
   }

charpos = 0;
 
while (recv(sd,&ch,1,0))
   {
   charpos++;
   
   if (f == '\0')
      {
      f = ch;
      }

   if (charpos == 4)  /* Multiline RFC in form 222-Message with hyphen at pos 4 */
      {
      rfclinetype = ch;
      }
   
   Debug("%c",ch);
   
   if (ch == '\n' || ch == '\0')
      {
      charpos = 0;
      
      if (rfclinetype == ' ')
         {
         break;
         }
      }
   }

return ((f == '2') || (f == '3')); /* return code 200 or 300 from smtp*/
}

/* EOF */


