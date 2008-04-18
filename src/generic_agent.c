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
/* File: generic_agent.c                                                     */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

extern struct option OPTIONS[];

/*****************************************************************************/

void GenericInitialize(int argc,char **argv)

{
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
/* Level 2                                                         */
/*******************************************************************/

void Syntax(char *component)

{ int i;

Version(component);
printf("\n");
printf("Options:\n\n");

for (i=0; OPTIONS[i].name != NULL; i++)
   {
   printf("--%-20s    (-%c)\n",OPTIONS[i].name,(char)OPTIONS[i].val);
   }

printf("\nDebug levels: 1=parsing, 2=running, 3=summary, 4=expression eval\n");

printf("\nBug reports to bug-cfengine@cfengine.org\n");
printf("General help to help-cfengine@cfengine.org\n");
printf("Info & fixes at http://www.cfengine.org\n");
}

/*******************************************************************/

void Version(char *component)

{
printf("Cfengine: %s\n%s\n%s\n",component,VERSION,COPYRIGHT);
}
