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
/* File: report.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int main (int argc,char *argv[]);
void CheckOpts(int argc,char **argv);
void ThisAgentInit(void);
void KeepKnowControlPromises(void);
void KeepKnowledgePromise(struct Promise *pp);


/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

extern struct BodySyntax CFRP_CONTROLBODY[];

int HTML = false;
int GRAPH = false;
char GRAPHDIR[CF_MAXVARSIZE];

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

  /* GNU STUFF FOR LATER #include "getopt.h" */
 
 struct option OPTIONS[12] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "version",no_argument,0,'V' },
      { "file",required_argument,0,'f' },
      { "syntax",no_argument,0,'S'},
      { "html",no_argument,0,'H'},
      { "xml",no_argument,0,'X'},
      { "sql",no_argument,0,'s'},
      { "graphs",no_argument,0,'g'},
      { NULL,0,0,'\0' }
      };

/*****************************************************************************/

int main(int argc,char *argv[])

{
GenericInitialize(argc,argv,"knowledge");
ThisAgentInit();
KeepKnowControlPromises();

return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  struct Item *actionList;
  int optindex = 0;
  int c;
  char ld_library_path[CF_BUFSIZE];

 
while ((c=getopt_long(argc,argv,"ghHd:vVf:Sst:r:",OPTIONS,&optindex)) != EOF)
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

      case 'v':
          VERBOSE = true;
          break;
          
      case 'V':
          Version("Knowledge agent");
          exit(0);
          
      case 'h':
          Syntax("Knowledge agent");
          exit(0);

      case 'H':
          HTML = 1;
          break;

      default: Syntax("Knowledge agent");
          exit(1);
          
      }
  }
}

/*****************************************************************************/

void ThisAgentInit()

{ char vbuff[CF_BUFSIZE];

strcpy(GRAPHDIR,"");
}

/*****************************************************************************/

void KeepKnowControlPromises()

{ struct Constraint *cp;
  char rettype;
  void *retval;

for (cp = ControlBodyConstraints(cf_report); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }

   if (GetVariable("control_reportagent",cp->lval,&retval,&rettype) == cf_notype)
      {
      CfOut(cf_error,"","Unknown lval %s in report agent control body",cp->lval);
      continue;
      }
   

   }
}



/* EOF */




