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

/*******************************************************************/
/*                                                                 */
/*  Promises cfpromises.c                                          */
/*                                                                 */
/*  Mark Burgess 1994/96                                           */
/*                                                                 */
/*******************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

extern FILE *yyin;
extern char *CFH[][2];

/*******************************************************************/
/* Functions internal to cfengine.c                                */
/*******************************************************************/

int main (int argc,char *argv[]);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

  /* GNU STUFF FOR LATER #include "getopt.h" */
 
 struct option OPTIONS[49] =
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
      { NULL,0,0,'\0' }
      };

/*******************************************************************/
/* Level 0 : Main                                                  */
/*******************************************************************/

int main(int argc,char *argv[])

{ void *rval;
  char rtype;
  struct Rlist *rp;

Initialize(argc,argv);
SetReferenceTime(true);
SetStartTime(false);

if (! NOHARDCLASSES)
   {
   SetNewScope("system");
   GetNameInfo3();
   GetInterfaceInfo3();
   GetV6InterfaceInfo();
   GetEnvironment();
   }

Cf3ParseFile(VINPUTFILE);

if (VINPUTLIST != NULL)
   {
   for (rp = VINPUTLIST; rp != NULL; rp=rp->next)
      {
      if (rp->type != CF_SCALAR)
         {
         snprintf(OUTPUT,CF_BUFSIZE,"Non file object %s in list\n",(char *)rp->item);
         CfLog(cferror,OUTPUT,"");
         }
      else
         {
         Cf3ParseFile((char *)rp->item);
         }
      }
   }

if ((FOUT = fopen("promise_output.html","w")) == NULL)
   {
   printf("Cannot open output file\n");
   return 0;
   }

XML = 1;

HashVariables();
SetAuditVersion();

fprintf(FOUT,"<h1>Expanded promise list</h1>");
fprintf(FOUT,"%s",CFH[0][0]);

VerifyPromises();

fprintf(FOUT,"%s",CFH[0][1]);
fclose(FOUT);
printf("Wrote expansion summary to promise_output.html\n");


if (ERRORCOUNT > 0)
   {
   FatalError("Unresolved errors in configuration");
   }

Report(VINPUTFILE);

return 0;
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/
 
void Initialize(int argc,char *argv[])

{ char *sp, **cfargv;;
 int i,j, cfargc, seed;
  struct stat statbuf;
  unsigned char s[16];
  char ebuff[CF_EXPANDSIZE];
  
PreLockState();

#ifndef HAVE_REGCOMP
re_syntax_options |= RE_INTERVALS;
#endif
  
OpenSSL_add_all_algorithms();
ERR_load_crypto_strings();
CheckWorkDirectories();
RandomSeed();
 
RAND_bytes(s,16);
s[15] = '\0';
seed = ElfHash(s);
srand48((long)seed);  

/* Note we need to fix the options since the argv mechanism doesn't */
/* work when the shell #!/bla/cfengine -v -f notation is used.      */
/* Everything ends up inside a single argument! Here's the fix      */

cfargc = 1;

/* Pass 1: Find how many arguments there are. */
for (i = 1, j = 1; i < argc; i++)
   {
   sp = argv[i];
   
   while (*sp != '\0')
      {
      while (*sp == ' ' && *sp != '\0') /* Skip to arg */
         {
         sp++;
         }
      
      cfargc++;
      
      while (*sp != ' ' && *sp != '\0') /* Skip to white space */
         {
         sp++;
         }
      }
   }

/* Allocate memory for cfargv. */

cfargv = (char **) malloc(sizeof(char *) * cfargc + 1);

if (!cfargv)
   {
   FatalError("cfagent: Out of memory parsing arguments\n");
   }

/* Pass 2: Parse the arguments. */

cfargv[0] = "cfagent";

for (i = 1, j = 1; i < argc; i++)
   {
   sp = argv[i];
   
   while (*sp != '\0')
      {
      while (*sp == ' ' && *sp != '\0') /* Skip to arg */
         {
         if (*sp == ' ')
            {
            *sp = '\0'; /* Break argv string */
            }
         sp++;
         }
      
      cfargv[j++] = sp;
      
      while (*sp != ' ' && *sp != '\0') /* Skip to white space */
         {
         sp++;
         }
      }
   }

cfargv[j] = NULL;

CheckOpts(argc,argv);

if (!MINUSF)
   {
   strcpy(VINPUTFILE,"../tests/promises.cf");
   }

CfenginePort();
StrCfenginePort();
FOUT = stdout;
AddClassToHeap("any");
strcpy(VPREFIX,"cfengine3");
}

/*******************************************************************/

void Cf3ParseFile(char *filename)

{ FILE *save_yyin = yyin;
  
PrependAuditFile(filename);
 
if ((yyin = fopen(filename,"r")) == NULL)      /* Open root file */
   {
   printf("Can't open file %s\n",filename);
   exit (1);
   }
 
P.line_no = 1;
P.line_pos = 1;
P.list_nesting = 0;
P.arg_nesting = 0;
P.filename = strdup(filename);

P.currentid = NULL;
P.currentstring = NULL;
P.currenttype = NULL;
P.currentclasses = NULL;   
P.currentRlist = NULL;
P.currentpromise = NULL;
P.promiser = NULL;

while (!feof(yyin))
   {
   yyparse();
   
   if (ferror(yyin))  /* abortable */
      {
      perror("cfengine");
      exit(1);
      }
   }

fclose (yyin);
}

/*******************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  struct Item *actionList;
  int optindex = 0;
  int c;
  
while ((c=getopt_long(argc,argv,"d:vnIf:pD:N:VSx",OPTIONS,&optindex)) != EOF)
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

      case 'V': Version();
          exit(0);
          
      case 'h': Syntax();
          exit(0);

      case 'S': SyntaxTree();
          exit(0);

      case 'x': SelfDiagnostic();
          exit(0);
          
      default:  Syntax();
          exit(1);
          
      }
  }

Debug("Set debugging\n");
}

/*******************************************************************/

void Report(char *fname)

{ char filename[CF_BUFSIZE];

snprintf(filename,CF_BUFSIZE-1,"%s.txt",fname);

FOUT = stdout;

if ((FOUT = fopen(filename,"w")) == NULL)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Could not write output log to %s",filename);
   FatalError(OUTPUT);
   }

printf("Summarizing promises as text to %s\n",filename);
ShowPromises(BUNDLES,BODIES);
fclose(FOUT);

if (DEBUG)
   {
   ShowScopedVariables(stdout);
   ShowContext();
   }

XML = true;

snprintf(filename,CF_BUFSIZE-1,"%s.html",fname);

if ((FOUT = fopen(filename,"w")) == NULL)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Could not write output log to %s",filename);
   FatalError(OUTPUT);
   }

printf("Summarizing promises as html to %s\n",filename);
ShowPromises(BUNDLES,BODIES);
fclose(FOUT);
}

/*******************************************************************/

void HashVariables()

{ struct Bundle *bp,*bundles;
  struct SubType *sp;
  struct Body *bdp;
  struct Scope *ptr;
  char buf[CF_BUFSIZE];

for (bp = BUNDLES; bp != NULL; bp = bp->next) /* get schedule */
   {
   SetNewScope(bp->name);

   for (sp = bp->subtypes; sp != NULL; sp = sp->next) /* get schedule */
      {      
      if (strcmp(sp->name,"vars") == 0)
         {
         CheckVariablePromises(bp->name,sp->promiselist);
         }
      }
   }

/* Only control bodies need to be hashed like variables */

for (bdp = BODIES; bdp != NULL; bdp = bdp->next) /* get schedule */
   {
   if (strcmp(bdp->name,"control") == 0)
      {
      snprintf(buf,CF_BUFSIZE,"%s_%s",bdp->name,bdp->type);
      SetNewScope(buf);
      CheckControlPromises(buf,bdp->type,bdp->conlist);
      }
   }

// Delete this 
for (ptr = VSCOPE; ptr != NULL; ptr=ptr->next)
   {
   fprintf(FOUT,"<h2>SCOPE %s</h2>\n\n",ptr->scope);
   PrintHashes(FOUT,ptr->hashtable);
   }
}

/*******************************************************************/

void VerifyPromises()

{ struct Bundle *bp,*bundles;
  struct SubType *sp;
  struct Promise *pp;
  struct Body *bdp;
  struct Scope *ptr;
  struct Rlist *rp;
  struct FnCall *fp;
  char buf[CF_BUFSIZE], *scope;

for (rp = BODYPARTS; rp != NULL; rp=rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
          if (!IsBody(BODIES,(char *)rp->item))
             {
             snprintf(OUTPUT,CF_BUFSIZE,"Undeclared promise body \"%s()\" was referenced in a promise\n",(char *)rp->item);
             CfLog(cferror,OUTPUT,"");
             }
          break;

      case CF_FNCALL:
          fp = (struct FnCall *)rp->item;
          if (!IsBody(BODIES,fp->name))
             {
             snprintf(OUTPUT,CF_BUFSIZE,"Undeclared promise body \"%s()\" was referenced in a promise\n",fp->name);
             CfLog(cferror,OUTPUT,"");
             }
          break;
      }
   }

/* Now look once through all the bundles themselves */

for (bp = BUNDLES; bp != NULL; bp = bp->next) /* get schedule */
   {
   scope = bp->name;
   
   for (sp = bp->subtypes; sp != NULL; sp = sp->next) /* get schedule */
      {
      for (pp = sp->promiselist; pp != NULL; pp=pp->next)
         {
         ExpandPromise(scope,pp);
         }
      }
   }

/*
  And finally if we get this far, execute the bundles in sequence,
  but this is not a job for the compiler, which is simply translating
  the rules. To take the next step, we need to connect this compiler
  to the components

   agent   cfagent
   monitor cfenvd
   server  cfservd  etc.

   This is the next stage and is not defined in this training release.
*/

}

/*******************************************************************/
/* Level 2                                                         */
/*******************************************************************/

void Syntax()

{ int i;

Version();
printf("\n");
printf("Options:\n\n");

for (i=0; OPTIONS[i].name != NULL; i++)
   {
   printf("--%-20s    (-%c)\n",OPTIONS[i].name,(char)OPTIONS[i].val);
   }

printf("\nDebug levels: 1=parsing, 2=running, 3=summary, 4=expression eval\n");

printf("\nBug reports to bug-cfengine@cfengine.org (News: gnu.cfengine.bug)\n");
printf("General help to help-cfengine@cfengine.org (News: gnu.cfengine.help)\n");
printf("Info & fixes at http://www.cfengine.org\n");
}

/*******************************************************************/

void Version()

{
printf("Cfengine: promise engine\n%s\n%s\n",VERSION,COPYRIGHT);
}

/*******************************************************************/

void CheckVariablePromises(char *scope,struct Promise *varlist)

{ struct Promise *pp;
  struct Constraint *cp;
  char *lval;
  void *rval = NULL;
  int i = 0,override = true;

Debug("CheckVariablePromises()\n");
  
for (pp = varlist; pp != NULL; pp=pp->next)
   {
   i = 0;

   if (IsExcluded(pp->classes))
      {
      continue;
      }
   
   for (cp = pp->conlist; cp != NULL; cp=cp->next)
      {
      i++;

      if (strcmp(cp->lval,"policy") == 0)
         {
         if (strcmp(cp->rval,"constant") == 0)
            {
            override = false;
            }
         continue;
         }
      else
         {
         rval = cp->rval;
         }

      if (i > 2)
         {
         snprintf(OUTPUT,CF_BUFSIZE,"Broken type-promise in %s",pp->promiser);
         CfLog(cferror,OUTPUT,"");
         snprintf(OUTPUT,CF_BUFSIZE,"Rule from %s at/before line %d\n",cp->audit->filename,cp->lineno);
         CfLog(cferror,OUTPUT,"");
         }

      if (rval != NULL)
         {
         struct Rval returnval; /* Must expand naked functions here for consistency */
         struct FnCall *fp = (struct FnCall *)rval;

         if (cp->type == CF_FNCALL)
            {
            returnval = EvaluateFunctionCall(fp,pp);
            DeleteFnCall(fp);
            cp->rval = rval = returnval.item;
            cp->type = returnval.rtype;
            }

         if (!AddVariableHash(scope,pp->promiser,rval,cp->type,Typename2Datatype(cp->lval),cp->audit->filename,cp->lineno))
            {
            snprintf(OUTPUT,CF_BUFSIZE,"Rule from %s at/before line %d\n",cp->audit->filename,cp->lineno);
            CfLog(cferror,OUTPUT,"");
            }
         }
      else
         {
         snprintf(OUTPUT,CF_BUFSIZE,"Variable %s has no promised value\n",pp->promiser);
         CfLog(cferror,OUTPUT,"");
         snprintf(OUTPUT,CF_BUFSIZE,"Rule from %s at/before line %d\n",cp->audit->filename,cp->lineno);
         CfLog(cferror,OUTPUT,"");
         }
      }
   }
}

/*******************************************************************/

void CheckControlPromises(char *scope,char *agent,struct Constraint *controllist)

{ struct Constraint *cp;
  struct SubTypeSyntax *sp;
  struct BodySyntax *bp = NULL;
  char *lval;
  void *rval = NULL;
  int i = 0,override = true;

Debug("CheckControlPromises()\n");

for (i = 0; CF_ALL_BODIES[i].bs != NULL; i++)
   {
   bp = CF_ALL_BODIES[i].bs;

   if (strcmp(agent,CF_ALL_BODIES[i].btype) == 0)
      {
      break;
      }
   }

if (bp == NULL)
   {
   FatalError("Unknown agent");
   }

for (cp = controllist; cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }

   if (!AddVariableHash(scope,cp->lval,cp->rval,cp->type,GetControlDatatype(cp->lval,bp),cp->audit->filename,cp->lineno))
      {
      snprintf(OUTPUT,CF_BUFSIZE,"Rule from %s at/before line %d\n",cp->audit->filename,cp->lineno);
      CfLog(cferror,OUTPUT,"");
      }
   }
}

/*******************************************************************/

void SetAuditVersion()

{ void *rval;
  char rtype = 'x';

  /* In addition, each bundle can have its own version */
 
switch (GetVariable("control_common","cfinputs_version",&rval,&rtype))
   {
   case cf_str:
       if (rtype != CF_SCALAR)
          {
          yyerror("non-scalar version string");
          }
       AUDITPTR->version = strdup((char *)rval);
       break;

   default:
       AUDITPTR->version = strdup("no specified version");
       break;
   }
}

/********************************************************************/


/* EOF */
