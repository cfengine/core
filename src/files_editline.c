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

int ScheduleEditLineOperations(char *filename,struct Bundle *bp,struct Attributes a,struct Promise *parentp)

{ enum editlinetypesequence type;
  struct SubType *sp;
  struct Promise *pp;

  // What about multipass ?
  
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
      pp->edcontext = parentp->edcontext;
      ExpandPromise(cf_agent,bp->name,pp,KeepEditLinePromise);
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

void KeepEditLinePromise(struct Promise *pp)

{
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

{ struct Item **start = &(pp->edcontext->file_start), *match, *prev;
  struct Attributes a;
  struct Item *begin_ptr,*end_ptr;

*(pp->donep) = true;

a = GetDeletionAttributes(pp);

/* Are we working in a restricted region? */

if (!a.haveregion)
   {
   begin_ptr = *start;
   end_ptr = EndOfList(*start);
   }
else if (!SelectRegion(*start,&begin_ptr,&end_ptr,a,pp))
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a,"The promised line deletion (%s) could not select an edit region",pp->promiser);
   return;
   }

if (DeletePromisedLinesMatching(start,begin_ptr,end_ptr,a,pp))
   {
   (pp->edcontext->num_edits)++;
   }
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

{ struct Item **start = &(pp->edcontext->file_start), *match, *prev;
  struct Item *begin_ptr,*end_ptr;
  struct Attributes a;

*(pp->donep) = true;

a = GetInsertionAttributes(pp);

/* Are we working in a restricted region? */

if (!a.haveregion)
   {
   begin_ptr = *start;
   end_ptr = EndOfList(*start);
   }
else if (!SelectRegion(*start,&begin_ptr,&end_ptr,a,pp))
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a,"The promised line insertion (%s) could not select an edit region",pp->promiser);
   return;
   }

/* Are we looking for an anchored line inside the region? */

if (a.location.line_matching == NULL)
   {
   if (InsertMissingLineToRegion(start,begin_ptr,end_ptr,a,pp))
      {
      (pp->edcontext->num_edits)++;
      }
   }
else
   {
   if (!SelectItemMatching(a.location.line_matching,begin_ptr,end_ptr,&match,&prev,a.location.first_last))
      {
      cfPS(cf_error,CF_INTERPT,"",pp,a,"The promised line insertion (%s) could not select a locator matching %s",pp->promiser,a.location.line_matching);
      return;
      }

   if (InsertMissingLineAtLocation(start,match,prev,a,pp))
      {
      (pp->edcontext->num_edits)++;
      }
   }
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

int SelectRegion(struct Item *start,struct Item **begin_ptr,struct Item **end_ptr,struct Attributes a,struct Promise *pp)

{ struct Item *ip,*beg = CF_UNDEFINED_ITEM,*end = CF_UNDEFINED_ITEM;

for (ip = start; ip != NULL; ip = ip->next)
   {
   if (a.region.select_start)
      {
      if (beg == CF_UNDEFINED_ITEM && FullTextMatch(a.region.select_start,ip->name))
         {
         beg = ip;
         }
      }

   if (a.region.select_end)
      {
      if (end == CF_UNDEFINED_ITEM && FullTextMatch(a.region.select_end,ip->name))
         {
         end = ip;
         }
      }

   if (beg != CF_UNDEFINED_ITEM && end != CF_UNDEFINED_ITEM)
      {
      break;
      }
   }

if (beg == CF_UNDEFINED_ITEM && a.region.select_start)
   {
   cfPS(cf_inform,CF_INTERPT,"",pp,a,"The promised start pattern (%s) was not found when selecting edit region",a.region.select_start);
   return false;
   }

if (end == CF_UNDEFINED_ITEM && a.region.select_end)
   {
   cfPS(cf_inform,CF_INTERPT,"",pp,a,"The promised end pattern (%s) was not found when selecting edit region",a.region.select_end);
   return false;
   }

*begin_ptr = beg;
*end_ptr = end;
return true;
}

/***************************************************************************/

int InsertMissingLineToRegion(struct Item **start,struct Item *begin_ptr,struct Item *end_ptr,struct Attributes a,struct Promise *pp)

{ struct Item *ip, *prev = CF_UNDEFINED_ITEM;

/* find prev for region */

if (IsItemInRegion(pp->promiser,begin_ptr,end_ptr))
   {
   cfPS(cf_verbose,CF_NOP,"",pp,a,"Promised line \"%s\" exists within selected region",pp->promiser);
   return false;
   }

if (a.location.before_after == cfe_before)
   {
   for (ip = *start; ip != NULL; ip=ip->next)
      {
      if (ip == begin_ptr)
         {
         return InsertMissingLineAtLocation(start,ip,prev,a,pp);
         }
      
      prev = ip;
      }   
   }

if (a.location.before_after == cfe_after)
   {
   for (ip = *start; ip != NULL; ip=ip->next)
      {
      if (ip == end_ptr)
         {
         return InsertMissingLineAtLocation(start,ip,prev,a,pp);
         }
      
      prev = ip;
      }   
   }

return false;
}

/***************************************************************************/
    
int InsertMissingLineAtLocation(struct Item **start,struct Item *location,struct Item *prev,struct Attributes a,struct Promise *pp)

/* Check line neighbourhood in whole file to avoid edge effects */
    
{
if (prev == CF_UNDEFINED_ITEM) /* Insert at first line */
   {
   if (a.location.before_after == cfe_before)
      {
      if (strcmp((*start)->name,pp->promiser) != 0)
         {
         PrependItemList(start,pp->promiser);
         (pp->edcontext->num_edits)++;
         cfPS(cf_verbose,CF_CHG,"",pp,a,"Prepending the promised line \"%s\"",pp->promiser);
         return true;
         }
      else
         {
         cfPS(cf_verbose,CF_NOP,"",pp,a,"Promised line \"%s\" exists at start of file (promise kept)",pp->promiser);
         return false;
         }
      }
   }
 
if (a.location.before_after == cfe_before)
   {
   if (NeighbourItemMatches(*start,location,pp->promiser,cfe_before))
      {
      cfPS(cf_verbose,CF_NOP,"",pp,a,"Promised line \"%s\" exists before locator (promise kept)",pp->promiser);
      return false;
      }
   else
      {
      InsertAfter(start,prev,pp->promiser);
      cfPS(cf_verbose,CF_CHG,"",pp,a,"Inserting the promised line \"%s\" before locator",pp->promiser);
      return true;
      }
   }
else
   {
   if (NeighbourItemMatches(*start,location,pp->promiser,cfe_after))
      {
      cfPS(cf_verbose,CF_NOP,"",pp,a,"Promised line \"%s\" exists after locator (promise kept)",pp->promiser);
      return false;
      }
   else
      {
      InsertAfter(start,location,pp->promiser);
      cfPS(cf_verbose,CF_CHG,"",pp,a,"Inserting the promised line \"%s\" after locator",pp->promiser);
      return true;
      }
   }
}

/***************************************************************************/
    
int DeletePromisedLinesMatching(struct Item **start,struct Item *begin,struct Item *end,struct Attributes a,struct Promise *pp)

{ struct Item *ip,*np,*lp;
 int in_region = false, retval = false;

for (ip = *start; ip != NULL; ip = ip->next)
   {
   if (ip == begin)
      {
      in_region = true;
      }

   if (in_region && FullTextMatch(pp->promiser,ip->name))
      {
      cfPS(cf_verbose,CF_CHG,"",pp,a,"Deleting the promised line \"%s\"",ip->name);
      retval = true;

      if (ip->name != NULL)
         {
         free(ip->name);
         }
      
      np = ip->next;
      
      if (ip == *start)
         {
         *start = np;
         }
      else
         {
         for (lp = *start; lp->next != ip; lp=lp->next)
            {
            }

         lp->next = np;
         }
      
      free((char *)ip);
      (pp->edcontext->num_edits)++;      
      }

   if (ip == end)
      {
      in_region = false;
      }
   }

return retval;
}
