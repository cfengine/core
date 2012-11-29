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

#include "assoc.h"

/*******************************************************************/

CfAssoc *NewAssoc(const char *lval, Rval rval, enum cfdatatype dt)
{
    CfAssoc *ap;

    ap = xmalloc(sizeof(CfAssoc));

/* Make a private copy because promises are ephemeral in expansion phase */

    ap->lval = xstrdup(lval);
    ap->rval = CopyRvalItem(rval);
    ap->dtype = dt;

    if (lval == NULL)
    {
        FatalError("Bad association in NewAssoc\n");
    }

    return ap;
}

/*******************************************************************/

void DeleteAssoc(CfAssoc *ap)
{
    if (ap == NULL)
    {
        return;
    }

    CfDebug(" ----> Delete variable association %s\n", ap->lval);

    free(ap->lval);
    DeleteRvalItem(ap->rval);

    free(ap);

}

/*******************************************************************/

CfAssoc *CopyAssoc(CfAssoc *old)
{
    if (old == NULL)
    {
        return NULL;
    }

    return NewAssoc(old->lval, old->rval, old->dtype);
}

/*******************************************************************/

/*******************************************************************/

CfAssoc *AssocNewReference(const char *lval, Rval rval, enum cfdatatype dtype)
{
    CfAssoc *ap = NULL;

    ap = xmalloc(sizeof(CfAssoc));

    ap->lval = xstrdup(lval);
    ap->rval = rval;
    ap->dtype = dtype;

    if (lval == NULL)
    {
        FatalError("Bad association in AssocNewReference\n");
    }

    return ap;
}
