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
/* File: server.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"
#include "cf3.server.h"

int main (int argc,char *argv[]);
void StartServer (int argc, char **argv);
int OpenReceiverChannel (void);
void PurgeOldConnections (struct Item **list,time_t now);
void SpawnConnection (int sd_reply, char *ipaddr);
void CheckFileChanges (int argc, char **argv, int sd);
void *HandleConnection (struct cfd_connection *conn);
int BusyWithConnection (struct cfd_connection *conn);
int MatchClasses (struct cfd_connection *conn);
void DoExec (struct cfd_connection *conn, char *sendbuffer, char *args);
int GetCommand (char *str);
int VerifyConnection (struct cfd_connection *conn, char *buf);
void RefuseAccess (struct cfd_connection *conn, char *sendbuffer, int size, char *errormsg);
int AccessControl (char *filename, struct cfd_connection *conn, int encrypt);
int CheckStoreKey  (struct cfd_connection *conn, RSA *key);
int StatFile (struct cfd_connection *conn, char *sendbuffer, char *filename);
void CfGetFile (struct cfd_get_arg *args);
void CompareLocalHash(struct cfd_connection *conn, char *sendbuffer, char *recvbuffer);
int CfOpenDirectory (struct cfd_connection *conn, char *sendbuffer, char *dirname);
int CfSecOpenDirectory (struct cfd_connection *conn, char *sendbuffer, char *dirname);
void Terminate (int sd);
void DeleteAuthList (struct Auth *ap);
int AllowedUser (char *user);
int AuthorizeRoles(struct cfd_connection *conn,char *args);
void ReplyNothing (struct cfd_connection *conn);
struct cfd_connection *NewConn (int sd);
void DeleteConn (struct cfd_connection *conn);
time_t SecondsTillAuto (void);
void SetAuto (int seconds);
int cfscanf (char *in, int len1, int len2, char *out1, char *out2, char *out3);
int AuthenticationDialogue (struct cfd_connection *conn,char *buffer, int buffersize);
char *MapAddress (char *addr);
int IsKnownHost (RSA *oldkey,RSA *newkey,char *addr,char *user);
void AddToKeyDB (RSA *key,char *addr);
int SafeOpen (char *filename);
void SafeClose (int fd);
int OptionFound(char *args, char *pos, char *word);
in_addr_t GetInetAddr (char *host);

extern struct BodySyntax CFS_CONTROLBODY[];

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

 char *ID = "The server daemon provides two services: it acts as a\n"
            "file server for remote file copying and it allows an\n"
            "authorized cf-runagent to start start a cf-agent process\n"
            "and set certain additional classes with role-based access\n"
            "control.\n";
 
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
      "Set the internal value of LD_LIBRARY_PATH for child processes",
      NULL
      };

/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

int CLOCK_DRIFT = 3600;   /* 1hr */
int CFD_MAXPROCESSES = 0;
int ACTIVE_THREADS = 0;
int NO_FORK = false;
int MULTITHREAD = false;
int CHECK_RFC931 = false;
int CFD_INTERVAL = 0;
int DENYBADCLOCKS = true;
int MULTIPLECONNS = false;
int TRIES = 0;
int MAXTRIES = 5;
int LOGCONNS = false;
int LOGENCRYPT = false;

struct Item *CONNECTIONLIST = NULL;
struct Auth *ROLES = NULL;
struct Auth *ROLESTOP = NULL;

struct Auth *VADMIT = NULL;
struct Auth *VADMITTOP = NULL;
struct Auth *VDENY = NULL;
struct Auth *VDENYTOP = NULL;

/*****************************************************************************/

int main(int argc,char *argv[])

{
CheckOpts(argc,argv);
GenericInitialize(argc,argv,"server");
ThisAgentInit();
KeepPromises();
Summarize();

StartServer(argc,argv);
return 0;
}

/*******************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  char ld_library_path[CF_BUFSIZE];
  struct Item *actionList;
  int optindex = 0;
  int c;
  
while ((c=getopt_long(argc,argv,"d:vIf:D:N:VSxLFM",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          VINPUTFILE[CF_BUFSIZE-1] = '\0';
          MINUSF = true;
          break;

      case 'd': 
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

          NO_FORK = true;
          break;
          
      case 'K': IGNORELOCK = true;
          break;
                    
      case 'D': NewClassesFromString(optarg);
          break;
          
      case 'N': NegateClassesFromString(optarg,&VNEGHEAP);
          break;
          
      case 'I':
          INFORM = true;
          break;
          
      case 'v':
          VERBOSE = true;
          NO_FORK = true;
          break;
          
      case 'F': NO_FORK = true;
         break;

      case 'L': CfOut(cf_verbose,"","Setting LD_LIBRARY_PATH=%s\n",optarg);
          snprintf(ld_library_path,CF_BUFSIZE-1,"LD_LIBRARY_PATH=%s",optarg);
          putenv(ld_library_path);
          break;

      case 'V': Version("cf-serverd");
          exit(0);
          
      case 'h': Syntax("cf-serverd - cfengine's server agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'M': ManPage("cf-serverd - cfengine's server agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'x': SelfDiagnostic();
          exit(0);
          
      default:  Syntax("cf-serverd - cfengine's server agent",OPTIONS,HINTS,ID);
          exit(1);
          
      }
  }

Debug("Set debugging\n");
}

/*******************************************************************/

void ThisAgentInit()

{ char vbuff[CF_BUFSIZE];
  int i;

umask(077);
}

/*******************************************************************/

void StartServer(int argc,char **argv)

{ char ipaddr[CF_MAXVARSIZE],intime[64];
  int sd,sd_reply;
  fd_set rset;
  time_t now;
  struct timeval timeout;
  int ret_val;
  struct Promise *pp = NewPromise("server_cfengine","the server daemon");
  struct Attributes dummyattr;
  struct CfLock thislock;

#if defined(HAVE_GETADDRINFO)
  int addrlen=sizeof(struct sockaddr_in6);
  struct sockaddr_in6 cin;
#else
  int addrlen=sizeof(struct sockaddr_in);
  struct sockaddr_in cin;
#endif
  
if ((sd = OpenReceiverChannel()) == -1)
   {
   CfOut(cf_error,"","Unable to start server");
   exit(1);
   }

signal(SIGINT,HandleSignals);
signal(SIGTERM,HandleSignals);
signal(SIGHUP,SIG_IGN);
signal(SIGPIPE,SIG_IGN);
signal(SIGCHLD,SIG_IGN);
signal(SIGUSR1,HandleSignals);
signal(SIGUSR2,HandleSignals);
 
if (listen(sd,queuesize) == -1)
   {
   CfOut(cf_error,"listen","listen failed");
   exit(1);
   }

thislock = AcquireLock(pp->promiser,VUQNAME,CFSTARTTIME,dummyattr,pp);

if (thislock.lock == NULL)
   {
   return;
   }

CfOut(cf_verbose,"","Listening for connections ...\n");

if ((!NO_FORK) && (fork() != 0))
   {
   CfOut(cf_inform,"","cfServerd starting %.24s\n",ctime(&CFDSTARTTIME));
   closelog();
   exit(0);
   }

if (!NO_FORK)
   {
   ActAsDaemon(sd);
   }

WritePID("cf-serverd.pid");

/* Andrew Stribblehill <ads@debian.org> -- close sd on exec */ 
fcntl(sd, F_SETFD, FD_CLOEXEC);
 
while (true)
   {
   if (ACTIVE_THREADS == 0)
      {
      CheckFileChanges(argc,argv,sd);
      }
   
   FD_ZERO(&rset);
   FD_SET(sd,&rset);
   
   timeout.tv_sec = 10;  /* Set a 10 second timeout for select */
   timeout.tv_usec = 0;
   
   ret_val = select((sd+1),&rset,NULL,NULL,&timeout);

   if (ret_val == -1)   /* Error received from call to select */
      {
      if (errno == EINTR)
         {
         continue;
         }
      else
         {
         CfOut(cf_error,"select","select failed");
         exit(1);
         }
      }
   else if (!ret_val)   /* No data waiting, we must have timed out! */
      {
      continue;
      }
   
   if ((sd_reply = accept(sd,(struct sockaddr *)&cin,&addrlen)) != -1)
      {
      memset(ipaddr,0,CF_MAXVARSIZE);
      snprintf(ipaddr,CF_MAXVARSIZE-1,"%s",sockaddr_ntop((struct sockaddr *)&cin));
      
      Debug("Obtained IP address of %s on socket %d from accept\n",ipaddr,sd_reply);
      
      if (NONATTACKERLIST && !IsFuzzyItemIn(NONATTACKERLIST,MapAddress(ipaddr)) && !IsRegexItemIn(NONATTACKERLIST,MapAddress(ipaddr)))   /* Allowed Subnets */
         {
         CfOut(cf_error,"","Not allowing connection from non-authorized IP %s\n",ipaddr);
         close(sd_reply);
         continue;
         }
      
      if (IsFuzzyItemIn(ATTACKERLIST,MapAddress(ipaddr)) || IsRegexItemIn(ATTACKERLIST,MapAddress(ipaddr)))
         {
         CfOut(cf_error,"","Denying connection from non-authorized IP %s\n",ipaddr);
         close(sd_reply);
         continue;
         }      
      
      if ((now = time((time_t *)NULL)) == -1)
         {
         now = 0;
         }
      
      PurgeOldConnections(&CONNECTIONLIST,now);
      
      if (!IsFuzzyItemIn(MULTICONNLIST,MapAddress(ipaddr)) && !IsRegexItemIn(MULTICONNLIST,MapAddress(ipaddr)))
         {
         if (IsItemIn(CONNECTIONLIST,MapAddress(ipaddr)))
            {
            CfOut(cf_error,"","Denying repeated connection from %s\n",ipaddr);
            close(sd_reply);
            continue;
            }
         }
      
      if (LOGCONNS)
         {
         CfOut(cf_inform,"","Accepting connection from %s\n",ipaddr);
         }
      
      snprintf(intime,63,"%d",(int)now);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
      if (pthread_mutex_lock(&MUTEX_COUNT) != 0)
         {
         CfOut(cf_error,"pthread_mutex_lock","pthread_mutex_lock failed");
         return;
         }
#endif

      PrependItem(&CONNECTIONLIST,MapAddress(ipaddr),intime);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
      if (pthread_mutex_unlock(&MUTEX_COUNT) != 0)
         {
         CfOut(cf_error,"pthread_mutex_unlock","pthread_mutex_unlock failed");
         return;
         }
#endif
      
      SpawnConnection(sd_reply,ipaddr);
      }
   }

YieldCurrentLock(thislock); /* We never get here - this is done by a signal handler */
}

/*******************************************************************************/

in_addr_t GetInetAddr(char *host)

{ struct in_addr addr;
  struct hostent *hp;
  char output[CF_BUFSIZE];
  
addr.s_addr = inet_addr(host);

if ((addr.s_addr == INADDR_NONE) || (addr.s_addr == 0)) 
   {
   if ((hp = gethostbyname(host)) == 0)
      {
      snprintf(output,CF_BUFSIZE,"\nhost not found: %s\n",host);
      FatalError(output);
      }
   
   if (hp->h_addrtype != AF_INET)
      {
      snprintf(output,CF_BUFSIZE,"unexpected address family: %d\n",hp->h_addrtype);
      FatalError(output);
      }
   
   if (hp->h_length != sizeof(addr))
      {
      snprintf(output,CF_BUFSIZE,"unexpected address length %d\n",hp->h_length);
      FatalError(output);
      }

   memcpy((char *) &addr, hp->h_addr, hp->h_length);
   }

return (addr.s_addr);
}


/*********************************************************************/
/* Level 2                                                           */
/*********************************************************************/

int OpenReceiverChannel()

{ int sd;
  int yes=1;
  char *ptr = NULL;

  struct linger cflinger;
#if defined(HAVE_GETADDRINFO)
    struct addrinfo query,*response,*ap;
#else
    struct sockaddr_in sin;
#endif
    
cflinger.l_onoff = 1;
cflinger.l_linger = 60;

#if defined(HAVE_GETADDRINFO)
  
memset(&query,0,sizeof(struct addrinfo));

query.ai_flags = AI_PASSIVE;
query.ai_family = AF_UNSPEC;
query.ai_socktype = SOCK_STREAM;

/*
 * HvB : Bas van der Vlies
*/
if (BINDINTERFACE[0] != '\0' )
  {
  ptr = BINDINTERFACE;
  }

if (getaddrinfo(ptr,STR_CFENGINEPORT,&query,&response) != 0)
   {
   CfOut(cf_error,"getaddrinfo","DNS/service lookup failure");
   return -1;   
   }

sd = -1;
 
for (ap = response ; ap != NULL; ap=ap->ai_next)
   {
   if ((sd = socket(ap->ai_family,ap->ai_socktype,ap->ai_protocol)) == -1)
      {
      continue;
      }

   if (setsockopt(sd, SOL_SOCKET,SO_REUSEADDR,(char *)&yes,sizeof (int)) == -1)
      {
      CfOut(cf_error,"setsockopt","Socket options were not accepted");
      exit(1);
      }
   
   if (setsockopt(sd, SOL_SOCKET, SO_LINGER,(char *)&cflinger,sizeof (struct linger)) == -1)
      {
      CfOut(cf_error,"setsockopt","Socket options were not accepted");
      exit(1);
      }

   if (bind(sd,ap->ai_addr,ap->ai_addrlen) == 0)
      {
      Debug("Bound to address %s on %s=%d\n",sockaddr_ntop(ap->ai_addr),CLASSTEXT[VSYSTEMHARDCLASS],VSYSTEMHARDCLASS);

      if (VSYSTEMHARDCLASS == openbsd || VSYSTEMHARDCLASS == freebsd || VSYSTEMHARDCLASS == netbsd || VSYSTEMHARDCLASS == dragonfly)
         {
         continue;  /* *bsd doesn't map ipv6 addresses */
         }
      else
         {
         break;
         }
      }
   
   CfOut(cf_error,"bind","Could not bind server address");
   close(sd);
   sd = -1;
   }

if (sd < 0)
   {
   CfOut(cf_error,"","Couldn't open bind an open socket\n");
   exit(1);
   }

if (response != NULL)
   {
   freeaddrinfo(response);
   }
#else 
 
memset(&sin,0,sizeof(sin));

if (BINDINTERFACE[0] != '\0' )
   {
   sin.sin_addr.s_addr = GetInetAddr(BINDINTERFACE);
   }
 else
   {
   sin.sin_addr.s_addr = INADDR_ANY;
   }

sin.sin_port = (unsigned short)SHORT_CFENGINEPORT;
sin.sin_family = AF_INET; 

if ((sd = socket(AF_INET,SOCK_STREAM,0)) == -1)
   {
   CfOut(cf_error,"socket","Couldn't open socket");
   exit (1);
   }

if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof (int)) == -1)
   {
   CfOut(cf_error,"sockopt","Couldn't set socket options");
   exit (1);
   }

 if (setsockopt(sd, SOL_SOCKET, SO_LINGER, (char *) &cflinger, sizeof (struct linger)) == -1)
   {
   CfOut(cf_error,"sockopt","Couldn't set socket options");
   exit (1);
   }

if (bind(sd,(struct sockaddr *)&sin,sizeof(sin)) == -1) 
   {
   CfOut(cf_error,"bind","Couldn't bind to socket");
   exit(1);
   }

#endif
 
return sd;
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

void PurgeOldConnections(struct Item **list,time_t now)

   /* Some connections might not terminate properly. These should be cleaned
      every couple of hours. That should be enough to prevent spamming. */

{ struct Item *ip;
 int then=0;

if (list == NULL)
   {
   return;
   }

Debug("Purging Old Connections...\n");

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
if (pthread_mutex_lock(&MUTEX_COUNT) != 0)
   {
   CfOut(cf_error,"pthread_mutex_lock","pthread_mutex_lock failed");
   return;
   }
#endif

for (ip = *list; ip != NULL; ip=ip->next)
   {
   sscanf(ip->classes,"%d",&then);

   if (now > then + 7200)
      {
      DeleteItem(list,ip);
      CfOut(cf_verbose,"","Purging IP address %s from connection list\n",ip->name);
      }
   }

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
if (pthread_mutex_unlock(&MUTEX_COUNT) != 0)
   {
   CfOut(cf_error,"pthread_mutex_unlock","pthread_mutex_unlock failed");
   return;
   }
#endif

Debug("Done purging\n");
}


/*********************************************************************/

void SpawnConnection(int sd_reply,char *ipaddr)

{ struct cfd_connection *conn;

#ifdef HAVE_PTHREAD_H
 pthread_t tid;
#endif

conn = NewConn(sd_reply);

strncpy(conn->ipaddr,ipaddr,CF_MAX_IP_LEN-1);

CfOut(cf_verbose,"","New connection...(from %s/%d)\n",conn->ipaddr,sd_reply);
 
#if defined HAVE_LIBPTHREAD || defined BUILTIN_GCC_THREAD

CfOut(cf_verbose,"","Spawning new thread...\n");

pthread_attr_init(&PTHREADDEFAULTS);
pthread_attr_setdetachstate(&PTHREADDEFAULTS,PTHREAD_CREATE_DETACHED);

#ifdef HAVE_PTHREAD_ATTR_SETSTACKSIZE
pthread_attr_setstacksize(&PTHREADDEFAULTS,(size_t)1024*1024);
#endif
 
if (pthread_create(&tid,&PTHREADDEFAULTS,(void *)HandleConnection,(void *)conn) != 0)
   {
   CfOut(cf_error,"create","pthread_create failed");
   HandleConnection(conn);
   }

pthread_attr_destroy(&PTHREADDEFAULTS);

#else

/* Can't fork here without getting a zombie unless we do some complex waiting? */

CfOut(cf_verbose,"","Single threaded...\n");

HandleConnection(conn);
 
#endif
}

/**************************************************************/

void CheckFileChanges(int argc,char **argv,int sd)

{ struct stat newstat;
  char filename[CF_BUFSIZE],*sp;
  int ok;
  
memset(&newstat,0,sizeof(struct stat));
memset(filename,0,CF_BUFSIZE);

if ((*VINPUTFILE != '.') && !IsAbsoluteFileName(VINPUTFILE)) /* Don't prepend to absolute names */
   {
   snprintf(filename,CF_BUFSIZE,"%s/inputs/",CFWORKDIR);
   }

strncat(filename,VINPUTFILE,CF_BUFSIZE-1-strlen(filename));

Debug("Checking file updates on %s (%x/%x)\n",filename, newstat.st_mtime, CFDSTARTTIME);

if (NewPromiseProposals())
   {
   CfOut(cf_inform,"","Rereading config files %s..\n",filename);

   /* Free & reload -- lock this to avoid access errors during reload */

   DeleteItemList(VHEAP);
   DeleteItemList(VNEGHEAP);
   DeleteItemList(TRUSTKEYLIST);
   DeleteItemList(SKIPVERIFY);
   DeleteItemList(DHCPLIST);
   DeleteItemList(ATTACKERLIST);
   DeleteItemList(NONATTACKERLIST);
   DeleteItemList(MULTICONNLIST);
   DeleteAuthList(VADMIT);
   DeleteAuthList(VDENY);
   DeleteRlist(VINPUTLIST);

   DeleteAllScope();

   strcpy(VDOMAIN,"undefined.domain");

   VADMIT = VADMITTOP = NULL;
   VDENY  = VDENYTOP  = NULL;
   VHEAP  = VNEGHEAP  = NULL;
   TRUSTKEYLIST = NULL;
   SKIPVERIFY = NULL;
   DHCPLIST = NULL;
   ATTACKERLIST = NULL;
   NONATTACKERLIST = NULL;
   MULTICONNLIST = NULL;
   VINPUTLIST = NULL;

   DeleteBundles(BUNDLES);
   DeleteBodies(BODIES);

   BUNDLES = NULL;
   BODIES  = NULL;
   ERRORCOUNT = 0;

   NewScope("sys");
   NewScope("const");
   NewScope("this");
   NewScope("control_server");
   NewScope("control_common");
   GetNameInfo3();
   GetInterfaceInfo3();
   FindV6InterfaceInfo();
   Get3Environment();
   OSClasses();
   SetReferenceTime(true);
   
   ok = CheckPromises(cf_server);

   if (ok)
      {
      ReadPromises(cf_server,CF_SERVERC);
      }
   else
      {
      snprintf(VINPUTFILE,CF_BUFSIZE-1,"%s/inputs/failsafe.cf",CFWORKDIR);
      ReadPromises(cf_server,CF_SERVERC);
      }

   KeepPromises();
   Summarize();
   }
}

/*********************************************************************/
/* Level 4                                                           */
/*********************************************************************/

void *HandleConnection(struct cfd_connection *conn)

{ char output[CF_BUFSIZE];
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
#ifdef HAVE_PTHREAD_SIGMASK
 sigset_t sigmask;

sigemptyset(&sigmask);
pthread_sigmask(SIG_BLOCK,&sigmask,NULL); 
#endif

if (conn == NULL)
   {
   Debug("Null connection\n");
   return NULL;
   }
 
if (pthread_mutex_lock(&MUTEX_COUNT) != 0)
   {
   CfOut(cf_error,"pthread_mutex_lock","pthread_mutex_lock failed");
   DeleteConn(conn);
   return NULL;
   }

ACTIVE_THREADS++;

if (pthread_mutex_unlock(&MUTEX_COUNT) != 0)
   {
   CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
   }  

if (ACTIVE_THREADS >= CFD_MAXPROCESSES)
   {
   if (pthread_mutex_lock(&MUTEX_COUNT) != 0)
      {
      CfOut(cf_error,"pthread_mutex_lock","pthread_mutex_lock failed");
      DeleteConn(conn);
      return NULL;
      }
   
   ACTIVE_THREADS--;
   
   if (TRIES++ > MAXTRIES)  /* When to say we're hung / apoptosis threshold */
      {
      CfOut(cf_error,"","Server seems to be paralyzed. DOS attack? Committing apoptosis...");
      HandleSignals(SIGTERM);
      }

   if (pthread_mutex_unlock(&MUTEX_COUNT) != 0)
      {
      CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
      }

   CfOut(cf_error,"","Too many threads (>=%d) -- increase MaxConnections?",CFD_MAXPROCESSES);
   snprintf(output,CF_BUFSIZE,"BAD: Server is currently too busy -- increase MaxConnections or Splaytime?");
   SendTransaction(conn->sd_reply,output,0,CF_DONE);
   DeleteConn(conn);
   return NULL;
   }

TRIES = 0;   /* As long as there is activity, we're not stuck */
 
#endif
 
while (BusyWithConnection(conn))
   {
   }

#if defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD

 Debug("Terminating thread...\n");
 
if (pthread_mutex_lock(&MUTEX_COUNT) != 0)
   {
   CfOut(cf_error,"pthread_mutex_lock","pthread_mutex_lock failed");
   DeleteConn(conn);
   return NULL;
   }

ACTIVE_THREADS--;

if (pthread_mutex_unlock(&MUTEX_COUNT) != 0)
   {
   CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
   }
 
#endif

DeleteConn(conn);
return NULL; 
}

/*********************************************************************/

int BusyWithConnection(struct cfd_connection *conn)

  /* This is the protocol section. Here we must   */
  /* check that the incoming data are sensible    */
  /* and extract the information from the message */

{ time_t tloc, trem = 0;
  char recvbuffer[CF_BUFSIZE+CF_BUFEXT], sendbuffer[CF_BUFSIZE],check[CF_BUFSIZE];  
  char filename[CF_BUFSIZE],buffer[CF_BUFSIZE],args[CF_BUFSIZE],out[CF_BUFSIZE];
  long time_no_see = 0;
  int len=0, drift, plainlen, received;
  struct cfd_get_arg get_args;

memset(recvbuffer,0,CF_BUFSIZE+CF_BUFEXT);
memset(&get_args,0,sizeof(get_args));

if ((received = ReceiveTransaction(conn->sd_reply,recvbuffer,NULL)) == -1)
   {
   return false;
   }

if (strlen(recvbuffer) == 0)
   {
   Debug("cfServerd terminating NULL transmission!\n");
   return false;
   }
  
Debug("Received: [%s] on socket %d\n",recvbuffer,conn->sd_reply);

switch (GetCommand(recvbuffer))
   {
   case cfd_exec:
       memset(args,0,CF_BUFSIZE);
       sscanf(recvbuffer,"EXEC %[^\n]",args);
       
       if (!conn->id_verified)
          {
          CfOut(cf_inform,"","Server refusal due to incorrect identity\n");
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       if (!AllowedUser(conn->username))
          {
          CfOut(cf_inform,"","Server refusal due to non-allowed user\n");
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       if (!conn->rsa_auth)
          {
          CfOut(cf_inform,"","Server refusal due to no RSA authentication\n");
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       if (!AccessControl(CFRUNCOMMAND,conn,false))
          {
          CfOut(cf_inform,"","Server refusal due to denied access to requested object\n");
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;   
          }
       
       if (!MatchClasses(conn))
          {
          CfOut(cf_inform,"","Server refusal due to failed class/context match\n");
          Terminate(conn->sd_reply);
          return false;
          }
       
       DoExec(conn,sendbuffer,args);
       Terminate(conn->sd_reply);
       return false;

   case cfd_version:

       if (! conn->id_verified)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          }

       snprintf(conn->output,CF_BUFSIZE,"OK: %s",VERSION);
       SendTransaction(conn->sd_reply,conn->output,0,CF_DONE);
       return conn->id_verified;
       
   case cfd_cauth:

       conn->id_verified = VerifyConnection(conn,(char *)(recvbuffer+strlen("CAUTH ")));
       
       if (! conn->id_verified)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          }
       
       return conn->id_verified; /* are we finished yet ? */
       
       
   case cfd_sauth:   /* This is where key agreement takes place */
       
       if (! conn->id_verified)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       if (!AuthenticationDialogue(conn,recvbuffer,received))
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       return true;
       
   case cfd_get:

       memset(filename,0,CF_BUFSIZE);
       sscanf(recvbuffer,"GET %d %[^\n]",&(get_args.buf_size),filename);
       
       if (get_args.buf_size < 0 || get_args.buf_size > CF_BUFSIZE)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       if (! conn->id_verified)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       if (!AccessControl(filename,conn,false))
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return true;   
          }
       
       memset(sendbuffer,0,CF_BUFSIZE);
       
       if (get_args.buf_size >= CF_BUFSIZE)
          {
          get_args.buf_size = 2048;
          }
       
       get_args.connect = conn;
       get_args.encrypt = false;
       get_args.replybuff = sendbuffer;
       get_args.replyfile = filename;
       
       CfGetFile(&get_args);
       
       return true;
       
   case cfd_sget:
       memset(buffer,0,CF_BUFSIZE);
       sscanf(recvbuffer,"SGET %d %d",&len,&(get_args.buf_size));
       if (received != len+CF_PROTO_OFFSET)
          {
          CfOut(cf_verbose,"","Protocol error SGET\n");
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       plainlen = DecryptString(recvbuffer+CF_PROTO_OFFSET,buffer,conn->session_key,len);
       
       cfscanf(buffer,strlen("GET"),strlen("dummykey"),check,sendbuffer,filename);
       
       if (strcmp(check,"GET") != 0)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return true;
          }
       
       if (get_args.buf_size < 0 || get_args.buf_size > 8192)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       if (get_args.buf_size >= CF_BUFSIZE)
          {
          get_args.buf_size = 2048;
          }
       
       Debug("Confirm decryption, and thus validity of caller\n");
       Debug("SGET %s with blocksize %d\n",filename,get_args.buf_size);
       
       if (! conn->id_verified)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       if (!AccessControl(filename,conn,true))
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;   
          }
       
       memset(sendbuffer,0,CF_BUFSIZE);
       
       get_args.connect = conn;
       get_args.encrypt = true;
       get_args.replybuff = sendbuffer;
       get_args.replyfile = filename;
       
       CfGetFile(&get_args);
       return true;

   case cfd_sopendir:

       memset(buffer,0,CF_BUFSIZE);
       sscanf(recvbuffer,"SOPENDIR %d",&len);

       if (received != len+CF_PROTO_OFFSET)
          {
          CfOut(cf_verbose,"","Protocol error OPENDIR: %d\n",len);
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }

       if (conn->session_key == NULL)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }

       memcpy(out,recvbuffer+CF_PROTO_OFFSET,len);
       
       plainlen = DecryptString(out,recvbuffer,conn->session_key,len);

       if (strncmp(recvbuffer,"OPENDIR",7) !=0)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return true;
          }

       memset(filename,0,CF_BUFSIZE);
       sscanf(recvbuffer,"OPENDIR %[^\n]",filename);
       
       if (! conn->id_verified)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       if (!AccessControl(filename,conn,true)) /* opendir don't care about privacy */
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;   
          }       
       
       CfSecOpenDirectory(conn,sendbuffer,filename);
       return true;
       
   case cfd_opendir:

       memset(filename,0,CF_BUFSIZE);
       sscanf(recvbuffer,"OPENDIR %[^\n]",filename);
       
       if (! conn->id_verified)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       if (!AccessControl(filename,conn,true)) /* opendir don't care about privacy */
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;   
          }       
       
       CfOpenDirectory(conn,sendbuffer,filename);
       return true;
       
       
   case cfd_ssynch:
       
       memset(buffer,0,CF_BUFSIZE);
       sscanf(recvbuffer,"SSYNCH %d",&len);

       if (received != len+CF_PROTO_OFFSET)
          {
          CfOut(cf_verbose,"","Protocol error SSYNCH: %d\n",len);
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }

       if (conn->session_key == NULL)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }

       memcpy(out,recvbuffer+CF_PROTO_OFFSET,len);
       
       plainlen = DecryptString(out,recvbuffer,conn->session_key,len);

       if (strncmp(recvbuffer,"SYNCH",5) !=0)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return true;
          }
       /* roll through, no break */

   case cfd_synch:
       if (! conn->id_verified)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       
       memset(filename,0,CF_BUFSIZE);
       sscanf(recvbuffer,"SYNCH %ld STAT %[^\n]",&time_no_see,filename);
       
       trem = (time_t) time_no_see;
       
       if (time_no_see == 0 || filename[0] == '\0')
          {
          break;
          }
       
       if ((tloc = time((time_t *)NULL)) == -1)
          {
          sprintf(conn->output,"Couldn't read system clock\n");
          CfOut(cf_inform,"time",conn->output);
          SendTransaction(conn->sd_reply,"BAD: clocks out of synch",0,CF_DONE);
          return true;
          }
       
       drift = (int)(tloc-trem);
       
       if (!AccessControl(filename,conn,true))
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return true;   
          }
       
       if (DENYBADCLOCKS && (drift*drift > CLOCK_DRIFT*CLOCK_DRIFT))
          {
          CfOut(cf_inform,"","BAD: Clocks are too far unsynchronized %ld/%ld\n",(long)tloc,(long)trem);
          SendTransaction(conn->sd_reply,conn->output,0,CF_DONE);
          return true;
          }
       else
          {
          Debug("Clocks were off by %ld\n",(long)tloc-(long)trem);
          StatFile(conn,sendbuffer,filename);
          }
       
       return true;

   case cfd_smd5:
       memset(buffer,0,CF_BUFSIZE);
       sscanf(recvbuffer,"SMD5 %d",&len);
       
       if (received != len+CF_PROTO_OFFSET)
          {
          Debug("Decryption error: %d\n",len);
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return true;
          }
       
       memcpy(out,recvbuffer+CF_PROTO_OFFSET,len);
       plainlen = DecryptString(out,recvbuffer,conn->session_key,len);
       
       if (strncmp(recvbuffer,"MD5",3) !=0)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return false;
          }
       /* roll through, no break */
       
   case cfd_md5:
       if (! conn->id_verified)
          {
          RefuseAccess(conn,sendbuffer,0,recvbuffer);
          return true;
          }
       
       memset(filename,0,CF_BUFSIZE);
       memset(args,0,CF_BUFSIZE);
       
       CompareLocalHash(conn,sendbuffer,recvbuffer);
       return true;
       
   }
 
 sprintf (sendbuffer,"BAD: Request denied\n");
 SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
 CfOut(cf_inform,"","Closing connection\n"); 
 return false;
}

/**************************************************************/
/* Level 4                                                    */
/**************************************************************/

int MatchClasses(struct cfd_connection *conn)

{ char recvbuffer[CF_BUFSIZE];
  struct Item *classlist = NULL, *ip;
  int count = 0;

Debug("Match classes\n");

while (true && (count < 10))  /* arbitrary check to avoid infinite loop, DoS attack*/
   {
   count++;

   if (ReceiveTransaction(conn->sd_reply,recvbuffer,NULL) == -1)
      {
      if (errno == EINTR) 
         {
         continue;
         }
      }

   Debug("Got class buffer %s\n",recvbuffer);

   if (strncmp(recvbuffer,CFD_TERMINATOR,strlen(CFD_TERMINATOR)) == 0)
      {
      if (count == 1)
         {
         Debug("No classes were sent, assuming no restrictions...\n");
         return true;
         }
      
      break;
      }
   
   classlist = SplitStringAsItemList(recvbuffer,' ');
   
   for (ip = classlist; ip != NULL; ip=ip->next)
      {
      CfOut(cf_verbose,"","Checking whether class %s can be identified as me...\n",ip->name);
      
      if (IsDefinedClass(ip->name))
         {
         Debug("Class %s matched, accepting...\n",ip->name);
         DeleteItemList(classlist);
         return true;
         }

      if (IsRegexItemIn(VHEAP,ip->name))
         {
         Debug("Class matched regular expression %s, accepting...\n",ip->name);
         DeleteItemList(classlist);
         return true;
         }
      
      if (strncmp(ip->name,CFD_TERMINATOR,strlen(CFD_TERMINATOR)) == 0)
         {
         CfOut(cf_verbose,"","No classes matched, rejecting....\n");
         ReplyNothing(conn);
         DeleteItemList(classlist);
         return false;
         }
      }
   }
 
 ReplyNothing(conn);
 CfOut(cf_verbose,"","No classes matched, rejecting....\n");
 DeleteItemList(classlist);
 return false;
}

/******************************************************************/

void DoExec(struct cfd_connection *conn,char *sendbuffer,char *args)

{ char ebuff[CF_EXPANDSIZE], line[CF_BUFSIZE], *sp,rtype;
  int print = false,i, opt=false;
  void *rval;
  FILE *pp;

if ((CFSTARTTIME = time((time_t *)NULL)) == -1)
   {
   CfOut(cf_error,"time","Couldn't read system clock\n");
   }

if (strlen(CFRUNCOMMAND) == 0)
   {
   CfOut(cf_verbose,"","cf-serverd exec request: no cfruncommand defined\n");
   sprintf(sendbuffer,"Exec request: no cfruncommand defined\n");
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   return;
   }

CfOut(cf_verbose,"","Examining command string: %s\n",args);

for (sp = args; *sp != '\0'; sp++) /* Blank out -K -f */
   {
   if (OptionFound(args,sp,"-K")||OptionFound(args,sp,"-f"))
      {
      *sp = ' ';
      *(sp+1) = ' ';
      }
   else if (OptionFound(args,sp,"--no-lock"))
      {
      for (i = 0; i < strlen("--no-lock"); i++)
         {
         *(sp+i) = ' ';
         }
      }
   else if (OptionFound(args,sp,"--file"))
      {
      for (i = 0; i < strlen("--file"); i++)
         {
         *(sp+i) = ' ';
         }
      }
   else if (OptionFound(args,sp,"--define")||OptionFound(args,sp,"-D"))
      {
      CfOut(cf_verbose,"","Attempt to activate a predefined role..\n");
      
      if (!AuthorizeRoles(conn,sp))
         {
         sprintf(sendbuffer,"You are not authorized to activate these classes/roles on host %s\n",VFQNAME);
         SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
         return;
         }
      }   
   }

snprintf(ebuff,CF_BUFSIZE,"%s --inform",CFRUNCOMMAND);

if (strlen(ebuff)+strlen(args)+6 > CF_BUFSIZE)
   {
   snprintf(sendbuffer,CF_BUFSIZE,"Command line too long with args: %s\n",ebuff);
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   return;
   }
else
   {
   if ((args != NULL) & (strlen(args) > 0))
      {
      strcat(ebuff," ");
      strncat(ebuff,args,CF_BUFSIZE-strlen(ebuff));
      snprintf(sendbuffer,CF_BUFSIZE,"cfServerd Executing %s\n",ebuff);
      SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
      }
   }

CfOut(cf_inform,"","Executing command %s\n",ebuff);
 
if ((pp = cf_popen(ebuff,"r")) == NULL)
   {
   CfOut(cf_error,"pipe","Couldn't open pipe to command %s\n",ebuff);
   snprintf(sendbuffer,CF_BUFSIZE,"Unable to run %s\n",ebuff);
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   return;
   }

while (!feof(pp))
   {
   if (ferror(pp))
      {
      fflush(pp);
      break;
      }

   ReadLine(line,CF_BUFSIZE,pp);
   
   if (ferror(pp))
      {
      fflush(pp);
      break;
      }  

   print = false;
  
   for (sp = line; *sp != '\0'; sp++)
      {
      if (! isspace((int)*sp))
         {
         print = true;
         break;
         }
      }
   
   if (print)
      {
      snprintf(sendbuffer,CF_BUFSIZE,"%s\n",line);
      if (SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE) == -1)
         {
         CfOut(cf_error,"send","Sending failed, aborting");
         break;
         }
      }
  }
      
cf_pclose(pp);
}

/**************************************************************/

int GetCommand (char *str)

{ int i;
  char op[CF_BUFSIZE];

sscanf(str,"%4095s",op);

for (i = 0; PROTOCOL[i] != NULL; i++)
   {
   if (strcmp(op,PROTOCOL[i])==0)
      {
      return i;
      }
   }

return -1;
}

/*********************************************************************/

int VerifyConnection(struct cfd_connection *conn,char buf[CF_BUFSIZE])

 /* Try reverse DNS lookup
    and RFC931 username lookup to check the authenticity. */

{ char ipstring[CF_MAXVARSIZE], fqname[CF_MAXVARSIZE], username[CF_MAXVARSIZE];
  char dns_assert[CF_MAXVARSIZE],ip_assert[CF_MAXVARSIZE];
  int matched = false;
  struct passwd *pw;
#if defined(HAVE_GETADDRINFO)
  struct addrinfo query, *response=NULL, *ap;
  int err;
#else
  struct sockaddr_in raddr;
  int i,j,len = sizeof(struct sockaddr_in);
  struct hostent *hp = NULL;
  struct Item *ip_aliases = NULL, *ip_addresses = NULL;
#endif

Debug("Connecting host identifies itself as %s\n",buf);

memset(ipstring,0,CF_MAXVARSIZE);
memset(fqname,0,CF_MAXVARSIZE);
memset(username,0,CF_MAXVARSIZE); 

sscanf(buf,"%255s %255s %255s",ipstring,fqname,username);

Debug("(ipstring=[%s],fqname=[%s],username=[%s],socket=[%s])\n",ipstring,fqname,username,conn->ipaddr); 

strncpy(dns_assert,ToLowerStr(fqname),CF_MAXVARSIZE-1);
strncpy(ip_assert,ipstring,CF_MAXVARSIZE-1);

/* It only makes sense to check DNS by reverse lookup if the key had to be accepted
   on trust. Once we have a positive key ID, the IP address is irrelevant fr authentication...
   We can save a lot of time by not looking this up ... */
 
if ((conn->trust == false) || IsFuzzyItemIn(SKIPVERIFY,MapAddress(conn->ipaddr)))
   {
   CfOut(cf_verbose,"","Allowing %s to connect without (re)checking ID\n",ip_assert);
   CfOut(cf_verbose,"","Non-verified Host ID is %s (Using skipverify)\n",dns_assert);
   strncpy(conn->hostname,dns_assert,CF_MAXVARSIZE); 
   CfOut(cf_verbose,"","Non-verified User ID seems to be %s (Using skipverify)\n",username); 
   strncpy(conn->username,username,CF_MAXVARSIZE);

   if ((pw=getpwnam(username)) == NULL) /* Keep this inside mutex */
      {      
      printf("username was");
      conn->uid = -2;
      }
   else
      {
      conn->uid = pw->pw_uid;
      }

   LastSaw(dns_assert,cf_accept);
   return true;
   }
 
if (strcmp(ip_assert,MapAddress(conn->ipaddr)) != 0)
   {
   CfOut(cf_verbose,"","IP address mismatch between client's assertion (%s) and socket (%s) - untrustworthy connection\n",ip_assert,conn->ipaddr);
   return false;
   }

if (strlen(dns_assert) == 0)
   {
   CfOut(cf_verbose,"","DNS asserted name was empty - untrustworthy connection\n");
   return false;
   }

if (strcmp(dns_assert,"skipident") == 0)
   {
   CfOut(cf_verbose,"","DNS asserted name was withheld before key exchange - untrustworthy connection\n");
   return false;   
   }


CfOut(cf_verbose,"","Socket caller address appears honest (%s matches %s)\n",ip_assert,MapAddress(conn->ipaddr));
 
CfOut(cf_verbose,"","Socket originates from %s=%s\n",ip_assert,dns_assert);

Debug("Attempting to verify honesty by looking up hostname (%s)\n",dns_assert);

/* Do a reverse DNS lookup, like tcp wrappers to see if hostname matches IP */
 
#if defined(HAVE_GETADDRINFO)

 Debug("Using v6 compatible lookup...\n"); 

memset(&query,0,sizeof(struct addrinfo));
 
query.ai_family = AF_UNSPEC;
query.ai_socktype = SOCK_STREAM;
query.ai_flags = AI_PASSIVE;
 
if ((err=getaddrinfo(dns_assert,NULL,&query,&response)) != 0)
   {
   CfOut(cf_error,"","Unable to lookup %s (%s)",dns_assert,gai_strerror(err));
   }
 
for (ap = response; ap != NULL; ap = ap->ai_next)
   {
   Debug("CMP: %s %s\n",MapAddress(conn->ipaddr),sockaddr_ntop(ap->ai_addr));
   
   if (strcmp(MapAddress(conn->ipaddr),sockaddr_ntop(ap->ai_addr)) == 0)
      {
      Debug("Found match\n");
      matched = true;
      }
   }

if (response != NULL)
   {
   freeaddrinfo(response);
   }

#else 

Debug("IPV4 hostnname lookup on %s\n",dns_assert);

# ifdef HAVE_PTHREAD_H  
 if (pthread_mutex_lock(&MUTEX_HOSTNAME) != 0)
    {
    CfOut(cf_error,"unlock","pthread_mutex_lock failed");
    exit(1);
    }
# endif
 
if ((hp = gethostbyname(dns_assert)) == NULL)
   {
   CfOut(cf_verbose,"","cfServerd Couldn't look up name %s\n",fqname);
   CfOut(cf_log,"gethostbyname","DNS lookup of %s failed",dns_assert);
   matched = false;
   }
else
   {
   matched = true;

   Debug("Looking for the peername of our socket...\n");
   
   if (getpeername(conn->sd_reply,(struct sockaddr *)&raddr,&len) == -1)
      {
      CfOut(cf_error,"getpeername","Couldn't get socket address\n");
      matched = false;
      }
   
   CfOut(cf_verbose,"","Looking through hostnames on socket with IPv4 %s\n",sockaddr_ntop((struct sockaddr *)&raddr));
   
   for (i = 0; hp->h_addr_list[i]; i++)
      {
      CfOut(cf_verbose,"","Reverse lookup address found: %d\n",i);
      if (memcmp(hp->h_addr_list[i],(char *)&(raddr.sin_addr),sizeof(raddr.sin_addr)) == 0)
         {
         CfOut(cf_verbose,"","Canonical name matched host's assertion - id confirmed as %s\n",dns_assert);
         break;
         }
      }
   
   if (hp->h_addr_list[0] != NULL)
      {
      CfOut(cf_verbose,"","Checking address number %d for non-canonical names (aliases)\n",i);
      for (j = 0; hp->h_aliases[j] != NULL; j++)
         {
         CfOut(cf_verbose,"","Comparing [%s][%s]\n",hp->h_aliases[j],ip_assert);
         if (strcmp(hp->h_aliases[j],ip_assert) == 0)
            {
            CfOut(cf_verbose,"","Non-canonical name (alias) matched host's assertion - id confirmed as %s\n",dns_assert);
            break;
            }
         }
      
      if ((hp->h_addr_list[i] != NULL) && (hp->h_aliases[j] != NULL))
         {
         CfOut(cf_log,"","Reverse hostname lookup failed, host claiming to be %s was %s\n",buf,sockaddr_ntop((struct sockaddr *)&raddr));
         matched = false;
         }
      else
         {
         CfOut(cf_verbose,"","Reverse lookup succeeded\n");
         }
      }
   else
      {
      CfOut(cf_log,"","No name was registered in DNS for %s - reverse lookup failed\n",dns_assert);
      matched = false;
      }   
   }
 
 
 if ((pw=getpwnam(username)) == NULL) /* Keep this inside mutex */
    {
    
    printf("username was");
    conn->uid = -2;
    }
 else
    {
    conn->uid = pw->pw_uid;
    }
 
 
# ifdef HAVE_PTHREAD_H  
 if (pthread_mutex_unlock(&MUTEX_HOSTNAME) != 0)
    {
    CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
    exit(1);
    }
# endif

#endif

if (!matched)
   {
   CfOut(cf_log,"gethostbyname","Failed on DNS reverse lookup of %s\n",dns_assert);
   CfOut(cf_log,"","Client sent: %s",buf);
   return false;
   }
 
CfOut(cf_verbose,"","Host ID is %s\n",dns_assert);
strncpy(conn->hostname,dns_assert,CF_MAXVARSIZE-1);
LastSaw(dns_assert,cf_accept); 
 
CfOut(cf_verbose,"","User ID seems to be %s\n",username); 
strncpy(conn->username,username,CF_MAXVARSIZE-1);
 
return true;   
}


/**************************************************************/

int AllowedUser(char *user)

{
if (IsItemIn(ALLOWUSERLIST,user))
   {
   CfOut(cf_verbose,"","User %s granted connection privileges\n",user);
   return true;
   }

CfOut(cf_verbose,"","User %s is not allowed on this server\n",user); 
return false;
}

/**************************************************************/

int AccessControl(char *filename,struct cfd_connection *conn,int encrypt)

{ struct Auth *ap;
  int access = false;
  char realname[CF_BUFSIZE],path[CF_BUFSIZE],lastnode[CF_BUFSIZE],*sp;
  struct stat statbuf;

Debug("AccessControl(%s)\n",filename);
memset(realname,0,CF_BUFSIZE);

/* Separate path first, else this breaks for lastnode = symlink */

for (sp = filename; *sp != '\0'; sp++) /*fix filename to not have windows slashes */
   {
   if (*sp == FILE_SEPARATOR)
      {
      *sp = '/';
      }
   }

strncpy(path,filename,CF_BUFSIZE-1);
strncpy(lastnode,ReadLastNode(filename),CF_BUFSIZE-1);
ChopLastNode(path);

/* Eliminate links from path */

#ifdef HAVE_REALPATH
if (realpath(path,realname) == NULL)
   {
   CfOut(cf_verbose,"lstat","Couldn't resolve filename %s from host %s\n",filename,conn->hostname);
   return false;
   }
#else
CompressPath(realname,path); /* in links.c */
#endif

/* Rejoin the last node and stat the real thing */

AddSlash(realname);
strcat(realname,lastnode);

if (lstat(realname,&statbuf) == -1)
   {
   CfOut(cf_verbose,"lstat","Couldn't stat filename %s (i.e. %s) from host %s\n",filename,realname,conn->hostname);
   return false;
   }

Debug("AccessControl, match(%s,%s) encrypt request=%d\n",realname,conn->hostname,encrypt);
 
if (VADMIT == NULL)
   {
   CfOut(cf_verbose,"","cfServerd access list is empty, no files are visible\n");
   return false;
   }
 
conn->maproot = false;
 
for (ap = VADMIT; ap != NULL; ap=ap->next)
   {
   int res = false;
   Debug("Examining rule in access list (%s,%s)?\n",realname,ap->path);
      
   if ((strlen(realname) > strlen(ap->path))  && strncmp(ap->path,realname,strlen(ap->path)) == 0 && realname[strlen(ap->path)] == '/')
      {
      res = true;    /* Substring means must be a / to link, else just a substring og filename */
      }

   if (strcmp(ap->path,realname) == 0)
      {
      res = true;    /* Exact match means single file to admit */
      }
   
   if (res)
      {
      CfOut(cf_verbose,"","Found a matching rule in access list (%s in %s)\n",realname,ap->path);

      if (stat(ap->path,&statbuf) == -1)
         {
         CfOut(cf_log,"","Warning cannot stat file object %s in admit/grant, or access list refers to dangling link\n",ap->path);
         continue;
         }
      
      if (!encrypt && (ap->encrypt == true))
         {
         CfOut(cf_error,"","File %s requires encrypt connection...will not serve\n",ap->path);
         access = false;
         }
      else
         {
         Debug("Checking whether to map root privileges..\n");
         
         if (IsRegexItemIn(ap->maproot,conn->hostname) ||
             IsRegexItemIn(ap->maproot,MapAddress(conn->ipaddr)) ||
             IsFuzzyItemIn(ap->maproot,MapAddress(conn->ipaddr)))
            {
            conn->maproot = true;
            CfOut(cf_verbose,"","Mapping root privileges\n");
            }
         else
            {
            CfOut(cf_verbose,"","No root privileges granted\n");
            }
         
         if (IsRegexItemIn(ap->accesslist,conn->hostname) ||
             IsRegexItemIn(ap->accesslist,MapAddress(conn->ipaddr)) ||
             IsFuzzyItemIn(ap->accesslist,MapAddress(conn->ipaddr)))
            {
            access = true;
            Debug("Access privileges - match found\n");
            }
         }
      break;
      }
   }
 
 for (ap = VDENY; ap != NULL; ap=ap->next)
    {
    if (strncmp(ap->path,realname,strlen(ap->path)) == 0)
       {
       if (IsRegexItemIn(ap->accesslist,conn->hostname) ||
           IsRegexItemIn(ap->accesslist,MapAddress(conn->ipaddr)) ||
           IsFuzzyItemIn(ap->accesslist,MapAddress(conn->ipaddr)))
          {
          access = false;
          CfOut(cf_verbose,"","Host %s explicitly denied access to %s\n",conn->hostname,realname);
          break;
          }
       }
    }
 
 if (access)
    {
    CfOut(cf_verbose,"","Host %s granted access to %s\n",conn->hostname,realname);
    
    if (encrypt && LOGENCRYPT)
       {
       /* Log files that were marked as requiring encryption */
       CfOut(cf_log,"","Host %s granted access to %s\n",conn->hostname,realname);
       }
    }
 else
    {
    CfOut(cf_verbose,"","Host %s denied access to %s\n",conn->hostname,realname);
    }
 
 if (!conn->rsa_auth)
    {
    CfOut(cf_verbose,"","Cannot map root access without RSA authentication");
    conn->maproot = false; /* only public files accessible */
    /* return false; */
    }
 
 return access;
}

/**************************************************************/

int AuthorizeRoles(struct cfd_connection *conn,char *args)

{ char *sp;
  struct Auth *ap;
  char userid1[CF_MAXVARSIZE],userid2[CF_MAXVARSIZE];
  struct Rlist *rp,*defines = NULL;
  int permitted = false;
  
snprintf(userid1,CF_MAXVARSIZE,"%s@%s",conn->username,conn->hostname);
snprintf(userid2,CF_MAXVARSIZE,"%s@%s",conn->username,conn->ipaddr);

CfOut(cf_verbose,"","Checking authorized roles in %s\n",args);

if (strncmp(args,"--define",strlen("--define")) == 0)
   {
   sp = args + strlen("--define");
   }
else
   {
   sp = args + strlen("-D");
   }

while (*sp == ' ')
   {
   sp++;
   }

defines = SplitRegexAsRList(sp,"[,:;]",99,false);

/* For each user-defined class attempt, check RBAC */

for (rp = defines; rp != NULL; rp = rp->next)
   {
   CfOut(cf_verbose,""," -> Verifying %s\n",rp->item);
   
   for (ap = ROLES; ap != NULL; ap=ap->next)
      {
      if (FullTextMatch(ap->path,rp->item))
         {
         /* We have a pattern covering this class - so are we allowed to activate it? */
         if (IsRegexItemIn(ap->accesslist,conn->hostname) ||
             IsRegexItemIn(ap->accesslist,MapAddress(conn->ipaddr)) ||
             IsRegexItemIn(ap->accesslist,MapAddress(userid1)) ||
             IsRegexItemIn(ap->accesslist,MapAddress(userid2)) ||
             IsRegexItemIn(ap->accesslist,MapAddress(conn->username))
             )
            {
            CfOut(cf_verbose,"","Attempt to define role/class %s is permitted\n",rp->item);
            permitted = true;
            }
         else
            {
            CfOut(cf_verbose,"","Attempt to define role/class %s is denied\n",rp->item);
            DeleteRlist(defines);
            return false;
            }
         }
      }
   
   }

if (permitted)
   {
   CfOut(cf_verbose,"","Role activation allowed\n");
   }
else
   {
   CfOut(cf_verbose,"","Role activation disallowed - abort execution\n");
   }

DeleteRlist(defines);
return permitted;
}

/**************************************************************/

int AuthenticationDialogue(struct cfd_connection *conn,char *recvbuffer, int recvlen)

{ char in[CF_BUFSIZE],*out, *decrypted_nonce;
  BIGNUM *counter_challenge = NULL;
  unsigned char digest[EVP_MAX_MD_SIZE+1];
  unsigned int crypt_len, nonce_len = 0,encrypted_len = 0;
  char sauth[10], iscrypt ='n';
  int len = 0,keylen;
  unsigned long err;
  RSA *newkey;

if (PRIVKEY == NULL || PUBKEY == NULL)
   {
   CfOut(cf_error,"","No public/private key pair exists, create one with cfkey\n");
   return false;
   }
 
/* proposition C1 */
/* Opening string is a challenge from the client (some agent) */

sauth[0] = '\0';

sscanf(recvbuffer,"%s %c %d %d",sauth,&iscrypt,&crypt_len,&nonce_len);

if (crypt_len == 0 || nonce_len == 0 || strlen(sauth) == 0)
   {
   CfOut(cf_inform,"","Protocol format error in authentation from IP %s\n",conn->hostname);
   return false;
   }

if (nonce_len > CF_NONCELEN*2)
   {
   CfOut(cf_inform,"","Protocol deviant authentication nonce from %s\n",conn->hostname);
   return false;   
   }

if (crypt_len > 2*CF_NONCELEN)
   {
   CfOut(cf_inform,"","Protocol abuse in unlikely cipher from %s\n",conn->hostname);
   return false;      
   }

/* Check there is no attempt to read past the end of the received input */

if (recvbuffer+CF_RSA_PROTO_OFFSET+nonce_len > recvbuffer+recvlen)
   {
   CfOut(cf_inform,"","Protocol consistency error in authentation from IP %s\n",conn->hostname);
   return false;   
   }

if ((strcmp(sauth,"SAUTH") != 0) || (nonce_len == 0) || (crypt_len == 0))
   {
   CfOut(cf_inform,"","Protocol error in RSA authentation from IP %s\n",conn->hostname);
   return false;
   }

Debug("Challenge encryption = %c, nonce = %d, buf = %d\n",iscrypt,nonce_len,crypt_len);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
 if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)
    {
    CfOut(cf_error,"lock","pthread_mutex_lock failed");
    }
#endif
 
if ((decrypted_nonce = malloc(crypt_len)) == NULL)
   {
   FatalError("memory failure");
   }
 
if (iscrypt == 'y')
   { 
   if (RSA_private_decrypt(crypt_len,recvbuffer+CF_RSA_PROTO_OFFSET,decrypted_nonce,PRIVKEY,RSA_PKCS1_PADDING) <= 0)
      {
      err = ERR_get_error();

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
      if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
         {
         CfOut(cf_error,"lock","pthread_mutex_unlock failed");
         }
#endif 

      CfOut(cf_error,"","Private decrypt failed = %s\n",ERR_reason_error_string(err));
      free(decrypted_nonce);
      return false;
      }
   }
 else
    {
    if (nonce_len > crypt_len)
       {
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
       if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
          {
          CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
          }
#endif 

       CfOut(cf_error,"","Illegal challenge\n");
       free(decrypted_nonce);
       return false;       
       }
    
    memcpy(decrypted_nonce,recvbuffer+CF_RSA_PROTO_OFFSET,nonce_len);  
    }
 
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
 if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
    {
    CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
    }
#endif
 
/* Client's ID is now established by key or trusted, reply with md5 */
 
HashString(decrypted_nonce,nonce_len,digest,cf_md5);
free(decrypted_nonce);
 
/* Get the public key from the client */
 
 
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
 if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)
    {
    CfOut(cf_error,"lock","pthread_mutex_lock failed");
    }
#endif
 
 newkey = RSA_new();
 
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
 if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
    {
    CfOut(cf_error,"unlock","pthread_mutex_lock failed");
    }
#endif 
 
 
/* proposition C2 */ 
 if ((len = ReceiveTransaction(conn->sd_reply,recvbuffer,NULL)) == -1)
    {
    CfOut(cf_inform,"","Protocol error 1 in RSA authentation from IP %s\n",conn->hostname);
    RSA_free(newkey);
    return false;
    }
 
 if (len == 0)
    {
    CfOut(cf_inform,"","Protocol error 2 in RSA authentation from IP %s\n",conn->hostname);
    RSA_free(newkey);
    return false;
    }
 
 if ((newkey->n = BN_mpi2bn(recvbuffer,len,NULL)) == NULL)
    {
    err = ERR_get_error();
    CfOut(cf_error,"","Private decrypt failed = %s\n",ERR_reason_error_string(err));
    RSA_free(newkey);
    return false;
    }
 
/* proposition C3 */ 
 
 if ((len=ReceiveTransaction(conn->sd_reply,recvbuffer,NULL)) == -1)
    {
    CfOut(cf_inform,"","Protocol error 3 in RSA authentation from IP %s\n",conn->hostname);
    RSA_free(newkey);
    return false;
    }
 
 if (len == 0)
    {
    CfOut(cf_inform,"","Protocol error 4 in RSA authentation from IP %s\n",conn->hostname);
    RSA_free(newkey);
    return false;
    }
 
 if ((newkey->e = BN_mpi2bn(recvbuffer,len,NULL)) == NULL)
    {
    err = ERR_get_error();
    CfOut(cf_error,"","Private decrypt failed = %s\n",ERR_reason_error_string(err));
    RSA_free(newkey);
    return false;
    }
 
 if (DEBUG||D2)
    {
    RSA_print_fp(stdout,newkey,0);
    }
 
 if (!CheckStoreKey(conn,newkey))    /* conceals proposition S1 */
    {
    if (!conn->trust)
       {
       RSA_free(newkey);       
       return false;
       }
    }
 
/* Reply with md5 of original challenge */

/* proposition S2 */ 
SendTransaction(conn->sd_reply,digest,16,CF_DONE);
 
/* Send counter challenge to be sure this is a live session */

counter_challenge = BN_new();
BN_rand(counter_challenge,256,0,0);
nonce_len = BN_bn2mpi(counter_challenge,in);
HashString(in,nonce_len,digest,cf_md5);
encrypted_len = RSA_size(newkey);         /* encryption buffer is always the same size as n */ 

 
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)
   {
   CfOut(cf_error,"lock","pthread_mutex_lock failed");
   }
#endif
 
if ((out = malloc(encrypted_len+1)) == NULL)
   {
   FatalError("memory failure");
   }

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
   {
   CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
   }
#endif

 
if (RSA_public_encrypt(nonce_len,in,out,newkey,RSA_PKCS1_PADDING) <= 0)
   {
   err = ERR_get_error();
   CfOut(cf_error,"","Public encryption failed = %s\n",ERR_reason_error_string(err));
   RSA_free(newkey);
   free(out);
   return false;
   }

/* proposition S3 */ 
SendTransaction(conn->sd_reply,out,encrypted_len,CF_DONE);

/* if the client doesn't have our public key, send it */
 
if (iscrypt != 'y')
   {
   /* proposition S4  - conditional */
   memset(in,0,CF_BUFSIZE); 
   len = BN_bn2mpi(PUBKEY->n,in);
   SendTransaction(conn->sd_reply,in,len,CF_DONE);

   /* proposition S5  - conditional */
   memset(in,0,CF_BUFSIZE);  
   len = BN_bn2mpi(PUBKEY->e,in); 
   SendTransaction(conn->sd_reply,in,len,CF_DONE); 
   }

/* Receive reply to counter_challenge */

/* proposition C4 */ 
memset(in,0,CF_BUFSIZE);

if (ReceiveTransaction(conn->sd_reply,in,NULL) == -1)
   {
   BN_free(counter_challenge);
   free(out);
   RSA_free(newkey);   
   return false;
   }
 
if (!HashesMatch(digest,in,cf_md5))  /* replay / piggy in the middle attack ? */
   {
   BN_free(counter_challenge);
   free(out);
   RSA_free(newkey);
   CfOut(cf_inform,"","Challenge response from client %s was incorrect - ID false?",conn->ipaddr);
   return false; 
   }
else
   {
   if (!conn->trust)
      {
      CfOut(cf_verbose,"","Strong authentication of client %s/%s achieved",conn->hostname,conn->ipaddr);
      }
   else
      {
      CfOut(cf_verbose,"","Weak authentication of trusted client %s/%s (key accepted on trust).\n",conn->hostname,conn->ipaddr);
      }
   }

/* Receive random session key, blowfish style ... */ 

/* proposition C5 */

memset(in,0,CF_BUFSIZE);

if ((keylen = ReceiveTransaction(conn->sd_reply,in,NULL)) == -1)
   {
   BN_free(counter_challenge);
   free(out);
   RSA_free(newkey); 
   return false;
   }

if (keylen > CF_BUFSIZE/2)
   {
   BN_free(counter_challenge);
   free(out);
   RSA_free(newkey); 
   CfOut(cf_inform,"","Session key length received from %s is too long",conn->ipaddr);
   return false;
   }
else
   {
   Debug("Got Blowfish size %d\n",keylen);
   DebugBinOut(in,keylen);
   }

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)
   {
   CfOut(cf_error,"lock","pthread_mutex_lock failed");
   }
#endif
 
conn->session_key = malloc(CF_BLOWFISHSIZE); 

if (conn->session_key == NULL)
   {
   BN_free(counter_challenge);
   free(out);
   RSA_free(newkey); 
   return false;
   }

if (keylen == CF_BLOWFISHSIZE) /* Old, non-encrypted */
   {
   memcpy(conn->session_key,in,CF_BLOWFISHSIZE);
   }
else /* New encrypted */
   {
   if (RSA_private_decrypt(keylen,in,out,PRIVKEY,RSA_PKCS1_PADDING) <= 0)
      {
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
      if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
         {
         CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
         }
#endif
      
      err = ERR_get_error();
      CfOut(cf_error,"","Private decrypt failed = %s\n",ERR_reason_error_string(err));
      return false;
      }
   
   memcpy(conn->session_key,out,CF_BLOWFISHSIZE);
   }

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
   if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
      {
      CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
      }
#endif

Debug("Got a session key...\n"); 
DebugBinOut(conn->session_key,16);

BN_free(counter_challenge);
free(out);
RSA_free(newkey); 
conn->rsa_auth = true;

return true; 
}

/**************************************************************/

int StatFile(struct cfd_connection *conn,char *sendbuffer,char *filename)


/* Because we do not know the size or structure of remote datatypes,*/
/* the simplest way to transfer the data is to convert them into */
/* plain text and interpret them on the other side. */

{ struct cfstat cfst;
 struct stat statbuf,statlinkbuf;
  char linkbuf[CF_BUFSIZE];
  int islink = false;

Debug("StatFile(%s)\n",filename);

memset(&cfst,0,sizeof(struct cfstat));
  
if (strlen(ReadLastNode(filename)) > CF_MAXLINKSIZE)
   {
   snprintf(sendbuffer,CF_BUFSIZE*2,"BAD: Filename suspiciously long [%s]\n",filename);
   CfOut(cf_error,"",sendbuffer);
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   return -1;
   }

if (lstat(filename,&statbuf) == -1)
   {
   snprintf(sendbuffer,CF_BUFSIZE,"BAD: unable to stat file %s",filename);
   CfOut(cf_verbose,"lstat",sendbuffer);
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   return -1;
   }

cfst.cf_readlink = NULL;
cfst.cf_lmode = 0;
cfst.cf_nlink = CF_NOSIZE;

memset(linkbuf,0,CF_BUFSIZE);

if (S_ISLNK(statbuf.st_mode))
   {
   islink = true;
   cfst.cf_type = cf_link;                   /* pointless - overwritten */
   cfst.cf_lmode = statbuf.st_mode & 07777;
   cfst.cf_nlink = statbuf.st_nlink;
       
   if (readlink(filename,linkbuf,CF_BUFSIZE-1) == -1)
      {
      sprintf(sendbuffer,"BAD: unable to read link\n");
      CfOut(cf_error,"readlink",sendbuffer);
      SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
      return -1;
      }

   Debug("readlink: %s\n",linkbuf);

   cfst.cf_readlink = linkbuf;
   }

if (!islink && (stat(filename,&statbuf) == -1))
   {
   CfOut(cf_verbose,"stat","BAD: unable to stat file %s\n",filename);
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   return -1;
   }

Debug("Getting size of link deref %s\n",linkbuf);

if (islink && (stat(filename,&statlinkbuf) != -1)) /* linktype=copy used by agent */
   {
   statbuf.st_size = statlinkbuf.st_size;
   statbuf.st_mode = statlinkbuf.st_mode;
   statbuf.st_uid = statlinkbuf.st_uid;
   statbuf.st_gid = statlinkbuf.st_gid;
   statbuf.st_mtime = statlinkbuf.st_mtime;
   statbuf.st_ctime = statlinkbuf.st_ctime;
   }

if (S_ISDIR(statbuf.st_mode))
   {
   cfst.cf_type = cf_dir;
   }

if (S_ISREG(statbuf.st_mode))
   {
   cfst.cf_type = cf_reg;
   }

if (S_ISSOCK(statbuf.st_mode))
   {
   cfst.cf_type = cf_sock;
   }

if (S_ISCHR(statbuf.st_mode))
   {
   cfst.cf_type = cf_char;
   }

if (S_ISBLK(statbuf.st_mode))
   {
   cfst.cf_type = cf_block;
   }

if (S_ISFIFO(statbuf.st_mode))
   {
   cfst.cf_type = cf_fifo;
   }

cfst.cf_mode     = statbuf.st_mode  & 07777;
cfst.cf_uid      = statbuf.st_uid   & 0xFFFFFFFF;
cfst.cf_gid      = statbuf.st_gid   & 0xFFFFFFFF;
cfst.cf_size     = statbuf.st_size;
cfst.cf_atime    = statbuf.st_atime;
cfst.cf_mtime    = statbuf.st_mtime;
cfst.cf_ctime    = statbuf.st_ctime;
cfst.cf_ino      = statbuf.st_ino;
cfst.cf_dev      = statbuf.st_dev;
cfst.cf_readlink = linkbuf;

if (cfst.cf_nlink == CF_NOSIZE)
   {
   cfst.cf_nlink = statbuf.st_nlink;
   }

#ifndef IRIX
if (statbuf.st_size > statbuf.st_blocks * DEV_BSIZE)
#else
# ifdef HAVE_ST_BLOCKS
if (statbuf.st_size > statbuf.st_blocks * DEV_BSIZE)
# else
if (statbuf.st_size > ST_NBLOCKS(statbuf) * DEV_BSIZE)
# endif
#endif
   {
   cfst.cf_makeholes = 1;   /* must have a hole to get checksum right */
   }
else
   {
   cfst.cf_makeholes = 0;
   }


memset(sendbuffer,0,CF_BUFSIZE);

 /* send as plain text */

Debug("OK: type=%d\n mode=%o\n lmode=%o\n uid=%d\n gid=%d\n size=%ld\n atime=%d\n mtime=%d\n",
 cfst.cf_type,cfst.cf_mode,cfst.cf_lmode,cfst.cf_uid,cfst.cf_gid,(long)cfst.cf_size,
 cfst.cf_atime,cfst.cf_mtime);


snprintf(sendbuffer,CF_BUFSIZE,"OK: %d %d %d %d %d %ld %d %d %d %d %d %d %d",
 cfst.cf_type,cfst.cf_mode,cfst.cf_lmode,cfst.cf_uid,cfst.cf_gid,(long)cfst.cf_size,
 cfst.cf_atime,cfst.cf_mtime,cfst.cf_ctime,cfst.cf_makeholes,cfst.cf_ino,
  cfst.cf_nlink,cfst.cf_dev);

SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);

memset(sendbuffer,0,CF_BUFSIZE);

if (cfst.cf_readlink != NULL)
   {
   strcpy(sendbuffer,"OK:");
   strcat(sendbuffer,cfst.cf_readlink);
   }
else
   {
   sprintf(sendbuffer,"OK:");
   }

SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
return 0;
}

/***************************************************************/

void CfGetFile(struct cfd_get_arg *args)

{ int sd,fd,n_read,total=0,cipherlen,sendlen=0,count = 0;
  char sendbuffer[CF_BUFSIZE+1],out[CF_BUFSIZE],*filename;
  struct stat statbuf;
  uid_t uid;
  unsigned char iv[] = {1,2,3,4,5,6,7,8}, *key;
  EVP_CIPHER_CTX ctx;

sd         = (args->connect)->sd_reply;
filename   = args->replyfile;
key        = (args->connect)->session_key;
uid        = (args->connect)->uid;

stat(filename,&statbuf);
Debug("CfGetFile(%s on sd=%d), size=%d\n",filename,sd,statbuf.st_size);

/* Now check to see if we have remote permission */

if (uid != 0 && !args->connect->maproot) /* should remote root be local root */
   {
   if (statbuf.st_uid == uid)
      {
      Debug("Caller %s is the owner of the file\n",(args->connect)->username);
      }
   else
      {
      /* We are not the owner of the file and we don't care about groups */
      if (statbuf.st_mode & S_IROTH)
         {
         Debug("Caller %s not owner of the file but permission granted\n",(args->connect)->username);
         }
      else
         {
         Debug("Caller %s is not the owner of the file\n",(args->connect)->username);
         RefuseAccess(args->connect,sendbuffer,args->buf_size,"");
         CfOut(cf_error,"open","Open error of file [%s]\n",filename);         
         snprintf(sendbuffer,CF_BUFSIZE,"%s",CF_FAILEDSTR);
         SendSocketStream(sd,sendbuffer,args->buf_size,0);
         return;
         }
      }
   }
 
 if (args->buf_size < SMALL_BLOCK_BUF_SIZE)
    {
    CfOut(cf_error,"","blocksize for %s was only %d\n",filename,args->buf_size);
    }
 
 if (args->encrypt)
    {
    EVP_CIPHER_CTX_init(&ctx);
    EVP_EncryptInit(&ctx,EVP_bf_cbc(),key,iv);    
    }
 
 if ((fd = SafeOpen(filename)) == -1)
    {
    CfOut(cf_error,"open","Open error of file [%s]\n",filename);
    snprintf(sendbuffer,CF_BUFSIZE,"%s",CF_FAILEDSTR);
    SendSocketStream(sd,sendbuffer,args->buf_size,0);
    }
 else
    {
    while(true)
       {
       memset(sendbuffer,0,CF_BUFSIZE);
       
       Debug("Now reading from disk...\n");
       
       if ((n_read = read(fd,sendbuffer,args->buf_size)) == -1)
          {
          CfOut(cf_error,"read","read failed in GetFile");
          break;
          }
       
       Debug("Read completed..\n");
       
       if (strncmp(sendbuffer,CF_FAILEDSTR,strlen(CF_FAILEDSTR)) == 0)
          {
          Debug("SENT FAILSTRING BY MISTAKE!\n");
          }
       
       if (n_read == 0)
          {
          break;
          }
       else
          { int savedlen = statbuf.st_size;
          
          /* This can happen with log files /databases etc */
          
          if (count++ % 3 == 0) /* Don't do this too often */
             {
             Debug("Restatting %s\n",filename);
             stat(filename,&statbuf);
             }
          
          if (statbuf.st_size != savedlen)
             {
             snprintf(sendbuffer,CF_BUFSIZE,"%s%s: %s",CF_CHANGEDSTR1,CF_CHANGEDSTR2,filename);
             if (SendSocketStream(sd,sendbuffer,args->buf_size,0) == -1)
                {
                CfOut(cf_verbose,"send","Send failed in GetFile");
                }
             
             Debug("Aborting transfer after %d: file is changing rapidly at source.\n",total);
             break;
             }
          
          if ((savedlen - total)/args->buf_size > 0)
             {
             sendlen = args->buf_size;
             }
          else if (savedlen != 0)
             {
             sendlen = (savedlen - total);
             }
          }
       
       total += n_read;
       
       if (args->encrypt)
          {
          if (!EVP_EncryptUpdate(&ctx,out,&cipherlen,sendbuffer,n_read))
             {
             close(fd);
             return;
             }
          
          if (cipherlen)
             {
             if (SendTransaction(sd,out,cipherlen,CF_MORE) == -1)
                {
                CfOut(cf_verbose,"send","Send failed in GetFile");
                break;
                }
             }
          }
       else
          {
          Debug("Sending data on socket (%d)\n",sendlen);
          
          if (SendSocketStream(sd,sendbuffer,sendlen,0) == -1)
             {
             CfOut(cf_verbose,"send","Send failed in GetFile");
             break;
             }
          
          Debug("Sending complete...\n");
          }     
       }
    
    if (args->encrypt)
       {
       if (!EVP_EncryptFinal(&ctx,out,&cipherlen))
          {
          close(fd);
          return;
          }
       
       Debug("Cipher len of extra data is %d\n",cipherlen);
       SendTransaction(sd,out,cipherlen,CF_DONE);
       EVP_CIPHER_CTX_cleanup(&ctx);
       }
    
    close(fd);    
    }
 
 Debug("Done with GetFile()\n"); 
}

/**************************************************************/

void CompareLocalHash(struct cfd_connection *conn,char *sendbuffer,char *recvbuffer)

{ unsigned char digest[EVP_MAX_MD_SIZE+1];
  char filename[CF_BUFSIZE];
  struct Promise *pp = NewPromise("server_cfengine","the server daemon");
  struct Attributes attr;
  char *sp;
  int i;

/* TODO - when safe change this to sha2 */
  
sscanf(recvbuffer,"MD5 %[^\n]",filename);

sp = recvbuffer + strlen(recvbuffer) + CF_SMALL_OFFSET;
 
for (i = 0; i < CF_MD5_LEN; i++)
   {
   digest[i] = *sp++;
   }
 
Debug("CompareHashes(%s,%s)\n",filename,HashPrint(cf_md5,digest));
memset(sendbuffer,0,CF_BUFSIZE);

if (FileHashChanged(filename,digest,cf_verbose,cf_md5,attr,pp))
   {
   sprintf(sendbuffer,"%s",CFD_TRUE);
   Debug("Hashes didn't match\n");
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   }
else
   {
   sprintf(sendbuffer,"%s",CFD_FALSE);
   Debug("Hashes matched ok\n");
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   }

DeletePromise(pp);
}

/**************************************************************/

int CfOpenDirectory(struct cfd_connection *conn,char *sendbuffer,char *dirname)

{ DIR *dirh;
  struct dirent *dirp;
  int offset;

Debug("CfOpenDirectory(%s)\n",dirname);
  
if (*dirname != '/')
   {
   sprintf(sendbuffer,"BAD: request to access a non-absolute filename\n");
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   return -1;
   }

if ((dirh = opendir(dirname)) == NULL)
   {
   Debug("cfengine, couldn't open dir %s\n",dirname);
   snprintf(sendbuffer,CF_BUFSIZE,"BAD: cfengine, couldn't open dir %s\n",dirname);
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   return -1;
   }

/* Pack names for transmission */

memset(sendbuffer,0,CF_BUFSIZE);

offset = 0;

for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
   {
   if (strlen(dirp->d_name)+1+offset >= CF_BUFSIZE - CF_MAXLINKSIZE)
      {
      SendTransaction(conn->sd_reply,sendbuffer,offset+1,CF_MORE);
      offset = 0;
      memset(sendbuffer,0,CF_BUFSIZE);
      }

   strncpy(sendbuffer+offset,dirp->d_name,CF_MAXLINKSIZE);
   offset += strlen(dirp->d_name) + 1;     /* + zero byte separator */
   }
 
strcpy(sendbuffer+offset,CFD_TERMINATOR);
SendTransaction(conn->sd_reply,sendbuffer,offset+2+strlen(CFD_TERMINATOR),CF_DONE);
Debug("END CfOpenDirectory(%s)\n",dirname);
closedir(dirh);
return 0;
}

/**************************************************************/

int CfSecOpenDirectory(struct cfd_connection *conn,char *sendbuffer,char *dirname)

{ DIR *dirh;
  struct dirent *dirp;
  int offset,cipherlen;
  char out[CF_BUFSIZE];

Debug("CfSecOpenDirectory(%s)\n",dirname);
  
if (*dirname != '/')
   {
   sprintf(sendbuffer,"BAD: request to access a non-absolute filename\n");
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   return -1;
   }

if ((dirh = opendir(dirname)) == NULL)
   {
   Debug("cfengine, couldn't open dir %s\n",dirname);
   snprintf(sendbuffer,CF_BUFSIZE,"BAD: cfengine, couldn't open dir %s\n",dirname);
   SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
   return -1;
   }

/* Pack names for transmission */

memset(sendbuffer,0,CF_BUFSIZE);

offset = 0;

for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
   {
   if (strlen(dirp->d_name)+1+offset >= CF_BUFSIZE - CF_MAXLINKSIZE)
      {
      cipherlen = EncryptString(sendbuffer,out,conn->session_key,offset+1);
      SendTransaction(conn->sd_reply,out,cipherlen,CF_MORE);
      offset = 0;
      memset(sendbuffer,0,CF_BUFSIZE);
      memset(out,0,CF_BUFSIZE);
      }

   strncpy(sendbuffer+offset,dirp->d_name,CF_MAXLINKSIZE);
   offset += strlen(dirp->d_name) + 1;     /* + zero byte separator */
   }
 
strcpy(sendbuffer+offset,CFD_TERMINATOR);
cipherlen = EncryptString(sendbuffer,out,conn->session_key,offset+2+strlen(CFD_TERMINATOR));
SendTransaction(conn->sd_reply,out,cipherlen,CF_DONE);
Debug("END CfSecOpenDirectory(%s)\n",dirname);
closedir(dirh);
return 0;
}

/***************************************************************/

void Terminate(int sd)

{ char buffer[CF_BUFSIZE];

memset(buffer,0,CF_BUFSIZE);

strcpy(buffer,CFD_TERMINATOR);

if (SendTransaction(sd,buffer,strlen(buffer)+1,CF_DONE) == -1)
   {
   CfOut(cf_verbose,"send","Unable to reply with terminator");
   CfOut(cf_verbose,"","Unable to reply with terminator...\n");
   }
}

/***************************************************************/

void DeleteAuthList(struct Auth *ap)

{
if (ap != NULL)
   {
   DeleteAuthList(ap->next);
   ap->next = NULL;

   DeleteItemList(ap->accesslist);

   free((char *)ap);
   }
}

/***************************************************************/
/* Level 5                                                     */
/***************************************************************/

int OptionFound(char *args, char *pos, char *word)
    
/*
 * Returns true if the current position 'pos' in buffer
 * 'args' corresponds to the word 'word'.  Words are
 * separated by spaces.
 */

{ size_t len;

if (pos < args)
   {
   return false;
   }

/* Single options do not have to have spaces between */

if (strlen(word) == 2 && strncmp(pos,word,2) == 0)
   {
   return true;
   }

len = strlen(word);

if (strncmp(pos, word, len) != 0)
   {
   return false;
   }

if (pos == args)
   {
   return true;
   }
else if (*(pos-1) == ' ' && (pos[len] == ' ' || pos[len] == '\0'))
   {
   return true;
   }
else
   {
   return false;
   }
}


/**************************************************************/

void RefuseAccess(struct cfd_connection *conn,char *sendbuffer,int size,char *errmesg)

{ char *hostname, *username, *ipaddr;
  static char *def = "?"; 

if (strlen(conn->hostname) == 0)
   {
   hostname = def;
   }
else
   {
   hostname = conn->hostname;
   }

if (strlen(conn->username) == 0)
   {
   username = def;
   }
else
   {
   username = conn->username;
   }

if (strlen(conn->ipaddr) == 0)
   {
   ipaddr = def;
   }
else
   {
   ipaddr = conn->ipaddr;
   }
 
snprintf(sendbuffer,CF_BUFSIZE,"%s",CF_FAILEDSTR);
SendTransaction(conn->sd_reply,sendbuffer,size,CF_DONE);

CfOut(cf_inform,"","From (host=%s,user=%s,ip=%s)",hostname,username,ipaddr);

if (strlen(errmesg) > 0)
   {
   if (LOGCONNS)
      {
      CfOut(cf_log,"","ID from connecting host: (%s)",errmesg);
      }
   else
      {
      CfOut(cf_verbose,"","ID from connecting host: (%s)",errmesg);
      }
   }
}

/***************************************************************/

void ReplyNothing(struct cfd_connection *conn)

{ char buffer[CF_BUFSIZE];

snprintf(buffer,CF_BUFSIZE,"Hello %s (%s), nothing relevant to do here...\n\n",conn->hostname,conn->ipaddr);

if (SendTransaction(conn->sd_reply,buffer,0,CF_DONE) == -1)
   {
   CfOut(cf_error,"send","");
   }
}

/***************************************************************/

char *MapAddress(char *unspec_address)

{ /* Is the address a mapped ipv4 over ipv6 address */

if (strncmp(unspec_address,"::ffff:",7) == 0)
   {
   return (char *)(unspec_address+7);
   }
else
   {
   return unspec_address;
   }
}

/***************************************************************/

int CheckStoreKey(struct cfd_connection *conn,RSA *key)

{ RSA *savedkey;
 char keyname[CF_MAXVARSIZE];

if (BooleanControl("control_server","hostnamekeys"))
   {
   snprintf(keyname,CF_MAXVARSIZE,"%s-%s",conn->username,conn->hostname);
   }
else
   {
   snprintf(keyname,CF_MAXVARSIZE,"%s-%s",conn->username,MapAddress(conn->ipaddr));
   }
 
if (savedkey = HavePublicKey(keyname))
   {
   CfOut(cf_verbose,"","A public key was already known from %s/%s - no trust required\n",conn->hostname,conn->ipaddr);

   CfOut(cf_verbose,"","Adding IP %s to SkipVerify - no need to check this if we have a key\n",conn->ipaddr);
   PrependItem(&SKIPVERIFY,MapAddress(conn->ipaddr),NULL);
   
   if ((BN_cmp(savedkey->e,key->e) == 0) && (BN_cmp(savedkey->n,key->n) == 0))
      {
      CfOut(cf_verbose,"","The public key identity was confirmed as %s@%s\n",conn->username,conn->hostname);
      SendTransaction(conn->sd_reply,"OK: key accepted",0,CF_DONE);
      RSA_free(savedkey);
      return true;
      }
   else
      {
      /* If we find a key, but it doesn't match, see if we permit dynamical IP addressing */
      
      if ((DHCPLIST != NULL) && IsFuzzyItemIn(DHCPLIST,MapAddress(conn->ipaddr)))
         {
         int result;
         result = IsKnownHost(savedkey,key,MapAddress(conn->ipaddr),conn->username);
         RSA_free(savedkey);
         if (result)
            {
            SendTransaction(conn->sd_reply,"OK: key accepted",0,CF_DONE);
            }
         else
            {
            SendTransaction(conn->sd_reply,"BAD: keys did not match",0,CF_DONE);
            }
         return result;
         }
      else /* if not, reject it */
         {
         CfOut(cf_verbose,"","The new public key does not match the old one! Spoofing attempt!\n");
         SendTransaction(conn->sd_reply,"BAD: keys did not match",0,CF_DONE);
         RSA_free(savedkey);
         return false;
         }
      }
   
   return true;
   }
else if ((DHCPLIST != NULL) && IsFuzzyItemIn(DHCPLIST,MapAddress(conn->ipaddr)))
   {
   /* If the host is expected to have a dynamic address, check for the key */
   
   if ((DHCPLIST != NULL) && IsFuzzyItemIn(DHCPLIST,MapAddress(conn->ipaddr)))
      {
      int result;
      result = IsKnownHost(savedkey,key,MapAddress(conn->ipaddr),conn->username);
      RSA_free(savedkey);
      if (result)
         {
         SendTransaction(conn->sd_reply,"OK: key accepted",0,CF_DONE);
         }
      else
         {
         SendTransaction(conn->sd_reply,"BAD: keys did not match",0,CF_DONE);
         }
      return result;
      }
   }

/* Finally, if we're still here, we should consider trusting a new key ... */

if ((TRUSTKEYLIST != NULL) && IsFuzzyItemIn(TRUSTKEYLIST,MapAddress(conn->ipaddr)))
   {
   CfOut(cf_verbose,"","Host %s/%s was found in the list of hosts to trust\n",conn->hostname,conn->ipaddr);
   conn->trust = true;
   /* conn->maproot = false; ?? */
   SendTransaction(conn->sd_reply,"OK: unknown key was accepted on trust",0,CF_DONE);
   SavePublicKey(keyname,key);
   AddToKeyDB(key,MapAddress(conn->ipaddr));
   return true;
   }
else
   {
   CfOut(cf_verbose,"","No previous key found, and unable to accept this one on trust\n");
   SendTransaction(conn->sd_reply,"BAD: key could not be accepted on trust",0,CF_DONE);
   return false; 
   }
}

/***************************************************************/

int IsKnownHost(RSA *oldkey,RSA *newkey,char *mipaddr,char *username)

/* This builds security from trust only gradually with DHCP - takes time!
   But what else are we going to do? ssh doesn't have this problem - it
   just asks the user interactively. We can't do that ... */

{ DBT key,value;
  DB *dbp;
  int trust = false;
  char keyname[CF_MAXVARSIZE];
  char keydb[CF_MAXVARSIZE];

snprintf(keyname,CF_MAXVARSIZE,"%s-%s",username,mipaddr);
snprintf(keydb,CF_MAXVARSIZE,"%s/ppkeys/dynamic",CFWORKDIR); 

Debug("The key does not match a known key but the host could have a dynamic IP...\n"); 
 
if ((TRUSTKEYLIST != NULL) && IsFuzzyItemIn(TRUSTKEYLIST,mipaddr))
   {
   Debug("We will accept a new key for this IP on trust\n");
   trust = true;
   }
else
   {
   Debug("Will not accept this key, unless we have seen it before\n");
   }

/* If the host is allowed to have a variable IP range, we can accept
   the new key on trust for the given IP address provided we have seen
   the key before.  Check for it in a database .. */

Debug("Checking to see if we have seen the key before..\n"); 

if (!OpenDB(keydb,&dbp))
   {
   return false;
   }

memset(&key,0,sizeof(newkey));       
memset(&value,0,sizeof(value));
      
key.data = newkey;
key.size = sizeof(RSA);

if ((errno = dbp->get(dbp,NULL,&key,&value,0)) != 0)
   {
   Debug("The new key is not previously known, so we need to use policy for trusting the host %s\n",mipaddr);

   if (trust)
      {
      Debug("Policy says to trust the changed key from %s and note that it could vary in future\n",mipaddr);
      memset(&key,0,sizeof(key));       
      memset(&value,0,sizeof(value));
      
      key.data = newkey;
      key.size = sizeof(RSA);
      
      value.data = mipaddr;
      value.size = strlen(mipaddr)+1;
      
      if ((errno = dbp->put(dbp,NULL,&key,&value,0)) != 0)
         {
         dbp->err(dbp,errno,NULL);
         }

      DeletePublicKey(keyname);
      }
   else
      {
      Debug("Found no grounds for trusting this new from %s\n",mipaddr);
      }
   }
else
   {
   CfOut(cf_verbose,"","Public key was previously owned by %s now by %s - updating\n",value.data,mipaddr);
   Debug("Now trusting this new key, because we have seen it before\n");
   DeletePublicKey(keyname);
   trust = true;
   }

/* save this new key in the database, for future reference, regardless
   of whether we accept, but only change IP if trusted  */ 

SavePublicKey(keyname,newkey);

dbp->close(dbp,0);
chmod(keydb,0644); 
 
return trust; 
}

/***************************************************************/

void AddToKeyDB(RSA *newkey,char *mipaddr)

{ DBT key,value;
  DB *dbp;
  char keydb[CF_MAXVARSIZE];

snprintf(keydb,CF_MAXVARSIZE,"%s/ppkeys/dynamic",CFWORKDIR); 
  
if ((DHCPLIST != NULL) && IsFuzzyItemIn(DHCPLIST,mipaddr))
   {
   /* Cache keys in the db as we see them is there are dynamical addresses */

   if (!OpenDB(keydb,&dbp))
      {
      return;
      }

   memset(&key,0,sizeof(key));       
   memset(&value,0,sizeof(value));

   /* This case is unusual, we're using the newkey as the key */
   
   key.data = newkey;
   key.size = sizeof(RSA);
   
   value.data = mipaddr;
   value.size = strlen(mipaddr)+1;
   
   if ((errno = dbp->put(dbp,NULL,&key,&value,0)) != 0)
      {
      dbp->err(dbp,errno,NULL);
      }
   
   dbp->close(dbp,0);
   chmod(keydb,0644); 
   }
}

/***************************************************************/
/* Toolkit/Class: conn                                         */
/***************************************************************/

struct cfd_connection *NewConn(int sd)  /* construct */

{ struct cfd_connection *conn;

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
 if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)
    {
    CfOut(cf_error,"lock","pthread_mutex_lock failed");
    }
#endif
 
conn = (struct cfd_connection *) malloc(sizeof(struct cfd_connection));

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
 if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
    {
    CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
    }
#endif
 
if (conn == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate conn");
   HandleSignals(SIGTERM);
   }
 
conn->sd_reply = sd;
conn->id_verified = false;
conn->rsa_auth = false;
conn->trust = false; 
conn->hostname[0] = '\0';
conn->ipaddr[0] = '\0';
conn->username[0] = '\0'; 
conn->session_key = NULL;

 
Debug("*** New socket [%d]\n",sd);
 
return conn;
}

/***************************************************************/

void DeleteConn(struct cfd_connection *conn) /* destruct */

{
Debug("***Closing socket %d from %s\n",conn->sd_reply,conn->ipaddr);

close(conn->sd_reply);

if (conn->session_key != NULL)
   {
   free(conn->session_key);
   }
 
if (conn->ipaddr != NULL)
   {
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
   if (pthread_mutex_lock(&MUTEX_COUNT) != 0)
      {
      CfOut(cf_error,"pthread_mutex_lock","pthread_mutex_lock failed");
      DeleteConn(conn);
      return;
      }
#endif

   DeleteItemMatching(&CONNECTIONLIST,MapAddress(conn->ipaddr));

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
   if (pthread_mutex_unlock(&MUTEX_COUNT) != 0)
      {
      CfOut(cf_error,"pthread_mutex_unlock","pthread_mutex_unlock failed");
      DeleteConn(conn);
      return;
      }
#endif
   }
 
free ((char *)conn);
}

/***************************************************************/
/* ERS                                                         */
/***************************************************************/

int SafeOpen(char *filename)

{ int fd;

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
 if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)
    {
    CfOut(cf_error,"pthread_mutex_lock","pthread_mutex_lock failed");
    }
#endif
 
fd = open(filename,O_RDONLY);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
 if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
    {
    CfOut(cf_error,"pthread_mutex_unlock","pthread_mutex_unlock failed");
    }
#endif

return fd;
}

    
/***************************************************************/
    
void SafeClose(int fd)

{
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
 if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)
    {
    CfOut(cf_error,"pthread_mutex_lock","pthread_mutex_lock failed");
    }
#endif
 
close(fd);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
 if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
    {
    CfOut(cf_error,"pthread_mutex_unlock","pthread_mutex_unlock failed");
    }
#endif
}

/***************************************************************/

int cfscanf(char *in,int len1,int len2,char *out1,char *out2,char *out3)
   
{  
int len3=0;
char *sp;
   
   
sp = in;
memcpy(out1,sp,len1);
out1[len1]='\0';
   
sp += len1 + 1;
memcpy(out2,sp,len2);
   
sp += len2 + 1;
len3=strlen(sp);
memcpy(out3,sp,len3);
out3[len3]='\0';
   
return (len1 + len2 + len3 + 2);
}  
   
/* EOF */

