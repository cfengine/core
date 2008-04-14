/*******************************************************************/
/*                                                                 */
/* vars.c                                                          */
/*                                                                 */
/*******************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"


/*******************************************************************/
/* Variables                                                       */
/*******************************************************************/

void NewScalar(char *scope,char *lval,char *rval,enum cfdatatype dt)

{ char *sp1,*sp2;
 
Debug("NewScalar(%s,%s,%s)\n",scope,lval,rval);

sp1 = strdup(lval);
sp2 = strdup((char *)rval);

AddVariableHash(scope,sp1,sp2,CF_SCALAR,dt,NULL,0);
}

/*******************************************************************/

void NewList(char *scope,char *lval,void *rval,enum cfdatatype dt)

{ char *sp1;
 
Debug("NewList(%s,%s,%s)\n",scope,lval,rval);

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

if (strstr(sval,"."))
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

if (ptr == NULL)
   {
   /* Assume current scope */
   strcpy(vlval,lval);
   ptr = GetScope(scopeid);
   }

i = slot = GetHash(vlval);

if (ptr == NULL || ptr->hashtable == NULL)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Scope %s does not seem to exist -- no \"common control\" body?\n",scope);
   return cf_notype;
   }

Debug("GetVariableValue(%s,%s): using scope '%s' for variable '%s'\n",scopeid,vlval,ptr->scope,vlval);

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
      return cf_notype;
      }
   }

Debug("return final variable type=%s, value={%s}\n",CF_DATATYPES[(ptr->hashtable[i])->dtype],(ptr->hashtable[i])->rval);

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

snprintf(varstr,CF_MAXVARSIZE-1,"${%s}",v);
if (strstr(s,varstr) != NULL)
   {
   return true;
   }

snprintf(varstr,CF_BUFSIZE,"$(%s)",v);
if (strstr(s,varstr) != NULL)
   {
   return true;
   }

snprintf(varstr,CF_MAXVARSIZE-1,"@{%s}",v);
if (strstr(s,varstr) != NULL)
   {
   return true;
   }

snprintf(varstr,CF_BUFSIZE,"@(%s)",v);
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

Debug1("IsVarString(%s) - syntax verify\n",str);
  
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
 
 
 if (bracks != 0)
    {
    yyerror("Incomplete variable syntax or bracket mismatch");
    return false;
    }
 
 Debug("Found %d variables in (%s)\n",vars,str); 
 return vars;
}

/*******************************************************************/

int DefinedVariable(char *name)

{ struct Rval rval;
 
if (GetVariable("this",name,&rval.item,&rval.rtype) == cf_notype)
   {
   return false;
   }

return true;
}
