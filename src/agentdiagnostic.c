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

/*****************************************************************************/
/*                                                                           */
/* File: selfdiagnostic.c                                                    */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/
/* new test suite                                                            */
/*****************************************************************************/

#ifdef BUILD_TESTSUITE

#include "testoutput.h"
#include "testinput.h"

// types: 1-100

#define CF_FILE 1
#define CF_DIR 2
#define CF_POLICY 3
#define CF_PROCESS 4
#define CF_LINE 5
#define CF_LINES 6
#define CF_EXTERNAL 7

#define CF_FILE_TEXT "file"
#define CF_DIR_TEXT "dir"
#define CF_POLICY_TEXT "policy"
#define CF_PROCESS_TEXT "process"
#define CF_LINE_TEXT "line"
#define CF_LINES_TEXT "lines"
#define CF_EXTERNAL_TEXT "external"

// actions: 101-200
#define CF_ACCESS 101
#define CF_BACKUP 102
#define CF_DELETE 103
#define CF_RESTORE 104
#define CF_EXISTS 105
#define CF_NOTEXISTS 106
#define CF_EXECUTE 107
#define CF_CREATE 108

// prepare actions (checks)
// #define CF_LINEEXISTS 109 // TODO: or line("some text", filename, EXISTS) ?? // due to parser limitations, this cannot be distinguished as either a type or an action; Used as action for now
// #define CF_LINEEXISTS_TEXT "line_exists" 

#define CF_ACCESS_TEXT "ACCESS"
#define CF_BACKUP_TEXT "BACKUP" 
#define CF_DELETE_TEXT "DELETE"
#define CF_RESTORE_TEXT "RESTORE"
#define CF_EXISTS_TEXT "EXISTS"
#define CF_NOTEXISTS_TEXT "NOTEXISTS"
#define CF_EXECUTE_TEXT "EXECUTE"
#define CF_CREATE_TEXT "CREATE"

// sub sections: 201-220, 
#define CF_VARS 201
#define CF_PRE 202
#define CF_EXEC 203
#define CF_OUT 204
#define CF_CLEAN 205
#define CF_CLASS 206

#define CF_VARS_TEXT "vars"
#define CF_PRE_TEXT "prepare"
#define CF_EXEC_TEXT "execute"
#define CF_OUT_TEXT "output"
#define CF_CLEAN_TEXT "cleanup"
#define CF_CLASS_TEXT "class"

// CF TESTSUITE CONSTANTS
#define CF_MAX_TESTS 100
#define CF_VARSIZE 20

// Output types: 501-600
#define CF_OUT_REPORT 501
#define CF_OUT_ADDLINE 502 //add line to a file 
#define CF_OUT_REMOVELINE 503
#define CF_OUT_KILLPROCESS 504
#define CF_OUT_STARTPROCESS 505
#define CF_OUT_STARTSERVICE 506
#define CF_OUT_STOPSERVICE 507
#define CF_OUT_INFO 508 // when running with -I option
#define CF_OUT_ADDUSER 509
#define CF_OUT_REMOVEUSER 510
#define CF_OUT_ADDLINES 511
#define CF_OUT_REMOVELINES 512
#define CF_OUT_REPORTS 513
#define CF_OUT_USEREXISTS 514
#define CF_OUT_FILEPERMS 515
#define CF_OUT_OWNER 516
#define CF_OUT_ISLINK 517
#define CF_OUT_LINKEDTO 518

#define CF_REPORT_TEXT "report" 
#define CF_ADDLINE_TEXT "addline" 
#define CF_REMOVELINE_TEXT "removeline"
#define CF_KILLPROCESS_TEXT "killprocess"
#define CF_STARTPROCESS_TEXT "startprocess"
#define CF_STARTSERVICE_TEXT "startservice"
#define CF_STOPSERICE_TEXT "stopservice"
#define CF_INFO_TEXT "info" // when running with -I option
#define CF_ADDUSER_TEXT "adduser"
#define CF_REMOVEUSER_TEXT "removeuser"
#define CF_ADDLINES_TEXT "addlines"
#define CF_REMOVELINES_TEXT "removelines"
#define CF_REPORTS_TEXT "reports"
#define CF_USEREXISTS_TEXT "userexists"
#define CF_FILEPERMS_TEXT "checkfileperms"
#define CF_OWNER_TEXT "checkowner"
#define CF_ISLINK_TEXT "islink"
#define CF_LINKEDTO_TEXT "linkedto"


#define CF_ALL_TEXT "all"
#define CF_REPORT_TEXT "report"
#define CF_SLIST_TEXT "slist"
#define CF_STRING_TEXT "string"
#define CF_PROCESS_TEXT "process"
#define CF_FILE_TEXT "file"
#define CF_DIR_TEXT "dir"
#define CF_CMD_TEXT "cmd"
#define CF_OTHER_TEXT "other"
#define CF_TOP10_TEXT "top10"

// predefined operations
#define BOOTSTRAP_TEXT "bootstrap"
#define BOOTSTRAP 601

// Functions: 701 - +
#define CF_FXN_APPENDLINE 701
#define CF_APPENDLINE_TEXT "appendLine"

#define CF_FXN_CMPENV 702
#define CF_GETENV_TEXT "cmpEnv"

#define CF_FXN_CLEARENV 703
#define CF_CLEARENV_TEXT "clearEnv"




/*****************************************************************************/

struct vars
   {
   char name[CF_VARSIZE]; 
   int value; // 0 = pass, 1 = fail
   int index;
   };

/*****************************************************************************/

struct line_data
   {
   int type;
   int action;
   char text[CF_BUFSIZE];
   char buf[CF_SMALLBUF];
   int var_index;
   };

/*****************************************************************************/

struct decision
   {
   int var_index;
   int value;
   };

/*****************************************************************************/

struct input 
   {
   char id[CF_SMALLBUF];
   int predefinedId;
   struct line_data PRE[10];
   int nPre;
   char policy_file[CF_SMALLBUF];
   char policy_opt[5];
   struct line_data expected[10];
   int nOutput;
   struct line_data POST[5];
   int nPost;
   
   struct vars internal[10]; // just 10 variables for now
   
   struct decision PASS_IF[10];
   int nVar;

   int test_class;	
   };

/*****************************************************************************/

struct cfoutput
   {
   char id[CF_SMALLBUF];
   char output[CF_EXPANDSIZE];
   int exec_ok;
   int pass; 
   
   struct decision results[10]; // variable results
   };

/*****************************************************************************/
/* new test suite                                                            */
/*****************************************************************************/

/*****************************************************************************/
/* Prototypes                                                                */
/*****************************************************************************/

void TestSuite(char *s);

struct input DATA[CF_MAX_TESTS];
int nInput;
int GetLineCount(char *file);
int LoadInput(struct cfoutput*, char*);
int RunPolicies(struct cfoutput*, int, struct input*, int);
int CompareOutput(struct cfoutput*, int, struct input*, int);

int Parse(struct input*, char *);
int GetSubSection(char *);
int GetExec(char *, char *, char *);
int GetExpectedOutput(char *, struct line_data *, char *);
int GetPrepareCleanup(char *, struct line_data *, char *);
int GetVarIndex(char *test_id, char *var);
int GetType1(char *); 
int GetClassType(char *s);

int RemoveChars(char *, char *);
int RemoveComment(char *); // at the end of line
int IsDuplicate(char*, struct input*, int);

// Before execution of test: Prepare
int DoIt(struct line_data*);
int GetAction(char *);
int CheckExists(struct line_data*);
int AppendTextToFile(char *, char *);

// Verify Output Section
int FindTextInFile(char *str, char *f);
int FindProcess(char *);
int MemGetExpectedOutput(char *buf, char *name);

// Show the results
void ShowResults(struct cfoutput* a, int n1);

//Cleanup
int Cleanup(struct cfoutput*,struct input*, int );

// Utility methods
long fsize(char *);
int ReplaceCharTS(char *str, char to, char frm);
int FlattenText(char *str);
int GetOptions(int argc, char *argv[]);
int MyCreate(struct line_data *);
int ReadLineInput(char *dst, char *frm );

//used only for internal testing
void PrintChars(char *);

// check if the operation is predefined
int CheckPredefined(char *);
int ExecutePreDefined(int id, char *);
int PerformBootstrap(char *);
int FileExists(char *file);

char TEST_ROOT_DIR[CF_BUFSIZE]; 

#endif

/*****************************************************************************/

void AgentDiagnostic(char *file)

{
if (VERBOSE || DEBUG)
   {
   FREPORT_TXT = stdout;
   FREPORT_HTML = fopen(NULLFILE,"w");
   FKNOW = fopen(NULLFILE,"w");
   }
else
   {
   FREPORT_TXT= fopen(NULLFILE,"w");
   FREPORT_HTML= fopen(NULLFILE,"w");
   FKNOW = fopen(NULLFILE,"w");
   }

//getcwd(cwd,CF_BUFSIZE);

printf("----------------------------------------------------------\n");
printf("Cfengine 3 - Performing level 2 self-diagnostic (dialogue)\n");
printf("----------------------------------------------------------\n\n");
TestVariableScan();
TestExpandPromise();
TestExpandVariables();
//TestSearchFilePromiser();
CheckInstalledLibraries();

#ifdef BUILD_TESTSUITE

if(file != NULL)
{	
   
printf("----------------------------------------------------------\n");
printf("Cfengine 3 - Performing test suite                        \n");
printf("----------------------------------------------------------\n\n");
InitializeGA(0,NULL);
 TestSuite(file);
}

#else  
printf("!! Extensive self-diagnostic capabilities not built in\n");
#endif

}

/******************************************************************/

void TestSearchFilePromiser()

{ struct Promise pp;

/* Still have diagnostic scope */
THIS_AGENT_TYPE = cf_agent;
   
pp.promiser = "the originator";
pp.promisee = "the recipient with $(two)";
pp.classes = "proletariat";
pp.petype = CF_SCALAR;
pp.lineno = 12;
pp.audit = NULL;
pp.conlist = NULL;
pp.agentsubtype = "none";

pp.bundletype = "bundle_type";
pp.bundle = "test_bundle";
pp.ref = "commentary";
pp.agentsubtype = strdup("files");
pp.done = false;
pp.next = NULL;
pp.cache = NULL;
pp.inode_cache = NULL;
pp.this_server = NULL;
pp.donep = &(pp.done);
pp.conn = NULL;
printf("\nTestSearchFilePromiser(%s)\n\n",pp.promiser);
LocateFilePromiserGroup(pp.promiser,&pp,VerifyFilePromise);

pp.promiser = "/var/[^/]*/[c|l].*";
printf("\nTestSearchFilePromiser(%s)\n\n",pp.promiser);
LocateFilePromiserGroup(pp.promiser,&pp,VerifyFilePromise);

pp.promiser = "/var/[c|l][A-Za-z0-9_ ]*";
printf("\nTestSearchFilePromiser(%s)\n\n",pp.promiser);
LocateFilePromiserGroup(pp.promiser,&pp,VerifyFilePromise);

AppendConstraint(&(pp.conlist),"path","literal",CF_SCALAR,NULL,false);
pp.promiser = "/var/[^/]*/[c|l].*";
printf("\nTestSearchFilePromiser(%s)\n\n",pp.promiser);
LocateFilePromiserGroup(pp.promiser,&pp,VerifyFilePromise);

pp.promiser = "/var/.*/h.*";
printf("\nTestSearchFilePromiser(%s)\n\n",pp.promiser);
LocateFilePromiserGroup(pp.promiser,&pp,VerifyFilePromise);
}

/*******************************************************************************/

#ifdef BUILD_TESTSUITE

void TestSuite(char *s)

{ char output[CF_EXPANDSIZE],command[CF_BUFSIZE], c[CF_BUFSIZE];  
  int i = 0, j = 0, nMap = 0, nInput = 0;

if(s == NULL)
{
   snprintf(s,CF_BUFSIZE,"%s","input.in");
   return;
}
   
if(FileExists(s) != 1)
  {
     printf("\tFatal Error: Couldn't find file \"%s\"\n",s);
     return;
  }
   
printf("\tParsing Input file (%s)... ",s);
int count = Parse(DATA,s);
printf("\tDone\n\n");
printf("\tNumber of tests  = %d\n", count);
printf("\tPrepare and run ... \n");

struct cfoutput ACTUAL[200];

int nErr = RunPolicies(ACTUAL, count, DATA, count);
printf("\tDone\n\n");
// TODO: what to do if some cases failed?

printf("\tComparing Outputs...");
int nFailed = CompareOutput(ACTUAL, count, DATA, count);
printf("\tDone\n\n");

printf("\tCleaning up ...");
Cleanup(ACTUAL, DATA, count);
printf("Done\n\n");

ShowResults(ACTUAL, count);
//printf("\nFailed Tests count = %d\n", nFailed);
}

/*********************************************************/

int Cleanup(struct cfoutput *out, struct input *in, int n)

{ int i, j;

 for(i = 0; i < n; i++)
    {
    for(j = 0; j < in[i].nPost; j++)
       {
       if (DoIt(&in[i].POST[j]) > 0)
	  {
          //if(strlen( in[i].internal[in[i].POST[j].var_index].name) > 0)
          {
          //actual[k].results[map[i].POST[j].var_index].value = 1;
          //  actual[k].results[map[i].PRE[j].var_index].var_index = m$
          }
          }
       }
    }
 
return 0;
}

/*********************************************************/

int GetLineCount(char *f)

{ char line[400];
 int i=0;
 FILE *in = fopen(f,"rb");

if (in == NULL)
   {
   printf("GetLineCount(): Error opening file: %s\n", f);
   return -1;
   }

while (fgets(line,400,in))
   {      
   i++;
   }
fclose(in);
return i;
}

/*********************************************************/

int LoadInput(struct cfoutput *i, char *f)

{ char line[CF_SMALLBUF];
  int n = 0, j;
  FILE *in = fopen(f,"rb");

if (in == NULL)
   {
   printf("LoadInput(): Error opening file: %s\n", f);
   return -1;
   }

while (fgets(line,CF_SMALLBUF,in))
   {
   // replace \n by \0
   for (j=0;j<CF_SMALLBUF;j++)
      {
      if (line[j] == '\n' || j == (CF_SMALLBUF - 1))
         {
         line[j] = '\0';
         }
      }
   
   snprintf(i[n].id,CF_SMALLBUF, "%s",line);
   n++;
   }

fclose(in);
return n;
}

/*********************************************************/

int MyCreate(struct line_data *p)

{ struct Attributes a = {{0}};
 char buf[CF_BUFSIZE];
 char file[CF_BUFSIZE];

a.transaction.report_level = cf_noreport;
snprintf(file,CF_BUFSIZE,"/tmp/%s",p->buf);
switch(p->type)
   {
   case CF_FILE:
       
       if (CfCreateFile(file,NULL,a))
	  {
          Debug("MyCreate(): Created file \"%s\n ",file);
          return 1;
	  }
       break;
       
   case CF_DIR:
       memset(buf, 0, CF_BUFSIZE);
       snprintf(buf, CF_BUFSIZE, "%s", file);
       strcat(buf, "/a");
       if(MakeParentDirectory(buf,true))
	  {
          Debug("MyCreate(): Created dir \"%s\n ",p->buf);
          return 1;
	  }
       break;
       
   }

return -1;
}

/*********************************************************/

int DoIt(struct line_data *p)

{ char buf[CF_EXPANDSIZE];
  char *pBuf = buf;
  struct Attributes a = {{0}};
  char file[CF_BUFSIZE];
 
switch(p->action)
   {
   case CF_ACCESS: //101
       snprintf(file,CF_BUFSIZE,"/tmp/%s",p->buf);      
       if (CfCreateFile(file,NULL,a))
	  {
          Debug("DoIt(): Created file \"%s\" ",file);
	  }
       sleep(1); // this is just a workaround (for time difference for the touch operation) 
       //TODO: find better method
       TouchFile(file,NULL,a,NULL);
       break;
       
   case CF_BACKUP: //102
       break;
       
   case CF_DELETE: //103
      snprintf(file,CF_BUFSIZE,"/tmp/%s",p->buf);      
       if(remove(file) == 0)
	  {
          return 1;
	  }       
       break;
       
   case CF_RESTORE: // 104
       break;
       
   case CF_EXISTS: //105
       if(CheckExists(p) > 0)
	  { 
          //printf("DoIt(): Hurray!!! The line exists\n");
          return 1;
	  }
       break;
       
   case CF_NOTEXISTS: //106
       if (CheckExists(p) < 0)
	  { 
          return 1;
	  }
       break;	
       
   case CF_EXECUTE: //106
       if (GetExecOutput(p->text,buf,false))
	  {
          Debug("DoIt(): Output of \"%s\": %s\n", p->text, buf);
	  }
       break;
       
   case CF_CREATE:
       
       if (MyCreate(p))
          {
          return 1;
          }
       
       break;
       
   default:
       switch(p->type)
	  {
          case CF_EXTERNAL:
              if (GetExecOutput(p->text,buf,false))
                 {
                 Debug("DoIt(): Output of \"%s\": %s\n", p->text, buf);
                 }
              
              break;
              
          case CF_OUT_FILEPERMS:
              
              break;
              
          case CF_FXN_APPENDLINE:
              //if (cfstat(m[j].expected[k].text, &sb) != -1)
	     snprintf(file,CF_BUFSIZE,"%s/tests/regression_test/tmp/%s",TEST_ROOT_DIR,p->buf);      
              strcat(file, "\n");
              if (AppendTextToFile(p->buf, p->text) != -1)
                 {                 
                 }
              else
                 {
                 printf("DoIt(): Error writing to file: %s\n", p->buf);
                 }
              
              break;
              
          case CF_FXN_CMPENV:

              memset(buf,0,CF_EXPANDSIZE);

              if ((pBuf = getenv(p->buf)) != NULL)
                 {
                 //if(strcmp)
                 Debug("######## ENV = %s\n", buf);
                 return 1;
                 }
              
              break;
              
          case CF_FXN_CLEARENV:
              break;              
	  }      
   }
return -1;
}

/*********************************************************/

int CheckExists(struct line_data *p)

{ char *text;
 struct stat sb;
 char file[CF_BUFSIZE];

switch(p->type)
   {
   case CF_DIR:
   case CF_FILE:
      snprintf(file,CF_BUFSIZE,"/tmp/%s",p->buf);      
       if(cfstat(p->buf, &sb) == 0)
	  {
          // printf("\"%s\" exists!!\n", p->buf);
          return 1;
	  }
//	printf("\"%s\" doesn't exist or you don't have enough permissions!!\n", p->buf);
       break;
       
      case CF_PROCESS:
          break;
          
   case CF_OUT_REMOVELINE:
   case CF_OUT_ADDLINE:
      case CF_LINE:
      
       if ((strlen(p->buf) < 1) || (strlen(p->text) < 1))
          {
          return -1;
          }
       snprintf(file,CF_BUFSIZE,"/tmp/%s",p->buf);      
       if(FindTextInFile(p->text, p->buf) > 0)
          {
          return 1;
          }
       
       break;
       
   case CF_OUT_REMOVELINES:
   case CF_OUT_ADDLINES:
   case CF_LINES:
       if((strlen(p->buf) < 1) || (strlen(p->text) < 1))
          {
          return -1;
          }
       
       text = (char *) malloc(CF_BUFSIZE * sizeof(char));

       if(text == NULL)
	  {
          printf("CheckExists(): Memory allocation error!!\n");
          return -1;
	  }
       
       if (MemGetExpectedOutput(text, p->text) != 0)
	  {
	  if (text == NULL)
	     {
             printf("CheckExists(): Error reading Expected Output for \"%s\"!!\n",p->text);
             return -1;
	     }   
	  }
       //text = (char *) CfReadFile(p->text, CF_BUFSIZE);
       
       FlattenText(text);
       Chop(text);
       
       if(FindTextInFile(text, p->buf) > 0)
	  {
          free(text);
          return 1;
	  }
       free(text);
       break;
       
   default: // Check: not necessary, parser handles this??
       break;
       
   }
return -1;
}

/*********************************************************/

int RunPolicies(struct cfoutput *actual, int nInput, struct input *map, int nMap)

{ int i = 0, j = 0, k = 0;
  char opts[] = " -";
  char command[CF_BUFSIZE], c[CF_BUFSIZE], output[CF_EXPANDSIZE];
  int failures = 0;   
  char full_filepath[CF_BUFSIZE]; 
  snprintf(c, CF_BUFSIZE, "%s%cbin%ccf-agent -f ",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR);
   
for (i = 0; i < nInput; i++)
   {
//   if ((map[i].test_class != CFTEST_CLASS) && (CFTEST_CLASS != 0))
//      {
//      continue;
//      }

   // execute predefined
   // ExecutePreDefined
   snprintf(actual[k].id,CF_SMALLBUF,"%s", map[i].id);
   if(map[i].predefinedId > 0)
	{
	   if(ExecutePreDefined(map[i].predefinedId, map[i].policy_file) == 1) //TODO: change policy_file to buf
	     {
		actual[k].pass = 1;
	     }
	   else
	     {
		actual[k].pass = 0;
	     }
	}
	
   snprintf(command,CF_BUFSIZE, "%s",c);
   snprintf(full_filepath,CF_BUFSIZE,"%s/tests/units/%s",TEST_ROOT_DIR,map[i].policy_file);
   strcat(command, full_filepath);
   strcat(command, opts);
   strcat(command, map[i].policy_opt);

//   printf("RunPolicies: Command = %s\n",command);
   
   // prepare
   for (j = 0; j < map[i].nPre; j++)
      {
      if (DoIt(&map[i].PRE[j]) > 0)
         {
         if(strlen( map[i].internal[map[i].PRE[j].var_index].name) > 0)
            {
            actual[k].results[map[i].PRE[j].var_index].value = 1;
            actual[k].results[map[i].PRE[j].var_index].var_index = map[i].PRE[j].var_index; // TODO: this might not be necessary, instead of struct decision[], int [] will do in the srtuct defn
	    }
         }
      
      }
      
   //		  execute policy
   if (GetExecOutput(command,output,false))
      {      
      snprintf(actual[k].output,CF_EXPANDSIZE, "%s",output);
      actual[k].exec_ok = 1;
      Debug("RunPolicies(%d): %s, %s\n%s\n", nInput,output, actual[k].id, command);
      }
   else
      {
      actual[k].exec_ok = 0;
      failures++;
      printf("RunPolicies(): Error running Cfengine Test Policy \"%s\"\n", actual[k].id);
      }
   k++;
   }
return failures;
}

/*********************************************************/

void ShowResults(struct cfoutput* a, int n1)//, struct input *m, int n2)

{ int i, j, k;

printf("\n \n------------------- Regression test Report -------------------\n");

for (i = 0; i < n1; i++)
   {
   printf("TEST ID = %s, ", a[i].id);
   
   printf(" Result = ");

   if(a[i].pass)
      {
      printf("PASS\n");
      }else
      {
      printf("FAIL\n");
      }
   }
printf("\n");
}

/*********************************************************/

int CompareOutput(struct cfoutput* a, int n1, struct input *m, int n2)

{ int i, j, count = 0/*num of FAILED tests*/, k;
  char r[] = "R: ";
  char any[] = ".*";
  char expected[CF_EXPANDSIZE];
  char *sp;
  char textToFind[CF_SMALLBUF],*filename, *user, *text;
  int pass = 0;
   
  // for file permission
  struct stat sb;
  int perm;
  
  char buf[CF_BUFSIZE];
  int temp;
   
  char file[CF_BUFSIZE];
  
//  snprintf(TMP_DIR,CF_BUFSIZE,"%s%s",CFWORKDIR,"/regression_test/tmp/");
   
 for(i = 0; i < n1; i++)
   {
   for(j = 0; j < n2; j++)
      {
//      if((m[j].test_class != CFTEST_CLASS) && (CFTEST_CLASS != 0) )
//         {
//         continue;
//         }
      
      if( strcmp(a[i].id, m[j].id) == 0)
         {
         if(strlen(a[i].id) > 0 && strlen(m[j].id)>0)
            {
            for(k = 0; k < m[j].nOutput;k++)		      
               {
               switch(m[j].expected[k].type)
                  {
                  case CF_OUT_REPORT:
                      
                      strcpy(expected, r);
                      strcat(expected, m[j].expected[k].text);
		     
                      if(FullTextMatch(expected, a[i].output))
                         {
                         a[i].pass = 1;
                         }else
                         {
                         a[i].pass = 0;
                         k = m[j].nOutput; // proceed to next ID
                         count++;
                         }
                      break;
                      
                  case CF_OUT_ADDLINE:
		     snprintf(file,CF_BUFSIZE,"/tmp/%s",m[j].expected[k].buf);      
                      if(FindTextInFile(m[j].expected[k].text,file) > 0)
                         {
                         a[i].pass = 1;				   
                         }else
                         {
                         a[i].pass = 0;	   
                         k = m[j].nOutput; // proceed to next ID
                         count++;
                         }
                      break;
                      
                  case CF_OUT_REMOVELINE:
		     snprintf(file,CF_BUFSIZE,"/tmp/%s",m[j].expected[k].buf);      
                      if((FindTextInFile(m[j].expected[k].text,file))<=0)
                         {
                         a[i].pass = 1;
                         }
                      else
                         {
                         a[i].pass = 0;
                         k = m[j].nOutput; // proceed to next ID
                         count++;
                         }
                      break;
                      
                  case CF_OUT_KILLPROCESS:
                      //			    printf("Kill Process: %s\n", m[j].expected[k].text);
                      if(FindProcess("apache2") > 0)
                         {
                         Debug("CompareOutput(): Found Process \"%s\"\n", "apache2");
                         }
                      break;
                      
                  case CF_OUT_STARTPROCESS:
                      // TODO: fill this space
                      break;
                      
                  case CF_OUT_STARTSERVICE:
                      break;
			    
                  case CF_OUT_STOPSERVICE:
                      break;
                      
                  case CF_OUT_INFO:
                      // flatten text

                      FlattenText(a[i].output);

                      if (FullTextMatch(m[j].expected[k].text, a[i].output))
                         {
                         a[i].pass = 1;				   
                         }else
                         {
                         a[i].pass = 0;
                         k = m[j].nOutput; // proceed to next ID
                         count++;
                         }
                      break;
                      
                  case CF_OUT_USEREXISTS: 
                  case CF_OUT_ADDUSER:
                      
                      user = m[j].expected[k].text;
                      filename = m[j].expected[k].buf;
                      
                      // just checking in the passwd file, and not shadow and group
                      snprintf(filename, CF_SMALLBUF, "%s", "passwd");
		     snprintf(file,CF_BUFSIZE,"/tmp/%s",filename);      
                      snprintf(textToFind, CF_SMALLBUF, "%s", "[ ]*");
                      strcat(textToFind, user);
                      strcat(textToFind, ":");
                      strcat(textToFind, any);
                      
                      if((FindTextInFile(textToFind, file))>0)
                         {
                         a[i].pass = 1;
                         }else
                         {
                         a[i].pass = 0;
                         k = m[j].nOutput; // proceed to next ID
                         count++;
                         }
                      break;
                      
                  case CF_OUT_REMOVEUSER:
                      break;
                      
                  case CF_FILE:
                  case CF_LINE:
                      if(DoIt(&m[j].expected[k]) > 0)
                         {
                         a[i].pass = 1;
                         }else
                         {
                         a[i].pass = 0;
                         k = m[j].nOutput; // proceed to next ID
                         count++;
                         }
                      break;
                      
                  case CF_OUT_REMOVELINES:
                  case CF_OUT_ADDLINES:
		   case CF_LINES:
                      if(DoIt(&m[j].expected[k]) > 0)
                         {
                         a[i].pass = 1;
                         }else
                         {
                         a[i].pass = 0;
                         k = m[j].nOutput; // proceed to next ID
                         count++;
                         }
                      break;
                      
                  case CF_OUT_REPORTS:
                      
		      if (MemGetExpectedOutput(buf, m[j].expected[k].text) == 0)
		       {			  
                         if(buf == NULL)
                          {
                            printf("CompareOutput(): Error Reading Expected Output for: %s", m[j].id);
                          }
		         else
                          {
                            FlattenText(a[i].output);
                            FlattenText(buf);
                         
			    if(FullTextMatch(buf, a[i].output))
                             {
                               a[i].pass = 1;
			       break;
                             }
                           }
		        }

		       // The test failed
		       a[i].pass = 0;
		       k = m[j].nOutput; // proceed to next ID
                       count++;
                      break;
                      
                  case CF_OUT_FILEPERMS:
                      if (cfstat(m[j].expected[k].text, &sb) != -1)
                         {
                         sb.st_mode & 0xFFFF;
                         sscanf(m[j].expected[k].buf, "%o", &perm);
                         if(perm == (sb.st_mode & 0777))
                            {
                            a[i].pass = 1;
                            break;
                            }
                         }
                      
                      //failure condition 
                      
                      a[i].pass = 0;
                      k = m[j].nOutput; // proceed to next ID
                      count++;
                      break;
                      
                  case CF_OUT_OWNER:
                      temp = GetOwnerName( m[j].expected[k].text, &sb, buf, sizeof(buf));
                      if (temp && (strcmp(buf, m[j].expected[k].buf) == 0))
                         {
                         a[i].pass = 1;
                         }			      
                      else
                         { 
                         a[i].pass = 0;
                         k = m[j].nOutput; // proceed to next ID
                         count++;
                         }
                      
                      break;
                      
                  case CF_OUT_ISLINK:			      
                      if (lstat(m[j].expected[k].text, &sb) != -1)
                         {				   
                         if(S_ISLNK(sb.st_mode))
                            {
                            a[i].pass = 1;
                            break;
                            }
                         }
                      //FAILURE
                      a[i].pass = 0;
                      k = m[j].nOutput; // proceed to next ID
                      count++;
                      break;
                      
                  case CF_OUT_LINKEDTO:

                      /* INCOMPLETE TODO
                         if (lstat(m[j].expected[k].text, &sb) != -1)
                         {				   
                         if(S_ISLNK(sb.st_mode))
                         {		
                         memset(buf,0,CF_BUFSIZE);
                         
                         readlink(m[j].expected[k].text,buf,CF_BUFSIZE-1);
                         a[i].pass = 1;
                         break;
                         }
                         }
                         //FAILURE
                         a[i].pass = 0;
                         k = m[j].nOutput; // proceed to next ID
                         count++;
                         
                      */
                      break;
                      
                      //case CF_LINES:
                      // break;
                  }
               }
            }
         }
      }
   }

return count; // failed Count
}

/*********************************************************/

int MemGetExpectedOutput(char *buf, char *name)

{ 
  if (strcmp(name, O_CHDIR_TEXT) == 0)
  {
    snprintf(buf, CF_BUFSIZE, "%s", O_CHDIR);
    return 0;
  }
  else if (strcmp(name, O_LISTS_TEXT) == 0)
  {
    snprintf(buf, CF_BUFSIZE, "%s", O_LISTS);
    return 0;
  }
  else if (strcmp(name, O_CLASSVAR_CONVERGENCE_TEXT) == 0)
  {
    snprintf(buf, CF_BUFSIZE, "%s", O_CLASSVAR_CONVERGENCE);
    return 0;
  }
  else if (strcmp(name, O_DOLLAR_TEXT) == 0)
  {
    snprintf(buf, CF_BUFSIZE, "%s", O_DOLLAR);
    return 0;
  }
  else if (strcmp(name, O_INSERT_LINES_TEXT) == 0)
  {
    snprintf(buf, CF_BUFSIZE, "%s", O_INSERT_LINES);
    return 0;
  }
   
 return -1;
}

/*********************************************************/

int AppendTextToFile(char *text, char *file)

{ FILE *f = fopen(file, "a");

if (f == NULL)
   {
   return -1;
   }

fprintf(f,"%s", text);

fclose(f);
return 1;
}

/*********************************************************/

int Parse(struct input *data,char *inputfile)

{ char line[CF_SMALLBUF], temp[CF_SMALLBUF];
  char f[CF_SMALLBUF];
  int n = 0, j, i;
  int section = -1;
  int nLineCount = 0;
  int isDuplicate = 0;
  int nLineNum = 0, preDefinedType = -1;
  unsigned int nTemp;
  int blockStarted = 0;
  struct input *in = data;
   
  snprintf(f,CF_SMALLBUF,"%s",inputfile);
  FILE *inFile = fopen(f,"r");

/* Locals for reading line from test script in memory */
  int nPointer = 0;
  int total_size;
  int size;
  char *frm;
  
  int global_var_found = 0;
   
//if (CF_TEST_INPUT == NULL)
//   {
//   printf("Parse(): Error in input test script!\n");
//   return -1;
//   }
//total_size = strlen(CF_TEST_INPUT);
//frm = CF_TEST_INPUT;
   
THIS_AGENT_TYPE = cf_agent;
while(fgets(line, CF_SMALLBUF, inFile))
//while(nPointer < total_size)
   { 
//   size = ReadLineInput(line, frm + nPointer);
//   nPointer += size + 1;

   nLineNum++;
   if (n > CF_MAX_TESTS)
      {
      printf("Parse(): Number of tests exceeded maximum allowed (%d)!!\n", CF_MAX_TESTS);
      break;
      }
   
   // replace \n by \0
   for (j=0;j<CF_SMALLBUF;j++)
      {
      if(line[j] == '\n' || j == (CF_SMALLBUF - 1))
         {         
         line[j] = '\0';
         }
      }
   
   char rtype,*retval;
   if (FullTextMatch("^[ ]*#.*",line))
      {
      // lINE IS A comment: ignore
      }
   else if (!blockStarted && FullTextMatch("^[ ]*([a-z0-9]+)(.*)[ ]*$",line))
	{
	   if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
	     {
		if( RemoveChars(retval,"();"))
		  {
		     snprintf(in[n].policy_file,CF_SMALLBUF,"%s",retval); // TODO: rename policy_file to a more generic name (eg. buffer)
		  }
	     }
	   
	   if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
	     {
		if( RemoveChars(retval,"();"))
		  {
		     preDefinedType = CheckPredefined(retval);
		     if(preDefinedType > 0)
		       {
			  snprintf(in[n].id, CF_SMALLBUF, "%s",retval);
			  in[n].predefinedId = preDefinedType;
			  n++;
		       }
		  }
	     }
	}
      else if (!blockStarted && FullTextMatch("^[ ]*@([A-Z_]+)=(.*)[ ]*$",line))
	{
	   global_var_found = 0;
	   if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
	     {
		if (strcmp(retval, "CF_DIR") == 0)
		  {
		     global_var_found = 1;
		  }
	     }
	  if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
	     {
		if(global_var_found == 1)
		  {
		     snprintf(TEST_ROOT_DIR,CF_BUFSIZE,"%s",retval); 
		  }
	     }
	}
      else if (FullTextMatch("^[ \t]*$",line))
     {
      // Line is empty: ignore
      }else	  
      {
      // trim spaces
      FullTextMatch("^[ \t]*(.+)[ \t]*$",line);
      
      if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
         {
         if (strcmp(retval,"{") == 0) //start of block
            {
	       blockStarted = 1;
            if(IsDuplicate(temp,in,n))
               {
               isDuplicate = 1;
               }else
               {
               snprintf(in[n].id, CF_SMALLBUF, "%s",temp);
               Debug("Parse(%d): %s\n", n, temp);
	       in[n].predefinedId = -1;
               }
            }
         else if(strcmp(retval,"}") == 0) //end of block of block
            {
            if(!isDuplicate)
               {
               n++;
               nInput = n;
               }
            isDuplicate = 0;
            section = -1;
	       blockStarted = 0;
            }
         else if(!isDuplicate) // inside a block
            {
            snprintf(temp,CF_SMALLBUF, "%s",retval);
            // sub-section names must contain only characters, TODO: finalize this defn
            if (FullTextMatch("^[ \t]*([A-Za-z]+)[ \t]*:[ \t]*$",temp))
               {
               if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype) // get the sub-sections
                  {
                  section = GetSubSection(retval);
                  Debug("Parse(%d): %s\n", section, retval);
                  nLineCount = 0; // reset the count for commands in each sub-section
                  continue;
                  }
               }

            // inside a sub-section
            switch(section)
               {
               case CF_VARS:
                   snprintf(in[n].internal[nLineCount].name, CF_VARSIZE, "%s", temp);
                   in[n].internal[nLineCount].index = nLineCount;
                   in[n].internal[nLineCount].value = -1;
                   in[n].nVar = ++nLineCount;
                   break;
                   
               case CF_PRE:
                   if(GetPrepareCleanup(in[n].id,&in[n].PRE[nLineCount], temp) > 0)
                      {
                      in[n].nPre = ++nLineCount;
                      }
                   break;
                   
               case CF_EXEC:
                   GetExec(in[n].policy_file,in[n].policy_opt, temp);
                   break;
                   
               case CF_OUT:
                   
                   if(GetExpectedOutput(in[n].id, &in[n].expected[nLineCount], temp))
                      {
                      in[n].nOutput = ++nLineCount;
                      }
                   break;
                   
               case CF_CLEAN:
                   if (GetPrepareCleanup(in[n].id, &in[n].POST[nLineCount], temp) > 0)
                      {
                      in[n].nPost = ++nLineCount;
                      }
                   break;
                   
               case CF_CLASS:
                   if((nTemp = GetClassType(temp)) >= 0)
                      {
                      in[n].test_class = nTemp;
                      }
                   break;	
               default: // TODO: not required??
                   Debug("Switch Default\n");
                   break;
               } /*switch*/
            } /* if -> inside block*/
         } /* if -> getvar*/
      }  /* if -> non-empty line */
      
//      free(line);
   } /*while getline*/
fclose(inFile);

// PRINT INPUT DATA
/*
for(i = 0; i<n; i++)
{
printf("\nParse(): ##### Test Policy %d  #####\n\n", i);
printf("Parse(): ID = %s\n",in[i].id);
	printf("Parse(): Policy File = %s\n",in[i].policy_file);
	printf("Parse(): Policy Option(s) = %s\n",in[i].policy_opt);
	printf("Parse(): Class Type = %d\n",in[i].test_class);

	for(j = 0; j < in[i].nVar; j++)
	  {
	     printf("\tParse(): Vars [%d] Index = %d\n", j, in[i].internal[j].index);
	     printf("\tParse(): Vars [%d] Index = %s\n", j, in[i].internal[j].name);
	  }

	for(j = 0; j < in[i].nPre; j++)
	  {
	     printf("\tParse(): Prepare [%d] Type = %d\n", j, in[i].PRE[j].type);
	     if(strlen(in[i].PRE[j].text) > 0)
	       {
		  printf("\tParse(): Prepare [%d] Line = %s\n", j, in[i].PRE[j].text);
	       }
	     printf("\tParse(): Prepare [%d] Action = %d\n", j, in[i].PRE[j].action);
	     printf("\tParse(): Prepare [%d] Name = %s\n", j, in[i].PRE[j].buf);
	     if(strlen(in[i].internal[in[i].expected[j].var_index].name))
	       printf("\tParse(): Variable[%d] = %s\n", in[i].PRE[j].var_index, in[i].internal[in[i].PRE[j].var_index].name);
	  }
	for(j = 0; j < in[i].nOutput; j++)
	  {
	     printf("\tParse(): Expected [%d] Type = %d\n", j, in[i].expected[j].type);
	     printf("\tParse(): Expected [%d] Result = %s\n", j, in[i].expected[j].text);
	     printf("\tParse(): Expected [%d] Action = %d\n", j, in[i].expected[j].action);
	     if(strlen(in[i].expected[j].buf) > 0)
	       printf("\tParse(): Expected [%d] Buffer(eg. filename) = %s\n", j, in[i].expected[j].buf);
	     if(strlen(in[i].internal[in[i].expected[j].var_index].name) > 0)
	       printf("\tParse(): Variable[%d] = %s\n", in[i].expected[j].var_index, in[i].internal[in[i].expected[j].var_index].name);
	  }
	for(j = 0; j < in[i].nPost; j++)
	  {
	     printf("\tParse(): Cleanup [%d] Type = %d\n", j, in[i].POST[j].type);
	     printf("\tParse(): Cleanup [%d] Action = %d\n", j, in[i].POST[j].action);
	     printf("\tParse(): Cleanup [%d] Name = %s\n", j, in[i].POST[j].buf);
	  }
	
	printf("### Counts ###\n");
	printf("Parse(): nPre = %d\n", in[i].nPre);
	printf("Parse(): nPost = %d\n", in[i].nPost);
	printf("Parse(): nPre = %d\n", in[i].nOutput);
	printf("Parse(): nVar = %d\n", in[i].nVar);
     } */
   return n;
}

/*********************************************************/

int GetClassType(char *s)

{   // TODO: make this function handle multiple class names separated by comma
   
if(strlen(s) < 1)
   {
   return -1;
   }

if(strcmp(s, CF_ALL_TEXT) == 0)
   {
   return CF_CLASS_ALL;
   }

if(strcmp(s, CF_REPORT_TEXT) == 0)
   {
   return CF_CLASS_REPORT;
   }

if(strcmp(s, CF_SLIST_TEXT) == 0)
   {
   return CF_CLASS_SLIST;
   }

if(strcmp(s, CF_STRING_TEXT) == 0)
   {
   return CF_CLASS_STRING;
   }

if(strcmp(s, CF_PROCESS_TEXT) == 0)
   {
   return CF_CLASS_PROCESS;
   }

if(strcmp(s, CF_FILE_TEXT) == 0)
   {
   return CF_CLASS_FILE;
   }

if(strcmp(s, CF_DIR_TEXT) == 0)
   {
   return CF_CLASS_DIR;
   }

if(strcmp(s, CF_CMD_TEXT) == 0)
   {
   return CF_CLASS_CMD;
   }

if(strcmp(s, CF_OTHER_TEXT) == 0)
   {
   return CF_CLASS_OTHER;
   }

if(strcmp(s, CF_TOP10_TEXT) == 0)
   {
   return CF_CLASS_TOP10;
   }

return CF_CLASS_ALL;
}

/*********************************************************/

int GetSubSection(char *s)

{
if(strlen(s) < 1)
   {
   return -1;
   }

if(strcmp(s, CF_PRE_TEXT) == 0)
   {
   return CF_PRE;
   }

if(strcmp(s, CF_EXEC_TEXT) == 0)
   {
   return CF_EXEC;
   }

if(strcmp(s, CF_OUT_TEXT) == 0)
   {
   return CF_OUT;
   }

if(strcmp(s, CF_CLEAN_TEXT) == 0)
   {
   return CF_CLEAN;
   }

if(strcmp(s, CF_VARS_TEXT) == 0)
   {
   return CF_VARS;
   }

if(strcmp(s, CF_CLASS_TEXT) == 0)
   {
   return CF_CLASS;
   }

return -1;
}

/*********************************************************/

int GetExec(char *filename, char *opts, char *s)

{ char rtype,*retval;
  char unwanted[] = "();";

if(FullTextMatch("[ \t]*([A-Za-z0-9_]+)[ \t]*[^A-Za-z0-9_/\"]\"(.+)\"[ \t]*,[ \t]*\"(.+)\"[ \t]*[^A-Za-z0-9_/\"][ \t]*",s))
   {
   if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
      {	
      if(strcmp(retval,CF_POLICY_TEXT) == 0)
         {
         if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
            {
            if( RemoveChars(retval,unwanted))
               {
               snprintf(filename, CF_BUFSIZE, "%s", retval);
               }
            Debug("GetExec(): %s\n",retval);
            
            if (GetVariable("match","3",(void *)&retval,&rtype) != cf_notype)
               {
               snprintf(opts, 5, "%s", retval); // TODO: change 5 to some constant defn; opts = test options, eg. K, KF
               }
            Debug("GetExec(): Options = %s\n", opts);
            }
         }
      }
   }
else
   {
   printf("GetExec(): Syntax Error\n"); // TODO: Display Line number
   return -1;
   }

return 0;
}

/*********************************************************/

int GetExpectedOutput(char *id, struct line_data *o, char *s)

{ char rtype,*retval;
  char unwanted[] = ";\")";
  int tmp;

if (strcmp(s,"NA") == 0) // assign NULL to the output ??
   {
   return -1;
   }

o->var_index = -1;

//TODO: Comments at the end of line not handled
// eg. var = line("line","file","EXISTS")'
// 

if(FullTextMatch("[ \t]*([A-Za-z0-9_]+)[ \t]*=[ \t]*(.+)[ \t]*[^A-Za-z0-9_/\"][ \t]*\"(.+)\"[ \t]*[,][ \t]*\"(.+)\"[ \t]*[,][ \t]*(.+)[^A-Za-z0-9_/\"]",s))
   {
   if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
      {
      if( RemoveChars(retval,unwanted))
         {
         if( (tmp = GetType1(retval)) > 0)
            {
            o->type = tmp;		    
            }
        // printf("Type = %s, %d\n", retval,tmp);
         }
      }
   
   if (GetVariable("match","3",(void *)&retval,&rtype) != cf_notype)
      {
      if( RemoveChars(retval,unwanted))
         {
         snprintf(o->text, CF_BUFSIZE, "%s", retval);
         }
      }
   if (GetVariable("match","4",(void *)&retval,&rtype) != cf_notype)
      {
      snprintf(o->buf, CF_SMALLBUF, "%s", retval);
      }
   if (GetVariable("match","5",(void *)&retval,&rtype) != cf_notype)
      {
      RemoveComment(retval);

      if ( RemoveChars(retval,unwanted))
         {		  
         o->action = GetAction(retval);
         }
      }
   if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
      {
      if((tmp = GetVarIndex(id, retval)) < 0)
         {
         printf(" GetPrepareCleanup(): Error!! Variable  %s is not defined in test \"%s\".",retval, id);
         o->var_index = -1;
         }
      else
         {
         o->var_index = tmp;
         }
      }
   return 1;
   }
else   
    // eg. var = addline("filename","text")
    if(FullTextMatch("[ \t]*([A-Za-z0-9_]+)[ \t]*=[ \t]*([A-Za-z0-9_]+)[ \t]*[^A-Za-z0-9_/\"]\"(.+)\"[ \t]*,[ \t]*\"(.+)\"[ \t]*[^A-Za-z0-9_/\"][ \t]*",s))
       {
       if (GetVariable("match","3",(void *)&retval,&rtype) != cf_notype)
	  {
          snprintf(o->text, CF_BUFSIZE, "%s", retval);
	     
          if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
             {
             // Debug(" GetExpectedOutput(): type = %s\n", retval);
             o->type = GetType1(retval);
             }
          
          if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
             {
             
             if((tmp = GetVarIndex(id, retval)) < 0)
                {
                Debug(" GetExpectedOutput(): Error!! Variable %s is not defined in test \"%s\"\n",retval, id);
                o->var_index = -1;
                }else
                {
                o->var_index = tmp;
//			 printf(" GetExpectedOutput(): var_index = %d\n", o->var_index);
                }
             }
          if (GetVariable("match","4",(void *)&retval,&rtype) != cf_notype)
             {
             Debug(" GetExpectedOutput(): Buffer = %s\n", retval);
             snprintf(o->buf, CF_SMALLBUF,"%s",retval);
             }
	  }
       
//	return 1;
       }
    else
        // eg. addline("line", "file")
        if(FullTextMatch("[ \t]*([A-Za-z0-9_]+)[ \t]*[^A-Za-z0-9_/\"]\"(.+)\"[ \t]*,[ \t]*\"(.+)\"[ \t]*[^A-Za-z0-9_/\"][ \t]*",s))
           {
           if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
              {
              snprintf(o->text, CF_BUFSIZE, "%s", retval);
              if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
		 {
                 // Debug(" GetExpectedOutput(): type = %s\n", retval);
                 o->type = GetType1(retval);
		 }

              if (GetVariable("match","3",(void *)&retval,&rtype) != cf_notype)
		 {
                 Debug(" GetExpectedOutput(): Buffer = %s\n", retval);
                 snprintf(o->buf, CF_SMALLBUF,"%s",retval);
		 }
              }
//	return 1;
           }
        else 
            if(FullTextMatch("[ \t]*(.+)[ \t]*[^A-Za-z0-9_/\"][ \t]*\"(.+)\"[ \t]*[,][ \t]*(.+)",s))
               {
               if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
                  {
                  if( RemoveChars(retval,unwanted))
//	    snprintf(type, CF_SMALLBUF, "%s", retval);
                      
                      if( (tmp = GetType1(retval)) > 0)
                         {
                         o->type = tmp;
                         }
                  
                  Debug("GetExpectedOutput(2): TYPE = %s; %d\n",retval,o->type);
                  }
               
               if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
                  {
                  if (RemoveChars(retval,unwanted))
                     {
                     snprintf(o->buf, CF_BUFSIZE, "%s", retval);
                     }
                  Debug("GetExpectedOutput(): NAME = %s\n", o->buf);
                  }       
               
               if (GetVariable("match","3",(void *)&retval,&rtype) != cf_notype)
                  {
                  RemoveComment(retval);
                  if ( RemoveChars(retval,unwanted))
                     {
                     //  snprintf(action, CF_SMALLBUF, "%s", retval);                     
                     o->action= GetAction(retval);
                     }
                  
                  Debug("GetExpectedOutput(): ACTION = %s, %d\n",retval, o->action);
                  }
               }   

// eg. insertuser("mark")
            else if(FullTextMatch("^([A-Za-z0-9_]+)[ \t]*[^A-Za-z0-9_/\"]\"(.+)\"[ \t]*[^A-Za-z0-9_/\"][ \t]*",s))
               {
               if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
                  {
                  //if( RemoveChars(retval,unwanted))
                  {
                  snprintf(o->text, CF_BUFSIZE, "%s", retval);
                  if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
                     {
                     // Debug(" GetExpectedOutput(): type = %s\n", retval);
                     o->type = GetType1(retval);
                     }
                  }
                  
                  Debug("GetExpectedOutput(): %s\n",retval);
//	  return 1;
                  }
               }
            else 
               {
               printf("GetExpectedOutput(): Syntax Error; %s\n",s); // display line number
               return -1;
               }

switch(o->type)
   {
   case CF_OUT_ADDLINES:
       o->action = CF_EXISTS;
       break;
       
   case CF_OUT_REMOVELINES:
       o->action = CF_NOTEXISTS;
       break;
       
   case CF_OUT_ADDLINE:
       o->action = CF_EXISTS;
       break;
       
   case CF_OUT_REMOVELINE:
       o->action = CF_NOTEXISTS;
       break;
   }

return 1;
}


/*********************************************************/

int GetType1(char *s)

{
if(strlen(s) < 0)
   {
   return -1;
   }

// output types
if(strcmp(CF_REPORT_TEXT, s) == 0)
   {
   return CF_OUT_REPORT;
   }

if(strcmp(CF_ADDLINE_TEXT, s) == 0)
   {
   return CF_OUT_ADDLINE;
   }

if(strcmp(CF_REMOVELINE_TEXT, s) == 0)
   {
   return CF_OUT_REMOVELINE;
   }

if(strcmp(CF_KILLPROCESS_TEXT, s) == 0)
   {
   return CF_OUT_KILLPROCESS;
   }

if(strcmp(CF_INFO_TEXT, s) == 0)
   {
   return CF_OUT_INFO;
   }

if(strcmp(CF_ADDUSER_TEXT, s) == 0)
   {
   return CF_OUT_ADDUSER;
   }

if(strcmp(CF_REMOVEUSER_TEXT, s) == 0)
   {
   return CF_OUT_REMOVEUSER;
   }

if(strcmp(CF_ADDLINES_TEXT, s) == 0)
   {
   return CF_OUT_ADDLINES;
   }

if(strcmp(CF_REMOVELINES_TEXT, s) == 0)
   {
   return CF_OUT_REMOVELINES;
   }

if(strcmp(CF_REPORTS_TEXT, s) == 0)
   {
   return CF_OUT_REPORTS;
   }

if(strcmp(CF_FILEPERMS_TEXT, s) == 0)
   {
   return CF_OUT_FILEPERMS;
   }

if(strcmp(CF_OWNER_TEXT, s) == 0)
   {
   return CF_OUT_OWNER;
   }

if(strcmp(CF_ISLINK_TEXT, s) == 0)
   {
   return CF_OUT_ISLINK;
   }

if(strcmp(CF_LINKEDTO_TEXT, s) == 0)
   {
        return CF_OUT_LINKEDTO;
   }

   //

if(strcmp(s,CF_FILE_TEXT) == 0)
   {
   return CF_FILE;
   }

if(strcmp(s, CF_DIR_TEXT) == 0)
   {
   return CF_DIR;
   }   

if(strcmp(s, CF_POLICY_TEXT) == 0)
   {
   return CF_POLICY;
   }   

if(strcmp(s, CF_PROCESS_TEXT) == 0)
   {
   return CF_PROCESS;
   }   

if(strcmp(s, CF_LINE_TEXT) == 0)
   {
   return CF_LINE;
   }

if(strcmp(s, CF_LINES_TEXT) == 0)
   {
   return CF_LINES;
   }

if(strcmp(s, CF_EXTERNAL_TEXT) == 0)
   {
   return CF_EXTERNAL;
   }

// functions
if(strcmp(s, CF_APPENDLINE_TEXT) == 0)
   {
   return CF_FXN_APPENDLINE;
   }

return -1;
}

/*********************************************************/

int GetPrepareCleanup(char *id, struct line_data *p, char *s)

{ char rtype,*retval;
  char unwanted[] = "() ;\"";
  char type[CF_SMALLBUF], name[CF_BUFSIZE], action[CF_SMALLBUF];
  
  struct line_data *data = p;
  int ret = -1;
  void *tmp;

if ((strcmp(s,"NA") == 0) || (strlen(s) < 1)) // assign NULL to the output ??
   {
   return -1;
   }

p->var_index = -1;

if (FullTextMatch("[ \t]*([A-Za-z0-9_]+)[ \t]*=[ \t]*(.+)[ \t]*[^A-Za-z0-9_/\"][ \t]*\"(.+)\"[ \t]*[,][ \t]*\"(.+)\"[ \t]*[,][ \t]*(.+)[^A-Za-z0-9_/\"]",s))
   {
   if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
      {
      if( RemoveChars(retval,unwanted))
         {
         snprintf(type, CF_SMALLBUF, "%s", retval);
         }
      
      if ((ret = GetType1(type)) > 0)
         {
         p->type = ret;
         }
//	  printf("GetPrepareCleanup(1): TYPE = %s; %d\n",type,p->type);
      }
   
   if (GetVariable("match","3",(void *)&retval,&rtype) != cf_notype)
      {
//	     if( RemoveChars(retval,unwanted))
      snprintf(p->text, CF_BUFSIZE, "%s", retval);
//	     printf("GetPrepareCleanup(1): LINE = %s; %s\n",type,p->text);
      }
   
   if (GetVariable("match","4",(void *)&retval,&rtype) != cf_notype)
      {
//	     if( RemoveChars(retval,unwanted))
      snprintf(p->buf, CF_SMALLBUF, "%s", retval);
//	     printf("GetPrepareCleanup(1): NAME = %s; %s\n",type,p->buf);
      }
   
   if (GetVariable("match","5",(void *)&retval,&rtype) != cf_notype)
      {
      RemoveComment(retval);
      if( RemoveChars(retval,unwanted))
         {
         snprintf(action, CF_SMALLBUF, "%s", retval);
         }
      
      p->action = GetAction(action);
//	     printf("GetPrepareCleanup(1): ACTION = %s; %d\n",action, p->action);
      }
   
   if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
      {
      if((ret = GetVarIndex(id, retval)) < 0)
         {
         printf(" GetPrepareCleanup(): Error!! Variable  %s is not defined in test \"%s\"\n",retval, id);
         p->var_index = -1;
         }
      else
         {
         p->var_index = ret;
//		  printf(" GetPrepareCleanup(1): var_index = %d\n", p->var_index);
         }
      }
   return 1;
   }
else if(FullTextMatch("[ \t]*([A-Za-z0-9_]+)[ \t]*=[ \t]*(.+)[ \t]*[^A-Za-z0-9_/\"][ \t]*\"(.+)\"[ \t]*[,][ \t]*(.+)",s))
   {
   if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
      {
      if( RemoveChars(retval,unwanted))
          snprintf(type, CF_SMALLBUF, "%s", retval);
      
      if ((ret = GetType1(type)) > 0)
         {
         p->type = ret;
         }
//	  printf("GetPrepareCleanup(1): TYPE = %s; %d\n",type,p->type);
      }
   
   if (GetVariable("match","3",(void *)&retval,&rtype) != cf_notype)
      {
      if( RemoveChars(retval,unwanted))
         {
         snprintf(p->buf, CF_BUFSIZE, "%s", retval);
//	     printf("GetPrepareCleanup(): NAME = %s\n",p->buf);
         }
      }       
   
   if (GetVariable("match","4",(void *)&retval,&rtype) != cf_notype)
      {
      RemoveComment(retval);
      
      if( RemoveChars(retval,unwanted))
         {
         snprintf(action, CF_SMALLBUF, "%s", retval);
         }
      
      p->action = GetAction(action);
      }
   
   if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
      {
         if((ret = GetVarIndex(id, retval)) < 0)
            {
            printf(" GetPrepareCleanup(): Error!! Variable  %s is not defined in test \"%s\"\n",retval, id);
            p->var_index = -1;
            }
         else
            {
            p->var_index = ret;
            //			 snprintf(o->buf, CF_SMALLBUF, "%s", retval);
//		  printf(" GetPrepareCleanup(): var_index = %d\n", p->var_index);
            }
      }
   return 1;
   }
else if(FullTextMatch("[ \t]*(.+)[ \t]*[^A-Za-z0-9_/\"][ \t]*\"(.+)\"[ \t]*[,][ \t]*(.+)",s))
   {
   if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
      {
      if( RemoveChars(retval,unwanted))
         {
         snprintf(type, CF_SMALLBUF, "%s", retval);
         }
      
      if ((ret = GetType1(type)) > 0)
         {
         p->type = ret;
         }
      
//	  printf("GetPrepareCleanup(2): TYPE = %s; %d\n",type,p->type);
      }
   
   if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
      {
      //if( RemoveChars(retval,unwanted))
      snprintf(p->buf, CF_BUFSIZE, "%s", retval);
//	     printf("GetPrepareCleanup(): NAME = %s\n",p->buf);
      }       
   
   if (GetVariable("match","3",(void *)&retval,&rtype) != cf_notype)
      {
      RemoveComment(retval);
      if (RemoveChars(retval,unwanted))
         {
         snprintf(action, CF_SMALLBUF, "%s", retval);
         }
      
      p->action = GetAction(action);
      
      if(p->action == -1)
         {
         snprintf(p->text, CF_BUFSIZE, "%s", retval);
         }	     
      }
   }
else if (FullTextMatch("[ \t]*([A-Za-z0-9_]+)[ \t]*[^A-Za-z0-9_/\"]\"(.+)\"[ \t]*[^A-Za-z0-9_/\"][ \t]*",s))
   {
   if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
      {
      if( (ret = GetType1(retval)) > 0)
         {
         p->type = ret;
         }
      if (GetVariable("match","2",(void *)&retval,&rtype) != cf_notype)
         {
//		    if( RemoveChars(retval,unwanted))
         snprintf(p->text, CF_BUFSIZE, "%s", retval);
         Debug("GetPrepareCleanup(): %s\n",retval);
         }
      }
   }
else
   {
   printf("GetPrepareCleanup(): Syntax Error; %s\n",s); // display line number
   return -1;
   }
   
return 1;
}

/*********************************************************/

int RemoveChars(char *s, char *u)

{ int len1, len2;
  int i,j, ok = 1, index = 0; 
  char tmp[CF_BUFSIZE];
  char src[CF_BUFSIZE];

len1 = strlen(s);

if (len1 < 1)
   {
   return 0;
   }

len2 = strlen(u);

if (len2 < 1)
   {
   return 0;
   }

len1++; // +1 for the terminating \0

snprintf(src, len1, "%s",s);
Debug("RemoveChars(): src= %s \n ", src);
for(i = 0; i < len1; i++)
   {
   ok = 1;
   for(j = 0; j < len2; j++)
      {
      if(src[i] == u[j])
         {
         ok = 0;
         break;
         }
      }
   if(ok)
      {
      tmp[index++] = src[i];
      }
   }

tmp[index] = '\0';

if(strlen(tmp) < 1)
   {
   return 0;
   }

snprintf(s, len1, "%s",tmp);
return 1;
}

/*********************************************************/

int RemoveComment(char *s)

{ int len = strlen(s) + 1;
  char tmp[len];
  char unwanted[] = "()";

snprintf(tmp, CF_BUFSIZE, "%s", s);

if (FullTextMatch("(.+);[ ]*#(.+)",tmp))
   {
   char rtype,*retval;
   if (GetVariable("match","1",(void *)&retval,&rtype) != cf_notype)
      {
      snprintf(s, CF_BUFSIZE, "%s", retval);
      Debug("RemoveComment(): %s\n", retval);
      }
   }
else
   {
   Debug("RemoveComment(): Comment not found\n");
   return -1;
   }

return 0;
}

/*********************************************************/

int GetAction(char *a)

{
if(strlen(a) < 1)
   {
   return -1;
   }

if(strcmp(a, CF_ACCESS_TEXT) == 0)
   {
   return CF_ACCESS; 
   } 

if(strcmp(a, CF_BACKUP_TEXT) == 0)
   {
   return CF_BACKUP;
   }

if(strcmp(a, CF_DELETE_TEXT) == 0)
   {
   return CF_DELETE; 
   }

if(strcmp(a, CF_RESTORE_TEXT) == 0)
   {
   return CF_RESTORE;
   }

if(strcmp(a, CF_EXISTS_TEXT) == 0)
   {
   return CF_EXISTS;
   }

if(strcmp(a, CF_CREATE_TEXT) == 0)
   {
   return CF_CREATE;
   }

return -1;
}

/*********************************************************/

int IsDuplicate(char *s, struct input *data, int n)

{ int i;

if (n < 0 || data == NULL || (strlen(s) < 1))
   {
   return 0;
   }

for(i = 0; i < n; i++)
   {
   if(strcmp(data[i].id,s) == 0)
      {
      Debug("Is Duplicate(): Found Duplicate: %s\n",s);
      return 1;
      }
   }

return 0;
}

/*********************************************************/
// used only for internal testing

void PrintChars(char *s)

{ int i = 0;
   
 while(s[i] != '\0')
    {
    printf("PrintChars(): %c\n", s[i]);
    i++;
    }
}

/*********************************************************/

int FindTextInFile(char *str, char *file)

{ char *buf, tmp[CF_BUFSIZE], bufEsc[CF_BUFSIZE];
  char any[] = ".*";

   buf = (char *) malloc(CF_BUFSIZE * sizeof(char));

if(buf == NULL)
   {
   printf("FindTextInFile(): Memory allocation error!!\n");
   return -1;
   }

buf = (char *) CfReadFile(file, CF_BUFSIZE);
if(buf == NULL)
   {
   printf("FindTextInFile(): Error reading file \"%s\"!!\n",file);
   return -1;
   }

Chop(buf);
// flatten text file
FlattenText(buf);

snprintf(tmp, CF_BUFSIZE, "%s", any);
strcat(tmp, str);
strcat(tmp, any);
if(FullTextMatch(tmp,buf))
   {
//   printf("FindTextInFile(): Found text!!\n");
   return 1; 
   }
free(buf);
return -1; 
}

/*********************************************************/
// TODO: use size_t instead of long return value??

long fsize(char* file)

{ long ret;
 FILE * f = fopen(file, "r");

if (f == NULL)
   {
   printf("Error Opening File: %s\n", file);
   return -1;
   }

fseek(f, 0, SEEK_END);
ret = (long) ftell(f);
fclose(f);
return ret;
}
		   
/*********************************************************/

int ReplaceCharTS(char *str, char to, char frm)

{ int ret = 0;
 char *sp;

for (sp = str; *sp != '\0'; sp++)
   {
   if (*sp == frm)
      {
      *sp = to;
      ret++;
      }
   }
return ret;
}

/*********************************************************/

int FlattenText(char *str)
{
return ReplaceCharTS(str, ' ', '\n');
}

/*********************************************************/

int ReadLineInput(char *dst, char *frm )

{ int len_prev, len_after, size;
  char buf[CF_BUFSIZE];
  char *tmp1, *tmp2;

tmp1 = frm;
tmp2 = buf;
len_prev = strlen(tmp1);
tmp2 = strstr(tmp1, "\n");
len_after = strlen(tmp2);
size = len_prev - len_after;
snprintf(dst, size + 1, "%s", frm);
return size;
}

/*********************************************************/

int FindProcess(char *str)

{ int retval = -1;
  struct Item *ip;  
  char *psopts = "aux";
  char any[] = ".*";
  char tmp[CF_SMALLBUF]; 
  int count = 0;
   
snprintf(tmp,CF_SMALLBUF,"%s",any);
strcat(tmp,str);
strcat(tmp,any);

if (LoadProcessTable(&PROCESSTABLE,psopts))
   {
   for (ip = PROCESSTABLE; ip != NULL; ip = ip->next )
      {
      if(FullTextMatch(tmp, ip->name))
         {
         count++;
         }
      }
   }
DeleteItemList(PROCESSTABLE);
if(count > 0)
  {
     retval = count;
  }
return retval;
}

/*********************************************************/

int GetVarIndex(char *test_id, char *var)

{ struct input *data = DATA;
  int i,j;

for(i = 0; i <= nInput; i++)
   {
   if(strcmp(data[i].id, test_id) == 0)
      {
      for(j = 0; j < data[i].nVar; j++)
         {
         if(strcmp(data[i].internal[j].name, var) == 0)
            {
            return data[i].internal[j].index;
		    }
         }
      }
   }   
return -1;
}
/*********************************************************/
int CheckPredefined(char *name)
{
   if(strcmp(name, BOOTSTRAP_TEXT) == 0)
     {
	return BOOTSTRAP;
     }
   else 
     {
	return -1;
     }
}

/*********************************************************/

int ExecutePreDefined(int id, char *opt)
{
   switch(id)
     {
      case BOOTSTRAP:
	return PerformBootstrap(opt);
	break;
     }
   
   return -1;   
}

/*********************************************************/
int PerformBootstrap(char *server)
{
   char buf[CF_BUFSIZE], output[CF_BUFSIZE];
   snprintf(buf,CF_BUFSIZE,"%s/ppkeys/localhost.pub",CFWORKDIR);
   if(!FileExists(buf))
     {
	snprintf(buf,CF_BUFSIZE,"%s/bin/cf-key",CFWORKDIR);
	if (GetExecOutput(buf,output,false))
	  {
	     
	  }
     }

   // bootstrapping
   snprintf(buf,CF_BUFSIZE,"%s/bin/cf-agent --bootstrap --policy-server %s",CFWORKDIR,server);
   printf("\tBootstrapping... [Command = \"%s\"]\n",buf);
   GetExecOutput(buf,output,false);
  // printf("%s\n",output);

   // check for promises.cf
   snprintf(buf,CF_BUFSIZE,"%s/inputs/promises.cf",CFWORKDIR);
   //printf("PerformBootstrap(): Checking for %s\n",buf);   
   if(!FileExists(buf))
     {
	return -1;
     }
   
   // check for 2 cf-execd processes
   snprintf(buf,CF_SMALLBUF,"cf-execd");
   if(FindProcess(buf) != 2)
     {
	return -1;
     }
   return 1;
}

/*********************************************************/
int FileExists(char *file)
{
   struct stat sb;
   if(cfstat(file, &sb) != 0)
     {
	return -1;
     }
   return 1;
}

#endif  /* BUILD_TESTSUITE */


/*********************************************************/


void CheckInstalledLibraries(void)
{
  printf("---- INSTALLED LIBRARIES ----\n");


   
   #ifndef HAVE_LIBACL
   printf("\t->LIBACL not found!!\n");
   #endif
   
   #ifndef HAVE_LIBPCRE
   printf("\t->LIBPCRE not found!!\n");
   #endif
   
   #ifndef HAVE_LIBPTHREAD
   printf("\t->LIBPTHREAD not found!!\n");
   #endif

   #if !defined(TCDB) && !defined(QDB) 
   printf("\t->TCDB and QDB  not found!!\n");
   #endif

   #ifndef HAVE_LIBMYSQLCLIENT
   printf("\t->LIBMYSQLCLIENT not found!!\n");
   #endif

   #ifdef HAVE_LIBPQ
     printf("\t-> LIBPQ (postgresql) version ???\n");
   #else
     printf("\t!! LIBPQ (postgresql) not found\n");
   #endif

   #ifdef HAVE_NOVA
   Nova_CheckInstalledLibraries();
   #else
   printf("\t->Nova not found!!\n");
   #endif
}

