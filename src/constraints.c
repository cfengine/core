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
/* File: constraints.c                                                       */
/*                                                                           */
/* Created: Wed Oct 17 13:00:08 2007                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/

struct Constraint *AppendConstraint(struct Constraint **conlist,char *lval, void *rval, char type,char *classes)

/* Note rval must be pre-allocated for this function, e.g. use
   CopyRvalItem in call.  This is to make the parser and var expansion
   non-leaky */
    
{ struct Constraint *cp,*lp;
  char *sp = NULL;

switch(type)
   {
   case CF_SCALAR:
       Debug("   Appending Constraint: %s => %s\n",lval,rval);
       break;
   case CF_FNCALL:
       Debug("   Appending a function call to rhs\n");
       break;
   case CF_LIST:
       Debug("   Appending a list to rhs\n");
   }

if ((cp = (struct Constraint *)malloc(sizeof(struct Constraint))) == NULL)
   {
   CfLog(cferror,"Unable to allocate Constraint","malloc");
   FatalError("");
   }

if ((sp = strdup(lval)) == NULL)
   {
   CfLog(cferror,"Unable to allocate Constraint lval","malloc");
   FatalError("");
   }

if (*conlist == NULL)
   {
   *conlist = cp;
   }
else
   {
   for (lp = *conlist; lp->next != NULL; lp=lp->next)
      {
      }

   lp->next = cp;
   }

if (classes != NULL)
   {
   if ((cp->classes = strdup(classes)) == NULL)
      {
      CfLog(cferror,"Unable to allocate Constraint classes","malloc");
      FatalError("");
      }
   }
else
   {
   cp->classes = NULL;
   }

cp->audit = AUDITPTR;
cp->lineno = P.line_no;
cp->lval = sp;
cp->rval = rval;
cp->type = type;  /* literal, bodyname, builtin function */
cp->next = NULL;
return cp;
}

/*****************************************************************************/

void DeleteConstraintList(struct Constraint *conlist)

{ struct Constraint *cp, *next;

Debug("DeleteConstraintList()\n");
 
for (cp = conlist; cp != NULL; cp = next)
   {
   Debug("Delete lval = %s,%c\n",cp->lval,cp->type);
   
   next = cp->next;

   if (cp->rval)
      {
      DeleteRvalItem(cp->rval,cp->type);
      }

   if (cp->lval)
      {
      free((char *)cp->lval);
      }
   
   if (cp->classes)
      {
      free(cp->classes);
      }

   free((char *)cp);
   }
}

/*****************************************************************************/

int GetBooleanConstraint(char *lval,struct Constraint *list)

{ struct Constraint *cp;
  int retval = CF_UNDEFINED;

for (cp = list; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (strcmp(cp->rval,"true") == 0)
         {
         if (IsDefinedClass(cp->classes))
            {
            if (retval != CF_UNDEFINED)
               {
               snprintf(OUTPUT,CF_BUFSIZE,"Multiple %s constraints break this promise\n",lval);
               CfLog(cferror,OUTPUT,"");
               }
            retval = true;
            }
         }
      
      if (strcmp(cp->rval,"false") == 0)
         {
         if (IsDefinedClass(cp->classes))
            {
            if (retval != CF_UNDEFINED)
               {
               snprintf(OUTPUT,CF_BUFSIZE,"Multiple %s constraints break this promise\n",lval);
               CfLog(cferror,OUTPUT,"");
               }
            retval = false;
            }
         }
      }
   }

if (retval == CF_UNDEFINED)
   {
   retval = false;
   }

return retval;
}

/*****************************************************************************/

void *GetConstraint(char *lval,struct Constraint *list)

{ struct Constraint *cp;
  void *retval = NULL;

for (cp = list; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != NULL)
            {
            snprintf(OUTPUT,CF_BUFSIZE,"Inconsistent %s constraints break this promise\n",lval);
            CfLog(cferror,OUTPUT,"");
            }
         retval = cp->rval;
         }
      }
   }

return retval;
}

