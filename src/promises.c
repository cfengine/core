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
/* File: promises.c                                                          */
/*                                                                           */
/* Created: Wed Oct 17 11:05:59 2007                                         */
/*                                                                           */
/* Author:                                           >                       */
/*                                                                           */
/* Revision: $Id$                                                            */
/*                                                                           */
/* Description:                                                              */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

struct Promise *DeRefCopyPromise(char *scopeid,struct Promise *pp)

{ struct Promise *pcopy;
  struct Constraint *cp,*scp;
  char scope[CF_BUFSIZE],naked[CF_MAXVARSIZE];
  struct Rval returnval;

if (pp->promisee)
   {
   Debug("CopyPromise(%s->",pp->promiser);
   if (DEBUG)
      {
      ShowRval(stdout,pp->promisee,pp->petype);
      }
   Debug("\n");
   }
else
   {
   Debug("CopyPromise(%s->)\n",pp->promiser);
   }

if ((pcopy = (struct Promise *)malloc(sizeof(struct Promise))) == NULL)
   {
   CfLog(cferror,"Promise allocation failure","malloc");
   FatalError("memory");
   }

if (pp->promiser)
   {
   pcopy->promiser = strdup(pp->promiser);
   }

if (pp->promisee)
   {
   pcopy->promisee = CopyRvalItem(pp->promisee,pp->petype);
   pcopy->petype = pp->petype;
   }

if (pp->classes)
   {
   pcopy->classes  = strdup(pp->classes);
   }

if (pcopy->promiser == NULL || (pp->promisee != NULL && pcopy->promisee == NULL) || pcopy->classes == NULL)
   {
   CfLog(cferror,"Promise detail allocation failure","malloc");
   FatalError("memory");
   }

pcopy->audit = pp->audit;
pcopy->lineno = pp->lineno;
pcopy->petype = pp->petype;      /* rtype of promisee - list or scalar recipient? */
pcopy->bundle = strdup(pp->bundle);

pcopy->conlist = NULL;
pcopy->next = NULL;

Debug("Copying promise constraints\n\n");

/* No further type checking should be necessary here, already done by CheckConstraintTypeMatch */

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   struct Body *bp;
   struct FnCall *fp;
   struct Rlist *rp,*rnew;
   enum cfdatatype dtype;
   char *bodyname = NULL;

   /* A body template reference could look like a scalar or fn to the parser w/w () */

   switch (cp->type)
      {
      case CF_SCALAR:
          bodyname = (char *)cp->rval;
          bp = IsBody(BODIES,bodyname);
          fp = NULL;
          break;
      case CF_FNCALL:
          fp = (struct FnCall *)cp->rval;
          bodyname = fp->name;
          bp = IsBody(BODIES,bodyname);
          break;
      default:
          bp = NULL;
          fp = NULL;
          bodyname = NULL;
          break;
      }

   /* First case is: we have a body template to expand lval = body(args), .. */
   
   if (bp != NULL) 
      {
      Debug("Handling body-lval \"%s\"\n",cp->lval);
      
      if (bp->args != NULL)
         {
         /* There are arguments to insert*/
         
         if (fp == NULL || fp->args == NULL)
            {
            snprintf(OUTPUT,CF_BUFSIZE,"Argument mismatch for body reference "
                     "\"%s\" in promise at line %d of %s\n",
                     bodyname,pp->lineno,(pp->audit)->filename);
            CfLog(cferror,OUTPUT,"");
            }
         
         NewScope("body");
         
         if (!MapBodyArgs("body",fp->args,bp->args))
            {
            snprintf(OUTPUT,CF_BUFSIZE,"Number of arguments does not match for body reference "
                     "\"%s\" in promise at line %d of %s\n",
                     bodyname,pp->lineno,(pp->audit)->filename);
            CfLog(cferror,OUTPUT,"");
            }
      
         for (scp = bp->conlist; scp != NULL; scp = scp->next)
            {
            Debug("Doing arg-mapped sublval = %s (promises.c)\n",scp->lval);
            returnval = ExpandPrivateRval("body",scp->rval,scp->type);
            AppendConstraint(&(pcopy->conlist),scp->lval,returnval.item,returnval.rtype,scp->classes);
            }

         DeleteScope("body");
         }
      else
         {
         /* No arguments to deal with or body undeclared */

         if (fp != NULL)
            {
            snprintf(OUTPUT,CF_BUFSIZE,"body \"%s()\" was undeclared,"
                     "but used in a promise near line %d of %s",
                     bodyname,pp->lineno,(pp->audit)->filename);
            CfLog(cferror,OUTPUT,"");
            }
         else
            {
            for (scp = bp->conlist; scp != NULL; scp = scp->next)
               {
               Debug("Doing sublval = %s (promises.c)\n",scp->lval);
               rnew = CopyRvalItem(scp->rval,scp->type);
               AppendConstraint(&(pcopy->conlist),scp->lval,rnew,scp->type,scp->classes);
               }
            }
         }
      }
   else
      {
      rnew = CopyRvalItem(cp->rval,cp->type);
      scp = AppendConstraint(&(pcopy->conlist),cp->lval,rnew,cp->type,cp->classes);
      }
   }

return pcopy;
}

/*****************************************************************************/

void DeletePromise(struct Promise *pp)

{
Debug("DeletePromise(%s->[%c])\n",pp->promiser,pp->petype);

if (pp->promiser != NULL)
   {
   free(pp->promiser);
   }

if (pp->promisee != NULL)
   {
   DeleteRvalItem(pp->promisee,pp->petype);
   }

DeleteConstraintList(pp->conlist);
}


/*****************************************************************************/

struct Promise *ExpandDeRefPromise(char *scopeid,struct Promise *pp)

{ struct Promise *pcopy;
  struct Constraint *cp,*scp;
  char scope[CF_BUFSIZE],naked[CF_MAXVARSIZE];
  struct Rval returnval,final;

Debug("ExpandDerefPromise()\n");

if ((pcopy = (struct Promise *)malloc(sizeof(struct Promise))) == NULL)
   {
   CfLog(cferror,"Promise allocation failure","malloc");
   FatalError("memory");
   }

returnval = ExpandPrivateRval("this",pp->promiser,CF_SCALAR);
pcopy->promiser = (char *)returnval.item;

if (pp->promisee)
   {
   returnval = EvaluateFinalRval(scopeid,pp->promisee,pp->petype,true,pp);
   pcopy->promisee = (struct Rlist *)returnval.item;
   pcopy->petype = returnval.rtype;
   }
else
   {
   pcopy->petype = CF_NOPROMISEE;
   pcopy->promisee = NULL;
   }

if (pp->classes)
   {
   pcopy->classes = strdup(pp->classes);
   }
else
   {
   pcopy->classes = strdup("any");
   }

if (pcopy->promiser == NULL || pcopy->classes == NULL)
   {
   CfLog(cferror,"ExpandPromise detail allocation failure","malloc");
   FatalError("memory");
   }

pcopy->audit = pp->audit;
pcopy->lineno = pp->lineno;
pcopy->bundle = strdup(pp->bundle);
pcopy->conlist = NULL;
pcopy->next = NULL;

/* No further type checking should be necessary here, already done by CheckConstraintTypeMatch */

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   struct FnCall *fp;
   struct Rlist *rp;
   struct Rval returnval;
   char type;

   /* The body reference could look like a scalar or fn to the parser w/w () */
   
   returnval = EvaluateFinalRval(scopeid,cp->rval,cp->type,false,pp);   
   final     = ExpandDanglers(scopeid,returnval,pp);

   AppendConstraint(&(pcopy->conlist),cp->lval,final.item,final.rtype,cp->classes);
   }

return pcopy;
}

/*****************************************************************************/

void DeleteDeRefPromise(char *scopeid,struct Promise *pp)

{ struct Constraint *cp;

Debug("ExpandDerefPromise()\n");

free(pp->promiser);

if (pp->promisee)
   {
   DeleteRvalItem(pp->promisee,pp->petype);
   }

if (pp->classes)
   {
   free(pp->classes);
   }

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   free(cp->lval);
   DeleteRvalItem(cp->rval,cp->type);
   }

free(pp);
}


/*******************************************************************/

struct Body *IsBody(struct Body *list,char *key)

{ struct Body *bp;

for (bp = list; bp != NULL; bp = bp->next)
   {
   if (strcmp(bp->name,key) == 0)
      {
      return bp;
      }
   }

return NULL;
}
