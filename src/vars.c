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

/*******************************************************************/
/*                                                                 */
/* vars.c                                                          */
/*                                                                 */
/*******************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

static void ExtendList(char *scope,char *lval,void *rval,enum cfdatatype dt);
static void IdempNewScalar(char *scope,char *lval,char *rval,enum cfdatatype dt);
static void DeleteAllVariables(char *scope);
static int IsCf3Scalar(char *str);

/*******************************************************************/

void LoadSystemConstants()

{
NewScalar("const","dollar","$",cf_str);
NewScalar("const","n","\n",cf_str);
NewScalar("const","r","\r",cf_str);
NewScalar("const","t","\t",cf_str);
NewScalar("const","endl","\n",cf_str);
/* NewScalar("const","0","\0",cf_str);  - this cannot work */

#ifdef HAVE_NOVA
Nova_EnterpriseDiscovery();
#endif
}

/*******************************************************************/
/* Variables                                                       */
/*******************************************************************/

void ForceScalar(char *lval,char *rval)

{ char rtype,*retval;

if (THIS_AGENT_TYPE != cf_agent && THIS_AGENT_TYPE != cf_know)
   {
   return;
   }

if (GetVariable("match",lval,(void *)&retval,&rtype) != cf_notype)
   {
   DeleteVariable("match",lval);
   }

NewScalar("match",lval,rval,cf_str);
Debug("Setting local variable \"match.%s\" context; $(%s) = %s\n",lval,lval,rval);
}

/*******************************************************************/

void NewScalar(char *scope,char *lval,char *rval,enum cfdatatype dt)

{ struct Rval rvald;
  struct Scope *ptr;

Debug("NewScalar(%s,%s,%s)\n",scope,lval,rval);

ptr = GetScope(scope);

if (ptr == NULL)
   {
   CfOut(cf_error, "", "!! Attempt to add variable \"%s\" to non-existant scope \"%s\" - ignored", lval, scope);
   return;
   }

// Newscalar allocates memory through NewAssoc

if (GetVariable(scope,lval,&rvald.item,&rvald.rtype) != cf_notype)
   {
   DeleteScalar(scope,lval);
   }

AddVariableHash(scope,lval,rval,CF_SCALAR,dt,NULL,0);
}

/*******************************************************************/

static void IdempNewScalar(char *scope,char *lval,char *rval,enum cfdatatype dt)

{
  struct Rval rvald;
 
Debug("IdempNewScalar(%s,%s,%s)\n",scope,lval,rval);

if (GetVariable(scope,lval,&rvald.item,&rvald.rtype) != cf_notype)
   {
   return;
   }

AddVariableHash(scope,lval,rval,CF_SCALAR,dt,NULL,0);
}

/*******************************************************************/

void DeleteScalar(char *scope_name, char *lval)

{
struct Scope *scope = GetScope(scope_name);

if (scope == NULL)
   {
   return;
   }

if (HashDeleteElement(scope->hashtable, lval) == false)
   {
   Debug("Attempt to delete non-existent variable %s in scope %s\n", lval, scope_name);
   }
}

/*******************************************************************/

void NewList(char *scope,char *lval,void *rval,enum cfdatatype dt)

{ char *sp1;
  struct Rval rvald;

if (GetVariable(scope,lval,&rvald.item,&rvald.rtype) != cf_notype)
   {
   DeleteVariable(scope,lval);
   }
 
sp1 = strdup(lval);
AddVariableHash(scope,sp1,rval,CF_LIST,dt,NULL,0);
}

/*******************************************************************/

static void ExtendList(char *scope,char *lval,void *rval,enum cfdatatype dt)

{
  struct Rval rvald;
  struct Rlist *list,*rp;

if (GetVariable(scope,lval,&rvald.item,&rvald.rtype) != cf_notype)
   {
   list = rvald.item;
   
   switch(rvald.rtype)
      {
      case CF_SCALAR:
          IdempAppendRlist(&list,rval,CF_SCALAR);
          break;
          
      case CF_LIST:
          for (rp = rval; rp != NULL; rp = rp->next)
             {
             IdempAppendRlist(&list,rval,CF_SCALAR);
             }
          break;
          
      case CF_FNCALL:
          rp = IdempAppendRScalar(&list,"dummy",CF_SCALAR);
          free(rp->item);
          rp->item = rval;
          rp->type = CF_FNCALL;
          break;
          
      default:
          CfOut(cf_error,"","Attempt to extend a list with something unknown");
          break;
      }
   }
else
   {
   NewList(scope,lval,rval,dt);
   }
}

/*******************************************************************/

enum cfdatatype GetVariable(const char *scope, const char *lval, void **returnv, char *rtype)

{
  struct Scope *ptr = NULL;
  char scopeid[CF_MAXVARSIZE],vlval[CF_MAXVARSIZE],sval[CF_MAXVARSIZE];
  char expbuf[CF_EXPANDSIZE];
  CfAssoc *assoc;

Debug("\nGetVariable(%s,%s) type=(to be determined)\n",scope,lval);

if (lval == NULL)
   {
   *returnv = (void*)lval;
   *rtype   = CF_SCALAR;
   return cf_notype;
   }

if (!IsExpandable(lval))
   {
   strncpy(sval,lval,CF_MAXVARSIZE-1);
   }
else
   {
   if (ExpandScalar(lval,expbuf))
      {
      strncpy(sval,expbuf,CF_MAXVARSIZE-1);
      }
   else
      {
      *returnv = (void*)lval;
      *rtype   = CF_SCALAR;
      Debug("Couldn't expand array-like variable (%s) due to undefined dependencies\n",lval);
      return cf_notype;
      }
   }

if (IsQualifiedVariable(sval))
   {
   scopeid[0] = '\0';
   sscanf(sval,"%[^.].%s",scopeid,vlval);
   Debug("Variable identifier %s is prefixed with scope id %s\n",vlval,scopeid);
   ptr = GetScope(scopeid);
   }
else
   {
   strcpy(vlval,sval);
   strcpy(scopeid,scope);
   }

Debug("Looking for %s.%s\n",scopeid,vlval);

if (ptr == NULL)
   {
   /* Assume current scope */
   strcpy(vlval,lval);
   ptr = GetScope(scopeid);
   }

if (ptr == NULL)
   {
   Debug("Scope for variable \"%s.%s\" does not seem to exist\n",scope,lval);
   *returnv = (void*)lval;
   *rtype   = CF_SCALAR;
   return cf_notype;
   }

Debug("GetVariable(%s,%s): using scope '%s' for variable '%s'\n",scopeid,vlval,ptr->scope,vlval);

assoc = HashLookupElement(ptr->hashtable, vlval);

if (assoc == NULL)
   {
   Debug("No such variable found %s.%s\n\n",scopeid,lval);
   *returnv = (void*)lval;
   *rtype   = CF_SCALAR;
   return cf_notype;
   }

Debug("return final variable type=%s, value={\n",CF_DATATYPES[assoc->dtype]);

if (DEBUG)
   {
   ShowRval(stdout,assoc->rval,assoc->rtype);
   }
Debug("}\n");

*returnv = assoc->rval;
*rtype   = assoc->rtype;

return assoc->dtype;
}

/*******************************************************************/

void DeleteVariable(char *scope,char *id)

{
struct Scope *ptr = GetScope(scope);

if (ptr == NULL)
   {
   return;
   }

if (HashDeleteElement(ptr->hashtable, id) == false)
   {
   Debug("No variable matched %s\n",id);
   }
}

/*******************************************************************/

int CompareVariable(const char *lval,struct CfAssoc *ap)

{

if (ap == NULL || lval == NULL)
   {
   return 1;
   }

return strcmp(ap->lval,lval);
}

/*******************************************************************/

int CompareVariableValue(void *rval,char rtype,struct CfAssoc *ap)

{
  struct Rlist *list, *rp;

if (ap == NULL || rval == NULL)
   {
   return 1;
   }

switch (rtype)
   {
   case CF_SCALAR:
       return strcmp(ap->rval,rval);

   case CF_LIST:
       list = (struct Rlist *)rval;
       
       for (rp = list; rp != NULL; rp=rp->next)
          {
          if (!CompareVariableValue(rp->item,rp->type,ap))
             {
             return -1;
             }
          }
       
       return 0;

   default:
       return 0;
   }
    
return strcmp(ap->rval,rval);
}

/*******************************************************************/

int UnresolvedVariables(struct CfAssoc *ap,char rtype)

{
  struct Rlist *list, *rp;

if (ap == NULL)
   {
   return false;
   }

switch (rtype)
   {
   case CF_SCALAR:
       return IsCf3VarString(ap->rval);
       
   case CF_LIST:
       list = (struct Rlist *)ap->rval;
       
       for (rp = list; rp != NULL; rp=rp->next)
          {
          if (IsCf3VarString(rp->item))
             {
             return true;
             }
          }
       
       return false;

   default:
       return false;
   }
}

/*******************************************************************/

int UnresolvedArgs(struct Rlist *args)
    
{ struct Rlist *rp;

for (rp = args; rp != NULL; rp = rp->next)
   {
   if (rp->type != CF_SCALAR)
      {
      return true;
      }
   
   if (IsCf3Scalar(rp->item))
      {
      return true;
      }
   }

return false;
}

/*******************************************************************/

static void DeleteAllVariables(char *scope)
{
struct Scope *ptr;
ptr = GetScope(scope);
HashClear(ptr->hashtable);
}

/******************************************************************/

int StringContainsVar(char *s,char *v)

{ char varstr[CF_MAXVARSIZE];

if (s == NULL)
   {
   return false;
   }
 
snprintf(varstr,CF_MAXVARSIZE-1,"${%s}",v);

if (strstr(s,varstr) != NULL)
   {
   return true;
   }

snprintf(varstr,CF_MAXVARSIZE-1,"$(%s)",v);
if (strstr(s,varstr) != NULL)
   {
   return true;
   }

snprintf(varstr,CF_MAXVARSIZE-1,"@{%s}",v);

if (strstr(s,varstr) != NULL)
   {
   return true;
   }

snprintf(varstr,CF_MAXVARSIZE-1,"@(%s)",v);
if (strstr(s,varstr) != NULL)
   {
   return true;
   }

return false;
}

/*********************************************************************/

int IsCf3VarString(char *str)

{ char *sp;
  char left = 'x', right = 'x';
  int dollar = false;
  int bracks = 0, vars = 0;

Debug1("IsCf3VarString(%s) - syntax verify\n",str);

if (str == NULL)
   {
   return false;
   }

for (sp = str; *sp != '\0' ; sp++)       /* check for varitems */
   {
   switch (*sp)
      {
      case '$':
      case '@':
          if (*(sp+1) == '{' || *(sp+1) == '(')
             {
             dollar = true;
             }
          break;
      case '(':
      case '{': 
          if (dollar)
             {
             left = *sp;    
             bracks++;
             }
          break;
      case ')':
      case '}': 
          if (dollar)
             {
             bracks--;
             right = *sp;
             }
          break;
      }

   /* Some chars cannot be in variable ids, e.g.
      $(/bin/cat file) is legal in bash */

   if (bracks > 0)
      {
      switch (*sp)
         {
         case '/':
             return false;
         }
      }
   
   if (left == '(' && right == ')' && dollar && (bracks == 0))
      {
      vars++;
      dollar=false;
      }
   
   if (left == '{' && right == '}' && dollar && (bracks == 0))
      {
      vars++;
      dollar = false;
      }
   }
 
 
if (dollar && (bracks != 0))
   {
   char output[CF_BUFSIZE];
   snprintf(output,CF_BUFSIZE,"Broken variable syntax or bracket mismatch in string (%s)",str);
   yyerror(output);
   return false;
   }

Debug("Found %d variables in (%s)\n",vars,str); 
return vars;
}

/*********************************************************************/

static int IsCf3Scalar(char *str)

{ char *sp;
  char left = 'x', right = 'x';
  int dollar = false;
  int bracks = 0, vars = 0;

Debug1("IsCf3Scalar(%s) - syntax verify\n",str);

if (str == NULL)
   {
   return false;
   }
  
for (sp = str; *sp != '\0' ; sp++)       /* check for varitems */
   {
   switch (*sp)
      {
      case '$':
          if (*(sp+1) == '{' || *(sp+1) == '(')
             {
             dollar = true;
             }
          break;
      case '(':
      case '{': 
          if (dollar)
             {
             left = *sp;    
             bracks++;
             }
          break;
      case ')':
      case '}': 
          if (dollar)
             {
             bracks--;
             right = *sp;
             }
          break;
      }
   
   /* Some chars cannot be in variable ids, e.g.
      $(/bin/cat file) is legal in bash */

   if (bracks > 0)
      {
      switch (*sp)
         {
         case '/':
             return false;
         }
      }

   if (left == '(' && right == ')' && dollar && (bracks == 0))
      {
      vars++;
      dollar=false;
      }
   
   if (left == '{' && right == '}' && dollar && (bracks == 0))
      {
      vars++;
      dollar = false;
      }
   }
 
 
if (dollar && (bracks != 0))
   {
   char output[CF_BUFSIZE];
   snprintf(output,CF_BUFSIZE,"Broken scalar variable syntax or bracket mismatch in \"%s\"",str);
   yyerror(output);
   return false;
   }

Debug("Found %d variables in (%s)\n",vars,str); 
return vars;
}

/*******************************************************************/

int DefinedVariable(char *name)

{ struct Rval rval;

if (name == NULL)
   {
   return false;
   }
 
if (GetVariable("this",name,&rval.item,&rval.rtype) == cf_notype)
   {
   return false;
   }

return true;
}

/*******************************************************************/

int BooleanControl(char *scope,char *name)

{ char varbuf[CF_BUFSIZE], rtype;

if (name == NULL)
   {
   return false;
   }
 
if (GetVariable(scope,name,(void *)varbuf,&rtype) != cf_notype)
   {
   return GetBoolean(varbuf);
   }

return false;
}

/*******************************************************************/

const char *ExtractInnerCf3VarString(const char *str,char *substr)

{ const char *sp;
  int bracks = 1;

Debug("ExtractInnerVarString( %s ) - syntax verify\n",str);

if (str == NULL || strlen(str) == 0)
   {
   return NULL;
   }

memset(substr,0,CF_BUFSIZE);

if (*(str+1) != '(' && *(str+1) != '{')
   {
   return NULL;
   }

/* Start this from after the opening $( */

for (sp = str+2; *sp != '\0' ; sp++)       /* check for varitems */
   {
   switch (*sp)
      {
      case '(':
      case '{': 
          bracks++;
          break;
      case ')':
      case '}': 
          bracks--;
          break;
          
      default:
          if (isalnum((int)*sp) || strchr("_[]$.:-", *sp))
             {
             }
          else
             {
             Debug("Illegal character found: '%c'\n", *sp);
             Debug("Illegal character somewhere in variable \"%s\" or nested expansion",str);
             }
      }
   
   if (bracks == 0)
      {
      strncpy(substr,str+2,sp-str-2);
      Debug("Returning substring value %s\n",substr);
      return substr;
      }
   }

if (bracks != 0)
   {
   char output[CF_BUFSIZE];
   if (strlen(substr) > 0)
      {
      snprintf(output,CF_BUFSIZE,"Broken variable syntax or bracket mismatch - inner (%s/%s)",str,substr);
      yyerror(output);
      }
   return NULL;
   }

return sp-1;
}

/*********************************************************************/

const char *ExtractOuterCf3VarString(const char *str, char *substr)

  /* Should only by applied on str[0] == '$' */
    
{ const char *sp;
  int dollar = false;
  int bracks = 0, onebrack = false;
  int nobracks = true;

Debug("ExtractOuterVarString(\"%s\") - syntax verify\n",str);

memset(substr,0,CF_BUFSIZE);
 
for (sp = str; *sp != '\0' ; sp++)       /* check for varitems */
   {
   switch (*sp)
      {
      case '$':
          dollar = true;
          switch (*(sp+1))
             {
             case '(':
             case '{': 
                 break;
             default:
                 /* Stray dollar not a variable */
                 return NULL;
             }
          break;
      case '(':
      case '{': 
          bracks++;
          onebrack = true;
          nobracks = false;
          break;
      case ')':
      case '}': 
          bracks--;
          break;
      }
   
   if (dollar && (bracks == 0) && onebrack)
      {
      strncpy(substr,str,sp-str+1);
      Debug("Extracted outer variable |%s|\n",substr);
      return substr;
      }
   }

if (dollar == false)
   {
   return str; /* This is not a variable*/
   }

if (bracks != 0)
   {
   char output[CF_BUFSIZE];
   snprintf(output,CF_BUFSIZE,"Broken variable syntax or bracket mismatch in - outer (%s/%s)",str,substr);
   yyerror(output);
   return NULL;
   }

/* Return pointer to first position in string (shouldn't happen)
   as long as we only call this function from the first $ position */

return str;
}

/*********************************************************************/

int IsQualifiedVariable(char *var)

{ int isarraykey = false;
  char *sp;
 
for (sp = var; *sp != '\0'; sp++)
   {
   if (*sp == '[')
      {
      isarraykey = true;
      }
   
   if (isarraykey)
      {
      return false;
      }
   else
      {
      if (*sp == '.')
         {
         return true;
         }      
      }
   }

return false;
}

/*********************************************************************/

int IsCfList(char *type)
{
  char *listTypes[] = { "sl", "il", "rl", "ml", NULL };
  int i;
  

  for(i = 0; listTypes[i] != NULL; i++)
    {
      if(strcmp(type, listTypes[i]) == 0)
	{
	  return true;
	}
    }

  return false;
}

/*******************************************************************/

int AddVariableHash(char *scope,char *lval,void *rval,char rtype,enum cfdatatype dtype,char *fname,int lineno)

{ struct Scope *ptr;
  struct Rlist *rp;
  struct CfAssoc *assoc;

if (rtype == CF_SCALAR)
   {
   Debug("AddVariableHash(%s.%s=%s (%s) rtype=%c)\n",scope,lval,rval,CF_DATATYPES[dtype],rtype);
   }
else
   {
   Debug("AddVariableHash(%s.%s=(list) (%s) rtype=%c)\n",scope,lval,CF_DATATYPES[dtype],rtype);
   }

if (lval == NULL || scope == NULL)
   {
   CfOut(cf_error,"","scope.value = %s.%s = %s",scope,lval,rval);
   ReportError("Bad variable or scope in a variable assignment");
   FatalError("Should not happen - forgotten to register a function call in fncall.c?");
   }

if (rval == NULL)
   {
   Debug("No value to assignment - probably a parameter in an unused bundle/body\n");
   return false;
   }

if (strlen(lval) > CF_MAXVARSIZE)
   {
   ReportError("variable lval too long");
   return false;
   }

/* If we are not expanding a body template, check for recursive singularities */

if (strcmp(scope,"body") != 0)
   {
   switch (rtype)
      {
      case CF_SCALAR:

          if (StringContainsVar((char *)rval,lval))
             {
             CfOut(cf_error,"","Scalar variable %s.%s contains itself (non-convergent): %s",scope,lval,(char *)rval);
             return false;
             }
          break;

      case CF_LIST:

          for (rp = rval; rp != NULL; rp=rp->next)
             {
             if (StringContainsVar((char *)rp->item,lval))
                {
                CfOut(cf_error,"","List variable %s contains itself (non-convergent)",lval);
                return false;
                }
             }
          break;

      }
   }

ptr = GetScope(scope);

if (ptr == NULL)
   {
   return false;
   }

// Look for outstanding lists in variable rvals

if (THIS_AGENT_TYPE == cf_common)
   {
   struct Rlist *listvars = NULL, *scalarvars = NULL;

   if (strcmp(CONTEXTID,"this") != 0)
      {
      ScanRval(CONTEXTID,&scalarvars,&listvars,rval,rtype,NULL);

      if (listvars != NULL)
         {
         CfOut(cf_error,""," !! Redefinition of variable \"%s\" (embedded list in RHS) in context \"%s\"",lval,CONTEXTID);
         }

      DeleteRlist(scalarvars);
      DeleteRlist(listvars);
      }
   }

assoc = HashLookupElement(ptr->hashtable, lval);

if (assoc)
   {
   if (CompareVariableValue(rval, rtype, assoc) == 0)
      {
      /* Identical value, keep as is */
      }
   else
      {
      /* Different value, bark and replace */
      if (!UnresolvedVariables(assoc, rtype))
         {
         CfOut(cf_inform,""," !! Duplicate selection of value for variable \"%s\" in scope %s",lval,ptr->scope);
         if (fname)
            {
            CfOut(cf_inform,""," !! Rule from %s at/before line %d\n",fname,lineno);
            }
         else
            {
            CfOut(cf_inform,""," !! in bundle parameterization\n",fname,lineno);
            }
         }
      DeleteRvalItem(assoc->rval, assoc->rtype);
      assoc->rval = CopyRvalItem(rval, rtype);
      assoc->rtype = rtype;
      assoc->dtype = dtype;
      Debug("Stored \"%s\" in context %s\n",lval,scope);
      }
   }
else
   {
   if (!HashInsertElement(ptr->hashtable, lval, rval, rtype, dtype))
      {
      FatalError("Hash table is full");
      }
   }

Debug("Added Variable %s in scope %s with value (omitted)\n",lval,scope);
return true;
}

/*******************************************************************/

void DeRefListsInHashtable(char *scope,struct Rlist *namelist,struct Rlist *dereflist)

// Go through scope and for each variable in name-list, replace with a
// value from the deref "lol" (list of lists) clock

{ int len;
  struct Scope *ptr;
  struct Rlist *rp,*state;
  struct CfAssoc *cplist;
  HashIterator i;
  CfAssoc *assoc;

if ((len = RlistLen(namelist)) != RlistLen(dereflist))
   {
   CfOut(cf_error,""," !! Name list %d, dereflist %d\n",len, RlistLen(dereflist));
   FatalError("Software Error DeRefLists... correlated lists not same length");
   }

if (len == 0)
   {
   return;
   }

ptr = GetScope(scope);
i = HashIteratorInit(ptr->hashtable);

while ((assoc = HashIteratorNext(&i)))
   {
   for (rp = dereflist; rp != NULL; rp = rp->next)
      {
      cplist = (struct CfAssoc *)rp->item;

      if (strcmp(cplist->lval,assoc->lval) == 0)
         {
         /* Link up temp hash to variable lol */

         state = (struct Rlist *)(cplist->rval);

         if (rp->state_ptr == NULL || rp->state_ptr->type == CF_FNCALL)
            {
            /* Unexpanded function, or blank variable must be skipped.*/
            return;
            }

         if (rp->state_ptr)
            {
            Debug("Rewriting expanded type for %s from %s to %s\n",assoc->lval,CF_DATATYPES[assoc->dtype],rp->state_ptr->item);

            // must first free existing rval in scope, then allocate new (should always be string)
            DeleteRvalItem(assoc->rval,assoc->rtype);

            // avoids double free - borrowing value from lol (freed in DeleteScope())
            assoc->rval = strdup(rp->state_ptr->item);
            }

         switch(assoc->dtype)
            {
            case cf_slist:
               assoc->dtype = cf_str;
               assoc->rtype = CF_SCALAR;
               break;
            case cf_ilist:
               assoc->dtype = cf_int;
               assoc->rtype = CF_SCALAR;
               break;
            case cf_rlist:
               assoc->dtype = cf_real;
               assoc->rtype = CF_SCALAR;
               break;
            }

         Debug(" to %s\n",CF_DATATYPES[assoc->dtype]);
         }
      }
   }
}
