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
  char output[CF_BUFSIZE];

Debug("Checking for bundle (%s,%s)\n",name,type);

for (bp = BUNDLES; bp != NULL; bp=bp->next)
   {
   if ((strcmp(name,bp->name) == 0) && (strcmp(type,bp->type) == 0))
      {
      snprintf(output,CF_BUFSIZE,"Redefinition of bundle %s for %s is a broken promise",name,type);
      ReportError(output);
      }
   }
}

/*********************************************************/

void CheckBody(char *name,char *type)

{ struct Body *bp;
  char output[CF_BUFSIZE];

for (bp = BODIES; bp != NULL; bp=bp->next)
   {
   if ((strcmp(name,bp->name) == 0) && (strcmp(type,bp->type) == 0))
      {
      snprintf(output,CF_BUFSIZE,"Redefinition of body %s for %s is a broken promise",name,type);
      ReportError(output);
      }
   }
}

/*********************************************************/

struct SubTypeSyntax CheckSubType(char *bundletype,char *subtype)

{ int i,j;
  struct SubTypeSyntax *ss;
  char output[CF_BUFSIZE];
  
if (subtype == NULL)
   {
   snprintf(output,CF_BUFSIZE,"Missing promise type category for %s bundle",subtype,bundletype);
   ReportError(output);
   return CF_NOSTYPE;
   }

for  (i = 0; i < CF3_MODULES; i++)
   {
   if ((ss = CF_ALL_SUBTYPES[i]) == NULL)
      {
      continue;
      }
   
   for (j = 0; ss[j].btype != NULL; j++)
      {
      if (subtype && strcmp(subtype,ss[j].subtype) == 0)
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

snprintf(output,CF_BUFSIZE,"%s is not a valid type category for %s bundle",subtype,bundletype);
ReportError(output);
snprintf(output,CF_BUFSIZE,"Possibly the bundle type \"%s\" itself is undefined",bundletype);
ReportError(output);
return CF_NOSTYPE;
}

/*********************************************************/

enum cfdatatype ExpectedDataType(char *lvalname)

{ int i,j,k;
  struct BodySyntax *bs;
  struct SubTypeSyntax *ss;

for (i = 0; i < CF3_MODULES; i++)
   {
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
      
      for (k = 0; bs[k].range != NULL; k++)
         {
         if (strcmp(lvalname,bs[k].lval) == 0)
            {
            return bs[k].dtype;
            }
         }
      }
   }

return cf_notype;
}

/*********************************************************/

void CheckConstraint(char *type,char *name,char *lval,void *rval,char rvaltype,struct SubTypeSyntax ss)

{ int lmatch = false;
  int i,j,k,l, allowed = false;
  struct BodySyntax *bs;
  char output[CF_BUFSIZE];
  
Debug("CheckConstraint(%s,%s,",type,lval);

if (DEBUG)
   {
   ShowRval(stdout,rval,rvaltype);
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
            else if (bs[l].dtype == cf_bundle)
               {
               Debug("Constraint syntax ok, but definition of relevant bundle is elsewhere %s=%c\n",lval,rvaltype);
               PrependRlist(&SUBBUNDLES,rval,rvaltype);
               return;
               }
            else
               {
               CheckConstraintTypeMatch(lval,rval,rvaltype,bs[l].dtype,(char *)(bs[l].range),0);
               return;
               }
            }
         }
      }
   }

/* Now check the functional modules - extra level of indirection
   Note that we only check body attributes relative to promise type.
   We can enter any promise types in any bundle, but only recognized
   types will be dealt with. */

for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
   {
   Debug1("CMP-common # %s,%s\n",lval,CF_COMMON_BODIES[i].lval);
   
   if (strcmp(lval,CF_COMMON_BODIES[i].lval) == 0)
      {
      Debug("Found a match for lval %s in the common constraint attributes\n",lval);
      return;
      }
   }


for (i = 0; CF_COMMON_EDITBODIES[i].lval != NULL; i++)
   {
   Debug1("CMP-common # %s,%s\n",lval,CF_COMMON_EDITBODIES[i].lval);
   
   if (strcmp(lval,CF_COMMON_EDITBODIES[i].lval) == 0)
      {
      Debug("Found a match for lval %s in the common edit constraint attributes\n",lval);
      return;
      }
   }


// Now check if it is in the common list...

if (!lmatch || !allowed)
   {
   snprintf(output,CF_BUFSIZE,"Constraint lvalue %s is not allowed in bundle category \'%s\'",lval,type);
   ReportError(output);
   }
}

/******************************************************************************************/

void CheckSelection(char *type,char *name,char *lval,void *rval,char rvaltype)

{ int lmatch = false;
  int i,j,k,l, allowed = false;
  struct SubTypeSyntax *ss;
  struct BodySyntax *bs,*bs2;
  enum cfdatatype ltype;
  char output[CF_BUFSIZE];
  
Debug("CheckSelection(%s,%s,",type,lval);

if (DEBUG)
   {
   ShowRval(stdout,rval,rvaltype);
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
            else if (bs[l].dtype == cf_bundle)
               {
               Debug("Constraint syntax ok, but definition of bundle is elsewhere\n");
               return;
               }
            else
               {
               CheckConstraintTypeMatch(lval,rval,rvaltype,bs[l].dtype,(char *)(bs[l].range),0);
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

               Debug1("CMP-module-subtypes (%s): %s,%s\n",ss[j].btype,ss[j].subtype,type);
               
               if (strcmp(ss[j].subtype,type) == 0 && strcmp(ss[j].subtype,"*") != 0)
                  {
                  snprintf(output,CF_BUFSIZE,"lval %s belongs to promise type \'%s:\' but this is '\%s\'\n",lval,ss[j].subtype,type);
                  ReportError(output);
                  return;
                  }
               
               if (strcmp(lval,bs2[k].lval) == 0)
                  {
                  Debug("Matched\n");
                  CheckConstraintTypeMatch(lval,rval,rvaltype,bs2[k].dtype,(char *)(bs2[k].range),0);
                  return;
                  }
               }
            }
         }
      }
   }

if (!lmatch)
   {
   snprintf(output,CF_BUFSIZE,"Constraint lvalue %s is not allowed in \'%s\' constraint body",lval,type);
   ReportError(output);
   }
}

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

void CheckConstraintTypeMatch(char *lval,void *rval,char rvaltype,enum cfdatatype dt,char *range,int level)

{ struct Rlist *rp;
  struct Item *checklist;
  char output[CF_BUFSIZE];

if (rval == NULL)
   {
   return;
   }
  
Debug(" ------------------------------------------------\n");

if (dt == cf_bundle || dt == cf_body)
   {
   Debug(" - Checking inline constraint/arg %s[%s] => mappedval (bundle/body)\n",lval,CF_DATATYPES[dt]);
   }
else
   {
   Debug(" - Checking inline constraint/arg %s[%s] => mappedval (%c) %s\n",lval,CF_DATATYPES[dt],rvaltype,range);
   }
Debug(" ------------------------------------------------\n");

/* Get type of lval */

switch(rvaltype)
   {
   case CF_SCALAR:
       switch(dt)
          {
          case cf_slist:
          case cf_ilist:
          case cf_rlist:
          case cf_clist:
          case cf_olist:
              if (level == 0)
                 {
                 snprintf(output,CF_BUFSIZE,"rhs is a scalar, but lhs (%s) is not a scalar type",CF_DATATYPES[dt]);
                 ReportError(output);
                 }
              break;
          }
       break;
       
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
              snprintf(output,CF_BUFSIZE,"rhs is a list, but lhs (%s) is not a list type",CF_DATATYPES[dt]);
              ReportError(output);
              break;
          }
       
       for (rp = (struct Rlist *)rval; rp != NULL; rp = rp->next)
          {
          CheckConstraintTypeMatch(lval,rp->item,rp->type,dt,range,1);
          }

       return;
       
   case CF_FNCALL:

       /* Fn-like objects are assumed to be parameterized bundles in these... */

       checklist = SplitString("bundlesequence,edit_line,edit_xml,usebundle",',');
       
       if (!IsItemIn(checklist,lval))
          {
          CheckFnCallType(lval,((struct FnCall *)rval)->name,dt,range);
          }

       DeleteItemList(checklist);
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
   case cf_bundle:
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

   case cf_irange:
       CheckParseIntRange(lval,(char *)rval,range);
       break;
       
   case cf_rrange:
       CheckParseRealRange(lval,(char *)rval,range);
       break;
       
   default:
       snprintf(output,CF_BUFSIZE,"Unknown (unhandled) datatype for lval = %s (CheckConstraintTypeMatch)\n",lval);
       FatalError(output);
   }

Debug("end CheckConstraintTypeMatch---------\n");
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

int CheckParseString(char *lval,char *s,char *range)

{ char output[CF_BUFSIZE];
  
Debug("\nCheckParseString(%s => %s/%s)\n",lval,s,range);

if (s == NULL)
   {
   return true;
   }

if (strlen(range) == 0)
   {
   return true;
   }

if (IsNakedVar(s,'@')||IsNakedVar(s,'$'))
   {
   Debug("Validation: Unable to verify variable expansion of %s at this stage\n",s);
   return false;
   }

/* Deal with complex strings as special cases */

if (strcmp(lval,"mode") == 0 || strcmp(lval,"search_mode") == 0)
   {
   mode_t plus,minus;
   
   if (!ParseModeString(s,&plus,&minus))
      {
      snprintf(output,CF_BUFSIZE,"Error parsing Unix permission string %s)",s);
      ReportError(output);
      return false;
      }
   }

if (FullTextMatch(range,s))
   {
   return true;
   }

if (IsCf3VarString(s))
   {
   Debug("Validation: Unable to verify syntax of %s due to variable expansion at this stage\n",s);
   }
else
   {
   snprintf(output,CF_BUFSIZE,"Scalar item in %s => { %s } in rvalue is out of bounds (value should match pattern %s)",lval,s,range);
   ReportError(output);
   return false;
   }

return true;
}

/****************************************************************************/

int CheckParseClass(char *lval,char *s,char *range)

{ regex_t rx;
  regmatch_t pmatch;
  char output[CF_BUFSIZE];

if (s == NULL)
   {
   return false;
   }
  
Debug("\nCheckParseString(%s => %s/%s)\n",lval,s,range);
  
if (strlen(range) == 0)
   {
   return true;
   }

if (FullTextMatch(range,s))
   {
   return true;
   }

snprintf(output,CF_BUFSIZE,"Class item on rhs of lval \'%s\' given as { %s } is out of bounds (should match %s)",lval,s,range);
ReportError(output);
return false;
}

/****************************************************************************/

void CheckParseInt(char *lval,char *s,char *range)
    
{ struct Item *split,*ip;
  int n;
  long max = CF_LOWINIT, min = CF_HIGHINIT, val;
  char output[CF_BUFSIZE];
  
/* Numeric types are registered by range separated by comma str "min,max" */
Debug("\nCheckParseInt(%s => %s/%s)\n",lval,s,range);

if (s == NULL)
   {
   return;
   }

split = SplitString(range,',');

if ((n = ListLen(split)) != 2)
   {
   snprintf(output,CF_BUFSIZE,"INTERN: format specifier for int rvalues is not ok for lval %s - got %d items",lval,n);
   FatalError(output);
   }

sscanf(split->name,"%ld",&min);

if (strcmp(split->next->name,"inf") == 0)
   {
   max = CF_INFINITY;
   }
else
   {
   sscanf(split->next->name,"%ld",&max);
   }

DeleteItemList(split);

if (min == CF_HIGHINIT || max == CF_LOWINIT)
   {
   snprintf(output,CF_BUFSIZE,"INTERN: could not parse format specifier for int rvalues for lval %s",lval);
   FatalError(output);
   }

if (IsCf3VarString(s))
   {
   Debug("Validation: Unable to verify syntax of int \'%s\' due to variable expansion at this stage\n",s);
   return;
   }

val = Str2Int(s);

if (val > max || val < min)
   {
   snprintf(output,CF_BUFSIZE,"Int item on rhs of lval \'%s\' given as {%s => %ld} is out of bounds (should be in [%s])",lval,s,val,range);
   ReportError(output);
   return;
   }

Debug("CheckParseInt - syntax verified\n\n");
}

/****************************************************************************/

void CheckParseIntRange(char *lval,char *s,char *range)
    
{ struct Item *split,*ip,*rangep;
  int n;
  long max = CF_LOWINIT, min = CF_HIGHINIT, val;
  char output[CF_BUFSIZE];

if (s == NULL)
   {
   return;
   }
  
/* Numeric types are registered by range separated by comma str "min,max" */
Debug("\nCheckParseIntRange(%s => %s/%s)\n",lval,s,range);

if (*s == '[' || *s == '(')
   {
   ReportError("Range specification should not be enclosed in brackets - just \"a,b\"");
   return;
   }

split = SplitString(range,',');

if ((n = ListLen(split)) != 2)
   {
   snprintf(output,CF_BUFSIZE,"INTERN:format specifier %s for irange rvalues is not ok for lval %s - got %d items",range,lval,n);
   FatalError(output);
   }

sscanf(split->name,"%ld",&min);

if (strcmp(split->next->name,"inf") == 0)
   {
   max = CF_INFINITY;
   }
else
   {
   sscanf(split->next->name,"%ld",&max);
   }

DeleteItemList(split);

if (min == CF_HIGHINIT || max == CF_LOWINIT)
   {
   snprintf(output,CF_BUFSIZE,"INTERN: could not parse irange format specifier for int rvalues for lval %s",lval);
   FatalError(output);
   }

if (IsCf3VarString(s))
   {
   Debug("Validation: Unable to verify syntax of int \'%s\' due to variable expansion at this stage\n",s);
   return;
   }

rangep = SplitString(s,',');

if ((n = ListLen(rangep)) != 2)
   {
   snprintf(output,CF_BUFSIZE,"Int range format specifier for lval %s should be of form \"a,b\" but got %d items",lval,n);
   ReportError(output);
   DeleteItemList(rangep);
   return;
   }

for (ip = rangep; ip != NULL; ip=ip->next)
   {
   val = Str2Int(ip->name);
   
   if (val > max || val < min)
      {
      snprintf(output,CF_BUFSIZE,"Int range item on rhs of lval \'%s\' given as {%s => %ld} is out of bounds (should be in [%s])",lval,s,val,range);
      ReportError(output);
      DeleteItemList(rangep);
      return;
      }
   }

DeleteItemList(rangep);

Debug("CheckParseIntRange - syntax verified\n\n");
}

/****************************************************************************/

void CheckParseReal(char *lval,char *s,char *range)
    
{ struct Item *split,*ip;
  double max = (double)CF_LOWINIT, min = (double)CF_HIGHINIT, val;
  int n;
  char output[CF_BUFSIZE];
  
Debug("\nCheckParseReal(%s => %s/%s)\n",lval,s,range);

if (s == NULL)
   {
   return;
   }

if (strcmp(s,"inf") == 0)
   {
   ReportError("keyword \"inf\" has an integer value, cannot be used as real");
   return;
   }

if (IsCf3VarString(s))
   {
   Debug("Validation: Unable to verify syntax of real %s due to variable expansion at this stage\n",s);
   return;
   }

/* Numeric types are registered by range separated by comma str "min,max" */

split = SplitString(range,',');

if ((n = ListLen(split)) != 2)
   {
   snprintf(output,CF_BUFSIZE,"INTERN:format specifier for real rvalues is not ok for lval %s - %d items",lval,n);
   FatalError(output);
   }

sscanf(split->name,"%lf",&min);
sscanf(split->next->name,"%lf",&max);
DeleteItemList(split);

if (min == CF_HIGHINIT || max == CF_LOWINIT)
   {
   snprintf(output,CF_BUFSIZE,"INTERN:could not parse format specifier for int rvalues for lval %s",lval);
   FatalError(output);
   }
   
val = Str2Double(s);

if (val > max || val < min)
   {
   snprintf(output,CF_BUFSIZE,"Real item on rhs of lval \'%s\' give as {%s => %.3lf} is out of bounds (should be in [%s])",lval,s,val,range);
   ReportError(output);
   }

Debug("CheckParseReal - syntax verified\n\n");
}

/****************************************************************************/

void CheckParseRealRange(char *lval,char *s,char *range)
    
{ struct Item *split,*rangep,*ip;
  double max = (double)CF_LOWINIT, min = (double)CF_HIGHINIT, val;
  int n;
  char output[CF_BUFSIZE];

if (s == NULL)
   {
   return;
   }
  
Debug("\nCheckParseRealRange(%s => %s/%s)\n",lval,s,range);

if (*s == '[' || *s == '(')
   {
   ReportError("Range specification should not be enclosed in brackets - just \"a,b\"");
   return;
   }

if (strcmp(s,"inf") == 0)
   {
   ReportError("keyword \"inf\" has an integer value, cannot be used as real");
   return;
   }

if (IsCf3VarString(s))
   {
   Debug("Validation: Unable to verify syntax of real %s due to variable expansion at this stage\n",s);
   return;
   }

/* Numeric types are registered by range separated by comma str "min,max" */

split = SplitString(range,',');

if ((n = ListLen(split)) != 2)
   {
   snprintf(output,CF_BUFSIZE,"INTERN:format specifier for real rvalues is not ok for lval %s - %d items",lval,n);
   FatalError(output);
   }

sscanf(split->name,"%lf",&min);
sscanf(split->next->name,"%lf",&max);
DeleteItemList(split);

if (min == CF_HIGHINIT || max == CF_LOWINIT)
   {
   snprintf(output,CF_BUFSIZE,"INTERN:could not parse format specifier for int rvalues for lval %s",lval);
   FatalError(output);
   }

rangep = SplitString(s,',');

if ((n = ListLen(rangep)) != 2)
   {
   snprintf(output,CF_BUFSIZE,"Real range format specifier in lval %s should be of form \"a,b\" but got %d items",lval,n);
   ReportError(output);
   DeleteItemList(rangep);
   return;
   }

for (ip = rangep; ip != NULL; ip=ip->next)
   {
   val = Str2Double(ip->name);
   
   if (val > max || val < min)
      {
      snprintf(output,CF_BUFSIZE,"Real range item on rhs of lval \'%s\' give as {%s => %.3lf} is out of bounds (should be in [%s])",lval,s,val,range);
      ReportError(output);
      }
   }

DeleteItemList(rangep);

Debug("CheckParseRealRange - syntax verified\n\n");
}

/****************************************************************************/

void CheckParseOpts(char *lval,char *s,char *range)

{ struct Item *split,*ip;
  int err = false;
  char output[CF_BUFSIZE];
 
/* List/menu types are separated by comma str "a,b,c,..." */

Debug("\nCheckParseOpts(%s => %s/%s)\n",lval,s,range);

if (s == NULL)
   {
   return;
   }

if (IsNakedVar(s,'@')||IsNakedVar(s,'$'))
   {
   Debug("Validation: Unable to verify variable expansion of %s at this stage\n",s);
   return;
   }

split = SplitString(range,',');

if (!IsItemIn(split,s))
   {
   snprintf(output,CF_BUFSIZE,"Selection on rhs of lval \'%s\' given as \'%s\' is out of bounds, should be in [%s]",lval,s,range);
   ReportError(output);
   err = true;
   }

DeleteItemList(split);

if (!err)
   {
   Debug("CheckParseOpts - syntax verified\n\n");
   }
}

/****************************************************************************/

void CheckFnCallType(char *lval,char *s,enum cfdatatype dtype,char *range)

{ int i;
  enum cfdatatype dt;
  char output[CF_BUFSIZE];

Debug("CheckFnCallType(%s => %s/%s)\n",lval,s,range);

if (s == NULL)
   {
   return;
   }

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

         snprintf(output,CF_BUFSIZE,"function %s() returns type %s but lhs requires %s",s,CF_DATATYPES[dt],CF_DATATYPES[dtype]);
         ReportError(output);
         return;
         }
      else
         {
         return;
         }
      }
   }

snprintf(output,CF_BUFSIZE,"Unknown built-in function %s()",s);
ReportError(output);
}


