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
/* File: files_edit_operators.c                                              */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

enum editlinetypesequence
   {
   elp_vars,
   elp_classes,
   elp_delete,
   elp_columns,
   elp_insert,
   elp_replace,
   elp_reports,
   elp_none
   };

char *EDITLINETYPESEQUENCE[] =
   {
   "vars",
   "classes",
   "delete_lines",
   "field_edits",
   "insert_lines",
   "replace_patterns",
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
  int pass;
  char *bp_stack = THIS_BUNDLE;
      
NewScope("edit");
NewScalar("edit","filename",filename,cf_str);
         
/* Reset the done state for every call here, since bundle is reusable */

for (pass = 1; pass < CF_DONEPASSES; pass++)
   {
   for (type = 0; EDITLINETYPESEQUENCE[type] != NULL; type++)
      {
      EditClassBanner(type);
      
      if ((sp = GetSubTypeForBundle(EDITLINETYPESEQUENCE[type],bp)) == NULL)
         {
         continue;      
         }
      
      BannerSubSubType(bp->name,sp->name);
      THIS_BUNDLE = bp->name;
      SetScope(bp->name);
            
      for (pp = sp->promiselist; pp != NULL; pp=pp->next)
         {
         pp->edcontext = parentp->edcontext;
         pp->this_server = filename;
         pp->donep = &(pp->done);
         
         ExpandPromise(cf_agent,bp->name,pp,KeepEditLinePromise);
         
         if (Abort())
            {
            THIS_BUNDLE = bp_stack;
            return false;
            }         
         }
      }
   }

/* Reset the promises after 3 passes since edit bundles are reusable */

for (type = 0; EDITLINETYPESEQUENCE[type] != NULL; type++)
   {
   if ((sp = GetSubTypeForBundle(EDITLINETYPESEQUENCE[type],bp)) == NULL)
      {
      continue;      
      }
   
   for (pp = sp->promiselist; pp != NULL; pp=pp->next)
      {
      pp->donep = false;
      }
   }

DeleteScope("edit");

THIS_BUNDLE = bp_stack;
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

CfOut(cf_verbose,"","     ??  Private class context\n");

for (ip = VADDCLASSES; ip != NULL; ip=ip->next)
   {
   CfOut(cf_verbose,"","     ??       %s\n",ip->name);
   }

CfOut(cf_verbose,"","\n");
}

/***************************************************************************/

void KeepEditLinePromise(struct Promise *pp)

{ char *sp = NULL;
 
if (!IsDefinedClass(pp->classes))
   {
   CfOut(cf_verbose,"","\n");
   CfOut(cf_verbose,"","   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
   CfOut(cf_verbose,"","   Skipping whole next edit promise, as context %s is not relevant\n",pp->classes);
   CfOut(cf_verbose,"","   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
   return;
   }

if (pp->done)
   {
   return;
   }

if (VarClassExcluded(pp,&sp))
   {
   CfOut(cf_verbose,"","\n");
   CfOut(cf_verbose,"",". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   CfOut(cf_verbose,"","Skipping whole next edit promise (%s), as var-context %s is not relevant\n",pp->promiser,sp);
   CfOut(cf_verbose,"",". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   return;
   }

PromiseBanner(pp);

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

if (strcmp("field_edits",pp->agentsubtype) == 0)
   {
   VerifyColumnEdits(pp);
   return;
   }

if (strcmp("insert_lines",pp->agentsubtype) == 0)
   {
   VerifyLineInsertions(pp);
   return;
   }

if (strcmp("replace_patterns",pp->agentsubtype) == 0)
   {
   VerifyPatterns(pp);
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
  char lockname[CF_BUFSIZE];
  struct CfLock thislock;

/* *(pp->donep) = true;	*/
	 
a = GetDeletionAttributes(pp);

snprintf(lockname,CF_BUFSIZE-1,"filedeletion-%s-%s",pp->promiser,pp->this_server);
thislock = AcquireLock(lockname,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
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
   cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised line deletion (%s) could not select an edit region in %s",pp->promiser,pp->this_server);
   YieldCurrentLock(thislock);
   return;
   }

if (DeletePromisedLinesMatching(start,begin_ptr,end_ptr,a,pp))
   {
   (pp->edcontext->num_edits)++;
   }

YieldCurrentLock(thislock);
}

/***************************************************************************/

void VerifyColumnEdits(struct Promise *pp)

{ struct Item **start = &(pp->edcontext->file_start), *match, *prev;
  struct Attributes a;
  struct Item *begin_ptr,*end_ptr;
  char lockname[CF_BUFSIZE];
  struct CfLock thislock;

/* *(pp->donep) = true; */

a = GetColumnAttributes(pp);

snprintf(lockname,CF_BUFSIZE-1,"filecolumnedits-%s-%s",pp->promiser,pp->this_server);
thislock = AcquireLock(lockname,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
   return;
   }

if (a.column.column_separator == NULL)
   {
   cfPS(cf_error,CF_WARN,"",pp,a,"No field_separator in promise to edit by column for %s",pp->promiser);
   PromiseRef(cf_error,pp);
   YieldCurrentLock(thislock);
   return;
   }

if (a.column.select_column <= 0)
   {
   cfPS(cf_error,CF_WARN,"",pp,a,"No select_field in promise to edit %s",pp->promiser);
   PromiseRef(cf_error,pp);
   YieldCurrentLock(thislock);
   return;   
   }

if (!a.column.column_value)
   {
   cfPS(cf_error,CF_WARN,"",pp,a,"No field_value is promised to column_edit %s",pp->promiser);
   PromiseRef(cf_error,pp);
   YieldCurrentLock(thislock);
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
   cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised column edit (%s) could not select an edit region in %s",pp->promiser,pp->this_server);
   YieldCurrentLock(thislock);
   return;
   }

/* locate and split line */

if (EditColumns(begin_ptr,end_ptr,a,pp))
   {
   (pp->edcontext->num_edits)++;
   }

YieldCurrentLock(thislock);
}

/***************************************************************************/

void VerifyPatterns(struct Promise *pp)

{ struct Item **start = &(pp->edcontext->file_start), *match, *prev;
  struct Attributes a;
  struct Item *begin_ptr,*end_ptr;
  char lockname[CF_BUFSIZE];
  struct CfLock thislock;

/* *(pp->donep) = true; */

CfOut(cf_verbose,""," -> Looking at pattern %s\n",pp->promiser);

/* Are we working in a restricted region? */

a = GetReplaceAttributes(pp);

snprintf(lockname,CF_BUFSIZE-1,"filepatterns-%s-%s",pp->promiser,pp->this_server);
thislock = AcquireLock(lockname,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
   return;
   }

if (!a.replace.replace_value)
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised pattern replace (%s) had no replacement string",pp->promiser);
   YieldCurrentLock(thislock);
   return;
   }

if (!a.haveregion)
   {
   begin_ptr = *start;
   end_ptr = NULL; //EndOfList(*start);
   }
else if (!SelectRegion(*start,&begin_ptr,&end_ptr,a,pp))
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised pattern replace (%s) could not select an edit region in %s",pp->promiser,pp->this_server);
   YieldCurrentLock(thislock);
   return;
   }

/* Make sure back references are expanded */

if (ReplacePatterns(begin_ptr,end_ptr,a,pp))
   {
   (pp->edcontext->num_edits)++;
   }

YieldCurrentLock(thislock);
}

/***************************************************************************/

void VerifyLineInsertions(struct Promise *pp)

{ struct Item **start = &(pp->edcontext->file_start), *match, *prev;
  struct Item *begin_ptr,*end_ptr;
  struct Attributes a;
  char lockname[CF_BUFSIZE];
  struct CfLock thislock;

/* *(pp->donep) = true; */

a = GetInsertionAttributes(pp);

if (!SanityCheckInsertions(a))
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised line insertion (%s) breaks its own promises",pp->promiser);
   return;
   }

snprintf(lockname,CF_BUFSIZE-1,"filepatterns-%s-%s",pp->promiser,pp->this_server);
thislock = AcquireLock(lockname,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
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
   cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised line insertion (%s) could not select an edit region in %s",pp->promiser,pp->this_server);
   YieldCurrentLock(thislock);
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
      cfPS(cf_error,CF_INTERPT,"",pp,a," !! The promised line insertion (%s) could not select a locator matching regex \"%s\" in %s",pp->promiser,a.location.line_matching,pp->this_server);
      YieldCurrentLock(thislock);
      return;
      }

   if (InsertMissingLinesAtLocation(start,begin_ptr,end_ptr,match,prev,a,pp))
      {
      (pp->edcontext->num_edits)++;
      }
   }

YieldCurrentLock(thislock);
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

   if (a.region.select_end && beg != CF_UNDEFINED_ITEM)
      {
      if (end == CF_UNDEFINED_ITEM && FullTextMatch(a.region.select_end,ip->name))
         {
         end = ip;
         break;
         }
      }

   if (beg != CF_UNDEFINED_ITEM && end != CF_UNDEFINED_ITEM)
      {
      break;
      }
   }

if (beg == CF_UNDEFINED_ITEM && a.region.select_start)
   {
   cfPS(cf_verbose,CF_INTERPT,"",pp,a," !! The promised start pattern (%s) was not found when selecting edit region in %s",a.region.select_start,pp->this_server);
   return false;
   }

if (end == CF_UNDEFINED_ITEM && a.region.select_end)
   {
   cfPS(cf_verbose,CF_INTERPT,"",pp,a," !! The promised end pattern (%s) was not found when selecting edit region in %s, abort edit",a.region.select_end,pp->this_server);
   end = NULL; /* End of file is null ptr if nothing else specified */
   return false;
   }

if (end == CF_UNDEFINED_ITEM)
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
   cfPS(cf_verbose,CF_NOP,"",pp,a," -> Promised line \"%s\" exists within selected region of %s (promise kept)",pp->promiser,pp->this_server);
   return false;
   }

if (*start == NULL)
   {
   return InsertMissingLinesAtLocation(start,begin_ptr,end_ptr,*start,prev,a,pp);
   }

if (a.location.before_after == cfe_before)
   {
   for (ip = *start; ip != NULL; ip=ip->next)
      {
      if (ip == begin_ptr)
         {
         return InsertMissingLinesAtLocation(start,begin_ptr,end_ptr,ip,prev,a,pp);
         }
      
      prev = ip;
      }   
   }

if (a.location.before_after == cfe_after)
   {
   for (ip = *start; ip != NULL; ip=ip->next)
      {
      if (ip->next != NULL && ip->next == end_ptr)
         {
         return InsertMissingLinesAtLocation(start,begin_ptr,end_ptr,ip,prev,a,pp);
         }

      if (ip->next == NULL)
         {
         return InsertMissingLinesAtLocation(start,begin_ptr,end_ptr,ip,prev,a,pp);
         }
      
      prev = ip;
      }   
   }

return false;
}

/***************************************************************************/

int InsertMissingLinesAtLocation(struct Item **start,struct Item *begin_ptr,struct Item *end_ptr,struct Item *location,struct Item *prev,struct Attributes a,struct Promise *pp)

{ FILE *fin;
  char buf[CF_BUFSIZE],exp[CF_EXPANDSIZE];
  struct Item *loc = NULL;
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

      if (feof(fin) && strlen(buf) == 0)
         {
         break;
         }
      
      if (a.expandvars)
         {
         ExpandScalar(buf,exp);
         }
      else
         {
         strcpy(exp,buf);
         }

      if (!SelectLine(exp,a,pp))
         {
         continue;
         }
      
      if (IsItemInRegion(exp,begin_ptr,end_ptr))
         {
         cfPS(cf_verbose,CF_NOP,"",pp,a," -> Promised file line \"%s\" exists within file %s (promise kept)",exp,pp->this_server);
         continue;
         }

      retval |= InsertMissingLineAtLocation(exp,start,loc,prev,a,pp);

      if (prev && prev != CF_UNDEFINED_ITEM)
         {
         prev = prev->next;
         }

      if (loc)
         {
         loc = loc->next;
         }
      }
   
   fclose(fin);
   return retval;
   }
else
   {
   if (strchr(pp->promiser,'\n') != NULL) /* Multi-line string */
      {
      char *sp;
      loc = location;
      
      for (sp = pp->promiser; sp <= pp->promiser+strlen(pp->promiser); sp++)
         {
         memset(buf,0,CF_BUFSIZE);
         sscanf(sp,"%[^\n]",buf);
         sp += strlen(buf);
         
         if (!SelectLine(buf,a,pp))
            {
            continue;
            }
         
         if (IsItemInRegion(buf,begin_ptr,end_ptr))
            {
            cfPS(cf_verbose,CF_NOP,"",pp,a," -> Promised file line \"%s\" exists within file %s (promise kept)",buf,pp->this_server);
            continue;
            }
         
         retval |= InsertMissingLineAtLocation(buf,start,loc,prev,a,pp);
         
         if (prev && prev != CF_UNDEFINED_ITEM)
            {
            prev = prev->next;
            }
         
         if (loc)
            {
            loc = loc->next;
            }
         }

      return retval;
      }
   else
      {
      return InsertMissingLineAtLocation(pp->promiser,start,location,prev,a,pp);
      }
   }
}

/***************************************************************************/
    
int DeletePromisedLinesMatching(struct Item **start,struct Item *begin,struct Item *end,struct Attributes a,struct Promise *pp)

{ struct Item *ip,*np,*lp;
 int in_region = false, retval = false, match, noedits = true;

if (start == NULL)
   {
   return false;
   }
  
for (ip = *start; ip != NULL; ip = np)
   {
   if (ip == begin)
      {
      in_region = true;
      }

   if (!in_region)
      {
      np = ip->next;
      continue;
      }

   if (a.not_matching)
      {
      match = !FullTextMatch(pp->promiser,ip->name);
      }
   else
      {
      match = FullTextMatch(pp->promiser,ip->name);
      }

   if (!SelectLine(ip->name,a,pp))
      {
      np = ip->next;
      continue;
      }
         
   if (in_region && match)
      {
      if (a.transaction.action == cfa_warn)
         {
         cfPS(cf_error,CF_WARN,"",pp,a," -> Need to delete line \"%s\" from %s - but only a warning was promised",ip->name,pp->this_server);
         np = ip->next;
         noedits = false;
         }
      else
         {
         cfPS(cf_verbose,CF_CHG,"",pp,a," -> Deleting the promised line \"%s\" from %s",ip->name,pp->this_server);
         retval = true;
         noedits = false;
         
         if (ip->name != NULL)
            {
            free(ip->name);
            }
         
         np = ip->next;
         free((char *)ip);
         
         lp = ip;
         
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

         if (ip == end)
            {
            in_region = false;
            break;
            }
         
         (pp->edcontext->num_edits)++;
         }
      }
   else
      {
      np = ip->next;
      }

   if (ip == end)
      {
      in_region = false;
      break;
      }
   }

if (noedits)
   {
   cfPS(cf_verbose,CF_NOP,"",pp,a," -> No need to delete lines from %s, ok",pp->this_server);
   }

return retval;
}

/********************************************************************/

int ReplacePatterns(struct Item *file_start,struct Item *file_end,struct Attributes a,struct Promise *pp)

{ char *sp, *start = NULL,*end,replace[CF_EXPANDSIZE],line_buff[CF_EXPANDSIZE];
  int match_len,start_off,end_off,once_only = false,retval = false;
  struct CfRegEx rex;
  struct Item *ip;
  int notfound = true;

if (a.replace.occurrences && (strcmp(a.replace.occurrences,"first") == 0))
   {
   CfOut(cf_inform,"","WARNING! Setting replace-occurrences policy to \"first\" is not convergent");
   once_only = true;
   }

for (ip = file_start; ip != file_end; ip=ip->next)
   {
   if (ip->name == NULL)
      {
      continue;
      }

   if (!BlockTextMatch(pp->promiser,ip->name,&start_off,&end_off))
      {
      continue;
      }
   else
      {
      match_len = end_off - start_off;
      ExpandScalar(a.replace.replace_value,replace);
      }

   CfOut(cf_verbose,""," -> Verifying replacement of \"%s\" with \"%s\"\n",pp->promiser,replace);  

   memset(line_buff,0,CF_BUFSIZE);
   sp = ip->name;   

   while (*sp != '\0')
      {
      strncat(line_buff,sp,start_off);
      sp += start_off;
      strncat(line_buff,replace,CF_BUFSIZE);
      sp += match_len;

      CfOut(cf_verbose,""," -> << \"%s\"\n",ip->name);
      CfOut(cf_verbose,""," -> >> \"%s\"\n",line_buff);

      notfound = false;
      
      if (once_only)
         {
         CfOut(cf_verbose,""," -> Replace first occurrence only (warning, this is not a convergent policy)");
         strncat(line_buff,ip->name+end_off,CF_BUFSIZE);
         break;
         }
      else
         {
         if (!BlockTextMatch(pp->promiser,sp,&start_off,&end_off))
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

   if (BlockTextMatch(pp->promiser,line_buff,&start_off,&end_off))
      {
      if (start_off == 0 && end_off == strlen(line_buff))
         {
         ExpandScalar(a.replace.replace_value,replace);
         
         if (strcmp(replace,line_buff) != 0)
            {      
            cfPS(cf_error,CF_INTERPT,"",pp,a," -> Promised replacement \"%s\" on line \"%s\" for pattern \"%s\" is not convergent while editing %s",line_buff,ip->name,pp->promiser,pp->this_server);
            CfOut(cf_error,"","Because the regular expression \"%s\" still matches the replacement string \"%s\"",pp->promiser,line_buff);
            PromiseRef(cf_error,pp);
            continue;
            }
         }
      }

   if (a.transaction.action == cfa_warn)
      {
      cfPS(cf_verbose,CF_WARN,"",pp,a," -> Need to replace line \"%s\" in %s - but only a warning was promised",pp->promiser,pp->this_server);
      continue;
      }
   else
      {
      free(ip->name);
      ip->name = strdup(line_buff);
      cfPS(cf_verbose,CF_CHG,"",pp,a," -> Replaced pattern \"%s\" in %s",pp->promiser,pp->this_server);
      (pp->edcontext->num_edits)++;
      retval = true;
      }

   if (once_only)
      {
      break;
      }
   }

if (notfound)
   {
   cfPS(cf_verbose,CF_NOP,"",pp,a," -> No pattern \"%s\" in %s",pp->promiser,pp->this_server);
   }

DeleteScope("match");
NewScope("match");
return retval;
}

/********************************************************************/

int EditColumns(struct Item *file_start,struct Item *file_end,struct Attributes a,struct Promise *pp)

{ char separator[CF_MAXVARSIZE]; 
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

   if (!FullTextMatch(pp->promiser,ip->name))
      {
      continue;
      }
   else
      {
      CfOut(cf_verbose,""," - Matched line (%s)\n",ip->name);
      }

   if (!BlockTextMatch(a.column.column_separator,ip->name,&s,&e))
      {
      cfPS(cf_error,CF_INTERPT,"",pp,a,"Field edit - no fields found by promised pattern %s in %s",a.column.column_separator,pp->this_server);
      return false;
      }

   if (e-s > CF_MAXVARSIZE / 2)
      {
      CfOut(cf_error,""," !! Line split criterion matches a huge part of the line -- seems to be in error");
      return false;
      }

   
   strncpy(separator,ip->name+s,e-s);
   separator[e-s]='\0';

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

int SanityCheckInsertions(struct Attributes a)

{ long not = 0;
  long with = 0;
  long ok = true;
  
if (a.line_select.startwith_from_list)
   {
   with++;
   }

if (a.line_select.not_startwith_from_list)
   {
   not++;
   }

if (a.line_select.match_from_list)
   {
   with++;
   }

if (a.line_select.not_match_from_list)
   {
   not++;
   }

if (a.line_select.contains_from_list)
   {
   with++;
   }

if (a.line_select.not_contains_from_list)
   {
   not++;
   }

if (not > 1)
   {
   CfOut(cf_error,"","Line insertion selection promise is meaningless - the alternatives are mutually exclusive (only one is allowed)");
   ok = false;
   }

if (with && not)
   {
   CfOut(cf_error,"","Line insertion selection promise is meaningless - cannot mix positive and negative constraints");
   ok = false;
   }

return ok;
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
         if (a.transaction.action == cfa_warn)
            {
            cfPS(cf_error,CF_WARN,"",pp,a," -> Need to insert the promised line \"%s\" in %s - but only a warning was promised",newline,pp->this_server);
            return true;
            }
         else
            {
            PrependItemList(start,newline);
            (pp->edcontext->num_edits)++;
            cfPS(cf_verbose,CF_CHG,"",pp,a," -> Inserting the promised line \"%s\" into %s",newline,pp->this_server);
            return true;
            }
         }
      
      if (strcmp((*start)->name,newline) != 0)
         {
         if (a.transaction.action == cfa_warn)
            {
            cfPS(cf_error,CF_WARN,"",pp,a," -> Need to prepend the promised line \"%s\" to %s - but only a warning was promised",newline,pp->this_server);
            return true;
            }
         else
            {
            PrependItemList(start,newline);
            (pp->edcontext->num_edits)++;
            cfPS(cf_verbose,CF_CHG,"",pp,a," -> Prepending the promised line \"%s\" to %s",newline,pp->this_server);
            return true;
            }
         }
      else
         {
         cfPS(cf_verbose,CF_NOP,"",pp,a," -> Promised line \"%s\" exists at start of file %s (promise kept)",newline,pp->this_server);
         return false;
         }
      }
   }
 
if (a.location.before_after == cfe_before)
   {
   if (NeighbourItemMatches(*start,location,newline,cfe_before))
      {
      cfPS(cf_verbose,CF_NOP,"",pp,a," -> Promised line \"%s\" exists before locator in (promise kept)",newline);
      return false;
      }
   else
      {
      if (a.transaction.action == cfa_warn)
         {
         cfPS(cf_error,CF_WARN,"",pp,a," -> Need to insert line \"%s\" into %s but only a warning was promised",newline,pp->this_server);
         return true;
         }
      else
         {
         InsertAfter(start,prev,newline);
         (pp->edcontext->num_edits)++;
         cfPS(cf_verbose,CF_CHG,"",pp,a," -> Inserting the promised line \"%s\" into %s before locator",newline,pp->this_server);
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
      if (a.transaction.action == cfa_warn)
         {
         cfPS(cf_error,CF_WARN,"",pp,a," -> Need to insert line \"%s\" in %s but only a warning was promised",newline,pp->this_server);
         return true;
         }
      else
         {
         InsertAfter(start,location,newline);
         cfPS(cf_verbose,CF_CHG,"",pp,a," -> Inserting the promised line \"%s\" into %s after locator",newline,pp->this_server);
         (pp->edcontext->num_edits)++;
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
       CfOut(cf_verbose,""," -> Stopped at field %d\n",count);
       break;
       }
    }

if (a.column.select_column > count)
   {
   if (!a.column.extend_columns)
      {
      cfPS(cf_error,CF_INTERPT,"",pp,a," !! The file %s has only %d fields, but there is a promise for field %d",pp->this_server,count,a.column.select_column);
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
            CfOut(cf_verbose,""," -> Stopped at column/field %d\n",count);
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
      if (a.transaction.action == cfa_warn)
         {
         cfPS(cf_error,CF_WARN,"",pp,a," -> Need to edit field in %s but only warning promised",pp->this_server);
         retval = false;
         }
      else
         {
         cfPS(cf_inform,CF_CHG,"",pp,a," -> Edited field inside file object %s",pp->this_server);
         (pp->edcontext->num_edits)++;
         free(rp->item);
         sep[0] = a.column.value_separator;
         sep[1] = '\0';
         rp->item = Rlist2String(this_column,sep);
         }
      }
   else
      {
      cfPS(cf_verbose,CF_NOP,"",pp,a," -> No need to edit field in %s",pp->this_server);
      }
   
   DeleteRlist(this_column);
   return retval;
   }
else
   {
   /* No separator, so we set the whole field to the value */

   if (a.column.column_operation && strcmp(a.column.column_operation,"delete") == 0)
      {
      if (a.transaction.action == cfa_warn)
         {
         cfPS(cf_error,CF_WARN,"",pp,a," -> Need to delete field field value %s in %s but only a warning was promised",rp->item,pp->this_server);
         return false;
         }
      else
         {
         cfPS(cf_inform,CF_CHG,"",pp,a," -> Deleting column field value %s in %s",rp->item,pp->this_server);
         (pp->edcontext->num_edits)++;
         free(rp->item);
         rp->item = strdup("");
         return true;
         }
      }
   else
      {
      if (a.transaction.action == cfa_warn)
         {
         cfPS(cf_error,CF_WARN,"",pp,a," -> Need to set column field value %s to %s in %s but only a warning was promised",rp->item,a.column.column_value,pp->this_server);
         return false;
         }
      else
         {
         cfPS(cf_inform,CF_CHG,"",pp,a," -> Setting whole column field value %s to %s in %s",rp->item,a.column.column_value,pp->this_server);
         free(rp->item);
         rp->item = strdup(a.column.column_value);
         (pp->edcontext->num_edits)++;
         return true;
         }
      }
   }

cfPS(cf_verbose,CF_NOP,"",pp,a," -> No need to edit column field value %s in %s",a.column.column_value,pp->this_server);

return false;
}

/***************************************************************************/

int SelectLine(char *line,struct Attributes a,struct Promise *pp)

{ struct Rlist *rp,*c;
  int s,e;
  char *selector;

if (c = a.line_select.startwith_from_list)
   {
   for (rp = c; rp != NULL; rp=rp->next)
      {
      selector = (char *)(rp->item);

      if (strncmp(selector,line,strlen(selector)) == 0)
         {
         return true;
         }      
      }

   return false;
   }

if (c = a.line_select.not_startwith_from_list)
   {
   for (rp = c; rp != NULL; rp=rp->next)
      {
      selector = (char *)(rp->item);
      
      if (strncmp(selector,line,strlen(selector)) == 0)
         {
         return false;
         }      
      }

   return true;
   }

if (c = a.line_select.match_from_list)
   {
   for (rp = c; rp != NULL; rp=rp->next)
      {
      selector = (char *)(rp->item);
      
      if (FullTextMatch(selector,line))
         {
         return true;
         }
      }
   
   return false;
   }

if (c = a.line_select.not_match_from_list)
   {
   for (rp = c; rp != NULL; rp=rp->next)
      {
      selector = (char *)(rp->item);
      
      if (FullTextMatch(selector,line))
         {
         return false;
         }
      }

   return true;
   }

if (c = a.line_select.contains_from_list)
   {
   for (rp = c; rp != NULL; rp=rp->next)
      {
      selector = (char *)(rp->item);
      
      if (BlockTextMatch(selector,line,&s,&e))
         {
         return true;
         }            
      }
   
   return false;
   }

if (c = a.line_select.not_contains_from_list)
   {
   for (rp = c; rp != NULL; rp=rp->next)
      {
      selector = (char *)(rp->item);
      
      if (BlockTextMatch(selector,line,&s,&e))
         {
         return false;
         }            
      }

   return true;
   }

return true;
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
      CfOut(cf_inform,""," -> Deleting column field sub-value %s in %s",a.column.column_value,pp->this_server);
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
         CfOut(cf_verbose,""," -> Field sub-value set as promised\n");
         return false;
         }
      }
   
   CfOut(cf_inform,""," -> Setting field sub-value %s in %s",a.column.column_value,pp->this_server);
   DeleteRlist(*columns);
   *columns = NULL;
   IdempPrependRScalar(columns,a.column.column_value,CF_SCALAR);

   return true;
   }

if (a.column.column_operation && strcmp(a.column.column_operation,"prepend") == 0)
   {
   if (IdempPrependRScalar(columns,a.column.column_value,CF_SCALAR))
      {
      CfOut(cf_inform,""," -> Prepending field sub-value %s in %s",a.column.column_value,pp->this_server);
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

