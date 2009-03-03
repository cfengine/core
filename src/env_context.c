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
/* File: env_context.c                                                       */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

extern char *DAY_TEXT[];

struct CfState
   {
   unsigned int expires;
   enum statepolicy policy;
   };

/*****************************************************************************/

void KeepClassContextPromise(struct Promise *pp)

{ struct Attributes a;

a = GetClassContextAttributes(pp);

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
      NewClass(pp->promiser);
      }

   /* These are global and loaded once */
   *(pp->donep) = true;

   return;
   }

if (strcmp(pp->bundletype,THIS_AGENT) == 0 || FullTextMatch("edit_.*",pp->bundletype))
   {
   if (EvalClassExpression(a.context.expression,pp))
      {
      Debug(" ?> defining class %s\n",pp->promiser);
      NewBundleClass(pp->promiser,pp->bundle);
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
VADDCLASSES = NULL;
}
   

/*****************************************************************************/

void NewPersistentContext(char *name,unsigned int ttl_minutes,enum statepolicy policy)

{ int errno;
  DBT key,value;
  DB *dbp;
  struct CfState state;
  time_t now = time(NULL);
  char filename[CF_BUFSIZE];

snprintf(filename,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_STATEDB_FILE);

if (!OpenDB(filename,&dbp))
   {
   return;
   }

chmod(filename,0644);  
memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));
      
key.data = name;
key.size = strlen(name)+1;

if ((errno = dbp->get(dbp,NULL,&key,&value,0)) != 0)
   {
   if (errno != DB_NOTFOUND)
      {
      dbp->err(dbp,errno,NULL);
      dbp->close(dbp,0);
      return;
      }
   }
 
if (value.data != NULL)
   {
   memcpy((void *)&state,value.data,sizeof(state));
   
   if (state.policy == cfpreserve)
      {
      if (now < state.expires)
         {
         CfOut(cf_verbose,"","Persisent state %s is already in a preserved state --  %d minutes to go\n",name,(state.expires-now)/60);
         dbp->close(dbp,0);
         return;
         }
      }
   }
 else
    {
    CfOut(cf_verbose,"","New persistent state %s but empty\n",key.data);
    }
 
 
memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));
      
key.data = name;
key.size = strlen(name)+1;
 
state.expires = now + ttl_minutes * 60;
state.policy = policy; 
 
value.data = &state;
value.size = sizeof(state);
 
if ((errno = dbp->put(dbp,NULL,&key,&value,0)) != 0)
   {
   CfOut(cf_error,"db->put","Database put failed in peristent class");
   }
else
   {
   CfOut(cf_verbose,"","(Re)Set persistent state %s for %d minutes\n",name,ttl_minutes);
   }

dbp->close(dbp,0);
}

/*****************************************************************************/

void DeletePersistentContext(char *name)

{ int errno;
  DBT key,value;
  DB *dbp;
  char filename[CF_BUFSIZE];

snprintf(filename,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_STATEDB_FILE);

if (!OpenDB(filename,&dbp))
   {
   return;
   }

chmod(filename,0644); 

memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));
      
key.data = name;
key.size = strlen(name)+1;

if ((errno = dbp->del(dbp,NULL,&key,0)) != 0)
   {
   CfOut(cf_error,"db_store","delete db failed");
   }
 
Debug("Deleted any persistent state %s\n",name); 
dbp->close(dbp,0);
}

/*****************************************************************************/

void LoadPersistentContext()

{ DBT key,value;
  DB *dbp;
  DBC *dbcp;
  DB_ENV *dbenv = NULL;
  int ret;
  time_t now = time(NULL);
  struct CfState q;
  char filename[CF_BUFSIZE];

Banner("Loading persistent classes");
  
snprintf(filename,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_STATEDB_FILE);

if (!OpenDB(filename,&dbp))
   {
   return;
   }

/* Acquire a cursor for the database. */

if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0)
   {
   CfOut(cf_error,"","Error reading from persistent state database");
   dbp->err(dbp, ret, "DB->cursor");
   return;
   }

 /* Initialize the key/data return pair. */

memset(&key, 0, sizeof(key));
memset(&value, 0, sizeof(value));
 
 /* Walk through the database and print out the key/data pairs. */

while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0)
   {
   memcpy((void *)&q,value.data,sizeof(struct CfState));

   Debug(" - Found key %s...\n",key.data);

   if (now > q.expires)
      {
      CfOut(cf_verbose,""," Persistent class %s expired\n",key.data);
      if ((errno = dbp->del(dbp,NULL,&key,0)) != 0)
         {
         CfOut(cf_error,"db_store","");
         }
      }
   else
      {
      CfOut(cf_verbose,""," Persistent class %s for %d more minutes\n",key.data,(q.expires-now)/60);
      CfOut(cf_verbose,""," Adding persistent class %s to heap\n",key.data);
      NewClass(key.data);
      }
   }
 
dbcp->c_close(dbcp);
dbp->close(dbp,0);

Banner("Loaded persistent memory");
}

/*****************************************************************************/

void AddEphemeralClasses(struct Rlist *classlist)

{ struct Rlist *rp;

for (rp = classlist; rp != NULL; rp = rp->next)
   {
   NewClass(rp->item);
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

   NewClass(CanonifyName(currentitem));
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

   NewClass(CanonifyName(pref));
   }
}

/*********************************************************************/

int IsHardClass(char *sp)  /* true if string matches a hardwired class e.g. hpux */

{ int i;

for (i = 2; CLASSTEXT[i] != '\0'; i++)
   {
   if (strcmp(CLASSTEXT[i],sp) == 0)
      {
      return(true);
      }
   }

for (i = 0; i < 7; i++)
   {
   if (strcmp(DAY_TEXT[i],sp)==0)
      {
      return(false);
      }
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
*classes = (char *)GetConstraint("ifvarclass",pp->conlist,CF_SCALAR);

if (*classes == NULL)
   {
   return false;
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

if (cp->type == CF_FNCALL)
   {
   /* Special expansion of functions for control, best effort only */
   struct Rval newret;
   struct FnCall *fp = (struct FnCall *)cp->rval;
   newret = EvaluateFunctionCall(fp,pp);
   DeleteFnCall(fp);
   cp->rval = newret.item;
   cp->type = newret.rtype;
   }

if (strcmp(cp->lval,"expression") == 0)
   {
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

for (rp = (struct Rlist *)cp->rval; rp != NULL; rp = rp->next)
   {
   result = IsDefinedClass((char *)(rp->item));

   result_and = result_and && result;
   result_or  = result_or || result;
   result_xor += result;

   if (total > 0)
      {
      prob = ((double)Str2Int(rp->item))/((double)total);
      cum += prob;
      
      if ((fluct < cum) || rp->next == NULL)
         {
         snprintf(buffer,CF_MAXVARSIZE,"%s_%s",pp->promiser,rp->item);
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

void NewClass(char *class)

{
Chop(class);
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

   NewClass(CanonifyName(pref));
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

if (IsItemIn(VADDCLASSES,copy))
   {
   return;
   }

AppendItem(&VADDCLASSES,copy,CONTEXTID);
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

   if (IsItemIn(VNEGHEAP,atom))
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

if (bracklevel != 0)
   {
   char output[CF_BUFSIZE];
   snprintf(output,CF_BUFSIZE,"Bracket mismatch, in [class=%s], level = %d\n",class,bracklevel);
   yyerror(output);
   FatalError("Aborted");
   }

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



