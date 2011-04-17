
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
/* File: item-lib.c                                                          */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************************/

int ItemListSize(struct Item *list)

{ int size = 0;
  struct Item *ip;
 
for (ip = list; ip != NULL; ip=ip->next)
   {
   if (ip->name)
      {
      size += strlen(ip->name);
      }
   }

return size;
}

/*********************************************************************/

void PurgeItemList(struct Item **list,char *name)

{ struct Item *ip,*copy = NULL;
  struct stat sb;

CopyList(&copy,*list);
  
for (ip = copy; ip != NULL; ip=ip->next)
   {
   if (cfstat(ip->name,&sb) == -1)
      {
      CfOut(cf_verbose,""," -> Purging file \"%s\" from %s list as it no longer exists",ip->name,name);
      DeleteItemLiteral(list,ip->name);
      }
   }

DeleteItemList(copy);
}

/*********************************************************************/

struct Item *ReturnItemIn(struct Item *list,char *item)

{ struct Item *ptr; 

if ((item == NULL) || (strlen(item) == 0))
   {
   return NULL;
   }
 
for (ptr = list; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->name,item) == 0)
      {
      return ptr;
      }
   }
 
return NULL;
}

/*********************************************************************/

struct Item *ReturnItemInClass(struct Item *list,char *item,char *classes)

{ struct Item *ptr; 

if ((item == NULL) || (strlen(item) == 0))
   {
   return NULL;
   }
 
for (ptr = list; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->name,item) == 0 && strcmp(ptr->classes,classes) == 0)
      {
      return ptr;
      }
   }
 
return NULL;
}

/*********************************************************************/

int GetItemIndex(struct Item *list,char *item)
/*
 * Returns index of first occurence of item.
 */
{ struct Item *ptr; 
  int i = 0;

if ((item == NULL) || (strlen(item) == 0))
   {
   return -1;
   }
 
for (ptr = list; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->name,item) == 0)
      {
      return i;
      }

   i++;
   }
 
return -1;
}

/*********************************************************************/

int IsItemIn(struct Item *list,const char *item)

{ struct Item *ptr; 

if ((item == NULL) || (strlen(item) == 0))
   {
   return true;
   }
 
for (ptr = list; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->name,item) == 0)
      {
      return(true);
      }
   }
 
return(false);
}

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

int IsItemInRegion(char *item,struct Item *begin_ptr,struct Item *end_ptr,struct Attributes a,struct Promise *pp)

{ struct Item *ip;
 
for (ip = begin_ptr; (ip != end_ptr && ip != NULL); ip = ip->next)
   {
   if (MatchPolicy(item,ip->name,a,pp))
      {
      return true;
      }
   }

return false;
}

/*********************************************************************/

struct Item *IdempPrependItem(struct Item **liststart,char *itemstring,char *classes)

{ struct Item *ip;

ip = ReturnItemIn(*liststart,itemstring);

if (ip)
   {
   return ip;
   }

PrependItem(liststart,itemstring,classes);
return *liststart;
}

/*********************************************************************/

struct Item *IdempPrependItemClass(struct Item **liststart,char *itemstring,char *classes)

{ struct Item *ip;

ip = ReturnItemInClass(*liststart,itemstring,classes);

if (ip)  // already exists
   {
   return ip;
   }

PrependItem(liststart,itemstring,classes);
return *liststart;
}


/*********************************************************************/

void IdempItemCount(struct Item **liststart,char *itemstring,char *classes)

{ struct Item *ip;
 
if ((ip = ReturnItemIn(*liststart,itemstring)))
   {
   ip->counter++;
   }
else
   {
   PrependItem(liststart,itemstring,classes);
   }

// counter+1 is the histogram of occurrences
}

/*********************************************************************/

void IdempAppendItem(struct Item **liststart,char *itemstring,char *classes)

{
if (!IsItemIn(*liststart,itemstring))
   {
   AppendItem(liststart,itemstring,classes);
   }
}

/*********************************************************************/

struct Item *PrependItem(struct Item **liststart,char *itemstring,char *classes)

{ struct Item *ip;
  char *sp,*spe = NULL;

if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL)
   {
   FatalError("memory allocation in prepend item");
   }

if ((sp = malloc(strlen(itemstring)+2)) == NULL)
   {
   FatalError("memory allocation in prepend item");
   }

if ((classes != NULL) && (spe = malloc(strlen(classes)+2)) == NULL)
   {
   FatalError("Memory allocation in prepend item");
   }

strcpy(sp,itemstring);
ip->name = sp;
ip->next = *liststart;
ip->counter = 0;
ip->time = 0;
*liststart = ip;

if (classes != NULL)
   {
   strcpy(spe,classes);
   ip->classes = spe;
   }
else
   {
   ip->classes = NULL;
   }

return *liststart;
}

/*********************************************************************/

void PrependFullItem(struct Item **liststart,char *itemstring,char *classes,int counter,time_t t)

{ struct Item *ip;
  char *sp,*spe = NULL;

if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL)
   {
   FatalError("memory allocation in prepend item");
   }

if ((sp = malloc(strlen(itemstring)+2)) == NULL)
   {
   FatalError("memory allocation in prepend item");
   }

if ((classes != NULL) && (spe = malloc(strlen(classes)+2)) == NULL)
   {
   FatalError("Memory allocation in prepend item");
   }

strcpy(sp,itemstring);
ip->name = sp;
ip->next = *liststart;
ip->counter = counter;
ip->time = t;
*liststart = ip;

if (classes != NULL)
   {
   strcpy(spe,classes);
   ip->classes = spe;
   }
else
   {
   ip->classes = NULL;
   }
}


/*********************************************************************/

void AppendItem(struct Item **liststart,char *itemstring,char *classes)

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

if (classes)
   {
   ip->classes = strdup(classes); /* unused now */
   }
else
   {
   ip->classes = NULL;
   }
}

/*********************************************************************/

void PrependItemList(struct Item **liststart,char *itemstring)

{ struct Item *ip;

if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL)
   {
   CfOut(cf_error,"malloc","Memory failure in Prepend");
   FatalError("");
   }

if ((ip->name = strdup(itemstring)) == NULL)
   {
   CfOut(cf_error,"malloc","Memory failure in Prepend");
   FatalError("");
   }

ip->next = *liststart;
ip->counter = 0;
*liststart = ip;
ip->classes = NULL; /* unused */
}

/*********************************************************************/

int ListLen(struct Item *list)

{ int count = 0;
  struct Item *ip;

Debug("Check ListLen\n");
  
for (ip = list; ip != NULL; ip=ip->next)
   {
   count++;
   }

return count; 
}

/*********************************************************************/

int RawSaveItemList(struct Item *liststart,char *file)

{ struct Item *ip;
  char new[CF_BUFSIZE],backup[CF_BUFSIZE];
  FILE *fp;
  time_t STAMPNOW;
  STAMPNOW = time((time_t *)NULL);

strcpy(new,file);
strcat(new,CF_EDITED);

strcpy(backup,file);
strcat(backup,CF_SAVED);

unlink(new); /* Just in case of races */ 
 
if ((fp = fopen(new,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","Couldn't write file %s\n",new);
   return false;
   }

for (ip = liststart; ip != NULL; ip=ip->next)
   {
   fprintf(fp,"%s\n",ip->name);
   }

if (fclose(fp) == -1)
   {
   CfOut(cf_error,"fclose","Unable to close file while writing");
   return false;
   }

if (cf_rename(new,file) == -1)
   {
   CfOut(cf_inform,"cf_rename","Error while renaming %s\n",file);
   return false;
   }       

return true;
}

/***************************************************************************/

void CopyList (struct Item **dest, struct Item *source)

/* Copy or concat lists */
    
{ struct Item *ip;

if (*dest != NULL)
   {
   FatalError("CopyList - list not initialized");
   }
 
if (source == NULL)
   {
   return;
   }
 
for (ip = source; ip != NULL; ip = ip ->next)
   {
   AppendItem(dest,ip->name,ip->classes);
   }
}

/*********************************************************************/

struct Item *ConcatLists (struct Item *list1,struct Item *list2)

/* Notes: * Refrain from freeing list2 after using ConcatLists
          * list1 must have at least one element in it */

{ struct Item *endOfList1;

if (list1 == NULL)
   {
   FatalError("ConcatLists: first argument must have at least one element");
   }

for (endOfList1=list1; endOfList1->next!=NULL; endOfList1=endOfList1->next)
   {
   }

endOfList1->next = list2;
return list1;
}

/***************************************************************************/
/* Search                                                                  */
/***************************************************************************/

int SelectItemMatching(struct Item *start,char *regex,struct Item *begin_ptr,struct Item *end_ptr,struct Item **match,struct Item **prev,char *fl)

{ struct Item *ip;
 int ret = false;

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
      ret = true;
      }
   }
else
   {
   if (SelectLastItemMatching(regex,begin_ptr,end_ptr,match,prev))
      {
      ret = true;
      }
   }

if (*match != CF_UNDEFINED_ITEM && *prev == CF_UNDEFINED_ITEM)
   {
   for (ip = start; ip != NULL && ip != *match; ip = ip->next)
      {
      *prev = ip;
      }
   }

return ret;
}

/*********************************************************************/ 

int SelectNextItemMatching(char *regexp,struct Item *begin,struct Item *end,struct Item **match,struct Item **prev) 

{ struct Item *ip,*ip_prev = CF_UNDEFINED_ITEM;

*match = CF_UNDEFINED_ITEM;
*prev = CF_UNDEFINED_ITEM;

for (ip = begin; ip != end; ip=ip->next)
   {
   if (ip->name == NULL)
      {
      continue;
      }

   if (FullTextMatch(regexp,ip->name))
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

{ struct Item *ip,*ip_last = NULL,*ip_prev = CF_UNDEFINED_ITEM;
 
*match = CF_UNDEFINED_ITEM;
*prev = CF_UNDEFINED_ITEM;

for (ip = begin; ip != end; ip=ip->next)
   {
   if (ip->name == NULL)
      {
      continue;
      }
   
   if (FullTextMatch(regexp,ip->name))
      {
      *prev = ip_prev;
      ip_last = ip;
      }

   ip_prev = ip;
   }

if (ip_last)
   {
   *match = ip_last;
   return true;
   }

return false;
}

/*********************************************************************/ 

int MatchRegion(char *chunk,struct Item *location,struct Item *begin,struct Item *end)

{ struct Item *ip = location;
  char *sp,buf[CF_BUFSIZE];

for (sp = chunk; sp <= chunk+strlen(chunk); sp++)
   {
   memset(buf,0,CF_BUFSIZE);
   sscanf(sp,"%[^\n]",buf);
   sp += strlen(buf);

   if (!FullTextMatch(buf,ip->name))
      {
      return false;
      }
   
   if (ip == end)
      {
      return false;
      }

   if (ip->next)
      {
      ip = ip->next;
      }
   else
      {
      if (++sp <= chunk+strlen(chunk))
         {
         return false;
         }
      
      break;
      }
   }

return true;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void InsertAfter(struct Item **filestart,struct Item *ptr,char *string)

{ struct Item *ip;

if (*filestart == NULL || ptr == CF_UNDEFINED_ITEM)
   {
   AppendItem(filestart,string,NULL);
   return;
   }

if (ptr == NULL)
   {
   AppendItem(filestart,string,NULL);
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

int NeighbourItemMatches(struct Item *file_start,struct Item *location,char *string,enum cfeditorder pos,struct Attributes a,struct Promise *pp)

{ struct Item *ip;

/* Look for a line matching proposed insert before or after location */
 
for (ip = file_start; ip != NULL; ip = ip->next)
   {
   if (pos == cfe_before)
      {
      if (ip->next && ip->next == location)
         {
         if (MatchPolicy(string,ip->name,a,pp))
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
         if (ip->next && MatchPolicy(string,ip->next->name,a,pp))
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

/*********************************************************************/

struct Item *SplitString(char *string,char sep)

 /* Splits a string containing a separator like : 
    into a linked list of separate items, */

{ struct Item *liststart = NULL;
  char *sp;
  char before[CF_BUFSIZE];
  int i = 0;
  
Debug("SplitString([%s],%c=%d)\n",string,sep,sep);

for (sp = string; (*sp != '\0') ; sp++,i++)
   {
   before[i] = *sp;
   
   if (*sp == sep)
      {
      /* Check the listsep is not escaped*/
      
      if ((sp > string) && (*(sp-1) != '\\'))
         {
         before[i] = '\0';
         AppendItem(&liststart,before,NULL);
         i = -1;
         }
      else if ((sp > string) && (*(sp-1) == '\\'))
         {
         i--;
         before[i] = sep;
         }
      else
         {
         before[i] = '\0';
         AppendItem(&liststart,before,NULL);
         i = -1;
         }
      }
   }

before[i] = '\0';
AppendItem(&liststart,before,"");

return liststart;
}

/*********************************************************************/

struct Item *SplitStringAsItemList(char *string,char sep)

 /* Splits a string containing a separator like : 
    into a linked list of separate items, */

{ struct Item *liststart = NULL;
  char format[9], *sp;
  char node[CF_MAXVARSIZE];
  
Debug("SplitStringAsItemList(%s,%c)\n",string,sep);

sprintf(format,"%%255[^%c]",sep);   /* set format string to search */

for (sp = string; *sp != '\0'; sp++)
   {
   memset(node,0,CF_MAXVARSIZE);
   sscanf(sp,format,node);

   if (strlen(node) == 0)
      {
      continue;
      }
   
   sp += strlen(node)-1;

   AppendItem(&liststart,node,NULL);

   if (*sp == '\0')
      {
      break;
      }
   }

return liststart;
}

/*********************************************************************/

char *ItemList2CSV(struct Item *list)

{ struct Item *ip;
  int len = 0;
  char *s;
  
for (ip = list; ip !=  NULL; ip=ip->next)
   {
   len += strlen(ip->name) + 1;
   }

s = malloc(len+1);
*s = '\0';

for (ip = list; ip !=  NULL; ip=ip->next)
   {
   strcat(s,ip->name);

   if (ip->next)
      {
      strcat(s,",");
      }
   }

return s;
}

/*********************************************************************/
/* Basic operations                                                  */
/*********************************************************************/

void IncrementItemListCounter(struct Item *list,char *item)

{ struct Item *ptr; 

if ((item == NULL) || (strlen(item) == 0))
   {
   return;
   }
 
for (ptr = list; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->name,item) == 0)
      {
      ptr->counter++;
      return;
      }
   }
}

/*********************************************************************/

void SetItemListCounter(struct Item *list,char *item,int value)

{ struct Item *ptr; 

if ((item == NULL) || (strlen(item) == 0))
   {
   return;
   }
 
for (ptr = list; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->name,item) == 0)
      {
      ptr->counter = value;
      return;
      }
   }
}

/*********************************************************************/

int IsMatchItemIn(struct Item *list,char *item)

/* Solve for possible regex/fuzzy models unified */
    
{ struct Item *ptr; 
 
if ((item == NULL) || (strlen(item) == 0))
   {
   return true;
   }
 
for (ptr = list; ptr != NULL; ptr=ptr->next)
   {
   if (FuzzySetMatch(ptr->name,item) == 0)
      {
      return(true);
      }
   
   if (IsRegex(ptr->name))
      {
      if (FullTextMatch(ptr->name,item))
         {
         return(true);
         }      
      }   
   }
 
return(false);
}

/*********************************************************************/

int IsFuzzyItemIn(struct Item *list,char *item)

 /* This is for matching ranges of IP addresses, like CIDR e.g.

 Range1 = ( 128.39.89.250/24 )
 Range2 = ( 128.39.89.100-101 )
 
 */

{ struct Item *ptr; 

Debug("\nFuzzyItemIn(LIST,%s)\n",item);
 
if ((item == NULL) || (strlen(item) == 0))
   {
   return true;
   }
 
for (ptr = list; ptr != NULL; ptr=ptr->next)
   {
   Debug(" Try FuzzySetMatch(%s,%s)\n",ptr->name,item);
   
   if (FuzzySetMatch(ptr->name,item) == 0)
      {
      return(true);
      }
   }
 
return(false);
}

/*********************************************************************/

void DeleteItemList(struct Item *item)  /* delete starting from item */
 
{
  struct Item *ip, *next;

  for(ip = item; ip != NULL; ip = next)
    {
      next = ip->next;  // save before free

      if (ip->name != NULL)
	{
	  free (ip->name);
	}
      
      if (ip->classes != NULL)
	{
	  free (ip->classes);
	}
      
      free((char *)ip);
    }
}

/*********************************************************************/

void DeleteItem(struct Item **liststart,struct Item *item)
 
{ struct Item *ip, *sp;

if (item != NULL)
   {
   if (item->name != NULL)
      {
      free(item->name);
      }

   if (item->classes != NULL)
      {
      free(item->classes);
      }

   sp = item->next;

   if (item == *liststart)
      {
      *liststart = sp;
      }
   else
      {
      for (ip = *liststart; ip != NULL && ip->next != item && ip->next != NULL; ip=ip->next)
         {
         }

      if (ip != NULL)
         {
         ip->next = sp;
         }
      }

   free((char *)item);
   }
}

/*********************************************************************/

void DebugListItemList(struct Item *liststart)

{ struct Item *ptr;

for (ptr = liststart; ptr != NULL; ptr=ptr->next)
   {
   if (ptr->classes)
      {
      printf("CFDEBUG: %s::[%s]\n",ptr->classes,ptr->name);
      }
   else
      {
      printf("CFDEBUG: [%s]\n",ptr->name);
      }
   }
}

/*********************************************************************/

int ItemListsEqual(struct Item *list1,struct Item *list2)

{ struct Item *ip1, *ip2;

ip1 = list1;
ip2 = list2;

while (true)
   {
   if ((ip1 == NULL) && (ip2 == NULL))
      {
      return true;
      }

   if ((ip1 == NULL) || (ip2 == NULL))
      {
      return false;
      }
   
   if (strcmp(ip1->name,ip2->name) != 0)
      {
      return false;
      }

   ip1 = ip1->next;
   ip2 = ip2->next;
   }

return true;
}

/*********************************************************************/

int OrderedListsMatch(struct Item *list1,struct Item *list2)

{ struct Item *ip1,*ip2;

for (ip1 = list1,ip2 = list2; (ip1!=NULL)&&(ip2!=NULL); ip1=ip1->next,ip2=ip2->next)
   {
   if (strcmp(ip1->name,ip2->name) != 0)
      {
      Debug("OrderedListMatch failed on (%s,%s)\n",ip1->name,ip2->name);
      return false;
      }
   }

if (ip1 != ip2)
   {
   return false;
   }
 
return true; 
}


/*********************************************************************/

int IsClassedItemIn(struct Item *list,char *item)

{ struct Item *ptr; 

if ((item == NULL) || (strlen(item) == 0))
   {
   return true;
   }
 
for (ptr = list; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->name,item) == 0)
      {
      if (IsExcluded(ptr->classes))
         {
         continue;
         }   

      return(true);
      }
   }
 
return(false);
}

/*********************************************************************/

/* DeleteItem* function notes:
 * -They all take an item list and an item specification ("string" argument.)
 * -Some of them treat the item spec as a literal string, while others
 *  treat it as a regular expression.
 * -They all delete the first item meeting their criteria, as below.
 *  function   deletes item
 *  ------------------------------------------------------------------------
 *  DeleteItemStarting  start is literally equal to string item spec
 *  DeleteItemLiteral  literally equal to string item spec
 *  DeleteItemMatching  fully matched by regex item spec
 *  DeleteItemContaining containing string item spec
 */

/*********************************************************************/

int DeleteItemGeneral(struct Item **list,char *string,enum matchtypes type)

{ struct Item *ip,*last = NULL;
  int match = 0, matchlen = 0;

if (list == NULL)
   {
   return false;
   }
 
switch (type)
   {
   case literalStart:
       matchlen = strlen(string);
       break;
   case regexComplete:
   case NOTregexComplete:
       break;
   }
 
 for (ip = *list; ip != NULL; ip=ip->next)
    {
    if (ip->name == NULL)
       {
       continue;
       }
    
    switch(type)
       {
       case NOTliteralStart:
           match = (strncmp(ip->name, string, matchlen) != 0);
           break;
       case literalStart:
           match = (strncmp(ip->name, string, matchlen) == 0);
           break;
       case NOTliteralComplete:
           match = (strcmp(ip->name, string) != 0);
           break;
       case literalComplete:
           match = (strcmp(ip->name, string) == 0);
           break;
       case NOTliteralSomewhere:
           match = (strstr(ip->name, string) == NULL);
           break;
       case literalSomewhere:
           match = (strstr(ip->name, string) != NULL);
           break;
       case NOTregexComplete:
       case regexComplete:
           /* To fix a bug on some implementations where rx gets emptied */
           match = FullTextMatch(string,ip->name);
           
           if (type == NOTregexComplete)
              {
              match = !match;
              }
           break;
       }
    
    if (match)
       {
       if (ip == *list)
          {
          free((*list)->name);
          if (ip->classes != NULL) 
             {
             free(ip->classes);
             }
          *list = ip->next;
          free((char *)ip);
          return true;
          }
       else
          {
          if (ip != NULL)
             {
             if (last != NULL)
                {
                last->next = ip->next;
                }
             
             free(ip->name);
             if (ip->classes != NULL) 
                {
                free(ip->classes);
                }
             free((char *)ip);
             }

          return true;
          }
       
       }
    last = ip;
    }
 
 return false;
}


/*********************************************************************/

int DeleteItemStarting(struct Item **list,char *string)  /* delete 1st item starting with string */

{
return DeleteItemGeneral(list,string,literalStart);
}

/*********************************************************************/

int DeleteItemNotStarting(struct Item **list,char *string)  /* delete 1st item starting with string */

{
return DeleteItemGeneral(list,string,NOTliteralStart);
}

/*********************************************************************/

int DeleteItemLiteral(struct Item **list,char *string)  /* delete 1st item which is string */

{
return DeleteItemGeneral(list,string,literalComplete);
}

/*********************************************************************/

int DeleteItemMatching(struct Item **list,char *string)  /* delete 1st item fully matching regex */

{
return DeleteItemGeneral(list,string,regexComplete);
}

/*********************************************************************/

int DeleteItemNotMatching(struct Item **list,char *string)  /* delete 1st item fully matching regex */

{
return DeleteItemGeneral(list,string,NOTregexComplete);
}

/*********************************************************************/

int DeleteItemContaining(struct Item **list,char *string) /* delete first item containing string */

{
return DeleteItemGeneral(list,string,literalSomewhere);
}

/*********************************************************************/

int DeleteItemNotContaining(struct Item **list,char *string) /* delete first item containing string */

{
return DeleteItemGeneral(list,string,NOTliteralSomewhere);
}

/*********************************************************************/

int CompareToFile(struct Item *liststart,char *file,struct Attributes a,struct Promise *pp)

/* returns true if file on disk is identical to file in memory */

{
  struct stat statbuf;
  struct Item *cmplist = NULL;

Debug("CompareToFile(%s)\n",file);

if (cfstat(file,&statbuf) == -1)
   {
   return false;
   }

if (liststart == NULL && statbuf.st_size == 0)
   {
   return true;
   }

if (liststart == NULL)
   {
   return false;
   }

if (!LoadFileAsItemList(&cmplist,file,a,pp))
   {
   return false;
   }

if (!ItemListsEqual(cmplist,liststart))
   {
   DeleteItemList(cmplist);
   return false;
   }

DeleteItemList(cmplist);
return (true);
}
