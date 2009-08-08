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
/* File: runagent.c                                                          */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int main (int argc,char *argv[]);
int HailServer(char *host,struct Attributes a,struct Promise *pp);
void ThisAgentInit(void);
int ParseHostname(char *hostname,char *new_hostname);
void SendClassData(struct cfagent_connection *conn);
struct Promise *MakeDefaultRunAgentPromise(void);
FILE *NewStream(char *name);
void DeleteStream(FILE *fp);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

 char *ID = "The run agent connects to a list of running instances of\n"
            "the cf-serverd service. The agent allows a user to\n"
            "forego the usual scheduling interval for the agent and\n"
            "activate cf-agent on a remote host. Additionally, a user\n"
            "can send additional classes to be defined on the remote\n"
            "host. Two kinds of classes may be sent: classes to decide\n"
            "on which hosts the agent will be started, and classes that\n"
            "the user requests the agent should define on execution.\n"
            "The latter type is regulated by cf-serverd's role based\n"
            "access control.";
 
 struct option OPTIONS[13] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "dry-run",no_argument,0,'n'},
      { "version",no_argument,0,'V' },
      { "file",required_argument,0,'f'},
      { "define-class",required_argument,0,'D' },
      { "select-class",required_argument,0,'s' },
      { "inform",no_argument,0,'I'},
      { "remote-options",required_argument,0,'o'},
      { "diagnostic",no_argument,0,'x'},
      { NULL,0,0,'\0' }
      };

 char *HINTS[13] =
      {
      "Print the help message",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "All talk and no action mode - make no changes, only inform of promises not kept",
      "Output the version of the software",
      "Specify an alternative input file than the default",
      "Define a list of comma separated classes to be sent to a remote agent",
      "Define a list of comma separated classes to be used to select remote agents by constraint",
      "Print basic information about changes made to the system, i.e. promises repaired",
      "Pass options to a remote server process",
      "Activate internal diagnostics (developers only)",
      NULL
      };


extern struct BodySyntax CFR_CONTROLBODY[];

int OUTPUT_TO_FILE = false;
int BACKGROUND = false;
int MAXCHILD = 50;
char REMOTE_AGENT_OPTIONS[CF_MAXVARSIZE];
struct Attributes RUNATTR;
struct Rlist *HOSTLIST = NULL;
char SENDCLASSES[CF_MAXVARSIZE];
char DEFINECLASSES[CF_MAXVARSIZE];

/*****************************************************************************/

int main(int argc,char *argv[])

{ struct Rlist *rp;
  struct Promise *pp;
  int count = 1;

CheckOpts(argc,argv);  
GenericInitialize(argc,argv,"runagent");
ThisAgentInit();
KeepControlPromises(); // Set RUNATTR using copy
pp = MakeDefaultRunAgentPromise();

if (HOSTLIST)
   {
   for (rp = HOSTLIST; rp != NULL; rp=rp->next)
      {
      HailServer(rp->item,RUNATTR,pp);

      if (count++ >= MAXCHILD)
         {
         BACKGROUND = false;
         }
      }
   }

return 0;
}

/*******************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  struct Item *actionList;
  int optindex = 0;
  int c;

DEFINECLASSES[0] = '\0';
SENDCLASSES[0] = '\0';  
  
while ((c=getopt_long(argc,argv,"d:vnKIf:D:VSxo:s:M",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':
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

      case 's': strncpy(SENDCLASSES,optarg,CF_MAXVARSIZE);
          
          if (strlen(optarg) > CF_MAXVARSIZE)
             {
             FatalError("Argument too long\n");
             }
          break;

      case 'D': strncpy(DEFINECLASSES,optarg,CF_MAXVARSIZE);
          
          if (strlen(optarg) > CF_MAXVARSIZE)
             {
             FatalError("Argument too long\n");
             }
          break;
          
      case 'o':
          strncpy(REMOTE_AGENT_OPTIONS,optarg,CF_MAXVARSIZE);
          break;
          
      case 'I': INFORM = true;
          break;
          
      case 'v': VERBOSE = true;
          break;
          
      case 'n': DONTDO = true;
          IGNORELOCK = true;
          NewClass("opt_dry_run");
          break;
          
      case 'V': Version("cf-runagent Run agent");
          exit(0);
          
      case 'h': Syntax("cf-runagent - Run agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'M': ManPage("cf-runagent - Run agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'x': SelfDiagnostic();
          exit(0);
          
      default:  Syntax("cf-runagent - Run agent",OPTIONS,HINTS,ID);
          exit(1);
          
      }
  }

Debug("Set debugging\n");
}

/*******************************************************************/

void ThisAgentInit()

{
umask(077);

if (strstr(REMOTE_AGENT_OPTIONS,"--file")||strstr(REMOTE_AGENT_OPTIONS,"-f"))
   {
   CfOut(cf_error,"","The specified remote options include a useless --file option. The remote server has promised to ignore this, thus it is disallowed.\n");
   exit(1);
   }
}

/********************************************************************/

int HailServer(char *host,struct Attributes a,struct Promise *pp)

{ struct cfagent_connection *conn;
  FILE *fp = stdout;
  char *sp,sendbuffer[CF_BUFSIZE],recvbuffer[CF_BUFSIZE],peer[CF_MAXVARSIZE];
  int n_read;

a.copy.portnumber = (short)ParseHostname(host,peer);

if (BACKGROUND)
   {
   CfOut(cf_inform,"","Hailing %s : %u, with options \"%s\" (parallel)\n",peer,a.copy.portnumber,REMOTE_AGENT_OPTIONS);
   if (fork() != 0)
      {
      return true; /* Child continues*/
      }   
   }
else
   {
   CfOut(cf_inform,"","...........................................................................\n");
   CfOut(cf_inform,""," * Hailing %s : %u, with options \"%s\" (serial)\n",peer,a.copy.portnumber,REMOTE_AGENT_OPTIONS);
   CfOut(cf_inform,"","...........................................................................\n");
   }

a.copy.servers = SplitStringAsRList(peer,'*');

if (a.copy.servers == NULL || strcmp(a.copy.servers->item,"localhost") == 0)
   {
   cfPS(cf_inform,CF_NOP,"",pp,a,"No hosts are registered to connect to");
   return false;
   }
else
   {
   conn = NewServerConnection(a,pp);

   if (conn == NULL)
      {
      CfOut(cf_verbose,"","No suitable server responded to hail\n");
      return false;
      }
   }

pp->cache = NULL;

if (strlen(DEFINECLASSES))
   {
   snprintf(sendbuffer,CF_BUFSIZE,"EXEC %s -D%s",REMOTE_AGENT_OPTIONS,DEFINECLASSES);
   }
else
   {
   snprintf(sendbuffer,CF_BUFSIZE,"EXEC %s",REMOTE_AGENT_OPTIONS);
   }

if (SendTransaction(conn->sd,sendbuffer,0,CF_DONE) == -1)
   {
   CfOut(cf_error,"send","Transmission rejected");
   ServerDisconnection(conn);
   return false;
   }

fp = NewStream(peer);
SendClassData(conn);

while (true)
   {
   memset(recvbuffer,0,CF_BUFSIZE);

   if ((n_read = ReceiveTransaction(conn->sd,recvbuffer,NULL)) == -1)
      {
      if (errno == EINTR) 
         {
         continue;
         }
      
      break;
      }

   if (n_read == 0)
      {
      break;
      }
   
   if (strlen(recvbuffer) == 0)
      {
      continue;
      }

   if ((sp = strstr(recvbuffer,CFD_TERMINATOR)) != NULL)
      {
      CfFile(fp," !!\n\n");
      break;
      }

   if ((sp = strstr(recvbuffer,"BAD:")) != NULL)
      {
      CfFile(fp," !! %s",recvbuffer+4);
      continue;
      }   

   if (strstr(recvbuffer,"too soon"))
      {
      CfFile(fp," !! %s",recvbuffer);
      continue;
      }

   CfFile(fp," -> %s",recvbuffer);
   }

ServerDisconnection(conn);
DeleteRlist(a.copy.servers);

DeleteStream(fp);

if (BACKGROUND)
   {
   /* Close parallel connection*/
   exit(0);
   }

return true;
}

/********************************************************************/
/* Level 2                                                          */
/********************************************************************/

void KeepControlPromises()

{ struct Constraint *cp;
  char rettype;
  void *retval;

RUNATTR.copy.trustkey = false;
RUNATTR.copy.encrypt = true;
RUNATTR.copy.force_ipv4 = false;
RUNATTR.copy.portnumber = SHORT_CFENGINEPORT;

/* Keep promised agent behaviour - control bodies */

for (cp = ControlBodyConstraints(cf_runagent); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }
   
   if (GetVariable("control_runagent",cp->lval,&retval,&rettype) == cf_notype)
      {
      CfOut(cf_error,"","Unknown lval %s in runagent control body",cp->lval);
      continue;
      }
   
   if (strcmp(cp->lval,CFR_CONTROLBODY[cfr_force_ipv4].lval) == 0)
      {
      RUNATTR.copy.force_ipv4 = GetBoolean(retval);
      CfOut(cf_verbose,"","SET force_ipv4 = %d\n",RUNATTR.copy.force_ipv4);
      continue;
      }
   
   if (strcmp(cp->lval,CFR_CONTROLBODY[cfr_trustkey].lval) == 0)
      {
      RUNATTR.copy.trustkey = GetBoolean(retval);
      CfOut(cf_verbose,"","SET trustkey = %d\n",RUNATTR.copy.trustkey);
      continue;
      }
   
   if (strcmp(cp->lval,CFR_CONTROLBODY[cfr_encrypt].lval) == 0)
      {
      RUNATTR.copy.encrypt = GetBoolean(retval);
      CfOut(cf_verbose,"","SET encrypt = %d\n",RUNATTR.copy.encrypt);
      continue;
      }

   if (strcmp(cp->lval,CFR_CONTROLBODY[cfr_portnumber].lval) == 0)
      {
      RUNATTR.copy.portnumber = (short)Str2Int(retval);
      CfOut(cf_verbose,"","SET default portnumber = %u\n",(int)RUNATTR.copy.portnumber);
      continue;
      }

   if (strcmp(cp->lval,CFR_CONTROLBODY[cfr_output_to_file].lval) == 0)
      {
      OUTPUT_TO_FILE = GetBoolean(retval);
      continue;
      }

   
   if (strcmp(cp->lval,CFR_CONTROLBODY[cfr_hosts].lval) == 0)
      {
      HOSTLIST = retval;
      continue;
      }   
   }
}

/********************************************************************/

struct Promise *MakeDefaultRunAgentPromise()

{ struct Promise *pp,*lp;
  char *sp = NULL,*spe = NULL;
  
/* The default promise here is to hail associates */

if ((pp = (struct Promise *)malloc(sizeof(struct Promise))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Promise");
   FatalError("");
   }

pp->audit = NULL;
pp->lineno = 0;
pp->bundle =  strdup("implicit internal bundle for runagent");
pp->promiser = strdup("runagent");
pp->promisee = NULL;
pp->petype = CF_NOPROMISEE;
pp->classes = NULL;
pp->conlist = NULL;
pp->done = false;
pp->donep = &(pp->done);
pp->ref = NULL;

pp->this_server = NULL;
pp->cache = NULL;
pp->conn = NULL;
pp->inode_cache = NULL;
pp->next = NULL;
return pp;
}

/********************************************************************/

int ParseHostname(char *name,char *hostname)

{ int port = ntohs(SHORT_CFENGINEPORT);

if (strchr(name,':'))
   {
   sscanf(name,"%250[^:]:%d",hostname,&port);
   }
else
   {
   strncpy(hostname,name,CF_MAXVARSIZE);
   }

return(port);
}

/********************************************************************/

void SendClassData(struct cfagent_connection *conn)

{ struct Rlist *classes,*rp;
  char sendbuffer[CF_BUFSIZE];
  int used;

classes = SplitRegexAsRList(SENDCLASSES,"[,: ]",99,false);

for (rp = classes; rp != NULL; rp = rp->next)
   {
   if (SendTransaction(conn->sd,rp->item,0,CF_DONE) == -1)
      {
      CfOut(cf_error,"send","Transaction failed");
      return;
      }
   }
   
snprintf(sendbuffer,CF_MAXVARSIZE,"%s",CFD_TERMINATOR);

if (SendTransaction(conn->sd,sendbuffer,0,CF_DONE) == -1)
   {
   CfOut(cf_error,"send","Transaction failed");
   return;
   }
}

/********************************************************************/

FILE *NewStream(char *name)

{ FILE *fp;
  char filename[CF_BUFSIZE];

snprintf(filename,CF_BUFSIZE,"%s/outputs/%s_runagent.out",CFWORKDIR,name);

if (OUTPUT_TO_FILE)
   {
   printf("Opening file...%s\n",filename);

   if ((fp = fopen(filename,"w")) == NULL)
      {
      CfOut(cf_error,"Unable to open file %s\n",filename);
      fp = stdout;
      }
   }
else
   {
   fp = stdout;
   }

return fp;
}

/********************************************************************/

void DeleteStream(FILE *fp)

{
if (fp != stdout)
   {
   fclose(fp);
   }
}
