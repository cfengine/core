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

/*******************************************************************/
/*                                                                 */
/* Compressed Arrays                                               */
/*                                                                 */
/*******************************************************************/

#include "cf3.defs.h"

/*******************************************************************/

int FixCompressedArrayValue(int i, char *value, CompressedArray **start)
{
    CompressedArray *ap;

    for (ap = *start; ap != NULL; ap = ap->next)
    {
        if (ap->key == i)
        {
            /* value already fixed */
            return false;
        }
    }

    CfDebug("FixCompressedArrayValue(%d,%s)\n", i, value);

    ap = xmalloc(sizeof(CompressedArray));

    ap->key = i;
    ap->value = xstrdup(value);
    ap->next = *start;
    *start = ap;
    return true;
}

/*******************************************************************/

void DeleteCompressedArray(CompressedArray *start)
{
    if (start != NULL)
    {
        DeleteCompressedArray(start->next);
        start->next = NULL;

        if (start->value != NULL)
        {
            free(start->value);
        }

        free(start);
    }
}

/*******************************************************************/

int CompressedArrayElementExists(CompressedArray *start, int key)
{
    CompressedArray *ap;

    CfDebug("CompressedArrayElementExists(%d)\n", key);

    for (ap = start; ap != NULL; ap = ap->next)
    {
        if (ap->key == key)
        {
            return true;
        }
    }

    return false;
}

/*******************************************************************/

char *CompressedArrayValue(CompressedArray *start, int key)
{
    CompressedArray *ap;

    for (ap = start; ap != NULL; ap = ap->next)
    {
        if (ap->key == key)
        {
            return ap->value;
        }
    }

    return NULL;
}
