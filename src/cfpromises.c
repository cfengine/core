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

/*******************************************************************/
/*                                                                 */
/*  Promises cfpromises.c                                          */
/*                                                                 */
/*  Cfengine AS 1994/96                                           */
/*                                                                 */
/*******************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/

int main (int argc,char *argv[]);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

 char *ID = "The promise agent is a validator and analysis tool for\n"
            "configuration files belonging to any of the components\n"
            "of Cfengine. Configurations that make changes must be\n"
            "approved by this validator before being executed.";
 
 struct option OPTIONS[14] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "dry-run",no_argument,0,'n'},
      { "version",no_argument,0,'V' },
      { "file",required_argument,0,'f'},
      { "define",required_argument,0,'D' },
      { "negate",required_argument,0,'N' },
      { "inform",no_argument,0,'I'},
      { "diagnostic",no_argument,0,'x'},
      { "analysis",no_argument,0,'a'},
      { "reports",no_argument,0,'r'},
      { NULL,0,0,'\0' }
      };


 char *HINTS[14] =
      {
      "Print the help message",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "All talk and no action mode - make no changes, only inform of promises not kept",
      "Output the version of the software",
      "Specify an alternative input file than the default",
      "Define a list of comma separated classes to be defined at the start of execution",
      "Define a list of comma separated classes to be undefined at the start of execution",
      "Print basic information about changes made to the system, i.e. promises repaired",
      "Activate internal diagnostics (developers only)",
      "Perform additional analysis of configuration",
      "Generate reports about configuration and insert into CFDB",
      NULL
      };

/*******************************************************************/
/* Level 0 : Main                                                  */
/*******************************************************************/

int main(int argc,char *argv[])

{
CheckOpts(argc,argv); 
GenericInitialize(argc,argv,"common");
ThisAgentInit();
AnalyzePromiseConflicts();
GenericDeInitialize();

if (ERRORCOUNT > 0)
   {
   CfOut(cf_verbose,""," !! Inputs are invalid\n");
   exit(1);
   }
else
   {
   CfOut(cf_verbose,""," -> Inputs are valid\n");
   exit(0);
   } 
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  char arg[CF_BUFSIZE];
  int optindex = 0;
  int c;
  
while ((c=getopt_long(argc,argv,"ad:vnIf:D:N:VSrxM",OPTIONS,&optindex)) != EOF)
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
          
      case 'N': NegateClassesFromString(optarg,&VNEGHEAP);
          break;
          
      case 'I': INFORM = true;
          break;
          
      case 'v': VERBOSE = true;
          break;
          
      case 'n': DONTDO = true;
          IGNORELOCK = true;
	  LOOKUP = true;
          NewClass("opt_dry_run");
          break;
          
      case 'V': Version("cf-promises");
          exit(0);
          
      case 'h': Syntax("cf-promises - cfengine's promise analyzer",OPTIONS,HINTS,ID);
          exit(0);

      case 'M': ManPage("cf-promises - cfengine's promise analyzer",OPTIONS,HINTS,ID);
          exit(0);

       case 'r':
          SHOWREPORTS = true;
          break;

      case 'x': SelfDiagnostic();
          exit(0);

      case 'a':
          printf("Self-analysis is not yet implemented.");
          exit(0);
          break;

      default:  Syntax("cf-promises - cfengine's promise analyzer",OPTIONS,HINTS,ID);
          exit(1);
          
      }
  }

if (argv[optind] != NULL)
   {
   CfOut(cf_error,"","Unexpected argument with no preceding option: %s\n",argv[optind]);
   }

Debug("Set debugging\n");
}

/*******************************************************************/

void ThisAgentInit()

{
SHOWREPORTS = false;
}


/* EOF */
