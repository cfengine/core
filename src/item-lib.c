
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

int IsItemIn(struct Item *list,char *item)

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

void PrependItem (struct Item **liststart,char *itemstring,char *classes)

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
  struct stat statbuf;
  char new[CF_BUFSIZE],backup[CF_BUFSIZE];
  FILE *fp;
  mode_t mask;
  char stamp[CF_BUFSIZE]; 
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

if (rename(new,file) == -1)
   {
   CfOut(cf_error,"rename","Error while renaming %s\n",file);
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

{ struct Item *ip,*ip_last = NULL,*ip_prev = CF_UNDEFINED_ITEM;;
 
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

/*******************************************************************/

/* Borrowed this algorithm from merge-sort implementation */

struct Item *SortItemListNames(struct Item *list) /* Alphabetical */

{ struct Item *p, *q, *e, *tail, *oldhead;
  int insize, nmerges, psize, qsize, i;

if (list == NULL)
   { 
   return NULL;
   }
 
 insize = 1;
 
 while (true)
    {
    p = list;
    oldhead = list;                /* only used for circular linkage */
    list = NULL;
    tail = NULL;
    
    nmerges = 0;  /* count number of merges we do in this pass */
    
    while (p)
       {
       nmerges++;  /* there exists a merge to be done */
       /* step `insize' places along from p */
       q = p;
       psize = 0;
       
       for (i = 0; i < insize; i++)
          {
          psize++;
          q = q->next;

          if (!q)
              {
              break;
              }
          }
       
       /* if q hasn't fallen off end, we have two lists to merge */
       qsize = insize;
       
       /* now we have two lists; merge them */
       while (psize > 0 || (qsize > 0 && q))
          {          
          /* decide whether next element of merge comes from p or q */
          if (psize == 0)
             {
             /* p is empty; e must come from q. */
             e = q; q = q->next; qsize--;
             }
          else if (qsize == 0 || !q)
             {
             /* q is empty; e must come from p. */
             e = p; p = p->next; psize--;
             }
          else if (strcmp(p->name, q->name) <= 0)
             {
             /* First element of p is lower (or same);
              * e must come from p. */
             e = p; p = p->next; psize--;
             }
          else
             {
             /* First element of q is lower; e must come from q. */
             e = q; q = q->next; qsize--;
             }
          
          /* add the next element to the merged list */
          if (tail)
             {
             tail->next = e;
             }
          else
             {
             list = e;
             }
          tail = e;
          }
       
       /* now p has stepped `insize' places along, and q has too */
       p = q;
       }
    tail->next = NULL;
    
    /* If we have done only one merge, we're finished. */

    if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
       {
       return list;
       }

    /* Otherwise repeat, merging lists twice the size */
    insize *= 2;
    }
}

/*******************************************************************/

struct Item *SortItemListCounters(struct Item *list) /* Biggest first */

{ struct Item *p, *q, *e, *tail, *oldhead;
  int insize, nmerges, psize, qsize, i;

if (list == NULL)
   { 
   return NULL;
   }
 
 insize = 1;
 
 while (true)
    {
    p = list;
    oldhead = list;                /* only used for circular linkage */
    list = NULL;
    tail = NULL;
    
    nmerges = 0;  /* count number of merges we do in this pass */
    
    while (p)
       {
       nmerges++;  /* there exists a merge to be done */
       /* step `insize' places along from p */
       q = p;
       psize = 0;
       
       for (i = 0; i < insize; i++)
          {
          psize++;
          q = q->next;

          if (!q)
              {
              break;
              }
          }
       
       /* if q hasn't fallen off end, we have two lists to merge */
       qsize = insize;
       
       /* now we have two lists; merge them */
       while (psize > 0 || (qsize > 0 && q))
          {          
          /* decide whether next element of merge comes from p or q */
          if (psize == 0)
             {
             /* p is empty; e must come from q. */
             e = q; q = q->next; qsize--;
             }
          else if (qsize == 0 || !q)
             {
             /* q is empty; e must come from p. */
             e = p; p = p->next; psize--;
             }
          else if (p->counter > q->counter)
             {
             /* First element of p is higher (or same);
              * e must come from p. */
             e = p; p = p->next; psize--;
             }
          else
             {
             /* First element of q is lower; e must come from q. */
             e = q; q = q->next; qsize--;
             }
          
          /* add the next element to the merged list */
          if (tail)
             {
             tail->next = e;
             }
          else
             {
             list = e;
             }
          tail = e;
          }
       
       /* now p has stepped `insize' places along, and q has too */
       p = q;
       }
    tail->next = NULL;
    
    /* If we have done only one merge, we're finished. */

    if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
       {
       return list;
       }

    /* Otherwise repeat, merging lists twice the size */
    insize *= 2;
    }
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
/* Basic operations                                                  */
/*********************************************************************/

void AppendItem (struct Item **liststart,char *itemstring,char *classes)

{ struct Item *ip, *lp;
  char *sp,*spe = NULL;

if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL)
   {
   FatalError("Memory allocation failure");
   }

if ((sp = malloc(strlen(itemstring)+CF_EXTRASPC)) == NULL)
   {
   FatalError("Memory allocation failure");
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

if ((classes != NULL) && (spe = malloc(strlen(classes)+2)) == NULL)
   {
   CfOut(cf_error,"","malloc failure");
   }

strcpy(sp,itemstring);
ip->name = sp;
ip->next = NULL;
ip->counter = 0;
 
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

void DeleteItemList(struct Item *item)                /* delete starting from item */
 
{
if (item != NULL)
   {
   if (item->next)
      {
      DeleteItemList(item->next);
      item->next = NULL;
      }

   if (item->name != NULL)
      {
      Debug("Unappending %s\n",item->name);
      free (item->name);
      }

   if (item->classes != NULL)
      {
      free (item->classes);
      }

   free((char *)item);
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
      for (ip = *liststart; ip->next != item; ip=ip->next)
         {
         }

      ip->next = sp;
      }

   free((char *)item);
   }
}

/*********************************************************************/

void DebugListItemList(struct Item *liststart)

{ struct Item *ptr;

for (ptr = liststart; ptr != NULL; ptr=ptr->next)
   {
   printf("CFDEBUG: [%s]\n",ptr->name);
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
  regex_t rx,rxcache;
  regmatch_t pmatch;

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
           memcpy(&rx,&rxcache,sizeof(rx));
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

int CompareToFile(struct Item *liststart,char *file)

/* returns true if file on disk is identical to file in memory */

{ FILE *fp;
  struct stat statbuf;
  struct Item *ip = liststart;
  unsigned char *finmem = NULL, fdata;
  unsigned long fip = 0, tmplen, idx;

Debug("CompareToFile(%s)\n",file);

if (stat(file,&statbuf) == -1)
   {
   return false;
   }

if (liststart == NULL)
   {
   return true;
   }

for (ip = liststart; ip != NULL; ip=ip->next)
   {
   tmplen = strlen(ip->name);

   if ((finmem = realloc(finmem, fip+tmplen+1)) == NULL)
      {
      Debug("CompareToFile(%s): can't realloc() memory\n",file);
      free(finmem);
      return false;
      }
   
   memcpy(finmem+fip, ip->name, tmplen);
   fip += tmplen;
   *(finmem+fip++) = '\n';
   }

if (statbuf.st_size != fip)
   {
   Debug("CompareToFile(%s): sizes are different: MEM:(%u) FILE:(%u)\n",file, fip, statbuf.st_size);
   free(finmem);
   return false;
   }

if ((fp = fopen(file,"r")) == NULL)
   {
   CfOut(cf_error,"fopen","Couldn't read file %s for editing\n",file);
   free(finmem);
   return false;
   }

for (idx = 0; idx < fip; idx++)
   {
   if (fread(&fdata, 1, 1, fp) != 1)
      {
      Debug("CompareToFile(%s): non-zero fread() before file-in-mem finished at %u-th byte MEM:(0x%x/%c)\n",file, idx, *(finmem+idx), *(finmem+idx));
      free(finmem);
      fclose(fp);
      return false;
      }
   
   if (fdata != *(finmem+idx))
      {
      printf("CompareToFile(%s): difference found at %u-th byte MEM:(0x%x/%c) != FILE:(0x%x/%c)\n",file, idx, *(finmem+idx), *(finmem+idx), fdata, fdata);
      free(finmem);
      fclose(fp);
      return false;
      }
   }

free(finmem);
fclose(fp);
return (true);
}
