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
/* File: env_context.c                                                       */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

extern char *DAY_TEXT[];

/*****************************************************************************/

void ValidateClassSyntax(char *str)

{ char *cp = str;
  int pcount = 0;

if (*cp == '&' || *cp == '|' || *cp == '.' || *cp == ')') 
   {
   yyerror("Illegal initial character for class specification");
   return;
   }

for (; *cp; cp++)
   {
   if (*cp == '(')
      {
      if (*(cp-1) == ')')
         {
         yyerror("Illegal use of parenthesis - you have ')(' with no intervening operator in your class specification");
         return;
         }

      pcount++;
      }
   else if (*cp == ')')
      {
      if (--pcount < 0)
         {
         yyerror("Unbalanced parenthesis - too many ')' in class specification");
         return;
         }
      if (*(cp-1) == '(')
         {
         yyerror("Empty parenthesis '()' illegal in class specifications");
         return;
         }
      }

// Rule out x&|y, x&.y, etc (but allow x&!y)
   else if (*cp == '.')
      {
      if (*(cp-1) == '|' || *(cp-1) == '&' || *(cp-1) == '!')
         {
         yyerror("Illegal operator combination");
         return;
         }
      }
   else if (*cp == '&')
      {
      if (*(cp-1) == '|' || *(cp-1) == '.' || *(cp-1) == '!')
         {
         yyerror("Illegal operator combination");
         return;
         }
      }
   else if (*cp == '|')
      {
      if (*(cp-1) == '&' || *(cp-1) == '.' || *(cp-1) == '!')
         {
         yyerror("Illegal operator combination");
         return;
         }
      }
   }

if (pcount)
   {
   yyerror("Unbalanced parenthesis - too many '(' in class specification");
   return;
   }
}

/*****************************************************************************/

void KeepClassContextPromise(struct Promise *pp)

{ struct Attributes a;

a = GetClassContextAttributes(pp);

if (!FullTextMatch("[a-zA-Z0-9_]+",pp->promiser))
   {
   CfOut(cf_verbose,"","Class identifier \"%s\" contains illegal characters - canonifying",pp->promiser);
   snprintf(pp->promiser, strlen(pp->promiser) + 1, "%s", CanonifyName(pp->promiser));
   }

if (a.context.broken)
   {
   cfPS(cf_error,CF_FAIL,"",pp,a,"Irreconcilable constraints in classes for %s (broken promise)",pp->promiser);
   return;
   }

if (strcmp(pp->bundletype,"common") == 0)
   {
   if (EvalClassExpression(a.context.expression,pp))
      {
      CfOut(cf_verbose,""," ?> defining additional global class %s\n",pp->promiser);

      if (!ValidClassName(pp->promiser))
         {
         cfPS(cf_error,CF_FAIL,"",pp,a," !! Attempted to name a class \"%s\", which is an illegal class identifier",pp->promiser);
         }
      else
         {
         NewClass(pp->promiser);
         }
      }

   /* These are global and loaded once */
   //*(pp->donep) = true;

   return;
   }

if (strcmp(pp->bundletype,THIS_AGENT) == 0 || FullTextMatch("edit_.*",pp->bundletype))
   {
   if (EvalClassExpression(a.context.expression,pp))
      {
      Debug(" ?> defining explicit class %s\n",pp->promiser);

      if (!ValidClassName(pp->promiser))
         {
         cfPS(cf_error,CF_FAIL,"",pp,a," !! Attempted to name a class \"%s\", which is an illegal class identifier",pp->promiser);
         }
      else
         {
         NewBundleClass(pp->promiser,pp->bundle);
         }
      }

   // Private to bundle, can be reloaded

   *(pp->donep) = false;   
   return;
   }
}

/*****************************************************************************/

void DeleteEntireHeap()

{
DeleteItemList(VHEAP);
VHEAP = NULL;
}

/*****************************************************************************/

void DeletePrivateClassContext()

{
DeleteItemList(VADDCLASSES);
DeleteItemList(VDELCLASSES);
VADDCLASSES = NULL;
VDELCLASSES = NULL;
}

/*****************************************************************************/

void PushPrivateClassContext()

{
PushStack(&PRIVCLASSHEAP,VADDCLASSES);
VADDCLASSES = NULL;
}

/*****************************************************************************/

void PopPrivateClassContext()

{ struct Item *list;

DeleteItemList(VADDCLASSES);
PopStack(&PRIVCLASSHEAP,(void *)&VADDCLASSES,sizeof(VADDCLASSES));
}

/*****************************************************************************/

void NewPersistentContext(char *name,unsigned int ttl_minutes,enum statepolicy policy)

{ int errno;
  CF_DB *dbp;
  struct CfState state;
  time_t now = time(NULL);
  char filename[CF_BUFSIZE];

snprintf(filename,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_STATEDB_FILE);
MapName(filename);

if (!OpenDB(filename,&dbp))
   {
   return;
   }

cf_chmod(filename,0644);  
      
if (ReadDB(dbp,name,&state,sizeof(state)))
   {
   if (state.policy == cfpreserve)
      {
      if (now < state.expires)
         {
         CfOut(cf_verbose,""," -> Persisent state %s is already in a preserved state --  %d minutes to go\n",name,(state.expires-now)/60);
 CloseDB(dbp);
         return;
         }
      }
   }
else
   {
   CfOut(cf_verbose,""," -> New persistent state %s\n",name);
   state.expires = now + ttl_minutes * 60;
   state.policy = policy;
   }
 
WriteDB(dbp,name,&state,sizeof(state));
CloseDB(dbp);
}

/*****************************************************************************/

void DeletePersistentContext(char *name)

{ int errno;
  CF_DB *dbp;
  char filename[CF_BUFSIZE];

snprintf(filename,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_STATEDB_FILE);
MapName(filename);

if (!OpenDB(filename,&dbp))
   {
   return;
   }

cf_chmod(filename,0644); 
DeleteDB(dbp,name);
Debug("Deleted any persistent state %s\n",name); 
CloseDB(dbp);
}

/*****************************************************************************/

void LoadPersistentContext()

{ CF_DB *dbp;
  CF_DBC *dbcp;
  int ret,ksize,vsize;
  char *key;
  void *value;
  time_t now = time(NULL);
  struct CfState q;
  char filename[CF_BUFSIZE];

Banner("Loading persistent classes");
  
snprintf(filename,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_STATEDB_FILE);
MapName(filename);

if (!OpenDB(filename,&dbp))
   {
   return;
   }

/* Acquire a cursor for the database. */

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan persistence cache");
   return;
   }

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   memcpy((void *)&q,value,sizeof(struct CfState));

   Debug(" - Found key %s...\n",key);

   if (now > q.expires)
      {
      CfOut(cf_verbose,""," Persistent class %s expired\n",key);
      DeleteDB(dbp,key);
      }
   else
      {
      CfOut(cf_verbose,""," Persistent class %s for %d more minutes\n",key,(q.expires-now)/60);
      CfOut(cf_verbose,""," Adding persistent class %s to heap\n",key);
      NewClass(key);
      }
   }

DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);

Banner("Loaded persistent memory");
}

/*****************************************************************************/

void AddEphemeralClasses(struct Rlist *classlist)

{ struct Rlist *rp;

for (rp = classlist; rp != NULL; rp = rp->next)
   {
   if (!IsItemIn(VHEAP,rp->item))
      {
      NewClass(rp->item);
      }
   }
}

/*********************************************************************/

void NewClassesFromString(char *classlist)

{ char *sp, currentitem[CF_MAXVARSIZE],local[CF_MAXVARSIZE];
 
if ((classlist == NULL) || strlen(classlist) == 0)
   {
   return;
   }

memset(local,0,CF_MAXVARSIZE);
strncpy(local,classlist,CF_MAXVARSIZE-1);

for (sp = local; *sp != '\0'; sp++)
   {
   memset(currentitem,0,CF_MAXVARSIZE);

   sscanf(sp,"%250[^.:,]",currentitem);

   sp += strlen(currentitem);
      
   if (IsHardClass(currentitem))
      {
      FatalError("cfengine: You cannot use -D to define a reserved class!");
      }

   NewClass(currentitem);
   }
}

/*********************************************************************/

void NegateClassesFromString(char *class,struct Item **heap)

{ char *sp = class;
  char cbuff[CF_MAXVARSIZE];

while(*sp != '\0')
   {
   sscanf(sp,"%255[^.]",cbuff);

   while ((*sp != '\0') && ((*sp !='.')||(*sp == '&')))
      {
      sp++;
      }

   if ((*sp == '.') || (*sp == '&'))
      {
      sp++;
      }

   if (IsHardClass(cbuff))
      { char err[CF_BUFSIZE];
      sprintf (err,"Cannot negate the reserved class [%s]\n",cbuff);
      FatalError(err);
      }

   AppendItem(heap,cbuff,NULL);
   }
}

/*********************************************************************/

void AddPrefixedClasses(char *name,char *classlist)

{ char *sp, currentitem[CF_MAXVARSIZE],local[CF_MAXVARSIZE],pref[CF_BUFSIZE];
 
if ((classlist == NULL) || strlen(classlist) == 0)
   {
   return;
   }

memset(local,0,CF_MAXVARSIZE);
strncpy(local,classlist,CF_MAXVARSIZE-1);

for (sp = local; *sp != '\0'; sp++)
   {
   memset(currentitem,0,CF_MAXVARSIZE);

   sscanf(sp,"%250[^.:,]",currentitem);

   sp += strlen(currentitem);

   pref[0] = '\0';
   snprintf(pref,CF_BUFSIZE,"%s_%s",name,currentitem);

   if (IsHardClass(pref))
      {
      FatalError("cfengine: You cannot use -D to define a reserved class!");
      }

   NewClass(pref);
   }
}

/*********************************************************************/

int IsHardClass(char *sp)  /* true if string matches a hardwired class e.g. hpux */

{ int i;
  static char *names[] =
     {
     "any","agent","Morning","Afternoon","Evening","Night","Q1","Q2","Q3","Q4",
     "SuSE","suse","fedora","Ubuntu","cfengine","ipv4","lsb_compliant","localhost",
     NULL
     };
 
for (i = 2; CLASSTEXT[i] != '\0'; i++)
   {
   if (strcmp(CLASSTEXT[i],sp) == 0)
      {
      return true;
      }
   }

for (i = 0; i < 7; i++)
   {
   if (strcmp(DAY_TEXT[i],sp)==0)
      {
      return true;
      }
   }

for (i = 0; i < 12; i++)
   {
   if (strncmp(MONTH_TEXT[i],sp,3) == 0)
      {
      return true;
      }
   }

for (i = 0; names[i] != NULL; i++)
   {
   if (strncmp(names[i],sp,strlen(names[i])) == 0)
      {
      return true;
      }
   }

if (strncmp(sp,"Min",3) == 0 && isdigit(*(sp+3)))
   {
   return true;
   }

if (strncmp(sp,"Hr",2) == 0 && isdigit(*(sp+2)))
   {
   return true;
   }

if (strncmp(sp,"Yr",2) == 0 && isdigit(*(sp+2)))
   {
   return true;
   }

if (strncmp(sp,"Day",3) == 0 && isdigit(*(sp+3)))
   {
   return true;
   }

if (strncmp(sp,"GMT",3) == 0 && *(sp+3) == '_')
   {
   return true;
   }

if (strncmp(sp,"Lcycle",strlen("Lcycle")) == 0)
   {
   return true;
   }

return(false);
}

/***************************************************************************/

int Abort()

{
if (ABORTBUNDLE)
   {
   ABORTBUNDLE = false;
   return true;
   }

return false;
}

/*****************************************************************************/

int VarClassExcluded(struct Promise *pp,char **classes)

{
*classes = (char *)GetConstraint("ifvarclass",pp,CF_SCALAR);

if (*classes == NULL)
   {
   return false;
   }

if (strchr(*classes,'$') || strchr(*classes,'@'))
   {
   Debug("Class expression did not evaluate");
   return true;
   }

if (*classes && IsDefinedClass(*classes))
   {
   return false;
   }
else
   {
   return true;
   }
}

/*******************************************************************/

void SaveClassEnvironment()

{ struct Item *ip;
  char file[CF_BUFSIZE];
  FILE *fp;
 
snprintf(file,CF_BUFSIZE,"%s/state/allclasses.txt",CFWORKDIR);

if ((fp = fopen(file,"w")) == NULL)
   {
   CfOut(cf_inform,"","Could not open allclasses cache file");
   return;
   }

for (ip = VHEAP; ip != NULL; ip=ip->next)
   {
   if (!IsItemIn(VNEGHEAP,ip->name))
      {
      fprintf(fp,"%s\n",ip->name);
      }
   }
 
for (ip = VADDCLASSES; ip != NULL; ip=ip->next)
   {
   if (!IsItemIn(VNEGHEAP,ip->name))
      {
      fprintf(fp,"%s\n",ip->name);
      }
   }

fclose(fp);
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int EvalClassExpression(struct Constraint *cp,struct Promise *pp)

{ int result_and = true;
  int result_or = false;
  int result_xor = 0;
  int result,total = 0;
  char *lval = cp->lval,buffer[CF_MAXVARSIZE];
  struct Rlist *rp;
  double prob,cum = 0,fluct;
  struct Rval newret;
  struct FnCall *fp;

if (cp == NULL)
   {
   CfOut(cf_error,""," !! EvalClassExpression internal diagnostic discovered an ill-formed condition");
   }

if (!IsDefinedClass(pp->classes))
   {
   return false;
   }

if (pp->done)
   {
   return false;
   }

if (IsDefinedClass(pp->promiser))
   {
   return false;
   }

switch (cp->type) 
   {
   case CF_FNCALL:
       
       fp = (struct FnCall *)cp->rval;
       /* Special expansion of functions for control, best effort only */
       newret = EvaluateFunctionCall(fp,pp);
       DeleteFnCall(fp);
       cp->rval = newret.item;
       cp->type = newret.rtype;
       break;

   case CF_LIST:
       for (rp = (struct Rlist *)cp->rval; rp != NULL; rp = rp->next)
          {
          newret = EvaluateFinalRval("this",rp->item,rp->type,true,pp);
          DeleteRvalItem(rp->item,rp->type);
          rp->item = newret.item;
          rp->type = newret.rtype;
          }
       break;
       
   default:

       newret = ExpandPrivateRval("this",cp->rval,cp->type);
       DeleteRvalItem(cp->rval,cp->type);
       cp->rval = newret.item;
       cp->type = newret.rtype;
       break;
   }

if (strcmp(cp->lval,"expression") == 0)
   {
   if (cp->type != CF_SCALAR)
      {
      return false;
      }

   if (IsDefinedClass((char *)cp->rval))
      {
      return true;
      }
   else
      {
      return false;
      }
   }

if (strcmp(cp->lval,"not") == 0)
   {
   if (cp->type != CF_SCALAR)
      {
      return false;
      }

   if (IsDefinedClass((char *)cp->rval))
      {
      return false;
      }
   else
      {
      return true;
      }
   }

if (strcmp(cp->lval,"dist") == 0)
   {
   for (rp = (struct Rlist *)cp->rval; rp != NULL; rp = rp->next)
      {
      result = Str2Int(rp->item);
      
      if (result < 0)
         {
         CfOut(cf_error,"","Non-positive integer in class distribution");
         PromiseRef(cf_error,pp);
         return false;
         }
      
      total += result;
      }
   }

fluct = drand48(); /* Get random number 0-1 */
cum = 0.0;

/* If we get here, anything remaining on the RHS must be a clist */

for (rp = (struct Rlist *)cp->rval; rp != NULL; rp = rp->next)
   {
   if (rp->type != CF_SCALAR)
      {
      return false;
      }

   result = IsDefinedClass((char *)(rp->item));

   result_and = result_and && result;
   result_or  = result_or || result;
   result_xor += result;

   if (total > 0) // dist class
      {
      prob = ((double)Str2Int(rp->item))/((double)total);
      cum += prob;

      if ((fluct < cum) || rp->next == NULL)
         {
         snprintf(buffer,CF_MAXVARSIZE-1,"%s_%s",pp->promiser,rp->item);
         *(pp->donep) = true;

         if (strcmp(pp->bundletype,"common") == 0)
            {
            NewClass(buffer);
            }
         else
            {
            NewBundleClass(buffer,pp->bundle);
            }

         Debug(" ?? \'Strategy\' distribution class interval -> %s\n",buffer);
         return true;
         }
      }
   }

if (strcmp(cp->lval,"or") == 0)
   {
   return result_or;
   }

if (strcmp(cp->lval,"xor") == 0)
   {
   return (result_xor == 1) ? true : false;
   }

if (strcmp(cp->lval,"and") == 0)
   {
   return result_and;
   }

return false;
}

/*******************************************************************/

void NewClass(char *oclass)

{ struct Item *ip;
  char class[CF_MAXVARSIZE];

Chop(oclass);
strncpy(class,CanonifyName(oclass),CF_MAXVARSIZE);  

Debug("NewClass(%s)\n",class);

if (strlen(class) == 0)
   {
   return;
   }

if (IsRegexItemIn(ABORTBUNDLEHEAP,class))
   {
   CfOut(cf_error,"","Bundle aborted on defined class \"%s\"\n",class);
   ABORTBUNDLE = true;
   }

if (IsRegexItemIn(ABORTHEAP,class))
   {
   CfOut(cf_error,"","cf-agent aborted on defined class \"%s\"\n",class);
   exit(1);
   }

if (IsItemIn(VHEAP,class))
   {
   return;
   }

AppendItem(&VHEAP,class,NULL);

for (ip = ABORTHEAP; ip != NULL; ip = ip->next)
   {
   if (IsDefinedClass(ip->name))
      {
      CfOut(cf_error,"","cf-agent aborted on defined class \"%s\" defined in bundle %s\n",class,THIS_BUNDLE);
      exit(1);
      }
   }

if (!ABORTBUNDLE)
   {
   for (ip = ABORTBUNDLEHEAP; ip != NULL; ip = ip->next)
      {
      if (IsDefinedClass(ip->name))
         {
         CfOut(cf_error,""," -> Setting abort for \"%s\" when setting \"%s\"",ip->name,class);
         ABORTBUNDLE = true;
         break;
         }
      }
   }
}

/*********************************************************************/

void NewPrefixedClasses(char *name,char *classlist)

{ char *sp, currentitem[CF_MAXVARSIZE],local[CF_MAXVARSIZE],pref[CF_BUFSIZE];
 
if ((classlist == NULL) || strlen(classlist) == 0)
   {
   return;
   }

memset(local,0,CF_MAXVARSIZE);
strncpy(local,classlist,CF_MAXVARSIZE-1);

for (sp = local; *sp != '\0'; sp++)
   {
   memset(currentitem,0,CF_MAXVARSIZE);

   sscanf(sp,"%250[^.:,]",currentitem);

   sp += strlen(currentitem);

   pref[0] = '\0';
   snprintf(pref,CF_BUFSIZE,"%s_%s",name,currentitem);

   if (IsHardClass(pref))
      {
      FatalError("cfengine: You cannot use -D to define a reserved class!");
      }

   NewClass(pref);
   }
}

/*******************************************************************/

void DeleteClass(char *class)

{
DeleteItemLiteral(&VHEAP,class);
DeleteItemLiteral(&VADDCLASSES,class);
}

/*******************************************************************/

void NewBundleClass(char *class,char *bundle)

{ char copy[CF_BUFSIZE];
  struct Item *ip;

memset(copy,0,CF_BUFSIZE);
strncpy(copy,class,CF_MAXVARSIZE);
Chop(copy);

if (strlen(copy) == 0)
   {
   return;
   }

Debug("NewBundleClass(%s)\n",copy);

if (IsRegexItemIn(ABORTBUNDLEHEAP,copy))
   {
   CfOut(cf_error,"","Bundle %s aborted on defined class \"%s\"\n",bundle,copy);
   ABORTBUNDLE = true;
   }

if (IsRegexItemIn(ABORTHEAP,copy))
   {
   CfOut(cf_error,"","cf-agent aborted on defined class \"%s\" defined in bundle %s\n",copy,bundle);
   exit(1);
   }

if (IsItemIn(VHEAP,copy))
   {
   CfOut(cf_error,"","WARNING - private class \"%s\" in bundle \"%s\" shadows a global class - you should choose a different name to avoid conflicts",copy,bundle);
   }

if (IsItemIn(VADDCLASSES,copy))
   {
   return;
   }

AppendItem(&VADDCLASSES,copy,CONTEXTID);

for (ip = ABORTHEAP; ip != NULL; ip = ip->next)
   {
   if (IsDefinedClass(ip->name))
      {
      CfOut(cf_error,"","cf-agent aborted on defined class \"%s\" defined in bundle %s\n",copy,bundle);
      exit(1);
      }
   }

if (!ABORTBUNDLE)
   {
   for (ip = ABORTBUNDLEHEAP; ip != NULL; ip = ip->next)
      {
      if (IsDefinedClass(ip->name))
         {
         CfOut(cf_error,""," -> Setting abort for \"%s\" when setting \"%s\"",ip->name,class);
         ABORTBUNDLE = true;
         break;
         }
      }
   }
}

/*********************************************************************/

int IsExcluded(char *exception)

{
if (!IsDefinedClass(exception))
   {
   Debug2("%s is excluded!\n",exception);
   return true;
   }  

return false;
}

/*********************************************************************/

int IsDefinedClass(char *class) 

  /* Evaluates a.b.c|d.e.f etc and returns true if the class */
  /* is currently true, given the defined heap and negations */

{ int ret;

if (class == NULL)
   {
   return true;
   }

Debug("IsDefinedClass(%s,VADDCLASSES)\n",class);

ret = EvaluateORString(class,VADDCLASSES,0);

return ret;
}


/*********************************************************************/
/* Level 2                                                           */
/*********************************************************************/

int EvaluateORString(char *class,struct Item *list,int fromIsInstallable)

{ char *sp, cbuff[CF_BUFSIZE];
  int result = false;

if (class == NULL)
   {
   return false;
   }

Debug("\n--------\nEvaluateORString(%s)\n",class);
 
for (sp = class; *sp != '\0'; sp++)
   {
   while (*sp == '|')
      {
      sp++;
      }

   memset(cbuff,0,CF_BUFSIZE);

   sp += GetORAtom(sp,cbuff);

   if (strlen(cbuff) == 0)
      {
      break;
      }

   if (IsBracketed(cbuff)) /* Strip brackets */
      {
      cbuff[strlen(cbuff)-1] = '\0';

      result |= EvaluateORString(cbuff+1,list,fromIsInstallable);
      Debug("EvalORString-temp-result-y=%d (%s)\n",result,cbuff+1);
      }
   else
      {
      result |= EvaluateANDString(cbuff,list,fromIsInstallable);
      Debug("EvalORString-temp-result-n=%d (%s)\n",result,cbuff);
      }

   if (*sp == '\0')
      {
      break;
      }
   }

Debug("EvaluateORString(%s) returns %d\n",class,result); 
return result;
}

/*********************************************************************/

int ValidClassName(char *name)
{
if (FullTextMatch(CF_CLASSRANGE,name))
   {
   return true;
   }
else
   {
   return false;
   }
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

int EvaluateANDString(char *class,struct Item *list,int fromIsInstallable)

{ char *sp, *atom;
  char cbuff[CF_BUFSIZE];
  int count = 1;
  int negation = false;

Debug("EvaluateANDString(%s)\n",class);

count = CountEvalAtoms(class);
sp = class;
 
while(*sp != '\0')
   {
   negation = false;

   while (*sp == '!')
      {
      negation = !negation;
      sp++;
      }

   memset(cbuff,0,CF_BUFSIZE);

   sp += GetANDAtom(sp,cbuff) + 1;

   atom = cbuff;

     /* Test for parentheses */
   
   if (IsBracketed(cbuff))
      {
      atom = cbuff+1;

      Debug("Checking AND Atom %s?\n",atom);
      
      cbuff[strlen(cbuff)-1] = '\0';
      
      if (EvaluateORString(atom,list,fromIsInstallable))
         {
         if (negation)
            {
            Debug("EvalANDString-temp-result-neg1=false\n");
            return false;
            }
         else
            {
            Debug("EvalORString-temp-result count=%d\n",count);
            count--;
            }
         }
      else
         {
         if (negation)
            {
            Debug("EvalORString-temp-result2 count=%d\n",count);
            count--;
            }
         else
            {
            return false;
            }
         }

      continue;
      }
   else
      {
      atom = cbuff;
      }
   
   /* End of parenthesis check */
   
   if (*sp == '.' || *sp == '&')
      {
      sp++;
      }

   Debug("Checking OR atom (%s)?\n",atom);

   if (IsItemIn(VNEGHEAP,atom)||IsItemIn(VDELCLASSES,atom))
      {
      if (negation)
         {
         Debug("EvalORString-temp-result3 count=%d\n",count);
         count--;
         }
      else
         {
         return false;
         }
      } 
   else if (IsItemIn(VHEAP,atom))
      {
      if (negation)
         {
         Debug("EvaluateANDString(%s) returns false by negation 1\n",class);
         return false;
         }
      else
         {
         Debug("EvalORString-temp-result3.5 count=%d\n",count);
         count--;
         }
      } 
   else if (IsItemIn(list,atom))
      {
      if (negation && !fromIsInstallable)
         {
         Debug("EvaluateANDString(%s) returns false by negation 2\n",class);
         return false;
         }
      else
         {
         Debug("EvalORString-temp-result3.6 count=%d\n",count);
         count--;
         }
      } 
   else if (negation)    /* ! (an undefined class) == true */
      {
      Debug("EvalORString-temp-result4 count=%d\n",count);
      count--;
      }
   else       
      {
      Debug("EvaluateANDString(%s) returns false ny negation 3\n",class);
      return false;
      }
   }

 
if (count == 0)
   {
   Debug("EvaluateANDString(%s) returns true\n",class);
   return(true);
   }
else
   {
   Debug("EvaluateANDString(%s) returns false\n",class);
   return(false);
   }
}

/*********************************************************************/

int GetORAtom(char *start,char *buffer)

{ char *sp = start;
  char *spc = buffer;
  int bracklevel = 0, len = 0;

while ((*sp != '\0') && !((*sp == '|') && (bracklevel == 0)))
   {
   if (*sp == '(')
      {
      Debug("+(\n");
      bracklevel++;
      }

   if (*sp == ')')
      {
      Debug("-)\n");
      bracklevel--;
      }

   Debug("(%c)",*sp);
   *spc++ = *sp++;
   len++;
   }

*spc = '\0';

Debug("\nGetORATom(%s)->%s\n",start,buffer); 
return len;
}

/*********************************************************************/
/* Level 4                                                           */
/*********************************************************************/

int GetANDAtom(char *start,char *buffer)

{ char *sp = start;
  char *spc = buffer;
  int bracklevel = 0, len = 0;

while ((*sp != '\0') && !(((*sp == '.')||(*sp == '&')) && (bracklevel == 0)))
   {
   if (*sp == '(')
      {
      Debug("+(\n");
      bracklevel++;
      }

   if (*sp == ')')
      {
      Debug("-)\n");
      bracklevel--;
      }

   *spc++ = *sp++;

   len++;
   }

*spc = '\0';
Debug("\nGetANDATom(%s)->%s\n",start,buffer);  

return len;
}

/*********************************************************************/

int CountEvalAtoms(char *class)

{ char *sp;
  int count = 0, bracklevel = 0;
  
for (sp = class; *sp != '\0'; sp++)
   {
   if (*sp == '(')
      {
      Debug("+(\n");
      bracklevel++;
      continue;
      }

   if (*sp == ')')
      {
      Debug("-)\n");
      bracklevel--;
      continue;
      }
   
   if ((bracklevel == 0) && ((*sp == '.')||(*sp == '&')))
      {
      count++;
      }
   }

// This is checked in the lexer now, and so can be eliminated?
//
// if (bracklevel != 0)
//    {
//    char output[CF_BUFSIZE];
//    snprintf(output,CF_BUFSIZE,"Bracket mismatch, in [class=\"%s\"], level = %d\n",class,bracklevel);
//    yyerror(output);
//    FatalError("Aborted");
//    }

return count+1;
}

/*********************************************************************/

int IsBracketed(char *s)

 /* return true if the entire string is bracketed, not just if
    if contains brackets */

{ int i, level= 0;

if (*s != '(')
   {
   return false;
   }

for (i = 0; i < strlen(s)-1; i++)
   {
   if (s[i] == '(')
      {
      level++;
      }
   
   if (s[i] == ')')
      {
      level--;
      }

   if (level == 0)
      {
      return false;  /* premature ) */
      }
   }

return true;
}



