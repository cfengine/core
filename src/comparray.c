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
#include "cf3.extern.h"

/*******************************************************************/

int FixCompressedArrayValue(int i,char *value,struct CompressedArray **start)

{ struct CompressedArray *ap;
  char *sp;

for (ap = *start; ap != NULL; ap = ap->next)
   {
   if (ap->key == i) 
      {
      /* value already fixed */
      return false;
      }
   }

Debug("FixCompressedArrayValue(%d,%s)\n",i,value);

if ((ap = (struct CompressedArray *)malloc(sizeof(struct CompressedArray))) == NULL)
   {
   FatalError("Memory allocation in FixCompressedArray");
   }

if ((sp = malloc(strlen(value)+2)) == NULL)
   {
   FatalError("Memory allocation in FixCompressedArray");
   }

strcpy(sp,value);
ap->key = i;
ap->value = sp;
ap->next = *start;
*start = ap;
return true;
}


/*******************************************************************/

void DeleteCompressedArray(struct CompressedArray *start)

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

int CompressedArrayElementExists(struct CompressedArray *start,int key)

{ struct CompressedArray *ap;

Debug("CompressedArrayElementExists(%d)\n",key);

for (ap = start; ap !=NULL; ap = ap->next)
   {
   if (ap->key == key)
      {
      return true;
      }
   }

return false;
}

/*******************************************************************/

char *CompressedArrayValue(struct CompressedArray *start,int key)

{ struct CompressedArray *ap;

for (ap = start; ap != NULL; ap = ap->next)
   {
   if (ap->key == key)
      {
      return ap->value;
      }
   }

return NULL;
}
