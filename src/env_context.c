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

#include "logic_expressions.h"

static bool ValidClassName(const char *str);
static int GetORAtom(char *start,char *buffer);
static int HasBrackets(char *s,struct Promise *pp);
static int IsBracketed(char *s);

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static int EvalClassExpression(struct Constraint *cp,struct Promise *pp)

{ int result_and = true;
  int result_or = false;
  int result_xor = 0;
  int result = 0,total = 0;
  char buffer[CF_MAXVARSIZE];
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
       
       fp = (struct FnCall *)cp->rval;        /* Special expansion of functions for control, best effort only */
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

// Class selection

if (strcmp(cp->lval,"select_class") == 0)
   {
   char splay[CF_MAXVARSIZE];
   int i,n;
   double hash;
   
   total = 0;

   for (rp = (struct Rlist *)cp->rval; rp != NULL; rp = rp->next)
      {
      total++;
      }

   if (total == 0)
      {
      CfOut(cf_error,""," !! No classes to select on RHS");
      PromiseRef(cf_error,pp);
      return false;      
      }

   snprintf(splay,CF_MAXVARSIZE,"%s+%s+%d",VFQNAME,VIPADDRESS,getuid());
   hash = (double)GetHash(splay);
   n = (int)(total*hash/(double)CF_HASHTABLESIZE);

   for (rp = (struct Rlist *)cp->rval,i = 0; rp != NULL; rp = rp->next,i++)
      {
      if (i == n)
         {
         NewClass(rp->item);
         return true;
         }
      }
   }

// Class distributions

if (strcmp(cp->lval,"dist") == 0)
   {
   for (rp = (struct Rlist *)cp->rval; rp != NULL; rp = rp->next)
      {
      result = Str2Int(rp->item);
      
      if (result < 0)
         {
         CfOut(cf_error,""," !! Non-positive integer in class distribution");
         PromiseRef(cf_error,pp);
         return false;
         }
      
      total += result;
      }

   if (total == 0)
      {
      CfOut(cf_error,""," !! An empty distribution was specified on RHS");
      PromiseRef(cf_error,pp);
      return false;      
      }
   }

fluct = drand48(); /* Get random number 0-1 */
cum = 0.0;

/* If we get here, anything remaining on the RHS must be a clist */

if (cp->type != CF_LIST)
   {
   CfOut(cf_error,""," !! RHS of promise body attribute \"%s\" is not a list\n",cp->lval);
   PromiseRef(cf_error,pp);
   return true;
   }

for (rp = (struct Rlist *)cp->rval; rp != NULL; rp = rp->next)
   {
   if (rp->type != CF_SCALAR)
      {
      return false;
      }

   result = IsDefinedClass((char *)(rp->item));

   result_and = result_and && result;
   result_or  = result_or || result;
   result_xor ^= result;

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

// Class combinations

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

/*******************************************************************/

void NewClass(const char *oclass)

{
struct Item *ip;
char class[CF_MAXVARSIZE];

strncpy(class, oclass, CF_MAXVARSIZE);
Chop(class);
CanonifyNameInPlace(class);

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

if (InAlphaList(VHEAP,class))
   {
   return;
   }

PrependAlphaList(&VHEAP,class);

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

void DeleteClass(char *class)

{ int i = (int)*class;

DeleteItemLiteral(&(VHEAP.list[i]),class);
DeleteItemLiteral(&(VADDCLASSES.list[i]),class);
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

if (InAlphaList(VHEAP,copy))
   {
   CfOut(cf_error,"","WARNING - private class \"%s\" in bundle \"%s\" shadows a global class - you should choose a different name to avoid conflicts",copy,bundle);
   }

if (InAlphaList(VADDCLASSES,copy))
   {
   return;
   }

PrependAlphaList(&VADDCLASSES,copy);

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

struct Rlist *SplitContextExpression(char *context,struct Promise *pp)

{ struct Rlist *list = NULL;
  char *sp,cbuff[CF_MAXVARSIZE];
  
if (context == NULL)
   {
   PrependRScalar(&list,"any",CF_SCALAR);
   }
else
   {
   for (sp = context; *sp != '\0'; sp++)
      {
      while (*sp == '|')
         {
         sp++;
         }
      
      memset(cbuff,0,CF_MAXVARSIZE);
      
      sp += GetORAtom(sp,cbuff);
      
      if (strlen(cbuff) == 0)
         {
         break;
         }
      
      if (IsBracketed(cbuff))
         {
         // Fully bracketed atom (protected)
         cbuff[strlen(cbuff)-1] = '\0';
         PrependRScalar(&list,cbuff+1,CF_SCALAR);
         }
      else
         {
         if (HasBrackets(cbuff,pp))             
            {
            struct Rlist *andlist = SplitRegexAsRList(cbuff,"[.&]+",99,false);
            struct Rlist *rp,*orlist = NULL;
            char buff[CF_MAXVARSIZE];
            char orstring[CF_MAXVARSIZE] = {0};
            char andstring[CF_MAXVARSIZE] = {0};

            // Apply distribution P.(A|B) -> P.A|P.B
            
            for (rp = andlist; rp != NULL; rp=rp->next)
               {
               if (IsBracketed(rp->item))
                  {
                  // This must be an OR string to be ORed and split into a list
                  *((char *)rp->item+strlen((char *)rp->item)-1) = '\0';

                  if (strlen(orstring) > 0)
                     {
                     strcat(orstring,"|");
                     }
                  
                  Join(orstring,(char *)(rp->item)+1,CF_MAXVARSIZE);
                  }
               else
                  {
                  if (strlen(andstring) > 0)
                     {
                     strcat(andstring,".");
                     }
                  
                  Join(andstring,rp->item,CF_MAXVARSIZE);
                  }

               // foreach ORlist, AND with AND string
               }

            if (strlen(orstring) > 0)
               {
               orlist = SplitRegexAsRList(orstring,"[|]+",99,false);
               
               for (rp = orlist; rp != NULL; rp=rp->next)
                  {
                  snprintf(buff,CF_MAXVARSIZE,"%s.%s",rp->item,andstring);
                  PrependRScalar(&list,buff,CF_SCALAR);
                  }
               }
            else
               {
               PrependRScalar(&list,andstring,CF_SCALAR);
               }
            
            DeleteRlist(orlist);
            DeleteRlist(andlist);
            }
         else
            {
            // Clean atom
            PrependRScalar(&list,cbuff,CF_SCALAR);
            }
         }
      
      if (*sp == '\0')
         {
         break;
         }
      }
   }
 
return list;
}

/*********************************************************************/

static int IsBracketed(char *s)

 /* return true if the entire string is bracketed, not just if
    if contains brackets */

{ int i, level= 0, yes = 0;

if (*s != '(')
   {
   return false;
   }

if (*(s+strlen(s)-1) != ')')
   {
   return false;
   }

if (strstr(s,")("))
   {
   CfOut(cf_error,""," !! Class expression \"%s\" has broken brackets",s);
   return false;
   }

for (i = 0; i < strlen(s); i++)
   {
   if (s[i] == '(')
      {
      yes++;
      level++;
      if (i > 0 && !strchr(".&|!(", s[i-1]))
         {
         CfOut(cf_error,""," !! Class expression \"%s\" has a missing operator in front of '('",s);
         }
      }
   
   if (s[i] == ')')
      {
      yes++;
      level--;
      if (i < strlen(s)-1 && !strchr(".&|!)", s[i+1]))
         {
         CfOut(cf_error,""," !! Class expression \"%s\" has a missing operator after of ')'",s);
         }
      }
   }


if (level != 0)
   {
   CfOut(cf_error,""," !! Class expression \"%s\" has broken brackets",s);
   return false;  /* premature ) */
   }

if (yes > 2)
   {
   // e.g. (a|b).c.(d|e)
   return false;
   }


return true;
}

/*********************************************************************/

static int GetORAtom(char *start,char *buffer)

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

static int HasBrackets(char *s,struct Promise *pp)

 /* return true if contains brackets */

{ int i, level= 0, yes = 0;

for (i = 0; i < strlen(s); i++)
   {
   if (s[i] == '(')
      {
      yes++;
      level++;
      if (i > 0 && !strchr(".&|!(", s[i+1]))
         {
         CfOut(cf_error,""," !! Class expression \"%s\" has a missing operator in front of '('",s);
         }
      }
   
   if (s[i] == ')')
      {
      level--;
      if (i < strlen(s)-1 && !strchr(".&|!)", s[i+1]))
         {
         CfOut(cf_error,""," !! Class expression \"%s\" has a missing operator after ')'",s);
         }
      }
   }

if (level != 0)
   {
   CfOut(cf_error,""," !! Class expression \"%s\" has unbalanced brackets",s);
   PromiseRef(cf_error,pp);
   return true;
   }

if (yes > 1)
   {
   CfOut(cf_error,""," !! Class expression \"%s\" has multiple brackets",s);
   PromiseRef(cf_error,pp);
   }
else if (yes)
   {
   return true;
   }

return false;
}

/**********************************************************************/
/* Utilities */
/**********************************************************************/

/* Return expression with error position highlighted. Result is on the heap. */

static char *HighlightExpressionError(const char *str, int position)
{
char *errmsg = malloc(strlen(str) + 3);
char *firstpart = strndup(str, position);
char *secondpart = strndup(str + position, strlen(str) - position);

sprintf(errmsg, "%s->%s", firstpart, secondpart);

free(secondpart);
free(firstpart);

return errmsg;
}

/**********************************************************************/
/* Debugging output */

static void IndentL(int level)
{
int i;
if (level > 0)
   {
   putc('\n', stderr);
   for(i = 0; i < level; ++i)
      {
      putc(' ', stderr);
      }
   }
}

/**********************************************************************/

static int IncIndent(int level, int inc)
{
if (level < 0)
   {
   return -level + inc;
   }
else
   {
   return level + inc;
   }
}

/**********************************************************************/

static void EmitStringExpression(StringExpression *e, int level)
{
if (!e)
   {
   return;
   }

switch (e->op)
   {
   case CONCAT:
      IndentL(level);
      fputs("(concat ", stderr);
      EmitStringExpression(e->val.concat.lhs, -IncIndent(level, 8));
      EmitStringExpression(e->val.concat.rhs, IncIndent(level, 8));
      fputs(")", stderr);
      break;
   case LITERAL:
      IndentL(level);
      fprintf(stderr, "\"%s\"", e->val.literal.literal);
      break;
   case VARREF:
      IndentL(level);
      fputs("($ ", stderr);
      EmitStringExpression(e->val.varref.name, -IncIndent(level, 3));
      break;
   default:
      FatalError("Unknown type of string expression: %d\n", e->op);
      break;
   }
}

/**********************************************************************/

static void EmitExpression(Expression *e, int level)
{
if (!e)
   {
   return;
   }

switch (e->op)
   {
   case OR:
   case AND:
      IndentL(level);
      fprintf(stderr, "(%s ", e->op == OR ? "|" : "&");
      EmitExpression(e->val.andor.lhs, -IncIndent(level, 3));
      EmitExpression(e->val.andor.rhs, IncIndent(level, 3));
      fputs(")", stderr);
      break;
   case NOT:
      IndentL(level);
      fputs("(- ", stderr);
      EmitExpression(e->val.not.arg, -IncIndent(level, 3));
      fputs(")", stderr);
      break;
   case EVAL:
      IndentL(level);
      fputs("(eval ", stderr);
      EmitStringExpression(e->val.eval.name, -IncIndent(level, 6));
      fputs(")", stderr);
      break;
   default:
      FatalError("Unknown logic expression type: %d\n", e->op);
   }
}

/*****************************************************************************/
/* Syntax-checking and evaluating various expressions */
/*****************************************************************************/

static void EmitParserError(const char *str, int position)
{
char *errmsg = HighlightExpressionError(str, position);
yyerror(errmsg);
free(errmsg);
}

/**********************************************************************/

/* To be used from parser only (uses yyerror) */
void ValidateClassSyntax(const char *str)
{
ParseResult res = ParseExpression(str, 0, strlen(str));

if (DEBUG || D1 || D2)
   {
   EmitExpression(res.result, 0);
   putc('\n', stderr);
   }

if (res.result)
   {
   FreeExpression(res.result);
   }

if (!res.result || res.position != strlen(str))
   {
   EmitParserError(str, res.position);
   }
}

/**********************************************************************/

static bool ValidClassName(const char *str)
{
ParseResult res = ParseExpression(str, 0, strlen(str));

if (res.result)
   {
   FreeExpression(res.result);
   }

return res.result && res.position == strlen(str);
}

/**********************************************************************/

static ExpressionValue EvalTokenAsClass(const char *classname, void *param)
{
if (IsItemIn(VNEGHEAP, classname))
   {
   return false;
   }
if (IsItemIn(VDELCLASSES, classname))
   {
   return false;
   }
if (InAlphaList(VHEAP, classname))
   {
   return true;
   }
if (InAlphaList(VADDCLASSES, classname))
   {
   return true;
   }
return false;
}

/**********************************************************************/

static char *EvalVarRef(const char *varname, void *param)
{
/*
 * There should be no unexpanded variables when we evaluate any kind of
 * logic expressions, until parsing of logic expression changes and they are
 * not pre-expanded before evaluation.
 */
return NULL;
}

/**********************************************************************/

bool IsDefinedClass(const char *class)
{
ParseResult res;

if (!class)
   {
   return true;
   }

res = ParseExpression(class, 0, strlen(class));

if (!res.result)
   {
   char *errexpr = HighlightExpressionError(class, res.position);
   CfOut(cf_error,"","Unable to parse class expression: %s", errexpr);
   free(errexpr);
   return false;
   }
else
   {
   ExpressionValue r = EvalExpression(res.result,
                                      &EvalTokenAsClass, &EvalVarRef,
                                      NULL);
   FreeExpression(res.result);

   Debug("Evaluate(%s) -> %d\n", class, r);

   /* r is EvalResult which could be ERROR */
   return r == true;
   }
}

/**********************************************************************/

bool IsExcluded(const char *exception)
{
return !IsDefinedClass(exception);
}

/**********************************************************************/

static ExpressionValue EvalTokenFromList(const char *token, void *param)
{
return InAlphaList(*(struct AlphaList *)param, token);
}

/**********************************************************************/

static bool EvalWithTokenFromList(const char *expr, struct AlphaList *token_list)
{
ParseResult res = ParseExpression(expr, 0, strlen(expr));

if (!res.result)
   {
   char *errexpr = HighlightExpressionError(expr, res.position);
   CfOut(cf_error, "", "Syntax error in expression: %s", errexpr);
   free(errexpr);
   return false; /* FIXME: return error */
   }
else
   {
   ExpressionValue r = EvalExpression(res.result,
                                      &EvalTokenFromList,
                                      &EvalVarRef,
                                      token_list);

   FreeExpression(res.result);

   /* r is EvalResult which could be ERROR */
   return r == true;
   }
}

/**********************************************************************/

/* Process result expression */

bool EvalProcessResult(const char *process_result, struct AlphaList *proc_attr)
{
return EvalWithTokenFromList(process_result, proc_attr);
}

/**********************************************************************/

/* File result expressions */

bool EvalFileResult(const char *file_result, struct AlphaList *leaf_attr)
{
return EvalWithTokenFromList(file_result, leaf_attr);
}

/*****************************************************************************/

void DeleteEntireHeap()

{
DeleteAlphaList(&VHEAP);
InitAlphaList(&VHEAP);
}

/*****************************************************************************/

void DeletePrivateClassContext()

{
DeleteAlphaList(&VADDCLASSES);
InitAlphaList(&VADDCLASSES);
DeleteItemList(VDELCLASSES);
VDELCLASSES = NULL;
}

/*****************************************************************************/

void PushPrivateClassContext()

{ struct AlphaList *ap = malloc(sizeof(struct AlphaList));

// copy to heap
PushStack(&PRIVCLASSHEAP,CopyAlphaListPointers(ap,&VADDCLASSES));
InitAlphaList(&VADDCLASSES);
}

/*****************************************************************************/

void PopPrivateClassContext()

{
  struct AlphaList *ap;
 
DeleteAlphaList(&VADDCLASSES);
PopStack(&PRIVCLASSHEAP,(void *)&ap,sizeof(VADDCLASSES));
CopyAlphaListPointers(&VADDCLASSES,ap);
free(ap);
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
  int ksize,vsize;
  char *key;
  void *value;
  time_t now = time(NULL);
  struct CfState q;
  char filename[CF_BUFSIZE];

if (LOOKUP)
  {
  return;
  }

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
   if (!InAlphaList(VHEAP,rp->item))
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

   sscanf(sp,"%250[^,]",currentitem);

   sp += strlen(currentitem);
      
   if (IsHardClass(currentitem))
      {
      FatalError("cfengine: You cannot use -D to define a reserved class!");
      }

   NewClass(currentitem);
   }
}

/*********************************************************************/

void NegateClassesFromString(char *classlist,struct Item **heap)

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

   sscanf(sp,"%250[^,]",currentitem);

   sp += strlen(currentitem);

   if (IsHardClass(currentitem))
      { char err[CF_BUFSIZE];
      sprintf (err,"Cannot negate the reserved class [%s]\n",currentitem);
      FatalError(err);
      }

   AppendItem(heap,currentitem,NULL);
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
     "SuSE","suse","fedora","Ubuntu","lsb_compliant","localhost",
     NULL
     };

  static char *prefixes[] =
     {
     "cfengine_","ipv4",
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
   if (strcmp(names[i],sp) == 0)
      {
      return true;
      }
   }

for (i = 0; prefixes[i] != NULL; i++)
   {
   if (strncmp(prefixes[i],sp,strlen(prefixes[i])) == 0)
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

{
  char file[CF_BUFSIZE];
  FILE *fp;
 
snprintf(file,CF_BUFSIZE,"%s/state/allclasses.txt",CFWORKDIR);

if ((fp = fopen(file,"w")) == NULL)
   {
   CfOut(cf_inform,"","Could not open allclasses cache file");
   return;
   }

ListAlphaList(fp,VHEAP,'\n');
ListAlphaList(fp,VADDCLASSES,'\n');
fclose(fp);
}

