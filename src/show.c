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

#include "cf3.defs.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

/*****************************************************************************/

void ShowAlphaList(const AlphaList *al)
{
    for (int i = 0; i < CF_ALPHABETSIZE; i++)
    {
        if (al->list[i] != NULL)
        {
            printf("%c :", (char) i);

            for (const Item *ip = al->list[i]; ip != NULL; ip = ip->next)
            {
                printf(" %s", ip->name);
            }

            printf("\n");
        }
    }
}

/*****************************************************************************/

void ShowAssoc(CfAssoc *cp)
{
    printf("ShowAssoc: lval = %s\n", cp->lval);
    printf("ShowAssoc: rval = ");
    ShowRval(stdout, cp->rval);
    printf("\nShowAssoc: dtype = %s\n", CF_DATATYPES[cp->dtype]);
}

/*********************************************************************/

#pragma GCC diagnostic pop
