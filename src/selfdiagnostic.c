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
/* File: selfdiagnostic.c                                                    */
/*                                                                           */
/* Created: Tue Oct  9 18:43:36 2007                                         */
/*                                                                           */
/* Author:                                           >                       */
/*                                                                           */
/* Revision: $Id$                                                            */
/*                                                                           */
/* Description:                                                              */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"


/*****************************************************************************/

void SelfDiagnostic()

{
FOUT = stdout;

printf("----------------------------------------------------------\n");
printf("Cfengine 3 - Performing level 2 self-diagnostic (dialogue)\n");
printf("----------------------------------------------------------\n\n");
TestVariableScan();
TestExpandPromise();
TestExpandVariables();
}

/*****************************************************************************/

void TestVariableScan()

{ int i;
  char *list_text1 = "$(administrator),a,b,c,d,e,f";
  char *list_text2 = "1,2,3,4,@(one)";
  struct Rlist *varlist1,*varlist2,*listoflists = NULL,*scalars = NULL;
  static char *varstrings[] =
    {
    "alpha $(one) beta $(two) gamma",
    "alpha $(five) beta $(none) gamma $(array[$(four)])",
    "alpha $(none) beta $(two) gamma",
    "alpha $(four) beta $(two) gamma $(array[$(diagnostic.three)])",
    NULL
    };

printf("Test variable scanning\n\n");
SetNewScope("diagnostic");

varlist1 = SplitStringAsRList(list_text1,',');
varlist2 = SplitStringAsRList(list_text2,',');

NewList("diagnostic","one",varlist1,cf_slist);
NewScalar("diagnostic","two","secondary skills",cf_str);
NewScalar("diagnostic","administrator","root",cf_str);
NewList("diagnostic","three",varlist2,cf_slist);
NewList("diagnostic","four",varlist2,cf_slist);
NewList("diagnostic","five",varlist2,cf_slist);

for (i = 0; varstrings[i] != NULL; i++)
   {
   printf("-----------------------------------------------------------\n");
   printf("Scanning: [%s]\n",varstrings[i]);
   ScanRval("diagnostic",&scalars,&listoflists,varstrings[i],CF_SCALAR);
   printf("Cumulative scan produced:\n");
   printf("   Scalar variables: ");
   ShowRlist(stdout,scalars);
   printf("\n");
   printf("   Lists variables: ");
   ShowRlist(stdout,listoflists);
   printf("\n");
   }
 
}

/*****************************************************************************/

void TestExpandPromise()

{ struct Promise pp,*pcopy;
  struct Body *bp;

printf("Testing promise duplication\n\n");
pp.promiser = "the originator";
pp.promisee = "the recipient";
pp.classes = "upper classes";
pp.petype = CF_SCALAR;
pp.lineno = 12;
pp.audit = NULL;
pp.conlist = NULL;

AppendConstraint(&(pp.conlist),"lval1",strdup("rval1"),CF_SCALAR,"lower classes1");
AppendConstraint(&(pp.conlist),"lval2",strdup("rval2"),CF_SCALAR,"lower classes2");

//getuid AppendConstraint(&(pp.conlist),"lval2",,CF_SCALAR,"lower classes2");

/* Now copy promise and delete */

pcopy = DeRefCopyPromise("diagnostic-scope",&pp);

printf("-----------------------------------------------------------\n");
printf("Raw test promises\n\n");
ShowPromise(&pp,4);
ShowPromise(pcopy,6);
DeletePromise(pcopy); 
}

/*****************************************************************************/

void TestExpandVariables()

{ struct Promise pp,*pcopy;
  struct Body *bp;
  int i;
  char *list_text1 = "a,b,c,d,e,f,g";
  char *list_text2 = "1,2,3,4,5,6,7";
  struct Rlist *rp, *args, *listvars = NULL, *scalarvars = NULL;
  struct Constraint *cp;
  struct FnCall *fp;

/* Still have diagnostic scope */
  
printf("Testing variable expansion\n\n");
pp.promiser = "the originator";
pp.promisee = "the recipient with $(two)";
pp.classes = "proletariat";
pp.petype = CF_SCALAR;
pp.lineno = 12;
pp.audit = NULL;
pp.conlist = NULL;

args = SplitStringAsRList("$(administrator)",',');
fp = NewFnCall("getuid",args);
    
AppendConstraint(&(pp.conlist),"lval1",strdup("@(one)"),CF_SCALAR,"lower classes1");
AppendConstraint(&(pp.conlist),"lval2",strdup("$(four)"),CF_SCALAR,"upper classes1");
AppendConstraint(&(pp.conlist),"lval3",fp,CF_FNCALL,"upper classes2");

/* Now copy promise and delete */

pcopy = DeRefCopyPromise("diagnostic",&pp);

ScanRval("diagnostic",&scalarvars,&listvars,pcopy->promiser,CF_SCALAR);

if (pcopy->promisee != NULL)
   {
   ScanRval("diagnostic",&scalarvars,&listvars,pp.promisee,pp.petype);
   }

for (cp = pcopy->conlist; cp != NULL; cp=cp->next)
   {
   ScanRval("diagnostic",&scalarvars,&listvars,cp->rval,cp->type);
   }

ExpandPromiseAndDo(cf_wildagent,"diagnostic",pcopy,scalarvars,listvars);
/* No cleanup */
}

