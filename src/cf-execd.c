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

#include "generic_agent.h"

/*******************************************************************/

static const int INF_LINES = -2;

/*******************************************************************/

extern BodySyntax CFEX_CONTROLBODY[];

/*******************************************************************/

static int NO_FORK;
static int ONCE;
static int WINSERVICE = true;

static Item *SCHEDULE;
static int SPLAYTIME = 0;

static char EXECCOMMAND[CF_BUFSIZE];

static char VMAILSERVER[CF_BUFSIZE];
static char MAILFROM[CF_BUFSIZE];
static char MAILTO[CF_BUFSIZE];
static int MAXLINES = 30;

/*******************************************************************/

static GenericAgentConfig CheckOpts(int argc,char **argv);
static void ThisAgentInit(void);
static bool ScheduleRun(void);
static void *LocalExecThread(void *scheduled_run);
static void LocalExec(bool scheduled_run);
static int FileChecksum(char *filename,unsigned char digest[EVP_MAX_MD_SIZE+1]);
static int CompareResult(char *filename,char *prev_file);
static void MailResult(char *file);
static int Dialogue(int sd,char *s);
static void Apoptosis(void);

void StartServer(void);
static void KeepPromises(void);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *ID = "The executor daemon is a scheduler and wrapper for\n"
                 "execution of cf-agent. It collects the output of the\n"
                 "agent and can email it to a specified address. It can\n"
                 "splay the start time of executions across the network\n"
                 "and work as a class-based clock for scheduling.";

static const struct option OPTIONS[15] =
      {
      { "help",no_argument,0,'h' },
      { "debug",no_argument,0,'d' },
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

static const char *HINTS[15] =
      {
      "Print the help message",
      "Enable debugging output",
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
GenericAgentConfig config = CheckOpts(argc,argv);
GenericInitialize("executor", config);
ThisAgentInit();
KeepPromises();

#ifdef MINGW
if(WINSERVICE)
  {
  NovaWin_StartExecService();
  }
else
#endif /* MINGW */
  {
  StartServer();
  }

return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

static GenericAgentConfig CheckOpts(int argc,char **argv)

{ extern char *optarg;
  int optindex = 0;
  int c;
  char ld_library_path[CF_BUFSIZE];
  GenericAgentConfig config = GenericAgentDefaultConfig(cf_executor);

while ((c=getopt_long(argc,argv,"d:vnKIf:D:N:VxL:hFV1gMW",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          if (optarg && strlen(optarg) < 5)
             {
             FatalError(" -f used but argument \"%s\" incorrect",optarg);
             }

          SetInputFile(optarg);
          MINUSF = true;
          break;

      case 'd':
         NewClass("opt_debug");
         DEBUG = true;
         break;

      case 'K': IGNORELOCK = true;
          break;

      case 'D': NewClassesFromString(optarg);
          break;

      case 'N':
          NegateClassesFromString(optarg);
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

      case 'L':
          snprintf(ld_library_path,CF_BUFSIZE-1,"LD_LIBRARY_PATH=%s",optarg);
          if (putenv(xstrdup(ld_library_path)) != 0)
             {
             }
          break;

      case 'W':
    	  WINSERVICE = false;
          break;

      case 'F':
          ONCE = true;
          NO_FORK = true;
          break;

      case 'V': PrintVersionBanner("cf-execd");
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

return config;
}

/*****************************************************************************/

static void ThisAgentInit(void)

{
umask(077);
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

static double GetSplay(void)
{
char splay[CF_BUFSIZE];
snprintf(splay, CF_BUFSIZE, "%s+%s+%d", VFQNAME, VIPADDRESS, getuid());

return ((double)GetHash(splay)) / CF_HASHTABLESIZE;
}

/*****************************************************************************/

static void KeepPromises(void)

{ Constraint *cp;
  Rval retval;

for (cp = ControlBodyConstraints(cf_executor); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }

   if (GetVariable("control_executor", cp->lval, &retval) == cf_notype)
      {
      CfOut(cf_error,"","Unknown lval %s in exec control body",cp->lval);
      continue;
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_mailfrom].lval) == 0)
      {
      strcpy(MAILFROM, retval.item);
      CfDebug("mailfrom = %s\n",MAILFROM);
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_mailto].lval) == 0)
      {
      strcpy(MAILTO, retval.item);
      CfDebug("mailto = %s\n",MAILTO);
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_smtpserver].lval) == 0)
      {
      strcpy(VMAILSERVER, retval.item);
      CfDebug("smtpserver = %s\n",VMAILSERVER);
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_execcommand].lval) == 0)
      {
      strcpy(EXECCOMMAND, retval.item);
      CfDebug("exec_command = %s\n",EXECCOMMAND);
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_executorfacility].lval) == 0)
      {
      SetFacility(retval.item);
      continue;
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_mailmaxlines].lval) == 0)
      {
      MAXLINES = Str2Int(retval.item);
      CfDebug("maxlines = %d\n",MAXLINES);
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_splaytime].lval) == 0)
      {
      int time = Str2Int(ScalarRvalValue(retval));
      SPLAYTIME = (int)(time * GetSplay());
      }

   if (strcmp(cp->lval,CFEX_CONTROLBODY[cfex_schedule].lval) == 0)
      {
      Rlist *rp;
      CfDebug("schedule ...\n");
      DeleteItemList(SCHEDULE);
      SCHEDULE = NULL;

      for (rp= (Rlist *)retval.item; rp != NULL; rp = rp->next)
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

/* Might be called back from NovaWin_StartExecService */
void StartServer(void)

{ time_t now = time(NULL);
  Promise *pp = NewPromise("exec_cfengine","the executor agent");
  Attributes dummyattr;
  CfLock thislock;

Banner("Starting executor");
memset(&dummyattr,0,sizeof(dummyattr));

dummyattr.restart_class = "nonce";
dummyattr.transaction.ifelapsed = CF_EXEC_IFELAPSED;
dummyattr.transaction.expireafter = CF_EXEC_EXPIREAFTER;

if (!ONCE)
   {
   thislock = AcquireLock(pp->promiser,VUQNAME,CFSTARTTIME,dummyattr,pp,false);

   if (thislock.lock == NULL)
      {
      DeletePromise(pp);
      return;
      }

   /* Kill previous instances of cf-execd if those are still running */
   Apoptosis();
   }


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
   LocalExec(false);
   CloseLog();
   }
else
   {
#ifdef HAVE_PTHREAD_H
   pthread_t tid;
#endif


   while (true)
      {
      if (ScheduleRun())
         {
         CfOut(cf_verbose,"","Sleeping for splaytime %d seconds\n\n",SPLAYTIME);
         sleep(SPLAYTIME);

#if (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

         pthread_attr_init(&PTHREADDEFAULTS);
         pthread_attr_setdetachstate(&PTHREADDEFAULTS,PTHREAD_CREATE_DETACHED);

#ifdef HAVE_PTHREAD_ATTR_SETSTACKSIZE
         pthread_attr_setstacksize(&PTHREADDEFAULTS,(size_t)2048*1024);
#endif

         if (pthread_create(&tid,&PTHREADDEFAULTS,LocalExecThread,(void *)true) != 0)
            {
            CfOut(cf_inform,"pthread_create","Can't create thread!");
            LocalExec(true);
            }

         ThreadLock(cft_system);
         pthread_attr_destroy(&PTHREADDEFAULTS);
         ThreadUnlock(cft_system);
#else
         LocalExec(true);
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

static void Apoptosis()

{ Promise pp = {0};
  Rlist *signals = NULL, *owners = NULL;
  char mypid[32];
  static char promiserBuf[CF_SMALLBUF];

#if defined(__CYGWIN__) || defined(__MINGW32__)
  return;
#endif

CfOut(cf_verbose,""," !! Programmed pruning of the scheduler cluster");

#ifdef MINGW
snprintf(promiserBuf, sizeof(promiserBuf), "cf-execd");	  // using '\' causes regexp problems
#else
snprintf(promiserBuf, sizeof(promiserBuf), "%s/bin/cf-execd", CFWORKDIR);
#endif

pp.promiser = promiserBuf;
pp.promisee = (Rval) { "cfengine", CF_SCALAR };
pp.classes = "any";
pp.offset.line = 0;
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

AppendConstraint(&(pp.conlist), "signals", (Rval) { signals, CF_LIST }, "any", false);
AppendConstraint(&(pp.conlist), "process_select", (Rval) { xstrdup("true"), CF_SCALAR }, "any", false);
AppendConstraint(&(pp.conlist), "process_owner", (Rval) { owners, CF_LIST }, "any", false);
AppendConstraint(&(pp.conlist), "ifelapsed", (Rval) { xstrdup("0"), CF_SCALAR }, "any", false);
AppendConstraint(&(pp.conlist), "process_count", (Rval) { xstrdup("true"), CF_SCALAR }, "any", false);
AppendConstraint(&(pp.conlist), "match_range", (Rval) { xstrdup("0,2"), CF_SCALAR }, "any", false);
AppendConstraint(&(pp.conlist), "process_result", (Rval) { xstrdup("process_owner.process_count"), CF_SCALAR }, "any", false);

CfOut(cf_verbose,""," -> Looking for cf-execd processes owned by %s",mypid);

if (LoadProcessTable(&PROCESSTABLE))
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

static bool ScheduleRun(void)
{
Item *ip;

CfOut(cf_verbose,"","Sleeping...\n");
sleep(CFPULSETIME);                /* 1 Minute resolution is enough */

// recheck license (in case of license updates or expiry)

if (EnterpriseExpiry())
  {
  CfOut(cf_error,"","Cfengine - autonomous configuration engine. This enterprise license is invalid.\n");
  exit(1);
  }

ThreadLock(cft_system);

DeleteAlphaList(&VHEAP);
InitAlphaList(&VHEAP);
DeleteAlphaList(&VADDCLASSES);
InitAlphaList(&VADDCLASSES);

DeleteItemList(IPADDRESSES);
IPADDRESSES = NULL;

DeleteScope("this");
DeleteScope("mon");
DeleteScope("sys");
NewScope("this");
NewScope("mon");
NewScope("sys");

CfGetInterfaceInfo(cf_executor);
Get3Environment();
BuiltinClasses();
OSClasses();
SetReferenceTime(true);
ThreadUnlock(cft_system);

for (ip = SCHEDULE; ip != NULL; ip = ip->next)
   {
   CfOut(cf_verbose,"","Checking schedule %s...\n",ip->name);

   if (IsDefinedClass(ip->name))
      {
      CfOut(cf_verbose,"","Waking up the agent at %s ~ %s \n",cf_ctime(&CFSTARTTIME),ip->name);
      return true;
      }
   }

return false;
}

/*************************************************************************/

static const char *TwinFilename(void)
{
#if defined(__CYGWIN__) || defined(__MINGW32__)
return "bin-twin/cf-agent.exe";
#else
return "bin/cf-twin";
#endif
}

/*************************************************************************/

static const char *AgentFilename(void)
{
#if defined(__CYGWIN__) || defined(__MINGW32__)
return "bin/cf-agent.exe";
#else
return "bin/cf-agent";
#endif
}

/*************************************************************************/

static bool TwinExists(void)
{
char twinfilename[CF_BUFSIZE];
struct stat sb;
snprintf(twinfilename, CF_BUFSIZE, "%s/%s", CFWORKDIR, TwinFilename());
MapName(twinfilename);

return stat(twinfilename, &sb) == 0 && IsExecutable(twinfilename);
}

/*************************************************************************/

/* Buffer has to be at least CF_BUFSIZE bytes long */
static void ConstructFailsafeCommand(bool scheduled_run, char *buffer)
{
bool twin_exists = TwinExists();

snprintf(buffer, CF_BUFSIZE,
         "\"%s/%s\" -f failsafe.cf "
         "&& \"%s/%s\" -Dfrom_cfexecd%s",
         CFWORKDIR, twin_exists ? TwinFilename(): AgentFilename(),
         CFWORKDIR, AgentFilename(),
         scheduled_run ? ":scheduled_run" : "");
}

/*************************************************************************/

static void LocalExec(bool scheduled_run)

{ FILE *pp;
  char line[CF_BUFSIZE],lineEscaped[sizeof(line)*2],filename[CF_BUFSIZE],*sp;
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
   ConstructFailsafeCommand(scheduled_run, cmd);
   }

strncpy(esc_command,MapName(cmd),CF_BUFSIZE-1);

snprintf(line,CF_BUFSIZE-1,"_%jd_%s",(intmax_t)starttime,CanonifyName(cf_ctime(&starttime)));
snprintf(filename,CF_BUFSIZE-1,"%s/outputs/cf_%s_%s_%p",CFWORKDIR,CanonifyName(VFQNAME),line,threadName);
MapName(filename);

/* What if no more processes? Could sacrifice and exec() - but we need a sentinel */

if ((fp = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","!! Couldn't open \"%s\" - aborting exec\n",filename);
   return;
   }

#if !defined(__MINGW32__)
/*
 * Don't inherit this file descriptor on fork/exec
 */

if (fileno(fp) != -1)
   {
   fcntl(fileno(fp), F_SETFD, FD_CLOEXEC);
   }
#endif

CfOut(cf_verbose,""," -> Command => %s\n",cmd);

if ((pp = cf_popen_sh(esc_command,"r")) == NULL)
   {
   CfOut(cf_error,"cf_popen","!! Couldn't open pipe to command \"%s\"\n",cmd);
   fclose(fp);
   return;
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
      // we must escape print format chars (%) from output

      ReplaceStr(line,lineEscaped,sizeof(lineEscaped),"%","%%");

      fprintf(fp,"%s\n",lineEscaped);
      count++;

      /* If we can't send mail, log to syslog */

      if (strlen(MAILTO) == 0)
         {
	 strncat(lineEscaped,"\n",sizeof(lineEscaped)-1-strlen(lineEscaped));
         if ((strchr(lineEscaped,'\n')) == NULL)
            {
	    lineEscaped[sizeof(lineEscaped)-2] = '\n';
            }

         CfOut(cf_inform,"", "%s", lineEscaped);
         }

      line[0] = '\0';
      lineEscaped[0] = '\0';
      }
   }

cf_pclose(pp);
CfDebug("Closing fp\n");
fclose(fp);

CfOut(cf_verbose,""," -> Command is complete\n");

if (count)
   {
   CfOut(cf_verbose,""," -> Mailing result\n");
   MailResult(filename);
   }
else
   {
   CfOut(cf_verbose,""," -> No output\n");
   unlink(filename);
   }
}

/*************************************************************************/

static void *LocalExecThread(void *scheduled_run)
{
LocalExec((bool)scheduled_run);
return NULL;
}

/******************************************************************************/
/* Level 4                                                                    */
/******************************************************************************/

static int FileChecksum(char *filename,unsigned char digest[EVP_MAX_MD_SIZE+1])

{ FILE *file;
  EVP_MD_CTX context;
  int len;
  unsigned int md_len;
  unsigned char buffer[1024];
  const EVP_MD *md = NULL;

CfDebug("FileChecksum(%s)\n",filename);

if ((file = fopen(filename,"rb")) == NULL)
   {
   printf ("%s can't be opened\n", filename);
   }
else
   {
   md = EVP_get_digestbyname("md5");

   if (!md)
      {
      return 0;
      }

   EVP_DigestInit(&context,md);

   while ((len = fread(buffer,1,1024,file)))
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

static int CompareResult(char *filename,char *prev_file)

{ int i;
  unsigned char digest1[EVP_MAX_MD_SIZE+1];
  unsigned char digest2[EVP_MAX_MD_SIZE+1];
  int  md_len1, md_len2;
  FILE *fp;
  int rtn = 0;

CfOut(cf_verbose,"","Comparing files  %s with %s\n", prev_file, filename);

if ((fp=fopen(prev_file,"r")) != NULL)
   {
   fclose(fp);

   md_len1 = FileChecksum(prev_file, digest1);
   md_len2 = FileChecksum(filename,  digest2);

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
   CfOut(cf_error, "", "!! Severe lock error when mailing in exec");
   return 1;
   }

/* replace old file with new*/

unlink(prev_file);

 if(!LinkOrCopy(filename,prev_file,true))
   {
     CfOut(cf_inform,"","Could not symlink or copy %s to %s",filename,prev_file);
     rtn = 1;
   }

ThreadUnlock(cft_count);
return(rtn);
}

/***********************************************************************/

static void MailResult(char *file)

{ int sd, count = 0, anomaly = false;
  char prev_file[CF_BUFSIZE],vbuff[CF_BUFSIZE];
  struct hostent *hp;
  struct sockaddr_in raddr;
  struct servent *server;
  struct stat statbuf;
  time_t now = time(NULL);
  FILE *fp;

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
   CfDebug("Nothing to report in %s\n",file);
   return;
   }

if (CompareResult(file,prev_file) == 0)
   {
   CfOut(cf_verbose,"","Previous output is the same as current so do not mail it\n");
   return;
   }

if ((strlen(VMAILSERVER) == 0) || (strlen(MAILTO) == 0))
   {
   /* Syslog should have done this */
   CfOut(cf_verbose, "", "Empty mail server or address - skipping");
   return;
   }

if (MAXLINES == 0)
   {
   CfDebug("Not mailing: EmailMaxLines was zero\n");
   return;
   }

CfDebug("Mailing results of (%s) to (%s)\n", file, MAILTO);


/* Check first for anomalies - for subject header */

if ((fp = fopen(file,"r")) == NULL)
   {
   CfOut(cf_inform,"fopen","!! Couldn't open file %s",file);
   return;
   }

while (!feof(fp))
   {
   vbuff[0] = '\0';
   if (fgets(vbuff,CF_BUFSIZE,fp) == NULL)
      {
      break;
      }

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

CfDebug("Looking up hostname %s\n\n",VMAILSERVER);

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

CfDebug("Connecting...\n");

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
CfDebug("%s",vbuff);

if (!Dialogue(sd,vbuff))
   {
   goto mail_err;
   }

 if (strlen(MAILFROM) == 0)
    {
    sprintf(vbuff,"MAIL FROM: <cfengine@%s>\r\n",VFQNAME);
    CfDebug("%s",vbuff);
    }
 else
    {
    sprintf(vbuff,"MAIL FROM: <%s>\r\n",MAILFROM);
    CfDebug("%s",vbuff);
    }

if (!Dialogue(sd,vbuff))
   {
   goto mail_err;
   }

sprintf(vbuff, "RCPT TO: <%s>\r\n", MAILTO);
CfDebug("%s",vbuff);

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
   CfDebug("%s",vbuff);
   }
else
   {
   sprintf(vbuff,"Subject: %s [%s/%s]\r\n",MailSubject(),VFQNAME,VIPADDRESS);
   CfDebug("%s",vbuff);
   }

send(sd,vbuff,strlen(vbuff),0);

#if defined LINUX || defined NETBSD || defined FREEBSD || defined OPENBSD
strftime(vbuff,CF_BUFSIZE,"Date: %a, %d %b %Y %H:%M:%S %z\r\n",localtime(&now));
send(sd,vbuff,strlen(vbuff),0);
#endif

 if (strlen(MAILFROM) == 0)
    {
    sprintf(vbuff,"From: cfengine@%s\r\n",VFQNAME);
    CfDebug("%s",vbuff);
    }
 else
    {
    sprintf(vbuff,"From: %s\r\n",MAILFROM);
    CfDebug("%s",vbuff);
    }

send(sd,vbuff,strlen(vbuff),0);

sprintf(vbuff, "To: %s\r\n\r\n", MAILTO);
CfDebug("%s",vbuff);
send(sd,vbuff,strlen(vbuff),0);

while(!feof(fp))
   {
   vbuff[0] = '\0';
   if (fgets(vbuff,CF_BUFSIZE,fp) == NULL)
      {
      break;
      }

   CfDebug("%s",vbuff);

   if (strlen(vbuff) > 0)
      {
      vbuff[strlen(vbuff)-1] = '\r';
      strcat(vbuff, "\n");
      count++;
      send(sd,vbuff,strlen(vbuff),0);
      }

   if ((MAXLINES != INF_LINES) && (count > MAXLINES))
      {
      sprintf(vbuff,"\r\n[Mail truncated by cfengine. File is at %s on %s]\r\n",file,VFQNAME);
      send(sd,vbuff,strlen(vbuff),0);
      break;
      }
   }

if (!Dialogue(sd,".\r\n"))
   {
   CfDebug("mail_err\n");
   goto mail_err;
   }

Dialogue(sd,"QUIT\r\n");
CfDebug("Done sending mail\n");
fclose(fp);
cf_closesocket(sd);
return;

mail_err:

fclose(fp);
cf_closesocket(sd);
CfOut(cf_log,"","Cannot mail to %s.", MAILTO);
}

/******************************************************************/
/* Level 5                                                        */
/******************************************************************/

static int Dialogue(int sd,char *s)

{ int sent;
  char ch,f = '\0';
  int charpos,rfclinetype = ' ';

if ((s != NULL) && (*s != '\0'))
   {
   sent = send(sd,s,strlen(s),0);
   CfDebug("SENT(%d)->%s",sent,s);
   }
else
   {
   CfDebug("Nothing to send .. waiting for opening\n");
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

   CfDebug("%c",ch);

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


