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
   elp_reports,
   elp_none
   };

char *EDITLINETYPESEQUENCE[] =
   {
   "delete_lines",
   "column_edits",
   "replace_patterns",
   "insert_lines",
   "reports",
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
   
   for (pp = sp->promiselist; pp != NULL; pp=pp->next)
      {
      pp->edcontext = parentp->edcontext;
      ExpandPromise(cf_agent,bp->name,pp,KeepEditLinePromise);
      }
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

/*
if (pp->done)
   {
   return;
   }
*/

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

if (strcmp("reports",pp->agentsubtype) == 0)
   {
   VerifyReportPromise(pp);
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

a = GetDeletionAttributes(pp);

/* Are we working in a restricted region? */

if (!a.haveregion)
   {
   begin_ptr = *start;
   end_ptr = NULL; //EndOfList(*start);
   }
else if (!SelectRegion(*start,&begin_ptr,&end_ptr,a,pp))
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised line deletion (%s) could not select an edit region",pp->promiser);
   return;
   }

if (DeletePromisedLinesMatching(start,begin_ptr,end_ptr,a,pp))
   {
   (pp->edcontext->num_edits)++;
   }
}

/***************************************************************************/

void VerifyColumnEdits(struct Promise *pp)

{ struct Item **start = &(pp->edcontext->file_start), *match, *prev;
  struct Attributes a;
  struct Item *begin_ptr,*end_ptr;

a = GetColumnAttributes(pp);

if (a.column.column_separator == NULL)
   {
   cfPS(cf_error,CF_WARN,"",pp,a,"No column_separator in promise to edit by column for %s",pp->promiser);
   PromiseRef(cf_error,pp);   
   return;
   }

if (a.column.select_column <= 0)
   {
   cfPS(cf_error,CF_WARN,"",pp,a,"No select_column in promise to edit %s",pp->promiser);
   PromiseRef(cf_error,pp);   
   return;   
   }

if (!a.column.column_value)
   {
   cfPS(cf_error,CF_WARN,"",pp,a,"No column_value is promised to column_edit %s",pp->promiser);
   PromiseRef(cf_error,pp);   
   return;   
   }

/* Are we working in a restricted region? */

if (!a.haveregion)
   {
   begin_ptr = *start;
   end_ptr =NULL; // EndOfList(*start);
   }
else if (!SelectRegion(*start,&begin_ptr,&end_ptr,a,pp))
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised line deletion (%s) could not select an edit region",pp->promiser);
   return;
   }

/* locate and split line */

if (EditColumns(begin_ptr,end_ptr,a,pp))
   {
   (pp->edcontext->num_edits)++;
   }
}

/***************************************************************************/

void VerifyPatterns(struct Promise *pp)

{ struct Item **start = &(pp->edcontext->file_start), *match, *prev;
  struct Attributes a;
  struct Item *begin_ptr,*end_ptr;

a = GetReplaceAttributes(pp);

if (!a.replace.replace_value)
   {
   cfPS(cf_error,CF_WARN,"",pp,a,"No replace_value in promise to replace pattern %s",pp->promiser);
   PromiseRef(cf_error,pp);
   return;
   }

/* Are we working in a restricted region? */

if (!a.haveregion)
   {
   begin_ptr = *start;
   end_ptr = NULL; //EndOfList(*start);
   }
else if (!SelectRegion(*start,&begin_ptr,&end_ptr,a,pp))
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised line deletion (%s) could not select an edit region",pp->promiser);
   return;
   }

/* Make sure back references are expanded */

if (ReplacePatterns(begin_ptr,end_ptr,a,pp))
   {
   (pp->edcontext->num_edits)++;
   }
}

/***************************************************************************/

void VerifyLineInsertions(struct Promise *pp)

{ struct Item **start = &(pp->edcontext->file_start), *match, *prev;
  struct Item *begin_ptr,*end_ptr;
  struct Attributes a;

a = GetInsertionAttributes(pp);

/* Are we working in a restricted region? */

if (!a.haveregion)
   {
   begin_ptr = *start;
   end_ptr = NULL; //EndOfList(*start);
   }
else if (!SelectRegion(*start,&begin_ptr,&end_ptr,a,pp))
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised line insertion (%s) could not select an edit region",pp->promiser);
   return;
   }

/* Are we looking for an anchored line inside the region? */

if (a.location.line_matching == NULL)
   {
   if (InsertMissingLinesToRegion(start,begin_ptr,end_ptr,a,pp))
      {
      (pp->edcontext->num_edits)++;
      }
   }
else
   {
   if (!SelectItemMatching(a.location.line_matching,begin_ptr,end_ptr,&match,&prev,a.location.first_last))
      {
      cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised line insertion (%s) could not select a locator matching %s",pp->promiser,a.location.line_matching);
      return;
      }

   if (InsertMissingLinesAtLocation(start,match,prev,a,pp))
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
         continue;
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
   cfPS(cf_inform,CF_INTERPT,"",pp,a," !! The promised start pattern (%s) was not found when selecting edit region",a.region.select_start);
   return false;
   }

if (end == CF_UNDEFINED_ITEM && a.region.select_end)
   {
   cfPS(cf_inform,CF_INTERPT,"",pp,a," !! The promised end pattern (%s) was not found when selecting edit region",a.region.select_end);
   return false;
   }
else
   {
   end = NULL; /* End of file is null ptr if nothing else specified */
   }

*begin_ptr = beg;
*end_ptr = end;
return true;
}

/***************************************************************************/

int InsertMissingLinesToRegion(struct Item **start,struct Item *begin_ptr,struct Item *end_ptr,struct Attributes a,struct Promise *pp)

{ struct Item *ip, *prev = CF_UNDEFINED_ITEM;

/* find prev for region */

if (IsItemInRegion(pp->promiser,begin_ptr,end_ptr))
   {
   cfPS(cf_verbose,CF_NOP,"",pp,a," -> Promised line \"%s\" exists within selected region (promise kept)",pp->promiser);
   return false;
   }

if (*start == NULL)
   {
   return InsertMissingLinesAtLocation(start,*start,prev,a,pp);
   }

if (a.location.before_after == cfe_before)
   {
   for (ip = *start; ip != NULL; ip=ip->next)
      {
      if (ip == begin_ptr)
         {
         return InsertMissingLinesAtLocation(start,ip,prev,a,pp);
         }
      
      prev = ip;
      }   
   }

if (a.location.before_after == cfe_after)
   {
   for (ip = *start; ip != NULL; ip=ip->next)
      {
      if (ip == end_ptr || ip->next == NULL)
         {
         return InsertMissingLinesAtLocation(start,ip,prev,a,pp);
         }
      
      prev = ip;
      }   
   }

return false;
}

/***************************************************************************/

int InsertMissingLinesAtLocation(struct Item **start,struct Item *location,struct Item *prev,struct Attributes a,struct Promise *pp)

{ FILE *fin;
  char buf[CF_BUFSIZE],exp[CF_EXPANDSIZE];
  struct Item *loc;
  int retval = false;
  
if (a.sourcetype && strcmp(a.sourcetype,"file") == 0)
   {
   if ((fin = fopen(pp->promiser,"r")) == NULL)
      {
      cfPS(cf_error,CF_INTERPT,"fopen",pp,a,"Could not read file %s",pp->promiser);
      return false;
      }

   loc = location;

   while (!feof(fin))
      {
      buf[0] = '\0';
      fgets(buf,CF_BUFSIZE,fin);
      Chop(buf);

      if (a.expandvars)
         {
         ExpandScalar(buf,exp);
         }
      else
         {
         strcpy(exp,buf);
         }

      retval |= InsertMissingLineAtLocation(exp,start,loc,prev,a,pp);

      prev = prev->next;
      loc = loc->next;

      if (loc == NULL)
         {
         // somthing funny ... shouldn't happen
         break;
         }
      }
   
   fclose(fin);
   return retval;
   }
else
   {
   return InsertMissingLineAtLocation(pp->promiser,start,location,prev,a,pp);
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
      if (DONTDO || a.transaction.action == cfa_warn)
         {
         cfPS(cf_verbose,CF_WARN,"",pp,a," -> Need to delete line \"%s\" - but only a warning was promised",ip->name);
         return false;
         }
      
      cfPS(cf_verbose,CF_CHG,"",pp,a," -> Deleting the promised line \"%s\"",ip->name);
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

/********************************************************************/

int ReplacePatterns(struct Item *file_start,struct Item *file_end,struct Attributes a,struct Promise *pp)

{ char *sp, *start = NULL,*end,replace[CF_EXPANDSIZE],line_buff[CF_EXPANDSIZE];
 int match_len,start_off,end_off,once_only = false,retval = false;
  struct CfRegEx rex;
  struct Item *ip;
 
rex = CompileRegExp(pp->promiser);

if (rex.failed)
   {
   return false;
   }

if (a.replace.occurrences && strcmp(a.replace.occurrences,"first") == 0)
   {
   once_only = true;
   }

for (ip = file_start; ip != file_end; ip=ip->next)
   {
   if (ip->name == NULL)
      {
      continue;
      }

   if (!RegExMatchSubString(rex,ip->name,&start_off,&end_off))
      {
      continue;
      }
   else
      {
      match_len = end_off - start_off;
      ExpandScalar(a.replace.replace_value,replace);
      }
   
   memset(line_buff,0,CF_BUFSIZE);
   sp = ip->name;   

   while (*sp != '\0')
      {
      strncat(line_buff,sp,start_off);
      sp += start_off;
      strncat(line_buff,replace,CF_BUFSIZE);
      sp += match_len;

      if (once_only)
         {
         strncat(line_buff,ip->name+end_off,CF_BUFSIZE);
         break;
         }
      else
         {
         if (!RegExMatchSubString(rex,sp,&start_off,&end_off))
            {
            strncat(line_buff,sp,CF_BUFSIZE);
            break;
            }
         else
            {
            match_len = end_off - start_off;
            ExpandScalar(a.replace.replace_value,replace);
            }
         }
      }

   if (RegExMatchSubString(rex,line_buff,&start_off,&end_off))
      {
      cfPS(cf_verbose,CF_INTERPT,"",pp,a," -> Promised replacement \"%s\" on line \"%s\" for pattern \"%s\" is not convergent",line_buff,ip->name,pp->promiser);
      continue;
      }

   if (once_only)
      {
      break;
      }
   
   if (DONTDO || a.transaction.action == cfa_warn)
      {
      cfPS(cf_verbose,CF_WARN,"",pp,a," -> Need to replace line \"%s\" - but only a warning was promised",pp->promiser);
      continue;
      }
   else
      {
      free(ip->name);
      ip->name = strdup(line_buff);
      cfPS(cf_verbose,CF_CHG,"",pp,a," -> Replaced pattern \"%s\"",pp->promiser);
      retval = true;
      }
   }

return retval;
}

/********************************************************************/

int EditColumns(struct Item *file_start,struct Item *file_end,struct Attributes a,struct Promise *pp)

{ char separator[CF_EXPANDSIZE]; 
  int s,e,retval = false;
  struct CfRegEx rex;
  struct Item *ip;
  struct Rlist *columns = NULL;
 
rex = CompileRegExp(pp->promiser);

if (rex.failed)
   {
   return false;
   }

for (ip = file_start; ip != file_end; ip=ip->next)
   {
   if (ip->name == NULL)
      {
      continue;
      }

   if (!RegExMatchFullString(rex,ip->name))
      {
      continue;
      }
   else
      {
      Verbose(" - Matched line (%s)\n",ip->name);
      }

   if (!BlockTextMatch(a.column.column_separator,ip->name,&s,&e))
      {
      cfPS(cf_error,CF_INTERPT,"",pp,a,"Column edit - no columns found by promised pattern %s",a.column.column_separator);
      return false;
      }

   strncpy(separator,ip->name+s,e-s);
   
   columns = SplitRegexAsRList(ip->name,a.column.column_separator,CF_INFINITY,a.column.blanks_ok);
   retval = EditLineByColumn(&columns,a,pp);

   if (retval)
      {
      free(ip->name);
      ip->name = Rlist2String(columns,separator);
      }
   
   DeleteRlist(columns);
   }

return retval;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

int InsertMissingLineAtLocation(char *newline,struct Item **start,struct Item *location,struct Item *prev,struct Attributes a,struct Promise *pp)

/* Check line neighbourhood in whole file to avoid edge effects */
    
{
if (prev == CF_UNDEFINED_ITEM) /* Insert at first line */
   {
   if (a.location.before_after == cfe_before)
      {
      if (*start == NULL)
         {
         if (DONTDO || a.transaction.action == cfa_warn)
            {
            cfPS(cf_verbose,CF_WARN,"",pp,a," -> Need to insert the promised line \"%s\" - but only a warning was promised",newline);
            return true;
            }
         else
            {
            PrependItemList(start,newline);
            (pp->edcontext->num_edits)++;
            cfPS(cf_verbose,CF_CHG,"",pp,a," -> Inserting the promised line \"%s\"",newline);
            return true;
            }

         }
      
      if (strcmp((*start)->name,newline) != 0)
         {
         if (DONTDO || a.transaction.action == cfa_warn)
            {
            cfPS(cf_verbose,CF_WARN,"",pp,a," -> Need to prepend the promised line \"%s\" - but only a warning was promised",newline);
            return true;
            }
         else
            {
            PrependItemList(start,newline);
            (pp->edcontext->num_edits)++;
            cfPS(cf_verbose,CF_CHG,"",pp,a," -> Prepending the promised line \"%s\"",newline);
            return true;
            }
         }
      else
         {
         cfPS(cf_verbose,CF_NOP,"",pp,a," -> Promised line \"%s\" exists at start of file (promise kept)",newline);
         return false;
         }
      }
   }
 
if (a.location.before_after == cfe_before)
   {
   if (NeighbourItemMatches(*start,location,newline,cfe_before))
      {
      cfPS(cf_verbose,CF_NOP,"",pp,a," -> Promised line \"%s\" exists before locator (promise kept)",newline);
      return false;
      }
   else
      {
      if (DONTDO || a.transaction.action == cfa_warn)
         {
         cfPS(cf_verbose,CF_WARN,"",pp,a," -> Need to insert line \"%s\" but only a warning was promised",newline);
         return true;
         }
      else
         {
         InsertAfter(start,prev,newline);
         cfPS(cf_verbose,CF_CHG,"",pp,a," -> Inserting the promised line \"%s\" before locator",newline);
         return true;
         }
      }
   }
else
   {
   if (NeighbourItemMatches(*start,location,newline,cfe_after))
      {
      cfPS(cf_verbose,CF_NOP,"",pp,a," -> Promised line \"%s\" exists after locator (promise kept)",newline);
      return false;
      }
   else
      {
      if (DONTDO || a.transaction.action == cfa_warn)
         {
         cfPS(cf_verbose,CF_WARN,"",pp,a," -> Need to insert line \"%s\" but only a warning was promised",newline);
         return true;
         }
      else
         {
         InsertAfter(start,location,newline);
         cfPS(cf_verbose,CF_CHG,"",pp,a," -> Inserting the promised line \"%s\" after locator",newline);
         return true;
         }
      }
   }
}

/***************************************************************************/

int EditLineByColumn(struct Rlist **columns,struct Attributes a,struct Promise *pp)

{ struct Rlist *rp,*this_column;
  char sep[CF_MAXVARSIZE];
  int i,count = 0,retval = false;

/* Now break up the line into a list - not we never remove an item/column */
 
for (rp = *columns; rp != NULL; rp=rp->next)
    {
    count++;
    
    if (count == a.column.select_column)
       {
       Verbose(" -> Stopped at column %d\n",count);
       break;
       }
    }

if (a.column.select_column > count)
   {
   if (!a.column.extend_columns)
      {
      cfPS(cf_error,CF_INTERPT,"",pp,a," !! The file has only %d columns, but there is a promise for column %d",count,a.column.select_column);
      return false;
      }
   else
      {
      for (i = 0; i < (a.column.select_column - count); i++)
         {
         AppendRScalar(columns,strdup(""),CF_SCALAR);
         }

      count = 0;
      
      for (rp = *columns; rp != NULL; rp=rp->next)
         {
         count++;         
         if (count == a.column.select_column)
            {
            Verbose(" -> Stopped at column %d\n",count);
            break;
            }
         }
      }
   }

if (a.column.value_separator != '\0')
   {
   /* internal separator, single char so split again */

   this_column = SplitStringAsRList(rp->item,a.column.value_separator);
   retval = EditColumn(&this_column,a,pp);

   if (retval)
      {
      if (DONTDO || a.transaction.action == cfa_warn)
         {
         cfPS(cf_error,CF_NOP,"",pp,a," -> Need to edit column but only warning promised");
         retval = false;
         }
      else
         {
         cfPS(cf_inform,CF_CHG,"",pp,a," -> Edited column inside file object");
         free(rp->item);
         sep[0] = a.column.value_separator;
         sep[1] = '\0';
         rp->item = Rlist2String(this_column,sep);
         }
      }
   
   DeleteRlist(this_column);
   return retval;
   }
else
   {
   /* No separator, so we set the whole field to the value */

   if (a.column.column_operation && strcmp(a.column.column_operation,"delete") == 0)
      {
      if (DONTDO)
         {
         cfPS(cf_error,CF_NOP,"",pp,a," -> Need to delete column field value %s",rp->item);
         return false;
         }
      else
         {
         cfPS(cf_inform,CF_CHG,"",pp,a," -> Deleting column field value %s",rp->item);
         free(rp->item);
         rp->item = strdup("");
         return true;
         }
      }
   else
      {
      if (DONTDO)
         {
         cfPS(cf_error,CF_NOP,"",pp,a," -> Need to set column field value %s to %s",rp->item,a.column.column_value);
         return false;
         }
      else
         {
         cfPS(cf_inform,CF_CHG,"",pp,a," -> Setting whole column field value %s to %s",rp->item,a.column.column_value);
         free(rp->item);
         rp->item = strdup(a.column.column_value);
         return true;
         }
      }
   }

return false;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

int EditColumn(struct Rlist **columns,struct Attributes a,struct Promise *pp)

{ struct Rlist *rp, *found;
 int count = 0,retval = false;

if (a.column.column_operation && strcmp(a.column.column_operation,"delete") == 0)
   {
   if (found = KeyInRlist(*columns,a.column.column_value))
      {
      CfOut(cf_inform,""," -> Deleting column field sub-value %s",a.column.column_value);
      DeleteRlistEntry(columns,found);
      return true;
      }
   else
      {
      return false;
      }
   }

if (a.column.column_operation && strcmp(a.column.column_operation,"set") == 0)
   {
   if (RlistLen(*columns) == 1)
      {
      if (strcmp((*columns)->item,a.column.column_value) == 0)
         {
         Verbose(" -> Column sub-value set as promised\n");
         return false;
         }
      }
   
   CfOut(cf_inform,""," -> Setting column field sub-value %s",a.column.column_value);
   DeleteRlist(*columns);
   *columns = NULL;
   IdempPrependRScalar(columns,a.column.column_value,CF_SCALAR);

   return true;
   }

if (a.column.column_operation && strcmp(a.column.column_operation,"prepend") == 0)
   {
   if (IdempPrependRScalar(columns,a.column.column_value,CF_SCALAR))
      {
      CfOut(cf_inform,""," -> Prepending column field sub-value %s",a.column.column_value);
      return true;
      }
   else
      {
      return false;
      }
   }

if (a.column.column_operation && strcmp(a.column.column_operation,"alphanum") == 0)
   {
   if (IdempPrependRScalar(columns,a.column.column_value,CF_SCALAR))
      {
      retval = true;
      }
   
   rp = AlphaSortRListNames(*columns);
   *columns = rp;
   return retval;
   }

/* default operation is append */

if (IdempAppendRScalar(columns,a.column.column_value,CF_SCALAR))
   {
   return true;
   }
else
   {
   return false;
   }

return false;
}

