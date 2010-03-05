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

/*******************************************************************/

void LoadSystemConstants()

{
NewScalar("const","dollar","$",cf_str);
NewScalar("const","n","\n",cf_str);
NewScalar("const","r","\r",cf_str);
NewScalar("const","t","\t",cf_str);
NewScalar("const","endl","\n",cf_str);
/* NewScalar("const","0","\0",cf_str);  - this cannot work */
}

/*******************************************************************/
/* Variables                                                       */
/*******************************************************************/

void ForceScalar(char *lval,char *rval)

{ char rtype,retval[CF_MAXVARSIZE];

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

{ char *sp1,*sp2;
  struct Rval rvald;
   
Debug("NewScalar(%s,%s,%s)\n",scope,lval,rval);

//sp1 = strdup(lval);
//sp2 = strdup((char *)rval);

if (GetVariable(scope,lval,&rvald.item,&rvald.rtype) != cf_notype)
   {
   DeleteScalar(scope,lval);
   }

sp1 = lval;
sp2 = rval;

AddVariableHash(scope,sp1,sp2,CF_SCALAR,dt,NULL,0);
}

/*******************************************************************/

void IdempNewScalar(char *scope,char *lval,char *rval,enum cfdatatype dt)

{ char *sp1,*sp2;
  struct Rval rvald;
 
Debug("IdempNewScalar(%s,%s,%s)\n",scope,lval,rval);

if (GetVariable(scope,lval,&rvald.item,&rvald.rtype) != cf_notype)
   {
   return;
   }

sp1 = strdup(lval);
sp2 = strdup((char *)rval);

AddVariableHash(scope,sp1,sp2,CF_SCALAR,dt,NULL,0);
}

/*******************************************************************/

void DeleteScalar(char *scope,char *lval)

{ struct Scope *ptr;
  struct CfAssoc *ap;
  int slot;
 
ptr = GetScope(scope);
slot = GetHash(lval);

if (ptr == NULL)
   {
   return;
   }
 
if (ap = (struct CfAssoc *)(ptr->hashtable[slot]))
   {
   DeleteAssoc(ap);
   ptr->hashtable[slot] = NULL;
   }
else
   {
   Debug("Attempt to delete non existent variable %s in scope %s\n",lval,scope);
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

enum cfdatatype GetVariable(char *scope,char *lval,void **returnv, char *rtype)

{ char *sp;
  int slot,i,found = false;
  struct Scope *ptr = NULL;
  char scopeid[CF_MAXVARSIZE],vlval[CF_MAXVARSIZE],sval[CF_MAXVARSIZE];
  char expbuf[CF_EXPANDSIZE];
  
Debug("\nGetVariable(%s,%s) type=(to be determined)\n",scope,lval);

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
      *returnv = lval;
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

i = slot = GetHash(vlval);

if (ptr == NULL || ptr->hashtable == NULL)
   {
   Debug("Scope for variable \"%s.%s\" does not seem to exist\n",scope,lval);
   *returnv = lval;
   *rtype   = CF_SCALAR;
   return cf_notype;
   }

Debug("GetVariable(%s,%s): using scope '%s' for variable '%s'\n",scopeid,vlval,ptr->scope,vlval);

if (CompareVariable(vlval,ptr->hashtable[slot]) != 0)
   {
   /* Recover from previous hash collision */
   
   while (true)
      {
      i++;

      if (i >= CF_HASHTABLESIZE-1)
         {
         i = 0;
         }

      if (CompareVariable(vlval,ptr->hashtable[i]) == 0)
         {
         found = true;
         break;
         }

      /* Removed autolookup in Unix environment variables -
         implement as getenv() fn instead */

      if (i == slot)
         {
         found = false;
         break;
         }
      }

   if (!found)
      {
      Debug("No such variable found %s.%s\n",scope,lval);
      *returnv = lval;
      *rtype   = CF_SCALAR;
      return cf_notype;
      }
   }

Debug("return final variable type=%s, value={\n",CF_DATATYPES[(ptr->hashtable[i])->dtype]);

if (DEBUG)
   {
   ShowRval(stdout,(ptr->hashtable[i])->rval,(ptr->hashtable[i])->rtype);
   }
Debug("}\n");

*returnv = ptr->hashtable[i]->rval;
*rtype   = ptr->hashtable[i]->rtype;

return (ptr->hashtable[i])->dtype;
}

/*******************************************************************/

void DeleteVariable(char *scope,char *id)

{ int slot,i;
  struct Scope *ptr;
  
i = slot = GetHash(id);
ptr = GetScope(scope);

if (ptr == NULL)
   {
   return;
   }

if (CompareVariable(id,ptr->hashtable[slot]) != 0)
   {
   while (true)
      {
      i++;
      
      if (i == slot)
         {
         Debug("No variable matched\n");
         break;
         }
      
      if (i >= CF_HASHTABLESIZE-1)
         {
         i = 0;
         }
      
      if (CompareVariable(id,ptr->hashtable[i]) == 0)
         {
         free(ptr->hashtable[i]);
         ptr->hashtable[i] = NULL;
         }
      }
   }
 else
    {
    free(ptr->hashtable[i]);
    ptr->hashtable[i] = NULL;
    }   
}

/*******************************************************************/

int CompareVariable(char *lval,struct CfAssoc *ap)

{ char buffer[CF_BUFSIZE];

if (ap == NULL || lval == NULL)
   {
   return 1;
   }

return strcmp(ap->lval,lval);
}

/*******************************************************************/

int CompareVariableValue(void *rval,char rtype,struct CfAssoc *ap)

{ char buffer[CF_BUFSIZE];
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

{ char buffer[CF_BUFSIZE];
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

void DeleteAllVariables(char *scope)

{ int i;
  struct Scope *ptr;
  
ptr = GetScope(scope);
 
for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   if (ptr->hashtable[i] != NULL)
      {
      DeleteAssoc(ptr->hashtable[i]);
      ptr->hashtable[i] = NULL;
      }
   }
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
   snprintf(output,CF_BUFSIZE,"Broken variable syntax or bracket mismatch in (%s)",str);
   yyerror(output);
   return false;
   }

Debug("Found %d variables in (%s)\n",vars,str); 
return vars;
}

/*********************************************************************/

int IsCf3Scalar(char *str)

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
   snprintf(output,CF_BUFSIZE,"Broken variable syntax or bracket mismatch in (%s)",str);
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

char *ExtractInnerCf3VarString(char *str,char *substr)

{ char *sp;
  int bracks = 1;

Debug("ExtractInnerVarString( %s ) - syntax verify\n",str);

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
          if (isalnum((int)*sp) || IsIn(*sp,"_[]$.:-"))
             {
             }
          else
             {
             Debug("Illegal character found: '%c'\n", *sp);
             CfOut(cf_error,"","Illegal character somewhere in variable \"%s\" or nested expansion",str);
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
   snprintf(output,CF_BUFSIZE,"Broken variable syntax or bracket mismatch - inner (%s/%s)",str,substr);
   yyerror(output);
   return NULL;
   }

return sp-1;
}

/*********************************************************************/

char *ExtractOuterCf3VarString(char *str,char *substr)

  /* Should only by applied on str[0] == '$' */
    
{ char *sp;
  int dollar = false;
  int bracks = 0, onebrack = false;
  int nobracks = true;

Debug("ExtractOuterVarString(%s) - syntax verify\n",str);

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

