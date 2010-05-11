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

/*********************************************************************/
/*                                                                   */
/*  Variable expansion in cf3                                        */
/*                                                                   */
/*********************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*

Expanding variables is easy -- expanding lists automagically requires
some thought. Remember that

promiser <=> CF_SCALAR
promisee <=> CF_LIST

Expanding all bodies in the constraint list, we have

lval <=> CF_LIST|CF_SCALAR

Now the rule for variable substitution is that any list variable @(name)
substituted directly for a LIST is not iterated, but dropped into
place, i.e. in list-lvals and the promisee (since this would be
equivalent to a re-concatenation of the expanded separate promises)

Any list variable occuring within a scalar or in place of a scalar
is assumed to be iterated i.e. $(name).

To expand a promise, we build temporary hash tables. There are two
stages, to this - one is to create a promise copy including all of the
body templates and translate the parameters. This requires one round
of expansion with scopeid "body". Then we use this fully assembled promise
and expand vars and function calls.

To expand the variables in a promise we need to 

   -- first get all strings, also parameterized bodies, which
      could also be lists
                                                                     /
        //  ScanRval("scope",&lol,"ksajd$(one)$(two)...$(three)"); \/ 
        
   -- compile an ordered list of variables involved , with types -           /
      assume all are lists - these are never inside sub-bodies by design,  \/
      so all expansion data are in the promise itself
      can also be variables based on list items - derived like arrays x[i]

   -- Copy the promise to a temporary promise + constraint list, expanding one by one,   /
      then execute that                                                                \/

      -- In a sub-bundle, create a new context and make hashes of the the
      transferred variables in the temporary context

   -- bodies cannot contain iterators

   -- we've already checked types of lhs rhs, must match so an iterator
      can only be in a non-naked variable?
   
   -- form the outer loops to generate combinations

Note, we map the current context into a fluid context "this" that maps
every list into a scalar during iteration. Thus "this" never contains
lists. This presents a problem for absolute references like $(abs.var),
since these cannot be mapped into "this" without some magic.
   
**********************************************************************/

void ExpandPromise(enum cfagenttype agent,char *scopeid,struct Promise *pp,void *fnptr)

{ struct Rlist *rp, *listvars = NULL, *scalarvars = NULL;
  struct Constraint *cp;
  struct Promise *pcopy;

Debug("****************************************************\n");
Debug("* ExpandPromises (scope = %s )\n",scopeid);
Debug("****************************************************\n\n");

DeleteScope("match"); /* in case we expand something expired accidentially */

pcopy = DeRefCopyPromise(scopeid,pp);

ScanRval(scopeid,&scalarvars,&listvars,pcopy->promiser,CF_SCALAR,pp);

if (pcopy->promisee != NULL)
   {
   ScanRval(scopeid,&scalarvars,&listvars,pp->promisee,pp->petype,pp);
   }

for (cp = pcopy->conlist; cp != NULL; cp=cp->next)
   {
   ScanRval(scopeid,&scalarvars,&listvars,cp->rval,cp->type,pp);
   }

PushThisScope();
ExpandPromiseAndDo(agent,scopeid,pcopy,scalarvars,listvars,fnptr);
PopThisScope();

DeletePromise(pcopy);
DeleteRlist(scalarvars);
DeleteRlist(listvars);
}

/*********************************************************************/

struct Rval ExpandDanglers(char *scopeid,struct Rval rval,struct Promise *pp)

{ struct Rval final;
  struct Rlist *rp;

 /* If there is still work left to do, expand and replace alloc */
 
switch (rval.rtype)
   {
   case CF_SCALAR:
       
       if (IsCf3VarString(rval.item))
          {
          final = EvaluateFinalRval(scopeid,rval.item,rval.rtype,false,pp);
          }
       else
          {
          final = rval;
          }
       break;

   default:
       final = rval;
       break;
   }

return final;
}

/*********************************************************************/

void ScanRval(char *scopeid,struct Rlist **scalarvars,struct Rlist **listvars,void *rval,char type,struct Promise *pp)

{ struct Rlist *rp;
  struct FnCall *fp;

if (rval == NULL)
   {
   return;
   }

switch(type)
   {
   case CF_SCALAR:

       ScanScalar(scopeid,scalarvars,listvars,(char *)rval,0,pp);
       break;
       
   case CF_LIST:
       for (rp = (struct Rlist *)rval; rp != NULL; rp=rp->next)
          {
          ScanRval(scopeid,scalarvars,listvars,rp->item,rp->type,pp);
          }
       break;
       
   case CF_FNCALL:
       fp = (struct FnCall *)rval;
       
       for (rp = (struct Rlist *)fp->args; rp != NULL; rp=rp->next)
          {
          Debug("Looking at arg for function-like object %s()\n",fp->name);
          ScanRval(scopeid,scalarvars,listvars,(char *)rp->item,rp->type,pp);
          }
       break;

   default:
       Debug("Unknown Rval type for scope %s",scopeid);
       break;
   }
}

/*********************************************************************/

void ScanScalar(char *scopeid,struct Rlist **scal,struct Rlist **its,char *string,int level,struct Promise *pp)

{ struct Rlist *rp;
  char *sp,rtype;
  void *rval;
  char v[CF_BUFSIZE],var[CF_EXPANDSIZE],exp[CF_EXPANDSIZE],temp[CF_BUFSIZE];
  
Debug("ScanScalar(\"%s\")\n",string);

if (string == NULL)
   {
   return;
   }

for (sp = string; (*sp != '\0') ; sp++)
   {
   v[0] = '\0';
   var[0] = '\0';
   exp[0] = '\0';

   if (*sp == '$')
      {
      if (ExtractInnerCf3VarString(sp,v))
         {
         char absscope[CF_MAXVARSIZE];

         if (strstr(v,"."))
            {
            strncpy(temp,var,CF_BUFSIZE-1);  
            absscope[0] = '\0';
            sscanf(temp,"%[^.].%s",absscope,v);
            }
         else
            {
            strncpy(absscope,scopeid,CF_MAXVARSIZE-1);  
            }

         ExpandPrivateScalar(absscope,v,var); 

         RegisterBundleDependence(absscope,pp);

         if (GetVariable(absscope,var,&rval,&rtype) != cf_notype)
            {
            if (rtype == CF_LIST)
               {
               Debug("Found list %s\n",var);
               ExpandScalar(var,exp); 

               /* embedded iterators should be incremented fastest,
                  so order list -- and MUST return de-scoped name
                  else list expansion cannot map var to this.name */
               
               if (level > 0)
                  {
                  IdempPrependRScalar(its,exp,CF_SCALAR);
                  }
               else
                  {
                  IdempAppendRScalar(its,exp,CF_SCALAR);
                  }
               }
            else if (rtype == CF_SCALAR)
               {
               Debug("Scalar variable $(%s) found\n",var);
               IdempAppendRScalar(scal,var,CF_SCALAR);
               }
            }
         else
            {
            Debug("Checking for nested vars, e.g. $(array[$(index)])....\n");
            
            if (IsExpandable(var))
               {
               Debug("Found embedded variables\n");
               ScanScalar(scopeid,scal,its,var,level+1,pp);
               }
            }
         sp += strlen(var)-1;
         }
      }
   }
}

/*********************************************************************/

int ExpandScalar(char *string,char buffer[CF_EXPANDSIZE])

{
Debug("ExpandScalar(context=%s,id=%s)\n",CONTEXTID,string);
return ExpandPrivateScalar(CONTEXTID,string,buffer); 
}

/*********************************************************************/

struct Rlist *ExpandList(char *scopeid,struct Rlist *list,int expandnaked)

{ struct Rlist *rp, *start = NULL;
  struct Rval returnval;
  char naked[CF_MAXVARSIZE];

for (rp = (struct Rlist *)list; rp != NULL; rp=rp->next)
   {
   if (!expandnaked && (rp->type == CF_SCALAR) && IsNakedVar(rp->item,'@'))
      {
      returnval.item = strdup(rp->item);
      returnval.rtype = CF_SCALAR;       
      }
   else if ((rp->type == CF_SCALAR) && IsNakedVar(rp->item,'@'))
      {
      GetNaked(naked,rp->item);

      if (GetVariable(scopeid,naked,&(returnval.item),&(returnval.rtype)) != cf_notype)
         {
         returnval = ExpandPrivateRval(scopeid,returnval.item,returnval.rtype);
         }
      else
         {
         returnval = ExpandPrivateRval(scopeid,rp->item,rp->type);
         }
      }
   else
      {
      returnval = ExpandPrivateRval(scopeid,rp->item,rp->type);
      }

   AppendRlist(&start,returnval.item,returnval.rtype);
   }

return start;
}

/*********************************************************************/

struct Rval ExpandPrivateRval(char *scopeid,void *rval,char type)

{ char buffer[CF_EXPANDSIZE];
 struct Rlist *rp, *start = NULL;
 struct FnCall *fp,*fpe;
 struct Rval returnval,extra;
     
Debug("ExpandPrivateRval(scope=%s,type=%c)\n",scopeid,type);

/* Allocates new memory for the copy */

returnval.item = NULL;
returnval.rtype = CF_NOPROMISEE;

switch (type)
   {
   case CF_SCALAR:

       ExpandPrivateScalar(scopeid,(char *)rval,buffer);
       returnval.item = strdup(buffer);
       returnval.rtype = CF_SCALAR;
       break;
       
   case CF_LIST:

       returnval.item = ExpandList(scopeid,rval,true);
       returnval.rtype = CF_LIST;
       break;
       
   case CF_FNCALL:
       
       /* Note expand function does not mean evaluate function, must preserve type */
       fp = (struct FnCall *)rval;
       fpe = ExpandFnCall(scopeid,fp,true);
       returnval.item = fpe;
       returnval.rtype = CF_FNCALL;
       break;       
   }

return returnval;
}

/*********************************************************************/

struct Rval ExpandBundleReference(char *scopeid,void *rval,char type)

{ char buffer[CF_EXPANDSIZE];
 struct Rlist *rp, *start = NULL;
 struct FnCall *fp,*fpe;
 struct Rval returnval,extra;
     
Debug("ExpandBundleReference(scope=%s,type=%c)\n",scopeid,type);

/* Allocates new memory for the copy */

returnval.item = NULL;
returnval.rtype = CF_NOPROMISEE;

switch (type)
   {
   case CF_SCALAR:

       ExpandPrivateScalar(scopeid,(char *)rval,buffer);
       returnval.item = strdup(buffer);
       returnval.rtype = CF_SCALAR;       
       break;
       
   case CF_FNCALL:
       
       /* Note expand function does not mean evaluate function, must preserve type */
       fp = (struct FnCall *)rval;
       fpe = ExpandFnCall(scopeid,fp,false);

       returnval.item = fpe;
       returnval.rtype = CF_FNCALL;
       break;       
   }

return returnval;
}

/*********************************************************************/

int ExpandPrivateScalar(char *scopeid,char *string,char buffer[CF_EXPANDSIZE]) 

{ char *sp,rtype;
  void *rval;
  int varstring = false;
  char currentitem[CF_EXPANDSIZE],temp[CF_BUFSIZE],name[CF_MAXVARSIZE];
  int len,increment, returnval = true;
  time_t tloc;
  
memset(buffer,0,CF_EXPANDSIZE);

if (string == 0 || strlen(string) == 0)
   {
   return false;
   }

Debug("\nExpandPrivateScalar(%s,%s)\n",scopeid,string);

for (sp = string; /* No exit */ ; sp++)       /* check for varitems */
   {
   char var[CF_BUFSIZE];
   
   memset(var,0,CF_BUFSIZE);
   increment = 0;

   if (*sp == '\0')
      {
      break;
      }

   memset(currentitem,0,CF_EXPANDSIZE);

   sscanf(sp,"%[^$]",currentitem);
   
   if (ExpandOverflow(buffer,currentitem))
      {
      FatalError("Can't expand varstring");
      }
   
   strcat(buffer,currentitem);
   sp += strlen(currentitem);

   Debug("  Aggregate result |%s|, scanning at \"%s\" (current delta %s)\n",buffer,sp,currentitem);
   
   if (*sp == '\0')
      {
      break;
      }

   if (*sp == '$')
      {
      switch (*(sp+1))
         {
         case '(':
             ExtractOuterCf3VarString(sp,var);
             varstring = ')';
             if (strlen(var) == 0)
                {
                strcat(buffer,"$");
                continue;
                }
             break;

         case '{':
             ExtractOuterCf3VarString(sp,var);
             varstring = '}';
             if (strlen(var) == 0)
                {
                strcat(buffer,"$");
                continue;
                }
             break;
         
         default: 
             strcat(buffer,"$");
             continue;
         }
      }

   memset(currentitem,0,CF_EXPANDSIZE);

   temp[0] = '\0';
   ExtractInnerCf3VarString(sp,temp);
   
   if (IsCf3VarString(temp))
      {
      Debug("  Nested variables - %s\n",temp);
      ExpandPrivateScalar(scopeid,temp,currentitem);
      }
   else
      {
      Debug("  Delta - %s\n",temp);
      strncpy(currentitem,temp,CF_BUFSIZE-1);
      }

   increment = strlen(var) - 1;

   switch (GetVariable(scopeid,currentitem,&rval,&rtype))
      {
      case cf_str:
      case cf_int:
      case cf_real:
          
          if (ExpandOverflow(buffer,(char*)rval))
             {
             FatalError("Can't expand varstring");
             }
          
          strcat(buffer,(char *)rval);
          break;

      case cf_slist:
      case cf_ilist:
      case cf_rlist:
      case cf_notype:
          Debug("  Currently non existent or list variable $(%s)\n",currentitem);
          
          if (varstring == '}')
             {
             snprintf(name,CF_MAXVARSIZE,"${%s}",currentitem);
             }
          else
             {
             snprintf(name,CF_MAXVARSIZE,"$(%s)",currentitem);
             }

          strcat(buffer,name);
          returnval = false;
          break;

      default:
          Debug("Returning Unknown Scalar (%s => %s)\n\n",string,buffer);
          return false;

      }

   sp += increment;
   currentitem[0] = '\0';
   }

if (returnval)
   {
   Debug("Returning complete scalar expansion (%s => %s)\n\n",string,buffer);

   /* Can we be sure this is complete? What about recursion */
   }
else
   {
   Debug("Returning partial / best effort scalar expansion (%s => %s)\n\n",string,buffer);
   }

return returnval;
}

/*********************************************************************/

void ExpandPromiseAndDo(enum cfagenttype agent,char *scopeid,struct Promise *pp,struct Rlist *scalarvars,struct Rlist *listvars,void (*fnptr)())

{ struct Rlist *lol = NULL; 
  struct Promise *pexp;
  struct Scope *ptr;
  int i = 1;
  char *handle = GetConstraint("handle",pp,CF_SCALAR);

lol = NewIterationContext(scopeid,listvars);

do
   {
   /* Set scope "this" first to ensure list expansion ! */
   SetScope("this");  
   DeRefListsInHashtable("this",listvars,lol);   

   /* Allow $(this.handle) */
   
   if (handle)
      {
      NewScalar("this","handle",handle,cf_str);
      }
   else
      {
      NewScalar("this","handle",PromiseID(pp),cf_str);
      }

   pexp = ExpandDeRefPromise("this",pp);
   
   switch (agent)
      {
      case cf_common:
          ShowPromise(pexp,6);
          ReCheckAllConstraints(pexp);
          break;
          
      default:

          if (fnptr != NULL)
             {
             (*fnptr)(pexp);
             }
          break;
      }
      
   if (strcmp(pp->agentsubtype,"vars") == 0)
      {
      ConvergeVarHashPromise(pp->bundle,pexp,true);
      }

   DeletePromise(pexp);

   if (lol && EndOfIteration(lol))
      {
      return;
      }

   /* End thread monitor */
   }
while (IncrementIterationContext(lol,1));

DeleteIterationContext(lol);
}

/*********************************************************************/

struct Rval EvaluateFinalRval(char *scopeid,void *rval,char rtype,int forcelist,struct Promise *pp)

{ struct Rlist *rp;
  struct Rval returnval,newret;
  char naked[CF_MAXVARSIZE];
  struct FnCall *fp;

Debug("EvaluateFinalRval -- type %c\n",rtype);
  
if ((rtype == CF_SCALAR) && IsNakedVar(rval,'@')) /* Treat lists specially here */
   {
   GetNaked(naked,rval);
   
   if (GetVariable(scopeid,naked,&(returnval.item),&(returnval.rtype)) == cf_notype || returnval.rtype != CF_LIST)
      {
      returnval = ExpandPrivateRval("this",rval,rtype);
      }
   else
      {
      returnval.item = ExpandList(scopeid,returnval.item,true);
      returnval.rtype = CF_LIST;
      }
   }
else
   {
   if (forcelist) /* We are replacing scalar @(name) with list */
      {
      returnval = ExpandPrivateRval(scopeid,rval,rtype);
      }
   else
      {
      if (IsBuiltinFnCall(rval,rtype))
         {
         returnval.item = CopyRvalItem(rval,rtype);
         returnval.rtype = rtype;
         }
      else
         {
         returnval = ExpandPrivateRval("this",rval,rtype);
         }
      }
   }

switch (returnval.rtype) 
   {
   case CF_SCALAR:
       break;
       
   case CF_LIST:
       for (rp = (struct Rlist *)returnval.item; rp != NULL; rp=rp->next)
          {
          if (rp->type == CF_FNCALL)
             {
             fp = (struct FnCall *)rp->item;
             newret = EvaluateFunctionCall(fp,pp);
             DeleteFnCall(fp);
             rp->item = newret.item;
             rp->type = newret.rtype;
             Debug("Replacing function call with new type (%c)\n",rp->type);
             }
          else
             {
             struct Scope *ptr = GetScope("this");

             if (ptr != NULL)
                {
                if (IsCf3VarString(rp->item))
                   {
                   newret = ExpandPrivateRval("this",rp->item,rp->type);
                   free(rp->item);
                   rp->item = newret.item;
                   }
                }
             }

          /* returnval unchanged */
          }
       break;
       
   case CF_FNCALL:

       // Also have to eval function now
       fp = (struct FnCall *)returnval.item;
       returnval = EvaluateFunctionCall(fp,pp);
       DeleteFnCall(fp);
       break;

   default:
       returnval.item = NULL;
       returnval.rtype = CF_NOPROMISEE;
       break;
   }

return returnval;
}

/*********************************************************************/
/* Tools                                                             */
/*********************************************************************/

int IsExpandable(char *str)

{ char *sp;
  char left = 'x', right = 'x';
  int dollar = false;
  int bracks = 0, vars = 0;

Debug1("IsExpandable(%s) - syntax verify\n",str);

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
   Debug("If this is an expandable variable string then it contained syntax errors");
   return false;
   }

Debug("Found %d variables in (%s)\n",vars,str); 
return vars;
}

/*********************************************************************/

int IsNakedVar(char *str, char vtype)

{ char *sp,last;
  int count=0;

if (str == NULL || strlen(str) == 0)
   {
   return false;
   }

last = *(str+strlen(str)-1);

if (strlen(str) < 3)
   {
   return false;
   }

if (*str != vtype)
   {
   return false;
   }

switch (*(str+1))
   {
   case '(':
       if (last != ')')
          {
          return false;
          }
       break;
       
   case '{':
       if (last != '}')
          {
          return false;
          }
       break;

   default:
       return false;
       break;
   }

for (sp = str; *sp != '\0'; sp++)
   {
   switch (*sp)
      {
      case '(':
      case '{':
      case '[':
          count++;
          break;
      case ')':
      case '}':
      case ']':
          count--;

          /* The last character must be the end of the variable */

          if (count == 0 && strlen(sp) > 1)
             {
             return false;
             }
          break;
      }
   }

if (count != 0)
   {
   return false;
   }

Debug1("IsNakedVar(%s,%c)!!\n",str,vtype);
return true;
}

/*********************************************************************/

void GetNaked(char *s2, char *s1)

/* copy @(listname) -> listname */
    
{
if (strlen(s1) < 4)
   {
   CfOut(cf_error,"","Naked variable expected, but \"%s\" is malformed",s1);
   strncpy(s2,s1,CF_MAXVARSIZE-1);
   return;
   }

memset(s2,0,CF_MAXVARSIZE);
strncpy(s2,s1+2,strlen(s1)-3);
}

/*********************************************************************/
/* General                                                           */
/*********************************************************************/

void ConvergeVarHashPromise(char *scope,struct Promise *pp,int allow_redefine)

{ struct Constraint *cp,*cp_save = NULL;
  struct Attributes a;
  char *lval,rtype;
  void *rval = NULL,*retval;
  int i = 0,ok_redefine = false,drop_undefined = false;;
  struct Rval returnval; /* Must expand naked functions here for consistency */
  struct Rlist *rp,*last = NULL;
  
if (pp->done)
   {
   return;
   }

if (IsExcluded(pp->classes))
   {
   return;
   }

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,"comment") == 0)
      {
      continue;
      }

   if (cp->rval == NULL)
      {
      continue;
      }

   if (strcmp(cp->lval,"ifvarclass") == 0)
      {
      if (IsExcluded(cp->rval))
         {
         return;
         }
      
      continue;
      }

   if (strcmp(cp->lval,"policy") == 0)
      {
      if (strcmp(cp->rval,"ifdefined") == 0)
         {
         drop_undefined = true;
         ok_redefine = false;
         }
      else if (strcmp(cp->rval,"constant") == 0)
         {
         ok_redefine = false;
         }
      else
         {
         ok_redefine = true;
         }
      }
   else if (strcmp(cp->lval,"string") == 0 || strcmp(cp->lval,"slist") == 0 ||
            strcmp(cp->lval,"int") == 0 || strcmp(cp->lval,"ilist") == 0 ||
            strcmp(cp->lval,"real") == 0 || strcmp(cp->lval,"rlist") == 0)
      {
      i++;
      rval = cp->rval;
      cp_save = cp;
      }
   }

cp = cp_save;

if (cp == NULL)
   {
   CfOut(cf_inform,"","Warning: Variable body for \"%s\" seems incomplete",pp->promiser);
   PromiseRef(cf_inform,pp);
   return;
   }

if (i > 2)
   {
   CfOut(cf_error,"","Variable \"%s\" breaks its own promise with multiple values (code %d)",pp->promiser,i);
   PromiseRef(cf_error,pp);
   return;
   }

// More consideration needs to be given to using these
//a.transaction = GetTransactionConstraints(pp);
//a.classes = GetClassDefinitionConstraints(pp);

if (rval != NULL)
   {
   struct FnCall *fp = (struct FnCall *)rval;
   
   if (cp->type == CF_FNCALL)
      {
      returnval = EvaluateFunctionCall(fp,pp);
      cp->rval = rval = returnval.item;
      cp->type = returnval.rtype;
      DeleteFnCall(fp);
      
      if (FNCALL_STATUS.status == FNCALL_FAILURE)
         {
         /* We do not assign variables to failed fn calls */
         return;
         }
      }

   if (Epimenides(pp->promiser,rval,cp->type,0))
      {
      CfOut(cf_error,"","Variable \"%s\" contains itself indirectly - an unkeepable promise",pp->promiser);
      exit(1);
      }
   else
      {
      /* See if the variable needs recursively expanding again */
      returnval = EvaluateFinalRval(scope,rval,cp->type,true,pp);
      DeleteRvalItem(cp->rval,cp->type);
      cp->rval = rval = returnval.item;
      cp->type = returnval.rtype;
      }

   if (GetVariable(scope,pp->promiser,(void *)&retval,&rtype) != cf_notype)
      {
      if (ok_redefine) /* only on second iteration, else we ignore broken promises */
         {
         DeleteVariable(scope,pp->promiser);
         }
      else if ((THIS_AGENT_TYPE == cf_common) && (CompareRval(retval,rtype,rval,cp->type) == false))
         {
         CfOut(cf_error,""," !! Redefinition of a constant variable \"%s\"",pp->promiser);
         PromiseRef(cf_error,pp);
         }
      }

   if (IsCf3VarString(pp->promiser))
      {
      // Unexpanded variables, we don't do anything with
      return;
      }
   
   if (!FullTextMatch("[a-zA-Z0-9_\200-\377.]+(\\[.+\\])*",pp->promiser))
      {
      CfOut(cf_error,""," !! Variable identifier contains illegal characters");
      PromiseRef(cf_error,pp);
      return;
      }

   if (drop_undefined && cp->type == CF_LIST)
      {
      for (rp = rval; rp != NULL; rp=rp->next)
         {
         if (IsNakedVar(rp->item,'@'))
            {
            if (rp == rval)
               {
               DeleteRvalItem(rp->item,rp->type);
               rval = rp->next;
               }
            else if (last)
               {
               last->next = rp->next;
               DeleteRvalItem(rp->item,rp->type);
               }
            }

         last = rp;
         }
      }

   if (!AddVariableHash(scope,pp->promiser,rval,cp->type,Typename2Datatype(cp->lval),cp->audit->filename,cp->lineno))
      {
      CfOut(cf_verbose,"","Unable to converge %s.%s value (possibly empty or infinite regression)\n",scope,pp->promiser);
      PromiseRef(cf_verbose,pp);
      }
   }
else
   {
   CfOut(cf_error,"","Variable %s has no promised value\n",pp->promiser);
   CfOut(cf_error,"","Rule from %s at/before line %d\n",cp->audit->filename,cp->lineno);
   }     

}

/*********************************************************************/

void ConvergePromiseValues(struct Promise *pp)

{ struct Constraint *cp;
  struct Rlist *rp;
  char expandbuf[CF_EXPANDSIZE];

switch (pp->petype)
   {
   case CF_SCALAR:
       
       if (IsCf3VarString((char *)pp->promisee))
          {             
          ExpandScalar(pp->promisee,expandbuf);
          if (strcmp(pp->promisee,expandbuf) != 0)
             {
             free(pp->promisee);
             pp->promisee = strdup(expandbuf);
             }
          }
       break;
       
   case CF_LIST:
       
       for (rp = (struct Rlist *)pp->promisee; rp != NULL; rp=rp->next)
          {
          if (IsCf3VarString((char *)rp->item))
             {             
             ExpandScalar(rp->item,expandbuf);
             if (strcmp(rp->item,expandbuf) != 0)
                {
                free(rp->item);
                rp->item = strdup(expandbuf);
                }
             }          
          }
       break;   
   }
 
for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   switch (cp->type)
      {
      case CF_SCALAR:

          if (IsCf3VarString((char *)cp->rval))
             {             
             ExpandScalar(cp->rval,expandbuf);
             if (strcmp(cp->rval,expandbuf) != 0)
                {
                free(cp->rval);
                cp->rval = strdup(expandbuf);
                }
             }
          break;

      case CF_LIST:

          for (rp = (struct Rlist *)cp->rval; rp != NULL; rp=rp->next)
             {
             if (IsCf3VarString((char *)rp->item))
                {             
                ExpandScalar(rp->item,expandbuf);
                if (strcmp(rp->item,expandbuf) != 0)
                   {
                   free(rp->item);
                   rp->item = strdup(expandbuf);
                   }
                }          
             }
          break;
      }
   }
}
      
/*********************************************************************/
/* Levels                                                            */
/*********************************************************************/

int Epimenides(char *var,char *rval,char rtype,int level)

{ struct Rlist *rp,*list;
  char exp[CF_EXPANDSIZE];

switch (rtype)
   {
   case CF_SCALAR:
       
       if (StringContainsVar(rval,var))
          {
          CfOut(cf_error,"","Scalar variable \"%s\" contains itself (non-convergent): %s",var,(char *)rval);
          return true;
          }

       if (IsCf3VarString(rval))
          {
          ExpandPrivateScalar(CONTEXTID,rval,exp);
          Debug("bling %d-%s: (look for %s) in \"%s\" => %s \n",level,CONTEXTID,var,rval,exp);

          if (level > 3)
             {
             return false;
             }
          
          if (Epimenides(var,exp,CF_SCALAR,level+1))
             {
             return true;
             }           
          }
       
       break;
       
   case CF_LIST:
       list = (struct Rlist *)rval;
       
       for (rp = list; rp != NULL; rp=rp->next)
          {
          if (Epimenides(var,rp->item,rp->type,level))
             {
             return true;
             }
          }
       break;
       
   default:
       return false;
   }

return false;
}
