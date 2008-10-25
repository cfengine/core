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
struct Promise *MakeDefaultRunAgentPromise();
FILE *NewStream(char *name);
void DeleteStream(FILE *fp);

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
      { "inform",no_argument,0,'I'},
      { "syntax",no_argument,0,'S'},
      { "remote_options",required_argument,0,'o'},
      { "diagnostic",no_argument,0,'x'},
      { NULL,0,0,'\0' }
      };

extern struct BodySyntax CFR_CONTROLBODY[];

int OUTPUT_TO_FILE = false;
char REMOTE_AGENT_OPTIONS[CF_MAXVARSIZE];
struct Attributes RUNATTR;
struct Rlist *HOSTLIST = NULL;
char SENDCLASSES[CF_MAXVARSIZE];

/*****************************************************************************/

int main(int argc,char *argv[])

{ struct Rlist *rp;
  struct Promise *pp;
 
GenericInitialize(argc,argv,"runagent");
ThisAgentInit();
KeepControlPromises(); // Set RUNATTR using copy

/* The default promise here is to hail associates */

pp = MakeDefaultRunAgentPromise();

if (HOSTLIST)
   {
   for (rp = HOSTLIST; rp != NULL; rp=rp->next)
      {
      HailServer(rp->item,RUNATTR,pp);
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
  
while ((c=getopt_long(argc,argv,"d:vnIf:pD:VSxo:",OPTIONS,&optindex)) != EOF)
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
                    
      case 'D': strncpy(SENDCLASSES,optarg,CF_MAXVARSIZE);
          
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
          
      case 'p': PARSEONLY = true;
          IGNORELOCK = true;
          break;          

      case 'V': Version("Run agent");
          exit(0);
          
      case 'h': Syntax("Run agent");
          exit(0);

      case 'x': SelfDiagnostic();
          exit(0);
          
      default:  Syntax("Run agent");
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

/********************************************************************/

int HailServer(char *host,struct Attributes a,struct Promise *pp)

{ struct cfagent_connection *conn;
  FILE *fp = stdout;
  char *sp,sendbuffer[CF_BUFSIZE],recvbuffer[CF_BUFSIZE],peer[CF_MAXVARSIZE];
  int n_read;

a.copy.portnumber = (short)ParseHostname(host,peer);
 
CfOut(cf_inform,"","Connecting to peer %s @ port %u, with options \"%s\"\n",peer,a.copy.portnumber,REMOTE_AGENT_OPTIONS);

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
      CfOut(cf_inform,"","No suitable server responded to hail\n");
      return false;
      }
   }

pp->cache = NULL;

snprintf(sendbuffer,CF_BUFSIZE,"EXEC %s",REMOTE_AGENT_OPTIONS);

if (SendTransaction(conn->sd,sendbuffer,0,CF_DONE) == -1)
   {
   CfOut(cf_error,"send","Transmission rejected");
   ServerDisconnection(conn,a,pp);
   DeleteAgentConn(conn);
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
      CfFile(fp," -> %s",recvbuffer);
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

   if (strstr(recvbuffer,"cfXen"))
      {
      CfFile(fp,"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
      continue;
      }

   CfFile(fp," -> %s",recvbuffer);
   }

ServerDisconnection(conn,a,pp);
DeleteAgentConn(conn);
DeleteRlist(a.copy.servers);

DeleteStream(fp);
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
      Verbose("SET force_ipv4 = %d\n",RUNATTR.copy.force_ipv4);
      continue;
      }
   
   if (strcmp(cp->lval,CFR_CONTROLBODY[cfr_trustkey].lval) == 0)
      {
      RUNATTR.copy.trustkey = GetBoolean(retval);
      Verbose("SET trustkey = %d\n",RUNATTR.copy.trustkey);
      continue;
      }
   
   if (strcmp(cp->lval,CFR_CONTROLBODY[cfr_encrypt].lval) == 0)
      {
      RUNATTR.copy.encrypt = GetBoolean(retval);
      Verbose("SET encrypt = %d\n",RUNATTR.copy.encrypt);
      continue;
      }

   if (strcmp(cp->lval,CFR_CONTROLBODY[cfr_portnumber].lval) == 0)
      {
      RUNATTR.copy.portnumber = (short)Str2Int(retval);
      Verbose("SET default portnumber = %u\n",(int)RUNATTR.copy.portnumber);
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

snprintf(filename,CF_BUFSIZE,"%s/%s_runagent.out",CFWORKDIR,name);

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
