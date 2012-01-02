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

#include "syntax.h"
#include "cf3.extern.h"

#include "json.h"

static const int PRETTY_PRINT_SPACES_PER_INDENT = 2;

static int CheckParseString(char *lv,char *s, const char *range);
static void CheckParseInt(char *lv,char *s, const char *range);
static void CheckParseReal(char *lv,char *s, const char *range);
static void CheckParseRealRange(char *lval,char *s, const char *range);
static void CheckParseIntRange(char *lval,char *s, const char *range);
static void CheckParseOpts(char *lv,char *s, const char *range);
static void CheckFnCallType(char *lval,const char *s,enum cfdatatype dtype, const char *range);

/*********************************************************/

void CheckBundle(char *name,char *type)

{ struct Bundle *bp;
  char output[CF_BUFSIZE];
  const char *reserved[] = { "sys", "const", "mon", "edit", "match", "mon", "this", NULL };

CfDebug("Checking for bundle (%s,%s)\n",name,type);
  
if (IsStrIn(name,reserved))
   {
   snprintf(output,CF_BUFSIZE,"Use of a reserved context as a bundle name \"%s\" ",name);
   ReportError(output);      
   }

for (bp = BUNDLES; bp != NULL; bp=bp->next)
   {
   if ((strcmp(name,bp->name) == 0) && (strcmp(type,bp->type) == 0))
      {
      snprintf(output,CF_BUFSIZE,"Redefinition of bundle %s for %s is a broken promise",name,type);
      ReportError(output);
      ERRORCOUNT++;
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
      snprintf(output,CF_BUFSIZE,"Redefinition of body \"%s\" for \"%s\" is a broken promise",name,type);
      ReportError(output);
      ERRORCOUNT++;
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
   snprintf(output,CF_BUFSIZE,"Missing promise type category for %s bundle",bundletype);
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
            CfDebug("Subtype %s syntax ok for %s\n",subtype,bundletype);
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

void CheckPromise(struct Promise *pp)

{ char output[CF_BUFSIZE];

if (strcmp(pp->agentsubtype,"vars") == 0)
   {
   char *data_type = NULL;
   struct Constraint *cp;

   for (cp = pp->conlist; cp != NULL; cp=cp->next)
      {
      if (IsDataType(cp->lval))
         {
    	 if (data_type != NULL)
    	    {
    		snprintf(output,CF_BUFSIZE,"Variable contains existing data type contstraint %s, tried to redefine with %s",data_type,cp->lval);
    		ReportError(output);
    		ERRORCOUNT++;
    	    }
    	    data_type = cp->lval;
         }
      }
   }
}

/*********************************************************/

enum cfdatatype ExpectedDataType(char *lvalname)

{ int i,j,k,l;
 const struct BodySyntax *bs,*bs2;
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

      for (k = 0; bs[k].range != NULL; k++)
         {
         if (bs[k].dtype == cf_body)
            {
            bs2 = (const struct BodySyntax *)(bs[k].range);

            if (bs2 == NULL || bs2 == (void *)CF_BUNDLE)
               {
               continue;
               }

            for (l = 0; bs2[l].dtype != cf_notype; l++)
               {
               if (strcmp(lvalname,bs2[l].lval) == 0)
                  {
                  return bs2[l].dtype;
                  }
               }
            }
         }

      }
   }

return cf_notype;
}

/*********************************************************/

void CheckConstraint(char *type,char *name,char *lval, struct Rval rval,struct SubTypeSyntax ss)

{ int lmatch = false;
  int i,l, allowed = false;
  const struct BodySyntax *bs;
  char output[CF_BUFSIZE];

CfDebug("CheckConstraint(%s,%s,",type,lval);

if (DEBUG)
   {
   ShowRval(stdout, rval);
   }

CfDebug(")\n");


if (ss.subtype != NULL) /* In a bundle */
   {
   if (strcmp(ss.subtype,type) == 0)
      {
      CfDebug("Found type %s's body syntax\n",type);
      
      bs = ss.bs;
      
      for (l = 0; bs[l].lval != NULL; l++)
         {
         CfDebug("CMP-bundle # (%s,%s)\n",lval,bs[l].lval);
         
         if (strcmp(lval,bs[l].lval) == 0)
            {
            /* If we get here we have found the lval and it is valid
               for this subtype */
            
            lmatch = true;
            CfDebug("Matched syntatically correct bundle (lval,rval) item = (%s) to its rval\n",lval);
            
            if (bs[l].dtype == cf_body)
               {
               CfDebug("Constraint syntax ok, but definition of body is elsewhere %s=%c\n",lval,rval.rtype);
               PrependRlist(&BODYPARTS,rval.item,rval.rtype);
               return;
               }
            else if (bs[l].dtype == cf_bundle)
               {
               CfDebug("Constraint syntax ok, but definition of relevant bundle is elsewhere %s=%c\n",lval,rval.rtype);
               PrependRlist(&SUBBUNDLES,rval.item,rval.rtype);
               return;
               }
            else
               {
               CheckConstraintTypeMatch(lval, rval, bs[l].dtype, (char *)(bs[l].range), 0);
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
   CfDebug("CMP-common # %s,%s\n",lval,CF_COMMON_BODIES[i].lval);
   
   if (strcmp(lval,CF_COMMON_BODIES[i].lval) == 0)
      {
      CfDebug("Found a match for lval %s in the common constraint attributes\n",lval);
      return;
      }
   }


for (i = 0; CF_COMMON_EDITBODIES[i].lval != NULL; i++)
   {
   CfDebug("CMP-common # %s,%s\n",lval,CF_COMMON_EDITBODIES[i].lval);
   
   if (strcmp(lval,CF_COMMON_EDITBODIES[i].lval) == 0)
      {
      CfDebug("Found a match for lval %s in the common edit constraint attributes\n",lval);
      return;
      }
   }


// Now check if it is in the common list...

if (!lmatch || !allowed)
   {
   snprintf(output,CF_BUFSIZE,"Constraint lvalue \'%s\' is not allowed in bundle category \'%s\'",lval,type);
   ReportError(output);
   }
}

/******************************************************************************************/

int LvalWantsBody(char *stype,char *lval)

{
  int i,j,l;
  struct SubTypeSyntax *ss;
  const struct BodySyntax *bs;

for  (i = 0; i < CF3_MODULES; i++)
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
      
      if (strcmp(ss[j].subtype,stype) != 0)
         {
         continue;
         }

      for (l = 0; bs[l].range != NULL; l++)
         {
         if (strcmp(bs[l].lval,lval) == 0)
            {
            if (bs[l].dtype == cf_body)
               {
               return true;
               }
            else
               {
               return false;
               }
            }
         }
      }
   }

return false;
}

/******************************************************************************************/

void CheckSelection(char *type,char *name,char *lval, struct Rval rval)

{ int lmatch = false;
  int i,j,k,l;
  struct SubTypeSyntax *ss;
  const struct BodySyntax *bs,*bs2;
  char output[CF_BUFSIZE];
  
CfDebug("CheckSelection(%s,%s,",type,lval);

if (DEBUG)
   {
   ShowRval(stdout, rval);
   }

CfDebug(")\n");

/* Check internal control bodies etc */

for (i = 0; CF_ALL_BODIES[i].subtype != NULL; i++)
   {
   if (strcmp(CF_ALL_BODIES[i].subtype,name) == 0 && strcmp(type,CF_ALL_BODIES[i].btype) == 0)
      {
      CfDebug("Found matching a body matching (%s,%s)\n",type,name);
      
      bs = CF_ALL_BODIES[i].bs;
      
      for (l = 0; bs[l].lval != NULL; l++)
         {
         if (strcmp(lval,bs[l].lval) == 0)
            {
            CfDebug("Matched syntatically correct body (lval) item = (%s)\n",lval);
            
            if (bs[l].dtype == cf_body)
               {
               CfDebug("Constraint syntax ok, but definition of body is elsewhere\n");
               return;
               }
            else if (bs[l].dtype == cf_bundle)
               {
               CfDebug("Constraint syntax ok, but definition of bundle is elsewhere\n");
               return;
               }
            else
               {
               CheckConstraintTypeMatch(lval, rval, bs[l].dtype, (char *)(bs[l].range), 0);
               return;
               }
            }
         }
      
      }
   }


/* Now check the functional modules - extra level of indirection */

for  (i = 0; i < CF3_MODULES; i++)
   {
   CfDebug("Trying function module %d for matching lval %s\n",i,lval);
   
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
      
      CfDebug("\nExamining subtype %s\n",ss[j].subtype);

      for (l = 0; bs[l].range != NULL; l++)
         {
         if (bs[l].dtype == cf_body)
            {
            bs2 = (const struct BodySyntax *)(bs[l].range);

            if (bs2 == NULL || bs2 == (void *)CF_BUNDLE)
               {
               continue;
               }

            for (k = 0; bs2[k].dtype != cf_notype; k++)
               {
               /* Either module defined or common */

               if (strcmp(ss[j].subtype,type) == 0 && strcmp(ss[j].subtype,"*") != 0)
                  {
                  snprintf(output,CF_BUFSIZE,"lval %s belongs to promise type \'%s:\' but this is '\%s\'\n",lval,ss[j].subtype,type);
                  ReportError(output);
                  return;
                  }
               
               if (strcmp(lval,bs2[k].lval) == 0)
                  {
                  CfDebug("Matched\n");
                  CheckConstraintTypeMatch(lval, rval, bs2[k].dtype, (char *)(bs2[k].range), 0);
                  return;
                  }
               }
            }
         }
      }
   }

if (!lmatch)
   {
   snprintf(output,CF_BUFSIZE,"Constraint lvalue \"%s\" is not allowed in \'%s\' constraint body",lval,type);
   ReportError(output);
   }
}

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

void CheckConstraintTypeMatch(char *lval, struct Rval rval, enum cfdatatype dt, const char *range, int level)

{ struct Rlist *rp;
  struct Item *checklist;
  char output[CF_BUFSIZE];

if (rval.item == NULL)
   {
   return;
   }
  
CfDebug(" ------------------------------------------------\n");

if (dt == cf_bundle || dt == cf_body)
   {
   CfDebug(" - Checking inline constraint/arg %s[%s] => mappedval (bundle/body)\n",lval,CF_DATATYPES[dt]);
   }
else
   {
   CfDebug(" - Checking inline constraint/arg %s[%s] => mappedval (%c) %s\n",lval,CF_DATATYPES[dt],rval.rtype,range);
   }
CfDebug(" ------------------------------------------------\n");

/* Get type of lval */

switch(rval.rtype)
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
                 snprintf(output,CF_BUFSIZE," !! Type mismatch -- rhs is a scalar, but lhs (%s) is not a scalar type",CF_DATATYPES[dt]);
                 ReportError(output);
                 }
              break;
          default:
             /* Only lists are incompatible with scalars */
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
              snprintf(output,CF_BUFSIZE,"!! Type mistach -- rhs is a list, but lhs (%s) is not a list type",CF_DATATYPES[dt]);
              ReportError(output);
              break;
          }
       
       for (rp = (struct Rlist *)rval.item; rp != NULL; rp = rp->next)
          {
          CheckConstraintTypeMatch(lval, (struct Rval) { rp->item, rp->type }, dt, range, 1);
          }

       return;
       
   case CF_FNCALL:

       /* Fn-like objects are assumed to be parameterized bundles in these... */

       checklist = SplitString("bundlesequence,edit_line,edit_xml,usebundle,service_bundle",',');
       
       if (!IsItemIn(checklist,lval))
          {
          CheckFnCallType(lval,((struct FnCall *)rval.item)->name,dt,range);
          }

       DeleteItemList(checklist);
       return;
   }

/* If we get here, we have a literal scalar type */

switch (dt)
   {
   case cf_str:
   case cf_slist:
       CheckParseString(lval,(char *)rval.item,range);
       break;

   case cf_int:
   case cf_ilist:
       CheckParseInt(lval,(char *)rval.item,range);
       break;
       
   case cf_real:
   case cf_rlist:
       CheckParseReal(lval,(char *)rval.item,range);
       break;

   case cf_body:
   case cf_bundle:
       CfDebug("Nothing to check for body reference\n");
       break;
       
   case cf_opts:
   case cf_olist:
       CheckParseOpts(lval,(char *)rval.item,range);
       break;

   case cf_class:
   case cf_clist:
       CheckParseClass(lval,(char *)rval.item,range);
       break;

   case cf_irange:
       CheckParseIntRange(lval,(char *)rval.item,range);
       break;
       
   case cf_rrange:
       CheckParseRealRange(lval,(char *)rval.item,range);
       break;
       
   default:
       snprintf(output,CF_BUFSIZE,"Unknown (unhandled) datatype for lval = %s (CheckConstraintTypeMatch)\n",lval);
       FatalError(output);
       break;
   }

CfDebug("end CheckConstraintTypeMatch---------\n");
}
            
/****************************************************************************/

enum cfdatatype StringDataType(char *scopeid,char *string)

{
  enum cfdatatype dtype;
  struct Rval rval;
  int islist = false;
  char var[CF_BUFSIZE];
  
CfDebug("StringDataType(%s)\n",string);

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

if (*string == '$')
   {
   if (ExtractInnerCf3VarString(string,var))
      {
      if ((dtype = GetVariable(scopeid,var,&rval)) != cf_notype)
         {
         if (rval.rtype == CF_LIST)
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

static int CheckParseString(char *lval,char *s, const char *range)

{ char output[CF_BUFSIZE];
  
CfDebug("\nCheckParseString(%s => %s/%s)\n",lval,s,range);

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
   CfDebug("Validation: Unable to verify variable expansion of %s at this stage\n",s);
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
   CfDebug("Validation: Unable to verify syntax of %s due to variable expansion at this stage\n",s);
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

int CheckParseClass(char *lval,char *s, const char *range)

{ char output[CF_BUFSIZE];

if (s == NULL)
   {
   return false;
   }
  
CfDebug("\nCheckParseClass(%s => %s/%s)\n",lval,s,range);
  
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

static void CheckParseInt(char *lval,char *s, const char *range)
    
{ struct Item *split;
  int n;
  long max = CF_LOWINIT, min = CF_HIGHINIT, val;
  char output[CF_BUFSIZE];
  
/* Numeric types are registered by range separated by comma str "min,max" */
CfDebug("\nCheckParseInt(%s => %s/%s)\n",lval,s,range);

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
   CfDebug("Validation: Unable to verify syntax of int \'%s\' due to variable expansion at this stage\n",s);
   return;
   }

val = Str2Int(s);

if (val == CF_NOINT)
   {
   snprintf(output,CF_BUFSIZE,"Int item on rhs of lval \'%s\' given as \'%s\' could not be parsed",lval,s);
   ReportError(output);
   return;
   }

if (val > max || val < min)
   {
   snprintf(output,CF_BUFSIZE,"Int item on rhs of lval \'%s\' given as {%s => %ld} is out of bounds (should be in [%s])",lval,s,val,range);
   ReportError(output);
   return;
   }

CfDebug("CheckParseInt - syntax verified\n\n");
}

/****************************************************************************/

static void CheckParseIntRange(char *lval,char *s, const char *range)
    
{ struct Item *split,*ip,*rangep;
  int n;
  long max = CF_LOWINIT, min = CF_HIGHINIT, val;
  char output[CF_BUFSIZE];

if (s == NULL)
   {
   return;
   }
  
/* Numeric types are registered by range separated by comma str "min,max" */
CfDebug("\nCheckParseIntRange(%s => %s/%s)\n",lval,s,range);

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
   CfDebug("Validation: Unable to verify syntax of int \'%s\' due to variable expansion at this stage\n",s);
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

CfDebug("CheckParseIntRange - syntax verified\n\n");
}

/****************************************************************************/

static void CheckParseReal(char *lval,char *s, const char *range)
    
{ struct Item *split;
  double max = (double)CF_LOWINIT, min = (double)CF_HIGHINIT, val;
  int n;
  char output[CF_BUFSIZE];
  
CfDebug("\nCheckParseReal(%s => %s/%s)\n",lval,s,range);

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
   CfDebug("Validation: Unable to verify syntax of real %s due to variable expansion at this stage\n",s);
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

CfDebug("CheckParseReal - syntax verified\n\n");
}

/****************************************************************************/

static void CheckParseRealRange(char *lval,char *s, const char *range)
    
{ struct Item *split,*rangep,*ip;
  double max = (double)CF_LOWINIT, min = (double)CF_HIGHINIT, val;
  int n;
  char output[CF_BUFSIZE];

if (s == NULL)
   {
   return;
   }
  
CfDebug("\nCheckParseRealRange(%s => %s/%s)\n",lval,s,range);

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
   CfDebug("Validation: Unable to verify syntax of real %s due to variable expansion at this stage\n",s);
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

CfDebug("CheckParseRealRange - syntax verified\n\n");
}

/****************************************************************************/

static void CheckParseOpts(char *lval,char *s, const char *range)

{ struct Item *split;
  int err = false;
  char output[CF_BUFSIZE];
 
/* List/menu types are separated by comma str "a,b,c,..." */

CfDebug("\nCheckParseOpts(%s => %s/%s)\n",lval,s,range);

if (s == NULL)
   {
   return;
   }

if (IsNakedVar(s,'@')||IsNakedVar(s,'$'))
   {
   CfDebug("Validation: Unable to verify variable expansion of %s at this stage\n",s);
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
   CfDebug("CheckParseOpts - syntax verified\n\n");
   }
}

/****************************************************************************/

int CheckParseVariableName(char *name)

{ const char *reserved[] = { "promiser", "handle", "promise_filename", "promise_linenumber", "this", NULL };
  char *sp,scopeid[CF_MAXVARSIZE],vlval[CF_MAXVARSIZE];
  int count = 0, level = 0;
  
if (IsStrIn(name,reserved))
   {
   return false;
   }

scopeid[0] = '\0';

if (strchr(name,'.'))
   {
   for (sp = name; *sp != '\0'; sp++)
      {
      switch (*sp)
         {
         case '.':
             if (++count > 1 && level != 1)
                {
                return false;
                }
             break;
             
         case '[':
             level++;
             break;
             
         case ']':
             level--;
             break;
             
         default:
             break;
         }

      if (level > 1)
         {
         yyerror("Too many levels of [] reserved for array use");
         return false;
         }

      }

   if (count == 1)
      {
      sscanf(name,"%[^.].%s",scopeid,vlval);
   
      if (strlen(scopeid) == 0 || strlen(vlval) == 0)
         {
         return false;
         }
      }
   }

return true;
}

/****************************************************************************/

bool IsDataType(const char *s)
{
return strcmp(s,"string") == 0 || strcmp(s,"slist") == 0 ||
	   strcmp(s,"int") == 0 || strcmp(s,"ilist") == 0 ||
	   strcmp(s,"real") == 0 || strcmp(s,"rlist") == 0;
}

/****************************************************************************/

static void CheckFnCallType(char *lval,const char *s,enum cfdatatype dtype, const char *range)

{
  enum cfdatatype dt;
  char output[CF_BUFSIZE];
  const FnCallType *fn;

CfDebug("CheckFnCallType(%s => %s/%s)\n",lval,s,range);

if (s == NULL)
   {
   return;
   }

fn = FindFunction(s);

if (fn)
   {
   dt = fn->dtype;

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
else
   {
   snprintf(output,CF_BUFSIZE,"Unknown built-in function %s()",s);
   ReportError(output);
   }
}


/****************************************************************************/

static char *PCREStringToJsonString(const char *pcre)
{
const char *src = pcre;
char *dst = NULL;
char *json = xcalloc((2 * strlen(pcre)) + 1, sizeof(char));

for (dst = json; *src != '\0'; src++)
   {
   if (*src == '\"')
      {
      dst += sprintf(dst, "\\\"");
      }
   else if (*src == '\'')
      {
      dst += sprintf(dst, "\\\'");
      }
   else if (*src == '\\')
      {
      dst += sprintf(dst, "\\\\");
      }
   else
      {
      *dst = *src;
      dst++;
      }
   }

*dst = '\0';

return json;
}

static JsonObject *ExportAttributesSyntaxAsJson(const struct BodySyntax attributes[])
{
JsonObject *json = NULL;
int i = 0;

if (attributes == NULL)
   {
   return json;
   }

for (i = 0; attributes[i].lval != NULL; i++)
   {
   if (attributes[i].range == CF_BUNDLE)
      {
      /* TODO: must handle edit_line somehow */
      continue;
      }
   else if (attributes[i].dtype == cf_body)
      {
      JsonObject *json_attributes = ExportAttributesSyntaxAsJson((const struct BodySyntax *)attributes[i].range);
      JsonObjectAppendObject(&json, attributes[i].lval, json_attributes);
      }
   else
      {
      JsonObject *attribute = NULL;

      JsonObjectAppendString(&attribute, "datatype", CF_DATATYPES[attributes[i].dtype]);

      if (strlen(attributes[i].range) == 0)
	 {
	 JsonObjectAppendString(&attribute, "pcre-range", ".*");
	 }
      else if (attributes[i].dtype == cf_opts ||
	       attributes[i].dtype == cf_olist)
	 {
	 JsonArray *options = NULL;
	 char options_buffer[CF_BUFSIZE];
	 char *option = NULL;

	 strcpy(options_buffer, attributes[i].range);
	 for (option = strtok(options_buffer, ","); option != NULL;
	      option = strtok(NULL, ","))
	    {
	    JsonArrayAppendString(&options, option);
	    }

	 JsonObjectAppendArray(&attribute, "pcre-range", options);
	 }
      else
	 {
	 char *pcre_range = PCREStringToJsonString(attributes[i].range);
	 JsonObjectAppendString(&attribute, "pcre-range", pcre_range);
	 }

      JsonObjectAppendObject(&json, attributes[i].lval, attribute);
      }
   }

return json;
}

static JsonObject *ExportBundleTypeSyntaxAsJson(char *bundle_type)
{
JsonObject *json = NULL;
struct SubTypeSyntax *st;
int i = 0, j = 0;

for (i = 0; i < CF3_MODULES; i++)
   {
   st = CF_ALL_SUBTYPES[i];

   for (j = 0; st[j].btype != NULL; j++)
      {
      if (strcmp(bundle_type, st[j].btype) == 0 || strcmp("*", st[j].btype) == 0)
	 {
	 JsonObject *attributes = ExportAttributesSyntaxAsJson(st[j].bs);
	 JsonObjectAppendObject(&json, st[j].subtype, attributes);
	 }
      }
   }

return json;
}

static JsonObject *ExportControlBodiesSyntaxAsJson()
{
JsonObject *control_bodies = NULL;
int i = 0;

for (i = 0; CF_ALL_BODIES[i].btype != NULL; i++)
   {
   JsonObject *attributes = ExportAttributesSyntaxAsJson(CF_ALL_BODIES[i].bs);
   JsonObjectAppendObject(&control_bodies, CF_ALL_BODIES[i].btype, attributes);
   }

return control_bodies;
}

void SyntaxPrintAsJson(Writer *writer)
{
JsonObject *syntax_tree = NULL;

   {
   JsonObject * control_bodies = ExportControlBodiesSyntaxAsJson();
   JsonObjectAppendObject(&syntax_tree, "control-bodies", control_bodies);
   }

   {
   JsonObject *bundle_types = NULL;
   int i = 0;

   for (i = 0; CF_ALL_BODIES[i].btype != NULL; i++)
      {
      JsonObject *bundle_type = ExportBundleTypeSyntaxAsJson(CF_ALL_BODIES[i].btype);
      JsonObjectAppendObject(&bundle_types, CF_ALL_BODIES[i].btype, bundle_type);
      }

   JsonObjectAppendObject(&syntax_tree, "bundle-types", bundle_types);
   }

JsonObjectPrint(writer, syntax_tree, 0);
JsonObjectDelete(syntax_tree);
}

/****************************************************************************/

static void JsonObjectAppendSize(JsonObject **parent, const char *name, size_t value)
{
char buffer[10];
snprintf(buffer, 10, "%ld", value);
JsonObjectAppendString(parent, name, buffer);
}


static JsonObject *ExportAttributeValueAsJson(struct Rval rval)
{
JsonObject *json_attribute = NULL;

switch (rval.rtype)
   {
   case CF_SCALAR:
      {
      char buffer[CF_BUFSIZE];
      EscapeQuotes((const char *)rval.item, buffer, sizeof(buffer));

      JsonObjectAppendString(&json_attribute, "type", "string");
      JsonObjectAppendString(&json_attribute, "value", buffer);
      }
      return json_attribute;

   case CF_LIST:
      {
      struct Rlist *rp = NULL;
      JsonArray *list = NULL;
      JsonObjectAppendString(&json_attribute, "type", "list");

      for (rp = (struct Rlist *)rval.item; rp != NULL; rp = rp->next)
	 {
	 JsonArrayAppendObject(&list, ExportAttributeValueAsJson((struct Rval) { rp->item, rp->type }));
	 }

      JsonObjectAppendArray(&json_attribute, "value", list);
      return json_attribute;
      }

   case CF_FNCALL:
      {
      struct Rlist *argp = NULL;
      struct FnCall *call = (struct FnCall *)rval.item;

      JsonObjectAppendString(&json_attribute, "type", "function-call");
      JsonObjectAppendString(&json_attribute, "name", call->name);

	 {
	 JsonArray *arguments = NULL;

	 for (argp = call->args; argp != NULL; argp = argp->next)
	    {
	    JsonArrayAppendObject(&arguments, ExportAttributeValueAsJson((struct Rval) { argp->item, argp->type }));
	    }

	 JsonObjectAppendArray(&json_attribute, "arguments", arguments);
	 }

      return json_attribute;
      }

   default:
      FatalError("Attempted to export attribute of type: %s", rval.rtype);
      return NULL;
   }
}

static JsonObject *CreateContextAsJson(const char *name, size_t offset,
      size_t offset_end, const char *children_name, JsonArray *children)
{
JsonObject *json = NULL;

JsonObjectAppendString(&json, "name", name);
JsonObjectAppendSize(&json, "offset", offset);
JsonObjectAppendSize(&json, "offset-end", offset_end);
JsonObjectAppendArray(&json, children_name, children);

return json;
}

static JsonArray *ExportBodyClassesAsJson(struct Constraint *constraints)
{
JsonArray *json_contexts = NULL;
JsonArray *json_attributes = NULL;
char *current_context = "any";
size_t context_offset_start = -1;
size_t context_offset_end = -1;
struct Constraint *cp = NULL;

for (cp = constraints; cp != NULL; cp = cp->next)
   {
   JsonObject *json_attribute = NULL;

   JsonObjectAppendSize(&json_attribute, "offset", cp->offset.start);
   JsonObjectAppendSize(&json_attribute, "offset-end", cp->offset.end);

   context_offset_start = cp->offset.context;
   context_offset_end = cp->offset.end;

   JsonObjectAppendString(&json_attribute, "lval", cp->lval);
   JsonObjectAppendObject(&json_attribute, "rval", ExportAttributeValueAsJson(cp->rval));
   JsonArrayAppendObject(&json_attributes, json_attribute);

   if (cp->next == NULL || strcmp(current_context, cp->next->classes) != 0)
      {
      JsonArrayAppendObject(&json_contexts,
      	    CreateContextAsJson(current_context,
      		                context_offset_start,
      			        context_offset_end,
      			        "attributes",
      			        json_attributes));

      current_context = cp->classes;
      }
   }

return json_contexts;
}

static JsonArray *ExportBundleClassesAsJson(struct Promise *promises)
{
JsonArray *json_contexts = NULL;
JsonArray *json_promises = NULL;
char *current_context = "any";
size_t context_offset_start = -1;
size_t context_offset_end = -1;
struct Promise *pp = NULL;

for (pp = promises; pp != NULL; pp = pp->next)
   {
   JsonObject *json_promise = NULL;

   JsonObjectAppendSize(&json_promise, "offset", pp->offset.start);

      {
      JsonArray *json_promise_attributes = NULL;
      struct Constraint *cp = NULL;

      for (cp = pp->conlist; cp != NULL; cp = cp->next)
	 {
	 JsonObject *json_attribute = NULL;

	 JsonObjectAppendSize(&json_attribute, "offset", cp->offset.start);
	 JsonObjectAppendSize(&json_attribute, "offset-end", cp->offset.end);

	 context_offset_end = cp->offset.end;

	 JsonObjectAppendString(&json_attribute, "lval", cp->lval);
	 JsonObjectAppendObject(&json_attribute, "rval", ExportAttributeValueAsJson(cp->rval));
	 JsonArrayAppendObject(&json_promise_attributes, json_attribute);
	 }

      JsonObjectAppendSize(&json_promise, "offset-end", context_offset_end);

      JsonObjectAppendString(&json_promise, "promiser", pp->promiser);
      /* FIXME: does not work for lists */
      if (pp->promisee.rtype == CF_SCALAR || pp->promisee.rtype == CF_NOPROMISEE)
         {
         JsonObjectAppendString(&json_promise, "promisee", pp->promisee.item);
         }

      JsonObjectAppendArray(&json_promise, "attributes", json_promise_attributes);
      }
   JsonArrayAppendObject(&json_promises, json_promise);

   if (pp->next == NULL || strcmp(current_context, pp->next->classes) != 0)
      {
      JsonArrayAppendObject(&json_contexts,
      	    CreateContextAsJson(current_context,
      		                context_offset_start,
      			        context_offset_end,
      			        "promises",
      			        json_promises));

      current_context = pp->classes;
      }
   }

return json_contexts;
}

static JsonObject *ExportBundleAsJson(struct Bundle *bundle)
{
JsonObject *json_bundle = NULL;

JsonObjectAppendSize(&json_bundle, "offset", bundle->offset.start);
JsonObjectAppendSize(&json_bundle, "offset-end", bundle->offset.end);

JsonObjectAppendString(&json_bundle, "name", bundle->name);
JsonObjectAppendString(&json_bundle, "bundle-type", bundle->type);

   {
   JsonArray *json_args = NULL;
   struct Rlist *argp = NULL;

   for (argp = bundle->args; argp != NULL; argp = argp->next)
      {
      JsonArrayAppendString(&json_args, argp->item);
      }

   JsonObjectAppendArray(&json_bundle, "arguments", json_args);
   }

   {
   JsonArray *json_promise_types = NULL;
   struct SubType *sp = NULL;

   for (sp = bundle->subtypes; sp != NULL; sp = sp->next)
      {
      JsonObject *json_promise_type = NULL;

      JsonObjectAppendSize(&json_promise_type, "offset", sp->offset.start);
      JsonObjectAppendSize(&json_promise_type, "offset-end", sp->offset.end);
      JsonObjectAppendString(&json_promise_type, "name", sp->name);
      JsonObjectAppendArray(&json_promise_type, "classes", ExportBundleClassesAsJson(sp->promiselist));

      JsonArrayAppendObject(&json_promise_types, json_promise_type);
      }

   JsonObjectAppendArray(&json_bundle, "promise-types", json_promise_types);
   }

return json_bundle;
}


static JsonObject *ExportBodyAsJson(struct Body *body)
{
JsonObject *json_body = NULL;

JsonObjectAppendSize(&json_body, "offset", body->offset.start);
JsonObjectAppendSize(&json_body, "offset-end", body->offset.end);

JsonObjectAppendString(&json_body, "name", body->name);
JsonObjectAppendString(&json_body, "body-type", body->type);

   {
   JsonArray *json_args = NULL;
   struct Rlist *argp = NULL;

   for (argp = body->args; argp != NULL; argp = argp->next)
      {
      JsonArrayAppendString(&json_args, argp->item);
      }

   JsonObjectAppendArray(&json_body, "arguments", json_args);
   }

JsonObjectAppendArray(&json_body, "classes", ExportBodyClassesAsJson(body->conlist));

return json_body;
}

void PolicyPrintAsJson(Writer *writer, const char *filename,
                       struct Bundle *bundles, struct Body *bodies)
{
JsonObject *json_policy = NULL;
JsonObjectAppendString(&json_policy, "name", filename);

   {
   JsonArray *json_bundles = NULL;
   struct Bundle *bp = NULL;

   for (bp = bundles; bp != NULL; bp = bp->next)
      {
      JsonArrayAppendObject(&json_bundles, ExportBundleAsJson(bp));
      }

   JsonObjectAppendArray(&json_policy, "bundles", json_bundles);
   }

   {
   JsonArray *json_bodies = NULL;
   struct Body *bdp = NULL;

   for (bdp = bodies; bdp != NULL; bdp = bdp->next)
      {
      JsonArrayAppendObject(&json_bodies, ExportBodyAsJson(bdp));
      }

   JsonObjectAppendArray(&json_policy, "bodies", json_bodies);
   }


JsonObjectPrint(writer, json_policy, 0);
JsonObjectDelete(json_policy);
}

/****************************************************************************/

static void IndentPrint(Writer *writer, int indent_level)
{
int i = 0;
for (i = 0; i < PRETTY_PRINT_SPACES_PER_INDENT * indent_level; i++)
   {
   WriterWriteChar(writer, ' ');
   }
}

static void RvalPrettyPrint(Writer *writer, struct Rval rval)
{
/* FIX: prettify */
RvalPrint(writer, rval);
}

static void AttributePrettyPrint(Writer *writer, struct Constraint *attribute, int indent_level)
{
WriterWriteF(writer, "%s => ", attribute->lval);
RvalPrettyPrint(writer, attribute->rval);
}

static void ArgumentsPrettyPrint(Writer *writer, struct Rlist *args)
{
struct Rlist *argp = NULL;

WriterWriteChar(writer, '(');
for (argp = args; argp != NULL; argp = argp->next)
   {
   WriterWriteF(writer, "%s", (char *)argp->item);

   if (argp->next != NULL)
      {
      WriterWrite(writer, ", ");
      }
   }
WriterWriteChar(writer, ')');
}

void BodyPrettyPrint(Writer *writer, struct Body *body)
{
struct Constraint *cp = NULL;
char *current_class = NULL;

WriterWriteF(writer, "body %s %s", body->type, body->name);
ArgumentsPrettyPrint(writer, body->args);
WriterWrite(writer, "\n{");

for (cp = body->conlist; cp != NULL; cp = cp->next)
   {
   if (current_class == NULL || strcmp(cp->classes, current_class) != 0)
      {
      current_class = cp->classes;

      if (strcmp(current_class, "any") == 0)
	 {
         WriterWrite(writer, "\n");
	 }
      else
	 {
         WriterWriteF(writer, "\n\n%s::", current_class);
	 }
      }

   WriterWriteChar(writer, '\n');
   IndentPrint(writer, 1);
   AttributePrettyPrint(writer, cp, 2);
   }

WriterWrite(writer, "\n}");
}

void BundlePrettyPrint(Writer *writer, struct Bundle *bundle)
{
struct SubType *promise_type = NULL;

WriterWriteF(writer, "bundle %s %s", bundle->type, bundle->name);
ArgumentsPrettyPrint(writer, bundle->args);
WriterWrite(writer, "\n{");

for (promise_type = bundle->subtypes; promise_type != NULL; promise_type = promise_type->next)
   {
   struct Promise* pp = NULL;
   WriterWriteF(writer, "\n%s:\n", promise_type->name);

   for (pp = promise_type->promiselist; pp != NULL; pp = pp->next)
      {
      struct Constraint *cp = NULL;
      char *current_class = NULL;

      if (current_class == NULL || strcmp(cp->classes, current_class) != 0)
         {
         current_class = cp->classes;

         if (strcmp(current_class, "any") != 0)
            {
            IndentPrint(writer, 1);
            WriterWriteF(writer, "%s::", current_class);
            }
         }

      IndentPrint(writer, 2);
      WriterWrite(writer, pp->promiser);

      /* FIX: add support
       *
      if (pp->promisee != NULL)
	 {
	 fprintf(out, " -> %s", pp->promisee);
	 }
      */

      for (cp = pp->conlist; cp != NULL; cp = cp->next)
         {
         WriterWriteChar(writer, '\n');
         IndentPrint(writer, 1);
         AttributePrettyPrint(writer, cp, 3);
         }
      }

   if (promise_type->next != NULL)
      {
      WriterWriteChar(writer, '\n');
      }
   }

WriterWrite(writer, "\n}");
}
