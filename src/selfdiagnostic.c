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

int NR = 0;
void CfDebugBreak(void);

/*****************************************************************************/

void SelfDiagnostic()

{ int intval,s1,s2,i,j;
  char *def;
 
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
       
printf("----------------------------------------------------------\n");
printf("Cfengine - Level 1 self-diagnostic \n");
printf("----------------------------------------------------------\n\n");

#ifdef HAVE_LIBCFNOVA
s1 = Nova_SizeCfSQLContainer();
s2 = SizeCfSQLContainer();

if (s1 != s2)
   {
   printf(" !!! Pathological build. Nova module has different database combinations than core. %d (Nova) versus %d (Core)\n",s1,s2);
   FatalError("stop");
   }
else
   {
   printf(" -> Consistent sql containers at %d bytes\n",s1);
   }
#endif

SDIntegerDefault("editfilesize",EDITFILESIZE);
SDIntegerDefault("editbinaryfilesize",EDITBINFILESIZE);


printf(" -> Internal consistency done\n\n");

printf("----------------------------------------------------------\n");
printf("Cfengine - Level 2 self-diagnostic \n");
printf("----------------------------------------------------------\n\n");
TestVariableScan();
TestFunctionIntegrity();
TestExpandPromise();
TestExpandVariables();
TestRegularExpressions();
TestAgentPromises();

//#ifdef BUILD_TESTSUITE
printf("7. Test expected non-local load balancing efficiency\n");

 char *names = "company comparison competition complete omplex condition connection conscious control cook copper copy cord cork cotton cough country cover dead dear death debt decision deep degree delicate dependent design desire destruction detail development different digestion direction dirty discovery discussion disease disgust distance distribution division do og door end engine enough equal error even event ever every example exchange group growth guide gun hair hammer hand hanging happy harbour hard harmony hat hate have he head healthy hear hearing heart heat help high history hole hollow hook hope horn horse hospital hour house how humour I ice idea if ill important impulse in increase industry ink insect instrument insurance interest invention iron island jelly jewel join journey judge jump keep kettle key kick kind kiss knee knife knot knowledge land language last late laugh law lead leaf learning leather left leg let letter level library lift light like limit line linen lip liquid list little living lock long look loose loss loud love low machine make male man manager map mark market married mass match material may meal measure meat medical meeting memory metal middle military milk mind mine minute mist mixed money monkey month moon morning mother motion mountain mouth move much muscle music nail name narrow nation natural near necessary neck need needle nerve net new news night no noise normal north nose not note now number nut observation of off offer office oil old on only open operation opinion opposite or orange order organization ornament other out oven over owner page pain paint paper parallel parcel part past paste payment peace pen pencil person physical picture pig pin pipe place plane plant plate play please pleasure plough pocket point poison polish political poor porter position possible pot potato powder power present price print prison private probable process produce profit property prose protest public pull pump punishment purpose push put quality question quick quiet quite rail rain range rat rate ray reaction reading ready reason receipt record red regret regular relation religion representative request respect responsible rest reward rhythm rice right ring river road rod roll roof room root rough round rub rule run sad safe sail salt same sand say scale school science scissors screw sea seat second secret secretary see seed seem selection self send sense separate serious servant sex shade shake shame sharp sheep shelf ship shirt shock shoe short shut side sign silk silver simple sister size skin  skirt sky sleep slip slope slow small smash smell smile smoke smooth snake sneeze snow so soap society sock soft solid some  son song sort sound soup south space spade special sponge spoon spring square stage stamp star start statement station steam steel stem step stick sticky stiff still stitch stocking stomach stone stop store story straight strange street stretch strong structure substance such sudden sugar suggestion summer sun support surprise sweet swim system table tail take talk tall taste tax teaching tendency test than that the then theory there thick thin thing this thought thread throat through through thumb thunder ticket tight till time tin tired to toe together tomorrow tongue tooth top touch town trade train transport tray tree trick trouble trousers true turn twist umbrella under unit up use value verse very vessel view violent voice waiting walk wall war warm wash waste watch water wave wax way weather week weight well west ";

 char *numbers = "128.23.67.1 128.23.67.2 128.23.67.3 128.23.67.4 128.23.67.5 128.23.67.6 128.23.67.7 128.23.67.8 128.23.67.9 128.23.67.11 128.23.67.21 128.23.67.31 128.23.67.41 128.23.67.51 128.23.67.61 128.23.67.71 128.23.67.81 128.23.67.91 128.23.67.21 128.23.67.22 128.23.67.23 128.23.67.24 128.23.67.25 128.23.67.26 128.23.67.27 128.23.67.28 128.23.67.29 128.23.67.31 128.23.67.32 128.23.67.33 128.23.67.34 128.23.67.35 128.23.67.36 128.23.67.37 128.23.67.38 128.23.67.39 128.23.67.41 128.23.67.42 128.23.67.43 128.23.67.44 128.23.67.45 128.23.67.46 128.23.67.47 128.23.67.48 128.23.67.49 128.23.67.51 128.23.67.52 128.23.67.53 128.23.67.54 128.23.67.55 128.23.67.56 128.23.67.57 128.23.67.58 128.23.67.59 128.23.67.61 128.23.67.62 128.23.67.63 128.23.67.64 128.23.67.65 128.23.67.66 128.23.67.67 128.23.67.68 128.23.67.69 128.23.67.71 128.23.67.72 128.23.67.73 128.23.67.74 128.23.67.75 128.23.67.76 128.23.67.77 128.23.67.78 128.23.67.79 128.23.67.81 128.23.67.82 128.23.67.83 128.23.67.84 128.23.67.85 128.23.67.86 128.23.67.87 128.23.67.88 128.23.67.89 128.23.67.91 128.23.67.92 128.23.67.93 128.23.67.94 128.23.67.95 128.23.67.96 128.23.67.97 128.23.67.98 128.23.67.99 128.23.67.11 128.23.67.12 128.23.67.13 128.23.67.14 128.23.67.15 128.23.67.16 128.23.67.17 128.23.67.18 128.23.67.19 128.23.67.111 128.23.67.121 128.23.67.131 128.23.67.141 128.23.67.151 128.23.67.161 128.23.67.171 128.23.67.181 128.23.67.191 128.23.67.121 128.23.67.122 128.23.67.123 128.23.67.124 128.23.67.125 128.23.67.126 128.23.67.127 128.23.67.128 128.23.67.129 128.23.67.131 128.23.67.132 128.23.67.133 128.23.67.134 128.23.67.135 128.23.67.136 128.23.67.137 128.23.67.138 128.23.67.139 128.23.67.141 128.23.67.142 128.23.67.143 128.23.67.144 128.23.67.145 128.23.67.146 128.23.67.147 128.23.67.148 128.23.67.149 128.23.67.151 128.23.67.152 128.23.67.153 128.23.67.154 128.23.67.155 128.23.67.156 128.23.67.157 128.23.67.158 128.23.67.159 128.23.67.161 128.23.67.162 128.23.67.163 128.23.67.164 128.23.67.165 128.23.67.166 128.23.67.167 128.23.67.168 128.23.67.169 128.23.67.171 128.23.67.172 128.23.67.173 128.23.67.174 128.23.67.175 128.23.67.176 128.23.67.177 128.23.67.178 128.23.67.179 128.23.67.181 128.23.67.182 128.23.67.183 128.23.67.184 128.23.67.185 128.23.67.186 128.23.67.187 128.23.67.188 128.23.67.189 128.23.67.191 128.23.67.192 128.23.67.193 128.23.67.194 128.23.67.195 128.23.67.196 128.23.67.197 128.23.67.198 128.23.67.199 128.23.67.21 128.23.67.22 128.23.67.23 128.23.67.24 128.23.67.25 128.23.67.26 128.23.67.27 128.23.67.28 128.23.67.29 128.23.67.211 128.23.67.221 128.23.67.231 128.23.67.241 128.23.67.251 128.23.67.261 128.23.67.271 128.23.67.281 128.23.67.291 128.23.67.221 128.23.67.222 128.23.67.223 128.23.67.224 128.23.67.225 128.23.67.226 128.23.67.227 128.23.67.228 128.23.67.229 128.23.67.231 128.23.67.232 128.23.67.233 128.23.67.234 128.23.67.235 128.23.67.236 128.23.67.237 128.23.67.238 128.23.67.239 128.23.67.241 128.23.67.242 128.23.67.243 128.23.67.244 128.23.67.245 128.23.67.246 128.23.67.247 128.23.67.248 128.23.67.249 128.23.67.251 128.23.67.252 128.23.67.253 128.23.67.254 128.23.67.255 128.23.67.256 128.23.67.257 128.23.67.258 128.23.67.259 128.23.67.261 128.23.67.262 128.23.67.263 128.23.67.264 128.23.67.265 128.23.67.266 128.23.67.267 128.23.67.268 128.23.67.269 128.23.67.271 128.23.67.272 128.23.67.273 128.23.67.274 128.23.67.275 128.23.67.276 128.23.67.277 128.23.67.278 128.23.67.279 128.23.67.281 128.23.67.282 128.23.67.283 128.23.67.284 128.23.67.285 128.23.67.286 128.23.67.287 128.23.67.288 128.23.67.289 128.23.67.291 128.23.67.292 128.23.67.293 128.23.67.294 128.23.67.295 128.23.67.296 128.23.67.297 128.23.67.298 128.23.67.299 168.67.67.1 168.67.67.2 168.67.67.3 168.67.67.4 168.67.67.5 168.67.67.6 168.67.67.7 168.67.67.8 168.67.67.9 168.67.67.11 168.67.67.21 168.67.67.31 168.67.67.41 168.67.67.51 168.67.67.61 168.67.67.71 168.67.67.81 168.67.67.91 168.67.67.21 168.67.67.22 168.67.67.23 168.67.67.24 168.67.67.25 168.67.67.26 168.67.67.27 168.67.67.28 168.67.67.29 168.67.67.31 168.67.67.32 168.67.67.33 168.67.67.34 168.67.67.35 168.67.67.36 168.67.67.37 168.67.67.38 168.67.67.39 168.67.67.41 168.67.67.42 168.67.67.43 168.67.67.44 168.67.67.45 168.67.67.46 168.67.67.47 168.67.67.48 168.67.67.49 168.67.67.51 168.67.67.52 168.67.67.53 168.67.67.54 168.67.67.55 168.67.67.56 168.67.67.57 168.67.67.58 168.67.67.59 168.67.67.61 168.67.67.62 168.67.67.63 168.67.67.64 168.67.67.65 168.67.67.66 168.67.67.67 168.67.67.68 168.67.67.69 168.67.67.71 168.67.67.72 168.67.67.73 168.67.67.74 168.67.67.75 168.67.67.76 168.67.67.77 168.67.67.78 168.67.67.79 168.67.67.81 168.67.67.82 168.67.67.83 168.67.67.84 168.67.67.85 168.67.67.86 168.67.67.87 168.67.67.88 168.67.67.89 168.67.67.91 168.67.67.92 168.67.67.93 168.67.67.94 168.67.67.95 168.67.67.96 168.67.67.97 168.67.67.98 168.67.67.99 168.67.67.11 168.67.67.12 168.67.67.13 168.67.67.14 168.67.67.15 168.67.67.16 168.67.67.17 168.67.67.18 168.67.67.19 168.67.67.111 168.67.67.121 168.67.67.131 168.67.67.141 168.67.67.151 168.67.67.161 168.67.67.171 168.67.67.181 168.67.67.191 168.67.67.121 168.67.67.122 168.67.67.123 168.67.67.124 168.67.67.125 168.67.67.126 168.67.67.127 168.67.67.128 168.67.67.129 168.67.67.131 168.67.67.132 168.67.67.133 168.67.67.134 168.67.67.135 168.67.67.136 168.67.67.137 168.67.67.138 168.67.67.139 168.67.67.141 168.67.67.142 168.67.67.143 168.67.67.144 168.67.67.145 168.67.67.146 168.67.67.147 168.67.67.148 168.67.67.149 168.67.67.151 168.67.67.152 168.67.67.153 168.67.67.154 168.67.67.155 168.67.67.156 168.67.67.157 168.67.67.158 168.67.67.159 168.67.67.161 168.67.67.162 168.67.67.163 168.67.67.164 168.67.67.165 168.67.67.166 168.67.67.167 168.67.67.168 168.67.67.169 168.67.67.171 168.67.67.172 168.67.67.173 168.67.67.174 168.67.67.175 168.67.67.176 168.67.67.177 168.67.67.178 168.67.67.179 168.67.67.181 168.67.67.182 168.67.67.183 168.67.67.184 168.67.67.185 168.67.67.186 168.67.67.187 168.67.67.188 168.67.67.189 168.67.67.191 168.67.67.192 168.67.67.193 168.67.67.194 168.67.67.195 168.67.67.196 168.67.67.197 168.67.67.198 168.67.67.199 168.67.67.21 168.67.67.22 168.67.67.23 168.67.67.24 168.67.67.25 168.67.67.26 168.67.67.27 168.67.67.28 168.67.67.29 168.67.67.211 168.67.67.221 168.67.67.231 168.67.67.241 168.67.67.251 168.67.67.261 168.67.67.271 168.67.67.281 168.67.67.291 168.67.67.221 168.67.67.222 168.67.67.223 168.67.67.224 168.67.67.225 168.67.67.226 168.67.67.227 168.67.67.228 168.67.67.229 168.67.67.231 168.67.67.232 168.67.67.233 168.67.67.234 168.67.67.235 168.67.67.236 168.67.67.237 168.67.67.238 168.67.67.239 168.67.67.241 168.67.67.242 168.67.67.243 168.67.67.244 168.67.67.245 168.67.67.246 168.67.67.247 168.67.67.248 168.67.67.249 168.67.67.251 168.67.67.252 168.67.67.253 168.67.67.254 168.67.67.255 168.67.67.256 168.67.67.257 168.67.67.258 168.67.67.259 168.67.67.261 168.67.67.262 168.67.67.263 168.67.67.264 168.67.67.265 168.67.67.266 168.67.67.267 168.67.67.268 168.67.67.269 168.67.67.271 168.67.67.272 168.67.67.273 168.67.67.274 168.67.67.275 168.67.67.276 168.67.67.277 168.67.67.278 168.67.67.279 168.67.67.281 168.67.67.282 168.67.67.283 168.67.67.284 168.67.67.285 168.67.67.286 168.67.67.287 168.67.67.288 168.67.67.289 168.67.67.291 168.67.67.292 168.67.67.293 168.67.67.294 168.67.67.295 168.67.67.296 168.67.67.297 168.67.67.298 168.67.67.299";

char *pattern = malloc(2000000);

TestHashEntropy(names,"names");
TestHashEntropy(numbers,"addresses");

memset(pattern,0,8*2000+4);

for (i = 0, j= 0; i < 2000; i++)
    {
    sprintf(pattern+j,"serv_%d ",i);
    j += strlen(pattern+j);
    }

TestHashEntropy(pattern,"pattern 1");

memset(pattern,0,8*2000+4);

for (i = 0, j= 0; i < 2000; i++)
    {
    sprintf(pattern+j,"serv_%d.domain.tld ",i);
    j += strlen(pattern+j);
    }

TestHashEntropy(pattern,"pattern 2");
//#endif
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
      ScanRval("diagnostic",&scalars,&listoflists,varstrings[i],CF_SCALAR,NULL);
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

void TestFunctionIntegrity()

{ int i,j;
  struct FnCallArg *args;

printf("%d. Testing internal function templates and knowledge\n",++NR);
 
for (i = 0; CF_FNCALL_TYPES[i].name != NULL; i++)
   {
   args = CF_FNCALL_TYPES[i].args;
   
   for (j = 0; args[j].pattern != NULL; j++)
      {
      CfOut(cf_verbose,""," -> .. arg %d %s = %s\n",j,args[j].pattern,args[j].description);
      }
   
   CfOut(cf_verbose,""," -> function %s (%d=%d args)\n",CF_FNCALL_TYPES[i].name,CF_FNCALL_TYPES[i].numargs,j);

   if (j != CF_FNCALL_TYPES[i].numargs)
      {
      printf(" !! Broken internal function declaration for \"%s\", prototype does not match declared number of args",CF_FNCALL_TYPES[i].name);
      }
   }

}

/*****************************************************************************/

void TestExpandPromise()

{ struct Promise pp = {0},*pcopy;
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


AppendConstraint(&(pp.conlist),"lval1",strdup("rval1"),CF_SCALAR,"lower classes1",false);
AppendConstraint(&(pp.conlist),"lval2",strdup("rval2"),CF_SCALAR,"lower classes2",false);

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

{ struct Promise pp = {0},*pcopy;
  struct Body *bp;
  int i;
  char *list_text1 = "a,b,c,d,e,f,g";
  char *list_text2 = "1,2,3,4,5,6,7";
  struct Rlist *rp, *args, *listvars = NULL, *scalarvars = NULL;
  struct Constraint *cp;
  struct FnCall *fp;

#ifdef MINGW
if(NovaWin_GetProgDir(CFWORKDIR, CF_BUFSIZE - sizeof("Cfengine")))
  {
  strcat(CFWORKDIR, "\\Cfengine");
  }
else
  {
  CfOut(cf_error, "", "!! Could not get CFWORKDIR from Windows environment variable, falling back to compile time dir (%s)", WORKDIR);
  strcpy(CFWORKDIR,WORKDIR);
  }
Debug("Setting CFWORKDIR=%s\n", CFWORKDIR);
#elif defined(CFCYG)
strcpy(CFWORKDIR,WORKDIR);
MapName(CFWORKDIR);
#else
if (getuid() > 0)
   {
   strncpy(CFWORKDIR,GetHome(getuid()),CF_BUFSIZE-10);
   strcat(CFWORKDIR,"/.cfagent");

   if (strlen(CFWORKDIR) > CF_BUFSIZE/2)
      {
      FatalError("Suspicious looking home directory. The path is too long and will lead to problems.");
      }
   }
else
   {
   strcpy(CFWORKDIR,WORKDIR);
   }
#endif
  
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

args = SplitStringAsRList("$(administrator)",',');
fp = NewFnCall("getuid",args);
    
AppendConstraint(&(pp.conlist),"lval1",strdup("@(one)"),CF_SCALAR,"lower classes1",false);
AppendConstraint(&(pp.conlist),"lval2",strdup("$(four)"),CF_SCALAR,"upper classes1",false);
AppendConstraint(&(pp.conlist),"lval3",fp,CF_FNCALL,"upper classes2",false);

/* Now copy promise and delete */

pcopy = DeRefCopyPromise("diagnostic",&pp);

ScanRval("diagnostic",&scalarvars,&listvars,pcopy->promiser,CF_SCALAR,NULL);

if (pcopy->promisee != NULL)
   {
   ScanRval("diagnostic",&scalarvars,&listvars,pp.promisee,pp.petype,NULL);
   }

for (cp = pcopy->conlist; cp != NULL; cp=cp->next)
   {
   ScanRval("diagnostic",&scalarvars,&listvars,cp->rval,cp->type,NULL);
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
printf(" -> Some Cfengine are features will not work in this current state.\n");
printf(" !! This diagnostic might hang if the library is broken\n");
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

/* We have to recompile this for each test - else seg fault - is this a bug? */
rex = CompileRegExp("#.*");

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

{ struct Attributes a = {0};
  struct Promise pp = {0};

pp.conlist = NULL;
pp.audit = NULL;

printf("%d. Testing promise attribute completeness (with no desired intention)\n",++NR);

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

/*****************************************************************************/

void SDIntegerDefault(char *ref,int cmp)

{ char *def;
  int intval;

if (def = GetControlDefault(ref))
   {
   sscanf(def,"%d",&intval);
   if (intval != cmp)
      {
      printf(" !! Mismatch in default specs for \"%s\" (%d/%d)\n",ref,intval,cmp);
      }
   else
      {
      printf(" -> %s ok (%d/%d)\n",ref,intval,cmp);
      }
   }
else
   {
   printf(" !! Missing default specs for \"%s\"\n",ref);
   }
}


/*****************************************************************************/

void TestHashEntropy(char *names,char *title)

{char word[32],*sp;
 int i,j,slot,eslot,sslot,hashtable[CF_HASHTABLESIZE],ehashtable[CF_HASHTABLESIZE],shashtable[CF_HASHTABLESIZE];
 int freq[10], efreq[10],sfreq[10];
 double tot = 0,etot = 0,stot = 0;
 
for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   hashtable[i] = 0;
   ehashtable[i] = 0;
   shashtable[i] = 0;
   }

printf(" -> Trial of \"%s\":\n",title); 

for (i = 0,sp = names; *sp != '\0'; sp += strlen(word)+1,i++)
   {
   struct timespec start,stop;
  
   word[0] = '\0';
   sscanf(sp,"%s",word);

   if (word[0] == '\0')
      {
      break;
      }

   clock_gettime(CLOCK_REALTIME, &start);
   slot = RefHash(word);
   clock_gettime(CLOCK_REALTIME, &stop);
   tot += (double)(stop.tv_sec - start.tv_sec)+(double)(stop.tv_nsec-start.tv_nsec);

   clock_gettime(CLOCK_REALTIME, &start);
   eslot = ElfHash(word);
   clock_gettime(CLOCK_REALTIME, &stop);
   etot += (double)(stop.tv_sec - start.tv_sec)+(double)(stop.tv_nsec-start.tv_nsec);

   clock_gettime(CLOCK_REALTIME, &start);
   sslot = OatHash(word);
   clock_gettime(CLOCK_REALTIME, &stop);
   stot += (double)(stop.tv_sec - start.tv_sec)+(double)(stop.tv_nsec-start.tv_nsec);
   
   hashtable[slot]++;
   ehashtable[eslot]++;
   shashtable[sslot]++;
   printf("SLOTS: %d,%d,%d\n",slot,eslot,sslot);
   }

printf("reference time %lf\n",tot/(double)CF_BILLION);
printf("elf time %lf\n",etot/(double)CF_BILLION);
printf("fast time %lf\n",stot/(double)CF_BILLION);

printf(" -> Hashed %d %s words into %d slots with the following spectra:\n",i,title,CF_HASHTABLESIZE);

for (j = 0; j < 10; j++)
   {
   freq[j] = efreq[j] = sfreq[j] = 0;
   }

for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   for (j = 0; j < 10; j++)
      {
      if (hashtable[i] == j)
         {
         freq[j]++;
         }

      if (ehashtable[i] == j)
         {
         efreq[j]++;
         }

      if (shashtable[i] == j)
         {
         sfreq[j]++;
         }      
      }
   }

printf("\n");

for (j = 1; j < 10; j++)
   {
   if (freq[j] > 0)
      {
      printf(" ->  F[%d] = %d\n",j,freq[j]);
      }
   }

printf("\n");

for (j = 1; j < 10; j++)
   {
   if (efreq[j] > 0)
      {
      printf(" -> eF[%d] = %d\n",j,efreq[j]);
      }
   }

printf("\n");

for (j = 1; j < 10; j++)
   {
   if (sfreq[j] > 0)
      {
      printf(" -> sF[%d] = %d\n",j,sfreq[j]);
      }
   }
}

/*****************************************************************************/

void CfDebugBreak() {   /* Called on error condition to break for inspection */ }
