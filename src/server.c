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
/* File: server.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"
#include "cf3.server.h"

int main (int argc,char *argv[]);

extern struct BodySyntax CFS_CONTROLBODY[];

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

  /* GNU STUFF FOR LATER #include "getopt.h" */
 
 struct option OPTIONS[14] =
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
      { "no-fork",no_argument,0,'F' },
      { "ld-library-path",required_argument,0,'L'},
      { NULL,0,0,'\0' }
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

/*****************************************************************************/

int main(int argc,char *argv[])

{
GenericInitialize(argc,argv,"server");
ThisAgentInit();
KeepPromises();

return 0;
}

/*******************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  char ld_library_path[CF_BUFSIZE];
  struct Item *actionList;
  int optindex = 0;
  int c;
  
while ((c=getopt_long(argc,argv,"d:vnIf:D:N:VSxLF",OPTIONS,&optindex)) != EOF)
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

      case 'F': NO_FORK = true;
         break;

      case 'L': Verbose("Setting LD_LIBRARY_PATH=%s\n",optarg);
          snprintf(ld_library_path,CF_BUFSIZE-1,"LD_LIBRARY_PATH=%s",optarg);
          putenv(ld_library_path);
          break;

      case 'V': Version("Server agent");
          exit(0);
          
      case 'h': Syntax("Server agent");
          exit(0);

      case 'x': SelfDiagnostic();
          exit(0);
          
      default:  Syntax("Server agent");
          exit(1);
          
      }
  }

Debug("Set debugging\n");
}

/*******************************************************************/

void ThisAgentInit()

{ char vbuff[CF_BUFSIZE];
  int i;

BINDINTERFACE[0] = '\0';
  
sprintf(VPREFIX, "cfservd");
//CfOpenLog();
CfenginePort();
StrCfenginePort();
AddClassToHeap("any");      /* This is a reserved word / wildcard */
LOGGING = true;                    /* Do output to syslog */
IDClasses();

 
if ((CFINITSTARTTIME = time((time_t *)NULL)) == -1)
   {
   CfLog(cferror,"Couldn't read system clock\n","time");
   }

if ((CFSTARTTIME = time((time_t *)NULL)) == -1)
   {
   CfLog(cferror,"Couldn't read system clock\n","time");
   }

 /* XXX Initialize workdir for non privileged users */

strcpy(CFWORKDIR,WORKDIR);

if (getuid() > 0)
   {
   char *homedir;
   if ((homedir = getenv("HOME")) != NULL)
      {
      strcpy(CFWORKDIR,homedir);
      strcat(CFWORKDIR,"/.cfagent");
      }
   }

snprintf(vbuff,CF_BUFSIZE,"%s/test",CFWORKDIR);

MakeDirectoriesFor(vbuff,'y');
strncpy(VLOGDIR,CFWORKDIR,CF_BUFSIZE-1);
strncpy(VLOCKDIR,CFWORKDIR,CF_BUFSIZE-1);

VIFELAPSED = CF_EXEC_IFELAPSED;
VEXPIREAFTER = CF_EXEC_EXPIREAFTER;
 
strcpy(VDOMAIN,"undefined.domain");

VCANONICALFILE = strdup(CanonifyName(VINPUTFILE));
VREPOSITORY = strdup("\0");
 
OpenSSL_add_all_algorithms();
ERR_load_crypto_strings();
CheckWorkDirectories();

RandomSeed(); 
LoadSecretKeys();

i = 0;
strncpy(vbuff,ToLowerStr(VDOMAIN),127);

if (StrStr(VSYSNAME.nodename,vbuff))
   {
   strncpy(VFQNAME,VSYSNAME.nodename,CF_MAXVARSIZE-1);
   
   while(VSYSNAME.nodename[i++] != '.')
      {
      }
   
   strncpy(VUQNAME,VSYSNAME.nodename,i-1);
   }
else
   {
   snprintf(VFQNAME,CF_BUFSIZE,"%s.%s",VSYSNAME.nodename,ToLowerStr(VDOMAIN));
   strncpy(VUQNAME,VSYSNAME.nodename,CF_MAXVARSIZE-1);
   }
}

/*******************************************************************/

void KeepPromises()

{ struct Body *body;
  struct Constraint *cp;
  char scope[CF_BUFSIZE], *rettype;
  void *retval;

CFD_MAXPROCESSES = 10;
MAXTRIES = 5;
CFD_INTERVAL = 0;
CHECKSUMUPDATES = true;
DENYBADCLOCKS = true;

/* Keep promised agent behaviour */
  
for (body = BODIES; body != NULL; body=body->next)
   {
   if (strcmp(body->type,CF_AGENTTYPES[cf_server]) == 0)
      {
      if (strcmp(body->name,"control") == 0)
         {
         Debug("%s body for type %s\n",body->name,body->type);
         
         for (cp = body->conlist; cp != NULL; cp=cp->next)
            {
            if (IsExcluded(cp->classes))
               {
               continue;
               }

            snprintf(scope,CF_BUFSIZE,"%s_%s",body->name,body->type);

            if (GetVariable(scope,cp->lval,&retval,&rettype) == cf_notype)
               {
               FatalError("Control variable vanished mysteriously - shouldn't happen\n");
               }
            
            if (strcmp(cp->lval,CFS_CONTROLBODY[cfs_checkident].lval) == 0)
               {
               CHECK_RFC931 = GetBoolean(retval);
               Verbose("SET CheckIdent = %d\n",CHECK_RFC931);
               continue;
               }

            if (strcmp(cp->lval,CFS_CONTROLBODY[cfs_denybadclocks].lval) == 0)
               {
               DENYBADCLOCKS = GetBoolean(retval);
               Verbose("SET denybadclocks = %d\n",DENYBADCLOCKS);
               continue;
               }
            
            if (strcmp(cp->lval,CFS_CONTROLBODY[cfs_maxconnections].lval) == 0)
               {
               CFD_MAXPROCESSES = atoi(retval);
               MAXTRIES = CFD_MAXPROCESSES / 3;
               Verbose("SET maxconnections = %d\n",CFD_MAXPROCESSES);
               continue;
               }

            if (strcmp(cp->lval,CFS_CONTROLBODY[cfs_cfruncommand].lval) == 0)
               {
               strncpy(CFRUNCOMMAND,retval,CF_BUFSIZE-1);
               Verbose("SET cfruncommand = %s\n",CFRUNCOMMAND);
               continue;
               }

            if (strcmp(cp->lval,CFS_CONTROLBODY[cfs_allowconnects].lval) == 0)
               {
               struct Rlist *rp;
               Debug("Allowing connections from ...\n");
               
               for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
                  {
                  if (!IsItemIn(CONNECTIONLIST,rp->item))
                     {
                     AppendItem(&CONNECTIONLIST,rp->item,cp->classes);
                     }
                  }

               continue;
               }

            if (strcmp(cp->lval,CFS_CONTROLBODY[cfs_allowallconnects].lval) == 0)
               {
               struct Rlist *rp;
               Debug("Allowing multiple connections from ...\n");
               
               for (rp  = (struct Rlist *)retval; rp != NULL; rp = rp->next)
                  {
                  if (!IsItemIn(MULTICONNLIST,rp->item))
                     {
                     AppendItem(&MULTICONNLIST,rp->item,cp->classes);
                     }
                  }

               continue;
               }

            if (strcmp(cp->lval,CFS_CONTROLBODY[cfs_trustkeysfrom].lval) == 0)
               {
               struct Rlist *rp;
               Debug("Trust keys from ...\n");
               
               for (rp  = (struct Rlist *)retval; rp != NULL; rp = rp->next)
                  {
                  if (!IsItemIn(TRUSTKEYLIST,rp->item))
                     {
                     AppendItem(&TRUSTKEYLIST,rp->item,cp->classes);
                     }
                  }
               
               continue;
               }

            if (strcmp(cp->lval,CFS_CONTROLBODY[cfs_allowusers].lval) == 0)
               {
               struct Rlist *rp;
               Debug("Allow users ...\n");
               
               for (rp  = (struct Rlist *)retval; rp != NULL; rp = rp->next)
                  {
                  if (!IsItemIn(ALLOWUSERLIST,rp->item))
                     {
                     AppendItem(&ALLOWUSERLIST,rp->item,cp->classes);
                     }
                  }

               continue;
               }
            
            }
         }
      }
   }

/* Keep promised resource behaviour */
}
