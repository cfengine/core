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
/* File: assoc.c                                                             */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

static void ShowAssoc (struct CfAssoc *cp);

/*******************************************************************/

struct CfAssoc *NewAssoc(const char *lval,void *rval,char rtype,enum cfdatatype dt)

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

if (ap->rval == NULL)
   {
   free(ap->lval);
   free(ap);
   return NULL;
   }

if (lval == NULL)
   {
   FatalError("Bad association in NewAssoc\n");
   }

return ap;
}

/*******************************************************************/

void DeleteAssoc(struct CfAssoc *ap)

{
if (ap == NULL)
   {
   return;
   }

Debug(" ----> Delete variable association %s\n",ap->lval);

if (ap->lval)
   {
   free(ap->lval);
   }

if (ap->rval)
   { 
   DeleteRvalItem(ap->rval,ap->rtype);
   }


free((char *)ap);

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

static void ShowAssoc (struct CfAssoc *cp)

{
printf("ShowAssoc: lval = %s\n",cp->lval);
printf("ShowAssoc: rval = ");
ShowRval(stdout,cp->rval,cp->rtype);
 
printf("\nShowAssoc: dtype = %s\n",CF_DATATYPES[cp->dtype]);
}
