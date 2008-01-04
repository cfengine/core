/*****************************************************************************/
/*                                                                           */
/* File: assoc.c                                                             */
/*                                                                           */
/* Created: Sun Nov 25 13:06:29 2007                                         */
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

/*******************************************************************/

struct CfAssoc *NewAssoc(char *lval,void *rval,char rtype,enum cfdatatype dt)

{ struct CfAssoc *ap;

if ((ap= malloc(sizeof(struct CfAssoc))) == NULL)
   {
   FatalError("malloc failure in NewAssoc\n");
   }

ap->lval = lval;
ap->rval = rval;
ap->dtype = dt;
ap->rtype = rtype;

if (lval == NULL || rval == NULL)
   {
   FatalError("Bad association in NewAssoc\n");
   }

return ap;
}

/*******************************************************************/

void DeleteAssoc(struct CfAssoc *ap)

{
if (ap != NULL)
   {
   free((char *)ap);
   }
}

/*******************************************************************/

struct CfAssoc *CopyAssoc(struct CfAssoc *old)

{
if (old == NULL)
   {
   return NULL;
   }

return NewAssoc(old->lval,old->rval,old->rtype,old->dtype);
}

/*******************************************************************/

void ShowAssoc (struct CfAssoc *cp)

{
 printf("ShowAssoc: lval = %s\n",cp->lval);
 printf("ShowAssoc: rval = ");
 ShowRval(stdout,cp->rval,cp->rtype);
 
 printf("\nShowAssoc: dtype = %s\n",CF_DATATYPES[cp->dtype]);
}
