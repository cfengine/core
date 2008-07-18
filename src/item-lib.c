
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
/* File: item-lib.c                                                          */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

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

/*********************************************************************/ 

struct Item *SelectNextItemMatching(struct Item *list,char *regexp,struct Item **prev) 

{ struct Item *ip;
  struct CfRegEx rex;
 
rex = CompileRegExp(regexp);

if (rex.failed)
   {
   return NULL;
   }

*prev = NULL;

for (ip = list; ip != NULL; ip=ip->next)
   {
   if (ip->name == NULL)
      {
      continue;
      }

   if (RegExMatchFullString(rex,ip->name))
      {
      return ip;
      }
   
   *prev = ip;
   }

return NULL;
}

/*********************************************************************/ 

struct Item *SelectLastItemMatching(struct Item *list,char *regexp,struct Item **prev) 

{ struct Item *ip,*ip_last = NULL,*ip_prev;
  struct CfRegEx rex;
 
rex = CompileRegExp(regexp);

if (rex.failed)
   {
   return NULL;
   }

*prev = NULL;

for (ip = list; ip != NULL; ip=ip->next)
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

return ip_last;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void InsertAfter(struct Item **filestart,struct Item *ptr,char *string)

{ struct Item *ip;
  char *sp;

if (*filestart == NULL || ptr == *filestart)
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

int NeighbourItemMatches(struct Item *filestart,struct Item *location,char *string,enum cfeditorder pos)

{ struct Item *ip;

/* Look for a line matching proposed insert before or after location */
 
for (ip = filestart; ip != NULL; ip = ip->next)
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

