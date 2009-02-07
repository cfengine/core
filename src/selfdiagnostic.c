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
/* File: selfdiagnostic.c                                                    */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int NR = 0;

/*****************************************************************************/

void SelfDiagnostic()

{
if (VERBOSE || DEBUG)
   {
   FREPORT_TXT = stdout;
   FREPORT_HTML = fopen("/dev/null","w");
   FKNOW = fopen("/dev/null","w");
   }
else
   {
   FREPORT_TXT= fopen("/dev/null","w");
   FREPORT_HTML= fopen("/dev/null","w");
   FKNOW = fopen("/dev/null","w");
   }
       
printf("----------------------------------------------------------\n");
printf("Cfengine 3 - Performing level 2 self-diagnostic (dialogue)\n");
printf("----------------------------------------------------------\n\n");
TestVariableScan();
TestExpandPromise();
TestExpandVariables();
TestRegularExpressions();
TestAgentPromises();
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

printf("%d. Test variable scanning\n",++NR);
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
   if (VERBOSE || DEBUG)
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
 
}

/*****************************************************************************/

void TestExpandPromise()

{ struct Promise pp,*pcopy;
  struct Body *bp;

printf("%d. Testing promise duplication and expansion\n",++NR);
pp.promiser = "the originator";
pp.promisee = "the recipient";
pp.classes = "upper classes";
pp.petype = CF_SCALAR;
pp.lineno = 12;
pp.audit = NULL;
pp.conlist = NULL;

pp.bundletype = "bundle_type";
pp.bundle = "test_bundle";
pp.ref = "commentary";
pp.agentsubtype = NULL;
pp.done = false;
pp.next = NULL;
pp.cache = NULL;
pp.inode_cache = NULL;
pp.this_server = NULL;
pp.donep = &(pp.done);
pp.conn = NULL;


AppendConstraint(&(pp.conlist),"lval1",strdup("rval1"),CF_SCALAR,"lower classes1");
AppendConstraint(&(pp.conlist),"lval2",strdup("rval2"),CF_SCALAR,"lower classes2");

//getuid AppendConstraint(&(pp.conlist),"lval2",,CF_SCALAR,"lower classes2");

/* Now copy promise and delete */

pcopy = DeRefCopyPromise("diagnostic-scope",&pp);
if (VERBOSE || DEBUG)
   {
   printf("-----------------------------------------------------------\n");
   printf("Raw test promises\n\n");
   ShowPromise(&pp,4);
   ShowPromise(pcopy,6);
   }
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
NewScope("control_common");
  
printf("%d. Testing variable expansion\n",++NR);
pp.promiser = "the originator";
pp.promisee = "the recipient with $(two)";
pp.classes = "proletariat";
pp.petype = CF_SCALAR;
pp.lineno = 12;
pp.audit = NULL;
pp.conlist = NULL;
pp.agentsubtype = "none";

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

ExpandPromiseAndDo(cf_common,"diagnostic",pcopy,scalarvars,listvars,NULL);
/* No cleanup */
}

/*****************************************************************************/

void TestRegularExpressions()

{ struct CfRegEx rex;
  int start,end;

printf("%d. Testing regular expression engine\n",++NR);

#ifdef HAVE_LIBPCRE
printf(" -> Regex engine is the Perl Compatible Regular Expression library\n");
#else
printf(" -> Regex engine is the POSIX Regular Expression library\n");
#endif

rex = CompileRegExp("#.*");

if (rex.failed)
   {
   CfOut(cf_error,"","Failed regular expression compilation\n");
   }
else
   {
   CfOut(cf_error,""," -> Regular expression compilation - ok\n");
   }

if (!RegExMatchSubString(rex,"line 1:\nline2: # comment to end\nline 3: blablab",&start,&end))
   {
   CfOut(cf_error,"","Failed regular expression extraction +1\n");
   }
else
   {
   CfOut(cf_error,""," -> Regular expression extraction - ok %d - %d\n",start,end);
   }

if (RegExMatchFullString(rex,"line 1:\nline2: # comment to end\nline 3: blablab"))
   {
   CfOut(cf_error,"","Failed regular expression extraction -1\n");
   }
else
   {
   CfOut(cf_error,""," -> Regular expression extraction - ok\n");
   }

if (FullTextMatch("[a-z]*","1234abcd6789"))
   {
   CfOut(cf_error,"","Failed regular expression match 1\n");
   }
else
   {
   CfOut(cf_verbose,""," -> FullTextMatch - ok 1\n");
   }

if (FullTextMatch("[1-4]*[a-z]*.*","1234abcd6789"))
   {
   CfOut(cf_error,""," -> FullTextMatch - ok 2\n");
   }
else
   {
   CfOut(cf_error,"","Failed regular expression match 2\n");
   }

if (BlockTextMatch("#.*","line 1:\nline2: # comment to end\nline 3: blablab",&start,&end))
   {
   CfOut(cf_error,""," -> BlockTextMatch - ok\n");
   
   if (start != 15)
      {
      CfOut(cf_error,"","Start was not at 15 -> %d\n",start);
      }
   
   if (end != 31)
      {
      CfOut(cf_error,"","Start was not at 31 -> %d\n",end);
      }
   }
else
   {
   CfOut(cf_error,"","Failed regular expression match 3\n");
   }

if (BlockTextMatch("[a-z]+","1234abcd6789",&start,&end))
   {
   CfOut(cf_error,""," -> BlockTextMatch - ok\n");
   
   if (start != 4)
      {
      CfOut(cf_error,"","Start was not at 4 -> %d\n",start);
      }
   
   if (end != 8)
      {
      CfOut(cf_error,"","Start was not at 8 -> %d\n",end);
      }
   }
else
   {
   CfOut(cf_error,"","Failed regular expression match 3\n");
   }
}


/*****************************************************************************/

void TestAgentPromises()

{ struct Attributes a;
  struct Promise pp;

pp.conlist = NULL;  

printf("%d. Testing promise attribute completeness\n",++NR);
 
a = GetFilesAttributes(&pp);
a = GetReportsAttributes(&pp);
a = GetExecAttributes(&pp);
a = GetProcessAttributes(&pp);
a = GetStorageAttributes(&pp);
a = GetClassContextAttributes(&pp);
a = GetTopicsAttributes(&pp);
a = GetOccurrenceAttributes(&pp);
GetMethodAttributes(&pp);
GetInterfacesAttributes(&pp);
GetInsertionAttributes(&pp);
GetDeletionAttributes(&pp);
GetColumnAttributes(&pp);
GetReplaceAttributes(&pp);

printf(" -> All non-listed items are accounted for\n");
}
