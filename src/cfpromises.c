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

/*******************************************************************/
/*                                                                 */
/*  Promises cfpromises.c                                          */
/*                                                                 */
/*  Mark Burgess 1994/96                                           */
/*                                                                 */
/*******************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/

int main (int argc,char *argv[]);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

  /* GNU STUFF FOR LATER #include "getopt.h" */
 
 struct option OPTIONS[13] =
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
      { "analysis",no_argument,0,'a'},
      { NULL,0,0,'\0' }
      };

/*******************************************************************/
/* Level 0 : Main                                                  */
/*******************************************************************/

int main(int argc,char *argv[])

{
GenericInitialize(argc,argv,"common");
ThisAgentInit();
SHOWREPORTS = true;
Verbose("Inputs are valid\n");

if (ERRORCOUNT > 0)
   {
   exit(1);
   }
else
   {
   exit(0);
   }
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  struct Item *actionList;
  int optindex = 0;
  int c;
  
while ((c=getopt_long(argc,argv,"ad:vnIf:pD:N:VSx",OPTIONS,&optindex)) != EOF)
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

      case 'V': Version("Promise engine");
          exit(0);
          
      case 'h': Syntax("Promise engine");
          exit(0);

      case 'S': SyntaxTree();
          exit(0);

      case 'x': SelfDiagnostic();
          exit(0);

      case 'a':

          printf("Self-analysis is not yet implemented.");
          exit(0);
          break;
          
      default:  Syntax("Promise engine");
          exit(1);
          
      }
  }

Debug("Set debugging\n");
}

/*******************************************************************/

void ThisAgentInit()

{
}





/* EOF */
