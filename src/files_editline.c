/* 
   Copyright (C) 2008 - Mark Burgess

   This file is part of Cfengine 3 - written and maintained by Mark Burgess.
 
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
/* File: files_edit_operators.c                                              */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

enum editlinetypesequence
   {
   elp_classes,
   elp_delete,
   elp_columns,
   elp_replace,
   elp_insert,
   elp_none
   };

char *EDITLINETYPESEQUENCE[] =
   {
   "delete_lines",
   "column_edits",
   "replace_patterns",
   "insert_lines",
   NULL
   };

void EditClassBanner(enum editlinetypesequence type);

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int ScheduleEditLineOperations(char *filename,struct Bundle *bp,struct Attributes a,struct Promise *pp)

{ enum editlinetypesequence type;
  struct SubType *sp;

  // What about multipass
  
for (type = 0; EDITLINETYPESEQUENCE[type] != NULL; type++)
   {
   EditClassBanner(type);
   
   if ((sp = GetSubTypeForBundle(EDITLINETYPESEQUENCE[type],bp)) == NULL)
      {
      continue;      
      }
   
   BannerSubSubType(bp->name,sp->name);
   
   //NewEditTypeContext(type);
   
   for (pp = sp->promiselist; pp != NULL; pp=pp->next)
      {
      ExpandPromise(cf_agent,bp->name,pp,KeepEditPromise);
      }
   
   //DeleteEditTypeContext(type);
   }

return true;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

void EditClassBanner(enum editlinetypesequence type)

{ struct Item *ip;
 
if (type != elp_delete)   /* Just parsed all local classes */
   {
   return;
   }

Verbose ("     ??  Private class context:\n");

for (ip = VADDCLASSES; ip != NULL; ip=ip->next)
   {
   Verbose("     ??       %s\n",ip->name);
   }

Verbose("\n");
}

/***************************************************************************/

void KeepEditPromise(struct Promise *pp)

{
// this.promiser contains filename

//NewScalar("this","promiser",line,cf_str); // Parameters may only be scalars
//NewScalar, back references..
 //SetContext this

 printf("Do edit....%s\n",pp->promiser);
 
if (!IsDefinedClass(pp->classes))
   {
   Verbose("\n");
   Verbose("   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
   Verbose("   Skipping whole next edit promise, as context %s is not valid\n",pp->classes);
   Verbose("   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
   return;
   }

if (pp->done)
   {
   return;
   }


if (strcmp("classes",pp->agentsubtype) == 0)
   {
   KeepClassContextPromise(pp);
   return;
   }

if (strcmp("delete_lines",pp->agentsubtype) == 0)
   {
   VerifyLineDeletions(pp);
   return;
   }

if (strcmp("column_edits",pp->agentsubtype) == 0)
   {
   VerifyColumnEdits(pp);
   return;
   }

if (strcmp("replace_patterns",pp->agentsubtype) == 0)
   {
   VerifyPatterns(pp);
   return;
   }

if (strcmp("insert_lines",pp->agentsubtype) == 0)
   {
   VerifyLineInsertions(pp);
   return;
   }
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

void VerifyLineDeletions(struct Promise *pp)

{
}

/***************************************************************************/

void VerifyColumnEdits(struct Promise *pp)

{
}

/***************************************************************************/

void VerifyPatterns(struct Promise *pp)

{
}

/***************************************************************************/

void VerifyLineInsertions(struct Promise *pp)

{
}
