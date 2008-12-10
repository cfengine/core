/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
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
/* File: assoc.c                                                             */
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

/* Make a private copy because promises are ephemeral in expansion phase */

ap->lval = strdup(lval);
ap->rval = CopyRvalItem(rval,rtype);
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
Debug(" ----> Delete variable association %s\n",ap->lval);

if (ap->lval)
   {
   free(ap->lval);
   }

if (ap->rval)
   {
   DeleteRvalItem(ap->rval,ap->rtype);
   }

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
