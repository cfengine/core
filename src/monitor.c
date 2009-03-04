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
/* File: monitor.c                                                           */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int main (int argc,char *argv[]);

#include <math.h>

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

extern int NO_FORK;
extern int HISTO;
extern short TCPDUMP;
extern double FORGETRATE;

extern struct BodySyntax CFM_CONTROLBODY[];
extern double FORGETRATE;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

 char *ID = "The monitoring agent is a machine-learning, sampling\n"
            "daemon which learns the normal state of the current\n"
            "host and classifies new observations in terms of the\n"
            "patterns formed by previous ones. The data are made\n"
            "available to and read by cf-agent for classification\n"
            "of responses to anomalous states.";
 
 struct option OPTIONS[14] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "dry-run",no_argument,0,'n'},
      { "version",no_argument,0,'V' },
      { "no-lock",no_argument,0,'K'},
      { "file",required_argument,0,'f'},
      { "inform",no_argument,0,'I'},
      { "diagnostic",no_argument,0,'x'},
      { "no-fork",no_argument,0,'F'},
      { "histograms",no_argument,0,'H'},
      { "tcpdump",no_argument,0,'T'},
      { NULL,0,0,'\0' }
      };

 char *HINTS[14] =
      {
      "Print the help message",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "All talk and no action mode - make no changes, only inform of promises not kept",
      "Output the version of the software",
      "Ignore system lock",
      "Specify an alternative input file than the default",
      "Print basic information about changes made to the system, i.e. promises repaired",
      "Activate internal diagnostics (developers only)",
      "Run process in foreground, not as a daemon",
      "Store informatino about histograms / distributions",
      "Interface with tcpdump if available to collect data about network",
      NULL
      };

/*****************************************************************************/

int main(int argc,char *argv[])

{
CheckOpts(argc,argv); 
GenericInitialize(argc,argv,"monitor");
ThisAgentInit();
KeepPromises();

StartServer(argc,argv);
return 0;
}

/*******************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  struct Item *actionList;
  int optindex = 0;
  int c;
  
while ((c=getopt_long(argc,argv,"d:vnIf:VSxHTKMF",OPTIONS,&optindex)) != EOF)
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
          
          NO_FORK = true;
          break;
          
      case 'K': IGNORELOCK = true;
          break;
                    
       case 'I': INFORM = true;
          break;
          
      case 'v': VERBOSE = true;
          NO_FORK = true;
          break;
          
      case 'F': NO_FORK = true;
         break;

      case 'H': HISTO = true;
         break;

      case 'T': TCPDUMP = true;
                break;

      case 'V': Version("cf-monitord");
          exit(0);
          
      case 'h': Syntax("cf-monitord - cfengine's monitoring agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'M': ManPage("cf-monitord - cfengine's monitoring agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'x': SelfDiagnostic();
          exit(0);
          
      default: Syntax("cf-monitord - cfengine's monitoring agent",OPTIONS,HINTS,ID);
          exit(1);
          
      }
  }

Debug("Set debugging\n");
}

/*****************************************************************************/

void KeepPromises()

{ struct Constraint *cp;
  char rettype;
  void *retval;

for (cp = ControlBodyConstraints(cf_monitor); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }
   
   if (GetVariable("control_monitor",cp->lval,&retval,&rettype) == cf_notype)
      {
      CfOut(cf_error,"","Unknown lval %s in monitor control body",cp->lval);
      continue;
      }
   
   if (strcmp(cp->lval,CFM_CONTROLBODY[cfm_histograms].lval) == 0)
      {
      HISTO = GetBoolean(retval);
      Debug("histograms = %d\n",HISTO);
      }
   
   if (strcmp(cp->lval,CFM_CONTROLBODY[cfm_tcpdump].lval) == 0)
      {
      TCPDUMP = GetBoolean(retval);
      Debug("use tcpdump = %d\n",TCPDUMP);
      }
   
   if (strcmp(cp->lval,CFM_CONTROLBODY[cfm_forgetrate].lval) == 0)
      {
      FORGETRATE = atof(retval);
      Debug("forget rate = %f\n",FORGETRATE);
      }
   
   }
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

void ThisAgentInit()

{
umask(077);
sprintf(VPREFIX, "cf-monitord");
Cf3OpenLog();

LOGGING = true;                    /* Do output to syslog */

SetReferenceTime(false);
SetStartTime(false);

signal(SIGINT,HandleSignals);
signal(SIGTERM,HandleSignals);
signal(SIGHUP,SIG_IGN);
signal(SIGPIPE,SIG_IGN);
signal(SIGCHLD,SIG_IGN);
signal(SIGUSR1,HandleSignals);
signal(SIGUSR2,HandleSignals);

HISTO = false;
FORGETRATE = 0.6;

MonInitialize();
}

