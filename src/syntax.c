/* 

        Copyright (C) 1994-
        Free Software Foundation, Inc.

   This file is part of GNU cfengine - written and maintained 
   by Mark Burgess, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
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
/* File: syntax.c                                                            */
/*                                                                           */
/* Created: Wed Aug  8 14:01:42 2007                                         */
/*                                                                           */
/*****************************************************************************/
 
#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************/

void CheckBundle(char *name,char *type)

{ struct Bundle *bp;

Debug("Checking for bundle (%s,%s)\n",name,type);

for (bp = BUNDLES; bp != NULL; bp=bp->next)
   {
   if ((strcmp(name,bp->name) == 0) && (strcmp(type,bp->type) == 0))
      {
      snprintf(OUTPUT,CF_BUFSIZE,"Redefinition of bundle %s for %s is a broken promise",name,type);
      yyerror(OUTPUT);
      }
   }
}

/*********************************************************/

void CheckBody(char *name,char *type)

{ struct Body *bp;

for (bp = BODIES; bp != NULL; bp=bp->next)
   {
   if ((strcmp(name,bp->name) == 0) && (strcmp(type,bp->type) == 0))
      {
      snprintf(OUTPUT,CF_BUFSIZE,"Redefinition of body %s for %s is a broken promise",name,type);
      yyerror(OUTPUT);
      }
   }
}

/*********************************************************/

struct SubTypeSyntax CheckSubType(char *bundletype,char *subtype)

{ int i,j;
  struct SubTypeSyntax *ss;

for  (i = 0; i < CF3_MODULES; i++)
   {
   if ((ss = CF_ALL_SUBTYPES[i]) == NULL)
      {
      continue;
      }
   
   for (j = 0; ss[j].btype != NULL; j++)
      {
      if (strcmp(subtype,ss[j].subtype) == 0)
         {
         if ((strcmp(bundletype,ss[j].btype) == 0) || (strcmp("*",ss[j].btype) == 0))
            {
            /* Return a pointer to bodies for this subtype */
            Debug("Subtype %s syntax ok for %s\n",subtype,bundletype);
            return ss[j];
            }
         }
      }
   }

snprintf(OUTPUT,CF_BUFSIZE,"%s is not a valid type category for %s bundles",subtype,bundletype);
yyerror(OUTPUT);
snprintf(OUTPUT,CF_BUFSIZE,"Possibly the bundle type %s itself is undefined",subtype,bundletype);
yyerror(OUTPUT);
return CF_NOSTYPE;
}

/*********************************************************/

void CheckConstraint(char *type,char *name,char *lval,void *rval,char rvaltype,struct SubTypeSyntax ss)

{ int lmatch = false;
  int i,j,k,l, allowed = false;
  struct BodySyntax *bs;

Debug("CheckConstraint(%s,%s,",type,lval);

if (DEBUG)
   {
   ShowRval(FOUT,rval,rvaltype);
   }

Debug(")\n");

if (ss.subtype != NULL) /* In a bundle */
   {
   if (strcmp(ss.subtype,type) == 0)
      {
      Debug("Found type %s's body syntax\n",type);
      
      bs = ss.bs;
      
      for (l = 0; bs[l].lval != NULL; l++)
         {
         Debug1("CMP-bundle # (%s,%s)\n",lval,bs[l].lval);
         
         if (strcmp(lval,bs[l].lval) == 0)
            {
            /* If we get here we have found the lval and it is valid
               for this subtype */
            
            lmatch = true;
            Debug("Matched syntatically correct bundle (lval,rval) item = (%s) to its rval\n",lval);
            
            if (bs[l].dtype == cf_body)
               {
               Debug("Constraint syntax ok, but definition of body is elsewhere %s=%c\n",lval,rvaltype);
               PrependRlist(&BODYPARTS,rval,rvaltype);
               return;
               }
            else
               {
               CheckConstraintTypeMatch(lval,rval,rvaltype,bs[l].dtype,(char *)(bs[l].range));
               return;
               }
            }
         }
      }
   }

/* Now check the functional modules - extra level of indirection */

for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
   {
   Debug1("CMP-common # %s,%s\n",lval,CF_COMMON_BODIES[i].lval);
   
   if (strcmp(lval,CF_COMMON_BODIES[i].lval) == 0)
      {
      Debug("Found a match for lval %s in the common constraint attributes\n",lval);
      return;
      }
   }


// Now check if it is in the common list...

if (!lmatch || ! allowed)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Constraint lvalue %s is not allowed in bundle category \'%s\'",lval,type);
   yyerror(OUTPUT);
   }
}

/******************************************************************************************/

void CheckSelection(char *type,char *name,char *lval,void *rval,char rvaltype)

{ int lmatch = false;
  int i,j,k,l, allowed = false;
  struct SubTypeSyntax *ss;
  struct BodySyntax *bs,*bs2;
  enum cfdatatype ltype;
  
Debug("CheckSelection(%s,%s,",type,lval);

if (DEBUG)
   {
   ShowRval(FOUT,rval,rvaltype);
   }

Debug(")\n");

/* Check internal control bodies etc */

for (i = 0; CF_ALL_BODIES[i].subtype != NULL; i++)
   {
   if (strcmp(CF_ALL_BODIES[i].subtype,name) == 0 && strcmp(type,CF_ALL_BODIES[i].btype) == 0)
      {
      Debug("Found matching a body matching (%s,%s)\n",type,name);
      
      bs = CF_ALL_BODIES[i].bs;
      
      for (l = 0; bs[l].lval != NULL; l++)
         {
         if (strcmp(lval,bs[l].lval) == 0)
            {
            Debug("Matched syntatically correct body (lval,rval) item = (%s,%s)\n",lval,rval);
            
            if (bs[l].dtype == cf_body)
               {
               Debug("Constraint syntax ok, but definition of body is elsewhere\n");
               return;
               }
            else
               {
               CheckConstraintTypeMatch(lval,rval,rvaltype,bs[l].dtype,(char *)(bs[l].range));
               return;
               }
            }
         }
      
      }
   }


/* Now check the functional modules - extra level of indirection */

for  (i = 0; i < CF3_MODULES; i++)
   {
   Debug("Trying function module %d for matching lval %s\n",i,lval);
   
   if ((ss = CF_ALL_SUBTYPES[i]) == NULL)
      {
      continue;
      }
  
   for (j = 0; ss[j].subtype != NULL; j++)
      {
      if ((bs = ss[j].bs) == NULL)
         {
         continue;
         }
      
      Debug("\nExamining subtype %s\n",ss[j].subtype);

      for (l = 0; bs[l].range != NULL; l++)
         {
         if (bs[l].dtype == cf_body)
            {
            bs2 = (struct BodySyntax *)(bs[l].range);

            if (bs2 == NULL || bs2 == (void *)CF_BUNDLE)
               {
               continue;
               }

            for (k = 0; bs2[k].dtype != cf_notype; k++)
               {
               /* Either module defined or common */

               Debug1("CMP-module-subtypes: %s,%s\n",ss[j].subtype,type);
               
               if (strcmp(ss[j].subtype,type) == 0 && strcmp(ss[j].subtype,"*") != 0)
                  {
                  snprintf(OUTPUT,CF_BUFSIZE,"lval %s belongs to promise type \'%s:\' but this is '\%s\'\n",lval,ss[j].subtype,type);
                  yyerror(OUTPUT);
                  return;
                  }
               
               if (strcmp(lval,bs2[k].lval) == 0)
                  {
                  Debug("Matched\n");
                  CheckConstraintTypeMatch(lval,rval,rvaltype,bs2[k].dtype,(char *)(bs2[k].range));
                  return;
                  }
               }
            }
         }
      }
   }

if (!lmatch)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Constraint lvalue %s is not allowed in \'%s\' constraint body",lval,type);
   yyerror(OUTPUT);
   }
}

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

void CheckConstraintTypeMatch(char *lval,void *rval,char rvaltype,enum cfdatatype dt,char *range)

{ struct Rlist *rp;

Debug("Checking inline constraint %s[%s] => mappedval\n",lval,CF_DATATYPES[dt]);

/* Get type of lval */

switch(rvaltype)
   {
   case CF_LIST:

       switch(dt)
          {
          case cf_slist:
          case cf_ilist:
          case cf_rlist:
          case cf_clist:
          case cf_olist:
              break;
          default:
              snprintf(OUTPUT,CF_BUFSIZE,"rhs is a list, but lhs (%s) is not a list type",CF_DATATYPES[dt]);
              yyerror(OUTPUT);
              break;
          }
       
       for (rp = (struct Rlist *)rval; rp != NULL; rp = rp->next)
          {
          CheckConstraintTypeMatch(lval,rp->item,rp->type,dt,range);
          }       
       return;
       
   case CF_FNCALL:

       /* Fn-like objects are assumed to be parameterized bundles in the bseq */
       
       if (strcmp(lval,"bundlesequence") != 0)
          {
          CheckFnCallType(lval,((struct FnCall *)rval)->name,dt,range);
          }
       return;
   }

/* If we get here, we have a literal scalar type */

switch (dt)
   {
   case cf_str:
   case cf_slist:
       CheckParseString(lval,(char *)rval,range);
       break;
   case cf_int:
   case cf_ilist:
       CheckParseInt(lval,(char *)rval,range);
       break;
       
   case cf_real:
   case cf_rlist:
       CheckParseReal(lval,(char *)rval,range);
       break;

   case cf_body:
       Debug("Nothing to check for body reference\n");
       break;
       
   case cf_opts:
   case cf_olist:
       CheckParseOpts(lval,(char *)rval,range);
       break;

   case cf_class:
   case cf_clist:
       CheckParseClass(lval,(char *)rval,range);
       break;
   }
}
            
/****************************************************************************/

enum cfdatatype StringDataType(char *scopeid,char *string)

{ struct Rlist *rp;
  enum cfdatatype dtype;
  char rtype;
  void *rval;
  int islist = false;
  char var[CF_BUFSIZE],exp[CF_EXPANDSIZE];
  
Debug("StringDataType(%s)\n",string);

/*-------------------------------------------------------
What happens if we embed vars in a literal string
       "$(list)withending" - a list?
       "$(list1)$(list2)"  - not a simple list
Disallow these manual concatenations as ambiguous.
Demand this syntax to work around

vars:

   "listvar" slist => EmbellishList("prefix$(list)suffix");
---------------------------------------------------------*/

var[0] = '\0';
exp[0] = '\0';

if (*string == '$')
   {
   if (ExtractInnerCf3VarString(string,var))
      {
      if ((dtype = GetVariable(scopeid,var,&rval,&rtype)) != cf_notype)
         {
         if (rtype == CF_LIST)
            {
            if (!islist)
               {
               islist = true;
               }
            else
               {
               islist = false;
               }
            }
         }

      if (strlen(var) == strlen(string))
         {
         /* Can trust variables own datatype  */
         return dtype;
         }
      else
         {
         /* Must force non-pure substitution to be generic type CF_SCALAR.cf_str */
         return cf_str;
         }
      }
   }

return cf_str;
}

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

void CheckParseString(char *lval,char *s,char *range)

{ regex_t rx;
  regmatch_t pmatch;

 Debug("\nCheckParseString(%s => %s/%s)\n",lval,s,range);
  
if (strlen(range) == 0)
   {
   return;
   }

if (IsNakedVar(s,'@')||IsNakedVar(s,'$'))
   {
   Verbose("Unable to verify variable expansion at this stage\n");
   return;
   }

if (CfRegcomp(&rx,range,REG_EXTENDED) != 0)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"regular expression for string rvalues is not ok for lval %s",lval);
   FatalError(OUTPUT);
   }

if (regexec(&rx,s,1,&pmatch,0) == 0)
   {
   if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(s)))
      {
      Debug("CheckParseString - syntax verified\n\n");
      regfree(&rx);
      return;
      }
   }

if (IsCf3VarString(s))
   {
   Verbose("Unable to verify syntax due to variable expansion at this stage\n");   
   }
else
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Scalar item in %s => { %s } in rvalue is out of bounds (value should match pattern %s)",lval,s,range);
   yyerror(OUTPUT);
   }
/*regfree(&rx); */
}

/****************************************************************************/

void CheckParseClass(char *lval,char *s,char *range)

{ regex_t rx;
  regmatch_t pmatch;

Debug("\nCheckParseString(%s => %s/%s)\n",lval,s,range);
  
if (strlen(range) == 0)
   {
   return;
   }

if (CfRegcomp(&rx,range,REG_EXTENDED) != 0)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"regular expression for class rvalues is not ok for lval %s",lval);
   FatalError(OUTPUT);
   }

if (regexec(&rx,s,1,&pmatch,0) == 0)
   {
   if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(s)))
      {
      Debug("CheckParseClass - syntax verified\n\n");
      regfree(&rx);
      return;
      }
   }

snprintf(OUTPUT,CF_BUFSIZE,"Class item in %s => { %s } in rvalue is out of bounds (should match %s)",lval,s,range);
yyerror(OUTPUT);
/*regfree(&rx); */
}

/****************************************************************************/

void CheckParseInt(char *lval,char *s,char *range)
    
{ struct Item *split,*ip;
  int n,max = CF_LOWINIT, min = CF_HIGHINIT, val;
 
/* Numeric types are registered by range separated by comma str "min,max" */
Debug("\nCheckParseInt(%s => %s/%s)\n",lval,s,range);

split = SplitString(range,',');

if ((n = ListLen(split)) != 2)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"INTERN:format specifier for int rvalues is not ok for lval %s - got %d items",lval,n);
   FatalError(OUTPUT);
   }

sscanf(split->name,"%d",&min);

if (strcmp(split->next->name,"inf") == 0)
   {
   max = CF_INFINITY;
   }
else
   {
   sscanf(split->next->name,"%d",&max);
   }

DeleteItemList(split);

if (min == CF_HIGHINIT || max == CF_LOWINIT)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"INTERN: could not parse format specifier for int rvalues for lval %s",lval);
   FatalError(OUTPUT);
   }

if (strcmp(s,"inf") == 0)
   {
   val = CF_INFINITY;
   }
else
   {    
   val = atoi(s);
   }

if (val > max || val < min)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Int item in %s => { %s or %d} in rvalue is out of bounds (should be in [%s])",lval,s,val,range);
   yyerror(OUTPUT);
   return;
   }

Debug("CheckParseInt - syntax verified\n\n");
}

/****************************************************************************/

void CheckParseReal(char *lval,char *s,char *range)
    
{ struct Item *split,*ip;
  double max = (double)CF_LOWINIT, min = (double)CF_HIGHINIT, val;
  int n;

Debug("\nCheckParseReal(%s => %s/%s)\n",lval,s,range);

if (strcmp(s,"inf") == 0)
   {
   yyerror("keyword \"inf\" has an integer value, cannot be used as real");
   return;
   }

/* Numeric types are registered by range separated by comma str "min,max" */

split = SplitString(range,',');

if ((n = ListLen(split)) != 2)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"INTERN:format specifier for real rvalues is not ok for lval %s - %d items",lval,n);
   FatalError(OUTPUT);
   }

sscanf(split->name,"%lf",&min);
sscanf(split->next->name,"%lf",&max);
DeleteItemList(split);

if (min == CF_HIGHINIT || max == CF_LOWINIT)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"INTERN:could not parse format specifier for int rvalues for lval %s",lval);
   FatalError(OUTPUT);
   }

val = atof(s);

if (val > max || val < min)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Real item in %s => { %s or %.3lf} in rvalue is out of bounds (should be in [%s])",lval,s,val,range);
   yyerror(OUTPUT);
   }

Debug("CheckParseReal - syntax verified\n\n");
}

/****************************************************************************/

void CheckParseOpts(char *lval,char *s,char *range)

{ struct Item *split,*ip;
  int err = false;
 
/* List/menu types are separated by comma str "a,b,c,..." */

Debug("\nCheckParseopts(%s => %s/%s)\n",lval,s,range);

split = SplitString(range,',');

if (!IsItemIn(split,s))
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Selection on rhs of %s => { %s } is out of bounds",lval,s);
   yyerror(OUTPUT);
   snprintf(OUTPUT,CF_BUFSIZE,"Should be in [%s]\n",range);
   yyerror(OUTPUT);
   err = true;
   }

DeleteItemList(split);

if (!err)
   {
   Debug("CheckParseInt - syntax verified\n\n");
   }
}

/****************************************************************************/

void CheckFnCallType(char *lval,char *s,enum cfdatatype dtype,char *range)

{ int i;
  enum cfdatatype dt;
     
Debug("CheckFnCallType(%s => %s/%s)\n",lval,s,range);

for (i = 0; CF_FNCALL_TYPES[i].name != NULL; i++)
   {
   if (strcmp(CF_FNCALL_TYPES[i].name,s) == 0)
      {
      dt = CF_FNCALL_TYPES[i].dtype;

      if (dtype != dt)
         {
         /* Ok to allow fn calls of correct element-type in lists */
         
         if (dt == cf_str && dtype == cf_slist)
            {
            return;
            }

         if (dt == cf_int && dtype == cf_ilist)
            {
            return;
            }

         if (dt == cf_real && dtype == cf_rlist)
            {
            return;
            }
         
         if (dt == cf_opts && dtype == cf_olist)
            {
            return;
            }
         
         if (dt == cf_class && dtype == cf_clist)
            {
            return;
            }

         snprintf(OUTPUT,CF_BUFSIZE,"function %s() returns type %s but lhs requires %s",s,CF_DATATYPES[dt],CF_DATATYPES[dtype]);
         yyerror(OUTPUT);
         return;
         }
      else
         {
         return;
         }
      }
   }

snprintf(OUTPUT,CF_BUFSIZE,"Unknown built-in function %s()",s);
yyerror(OUTPUT);
}


