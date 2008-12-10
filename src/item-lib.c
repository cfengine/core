
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
/* File: item-lib.c                                                          */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************************/

struct Item *EndOfList(struct Item *start)

{ struct Item *ip, *prev = CF_UNDEFINED_ITEM;

for (ip = start; ip !=  NULL; ip=ip->next)
   {
   prev = ip;
   }

return prev;
}

/*********************************************************************/

int IsItemInRegion(char *item,struct Item *begin_ptr,struct Item *end_ptr)

{ struct Item *ip;
 
for (ip = begin_ptr; (ip != end_ptr && ip != NULL); ip = ip->next)
   {
   if (strcmp(ip->name,item) == 0)
      {
      return true;
      }
   }

return false;
}

/*********************************************************************/

void AppendItemList(struct Item **liststart,char *itemstring)

{ struct Item *ip, *lp;

if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL)
   {
   CfOut(cf_error,"malloc","Failed to alloc in AppendItemList");
   FatalError("");
   }

if ((ip->name = strdup(itemstring)) == NULL)
   {
   CfOut(cf_error,"strdup","Failed to alloc in AppendItemList");
   FatalError("");
   }

if (*liststart == NULL)
   {
   *liststart = ip;
   }
else
   {
   for (lp = *liststart; lp->next != NULL; lp=lp->next)
      {
      }

   lp->next = ip;
   }

ip->next = NULL;
ip->counter = 0;
ip->classes = NULL; /* unused now */
}

/*********************************************************************/

void PrependItemList(struct Item **liststart,char *itemstring)

{ struct Item *ip;

if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL)
   {
   CfOut(cferror,"malloc","Memory failure in Prepend");
   FatalError("");
   }

if ((ip->name = strdup(itemstring)) == NULL)
   {
   CfOut(cferror,"malloc","Memory failure in Prepend");
   FatalError("");
   }

ip->next = *liststart;
ip->counter = 0;
*liststart = ip;
ip->classes = NULL; /* unused */
}

/***************************************************************************/

int SelectItemMatching(char *regex,struct Item *begin_ptr,struct Item *end_ptr,struct Item **match,struct Item **prev,char *fl)

{
*match = CF_UNDEFINED_ITEM;
*prev = CF_UNDEFINED_ITEM;

if (regex == NULL)
   {
   return false;
   }

if (fl && (strcmp(fl,"first") == 0))
   {
   if (SelectNextItemMatching(regex,begin_ptr,end_ptr,match,prev))
      {
      return true;
      }
   }
else
   {
   if (SelectLastItemMatching(regex,begin_ptr,end_ptr,match,prev))
      {
      return true;
      }
   }

return false;
}

/*********************************************************************/ 

int SelectNextItemMatching(char *regexp,struct Item *begin,struct Item *end,struct Item **match,struct Item **prev) 

{ struct Item *ip,*ip_prev = CF_UNDEFINED_ITEM;
  struct CfRegEx rex;

*match = CF_UNDEFINED_ITEM;
*prev = CF_UNDEFINED_ITEM;

rex = CompileRegExp(regexp);

if (rex.failed)
   {
   return false;
   }

for (ip = begin; ip != end; ip=ip->next)
   {
   if (ip->name == NULL)
      {
      continue;
      }

   if (RegExMatchFullString(rex,ip->name))
      {
      *match = ip;
      *prev = ip_prev;
      return true;
      }
   
   ip_prev = ip;
   }

return false;
}

/*********************************************************************/ 

int SelectLastItemMatching(char *regexp,struct Item *begin,struct Item *end,struct Item **match,struct Item **prev) 

{ struct Item *ip,*ip_last = NULL,*ip_prev = CF_UNDEFINED_ITEM;;
  struct CfRegEx rex;
 
*match = CF_UNDEFINED_ITEM;
*prev = CF_UNDEFINED_ITEM;

rex = CompileRegExp(regexp);

if (rex.failed)
   {
   return false;
   }

for (ip = begin; ip != end; ip=ip->next)
   {
   if (ip->name == NULL)
      {
      continue;
      }

   if (RegExMatchFullString(rex,ip->name))
      {
      *prev = ip_prev;
      ip_last = ip;
      }

   ip_prev = ip;
   }

*match = ip_last;
return false;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void InsertAfter(struct Item **filestart,struct Item *ptr,char *string)

{ struct Item *ip;
  char *sp;

if (*filestart == NULL || ptr == *filestart || ptr == CF_UNDEFINED_ITEM)
   {
   PrependItemList(filestart,string);
   return;
   }

if (ptr == NULL)
   {
   AppendItemList(filestart,string);
   return;
   }

if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL)
   {
   CfOut(cf_error,"","Can't allocate memory in InsertAfter()");
   FatalError("");
   }

ip->next = ptr->next;
ptr->next = ip;
ip->name = strdup(string);
ip->classes = NULL;
}

/*********************************************************************/

int NeighbourItemMatches(struct Item *file_start,struct Item *location,char *string,enum cfeditorder pos)

{ struct Item *ip;

/* Look for a line matching proposed insert before or after location */
 
for (ip = file_start; ip != NULL; ip = ip->next)
   {
   if (pos == cfe_before)
      {
      if (ip->next && ip->next == location)
         {
         if (strcmp(ip->name,string) == 0)
            {
            return true;
            }
         else
            {
            return false;
            }
         }
      }

   if (pos == cfe_after)
      {
      if (ip == location)
         {
         if (ip->next && strcmp(ip->next->name,string) == 0)
            {
            return true;
            }
         else
            {
            return false;
            }
         }   
      }
   }

return false;
}

