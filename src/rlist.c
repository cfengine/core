/* 

        Copyright (C) 1994-
        Free Software Foundation, Inc.

   This file is part of GNU cfengine - written and maintained 
   by Mark Burgess, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
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
/* File: rlist.c                                                             */
/*                                                                           */
/* Created: Wed Aug  8 14:11:39 2007                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/

void *CopyRvalItem(void *item, char type)

{ struct Rlist *rp,*start = NULL;
  struct FnCall *fp;
  void *new;

Debug("CopyRvalItem(%c)\n",type);
  
switch(type)
   {
   case CF_SCALAR:
       /* the rval is just a string */
       if ((new = strdup((char *)item)) ==  NULL)
          {
          CfLog(cferror,"Memory allocation","strdup");
          FatalError("CopyRvalItem");
          }

       return new;

   case CF_FNCALL:
       /* the rval is a fncall */
       fp = (struct FnCall *)item;
       return CopyFnCall(fp);

   case CF_LIST:
       /* The rval is an embedded rlist (2d) */
       for (rp = (struct Rlist *)item; rp != NULL; rp=rp->next)
          {
          AppendRlist(&start,rp->item,rp->type);
          }
       
       return start;
   }

snprintf(OUTPUT,CF_BUFSIZE,"Unknown type %c in CopyRvalItem - should not happen",type);
FatalError(OUTPUT);
return NULL;
}

/*******************************************************************/

struct Rlist *CopyRlist(struct Rlist *list)

{ struct Rlist *rp,*start = NULL;
  struct FnCall *fp;
  void *new;

Debug("CopyRlist()\n");
  
if (list == NULL)
   {
   return NULL;
   }

for (rp = list; rp != NULL; rp= rp->next)
   {
   new = CopyRvalItem(rp->item,rp->type);
   AppendRlist(&start,new,rp->type);
   }

return start;
}

/*******************************************************************/

void DeleteRlist(struct Rlist *list)

{
DeleteRvalItem(list,CF_LIST);
}

/*******************************************************************/

struct Rlist *IdempAppendRScalar(struct Rlist **start,void *item, char type)

{ char *scalar = strdup((char *)item);

if (type != CF_SCALAR)
   {
   FatalError("Cannot append non-scalars to lists");
   }

if (!KeyInRlist(*start,(char *)item))
   {
   return AppendRlist(start,scalar,type);
   }
else
   {
   return NULL;
   }
}

/*******************************************************************/

struct Rlist *IdempPrependRScalar(struct Rlist **start,void *item, char type)

{ char *scalar = strdup((char *)item);

if (type != CF_SCALAR)
   {
   FatalError("Cannot append non-scalars to lists");
   }

if (!KeyInRlist(*start,(char *)item))
   {
   return PrependRlist(start,scalar,type);
   }
else
   {
   return NULL;
   }
}

/*******************************************************************/

struct Rlist *IdempAppendRlist(struct Rlist **start,void *item, char type)

{ char *scalar;
 struct Rlist *rp,*ins = NULL;
 
if (type == CF_LIST)
   {
   for (rp = (struct Rlist *)item; rp != NULL; rp=rp->next)
      {
      ins = IdempAppendRlist(start,rp->item,rp->type);
      }
   return ins;
   }

scalar = strdup((char *)item);

if (!KeyInRlist(*start,(char *)item))
   {
   return AppendRlist(start,scalar,type);
   }
else
   {
   return NULL;
   }
}

/*******************************************************************/

struct Rlist *AppendRScalar(struct Rlist **start,void *item, char type)

{ char *scalar = strdup((char *)item);

if (type != CF_SCALAR)
   {
   FatalError("Cannot append non-scalars to lists");
   }
 
return AppendRlist(start,scalar,type);
}

/*******************************************************************/

struct Rlist *PrependRScalar(struct Rlist **start,void *item, char type)

{ char *scalar = strdup((char *)item);

if (type != CF_SCALAR)
   {
   FatalError("Cannot append non-scalars to lists");
   }
 
return PrependRlist(start,scalar,type);
}

/*******************************************************************/

struct Rlist *AppendRlist(struct Rlist **start,void *item, char type)

   /* Allocates new memory for objects - careful, could leak!  */
    
{ struct Rlist *rp,*lp = *start;
  struct FnCall *fp;
  char *sp = NULL;

switch(type)
   {
   case CF_SCALAR:
       Debug("Appending scalar to rval-list [%s]\n",item);
       break;

   case CF_FNCALL:
       Debug("Appending function to rval-list function call: ");
       fp = (struct FnCall *)item;
       if (DEBUG)
          {
          ShowFnCall(stdout,fp);
          }
       Debug("\n");
       break;

   case CF_LIST:
       Debug("Expanding and appending list object\n");
       
       for (rp = (struct Rlist *)item; rp != NULL; rp=rp->next)
          {
          lp = AppendRlist(start,rp->item,rp->type);
          }
       
       return lp;
       
   default:
       Debug("Cannot append %c to rval-list [%s]\n",type,item);
       return NULL;
   }

if ((rp = (struct Rlist *)malloc(sizeof(struct Rlist))) == NULL)
   {
   CfLog(cferror,"Unable to allocate Rlist","malloc");
   FatalError("");
   }

if (*start == NULL)
   {
   *start = rp;
   }
else
   {
   for (lp = *start; lp->next != NULL; lp=lp->next)
      {
      }

   lp->next = rp;
   }

rp->item = CopyRvalItem(item,type);
rp->type = type;  /* scalar, builtin function */

if (type == CF_LIST)
   {
   rp->state_ptr = rp->item;
   }
else
   {
   rp->state_ptr = NULL;
   }

rp->next = NULL;
return rp;
}

/*******************************************************************/

struct Rlist *PrependRlist(struct Rlist **start,void *item, char type)

   /* heap memory for item must have already been allocated */
    
{ struct Rlist *rp,*lp = *start;
  struct FnCall *fp;
  char *sp = NULL;

switch(type)
   {
   case CF_SCALAR:
       Debug("Prepending scalar to rval-list [%s]\n",item);
       break;

   case CF_LIST:
       
       Debug("Expanding and prepending list (ends up in reverse)\n");

       for (rp = (struct Rlist *)item; rp != NULL; rp=rp->next)
          {
          lp = PrependRlist(start,rp->item,rp->type);
          }
       return lp;

   case CF_FNCALL:
       Debug("Prepending function to rval-list function call: ");
       fp = (struct FnCall *)item;
       if (DEBUG)
          {
          ShowFnCall(stdout,fp);
          }
       Debug("\n");
       break;
   default:
       Debug("Cannot prepend %c to rval-list [%s]\n",type,item);
       return NULL;
   }

if ((rp = (struct Rlist *)malloc(sizeof(struct Rlist))) == NULL)
   {
   CfLog(cferror,"Unable to allocate Rlist","malloc");
   FatalError("");
   }

rp->next = *start;
rp->item = CopyRvalItem(item,type);
rp->type = type;  /* scalar, builtin function */

if (type == CF_LIST)
   {
   rp->state_ptr = rp->item;
   }
else
   {
   rp->state_ptr = NULL;
   }

*start = rp;

return rp;
}

/*******************************************************************/

struct Rlist *OrthogAppendRlist(struct Rlist **start,void *item, char type)

   /* Allocates new memory for objects - careful, could leak!  */
    
{ struct Rlist *rp,*lp;
  struct FnCall *fp;
  char *sp = NULL;

Debug("OrthogAppendRlist\n");

switch(type)
   {
   case CF_LIST:
       Debug("Expanding and appending list object, orthogonally\n");
       break;
   default:
       Debug("Cannot append %c to rval-list [%s]\n",type,item);
       return NULL;
   }

if ((rp = (struct Rlist *)malloc(sizeof(struct Rlist))) == NULL)
   {
   CfLog(cferror,"Unable to allocate Rlist","malloc");
   FatalError("");
   }

if (*start == NULL)
   {
   *start = rp;
   }
else
   {
   for (lp = *start; lp->next != NULL; lp=lp->next)
      {
      }

   lp->next = rp;
   }

rp->item = item;
rp->type = CF_LIST;
rp->state_ptr = rp->item;
rp->next = NULL;

return rp;
}

/*******************************************************************/

int RlistLen(struct Rlist *start)

{ int count = 0;
  struct Rlist *rp;
  
for (rp = start; rp != NULL; rp=rp->next)
   {
   count++;
   }

return count;
}

/*******************************************************************/

void ShowRlist(FILE *fp,struct Rlist *list)

{ struct Rlist *rp;

fprintf(fp," {");
 
for (rp = list; rp != NULL; rp=rp->next)
   {
   fprintf(fp,"\'");
   ShowRval(fp,rp->item,rp->type);
   fprintf(fp,"\'");
   
   if (rp->next != NULL)
      {
      fprintf(fp,",");
      }
   }
fprintf(fp,"}");
}

/*******************************************************************/

void ShowRlistState(FILE *fp,struct Rlist *list)

{ struct Rlist *rp;

ShowRval(fp,list->state_ptr->item,list->type);
}

/*******************************************************************/

struct Rlist *KeyInRlist(struct Rlist *list,char *key)

{ struct Rlist *rp;

for (rp = list; rp != NULL; rp = rp->next)
   {
   if (rp->type != CF_SCALAR)
      {
      continue;
      }
   
   if (strcmp((char *)rp->item,key) == 0)
      {
      return rp;
      }
   }

return NULL;
}


/*******************************************************************/

void ShowRval(FILE *fp,void *rval,char type)

{
if (rval == NULL)
   {
   return;
   }

switch (type)
   {
   case CF_SCALAR:
       fprintf(fp,"%s",(char *)rval);
       break;
       
   case CF_LIST:
       ShowRlist(fp,(struct Rlist *)rval);
       break;
       
   case CF_FNCALL:
       ShowFnCall(fp,(struct FnCall *)rval);
       break;

   case CF_NOPROMISEE:
       fprintf(fp,"(no-one)");
       break;
   }
}

/*******************************************************************/

void DeleteRvalItem(void *rval, char type)

{ struct Rlist *clist;

Debug("DeleteRvalItem(%c)\n",type);

if (rval == NULL)
   {
   return;
   }

switch (type)
   {
   case CF_SCALAR:
       free((char *)rval);
       break;
       
   case CF_LIST:

       /* rval is now a list whose first item is list->item */
       clist = (struct Rlist *)rval;

       if (clist->item != NULL)
          {
          free(clist->item);
          }
       
       if (clist->next != NULL)
          {
          DeleteRvalItem(clist->next,clist->next->type);
          }
       break;
       
   case CF_FNCALL:
       if (rval)
          {
          DeleteFnCall((struct FnCall *)rval);
          }
       break;

   default:
       Debug("Nothing to do\n");
       return;
   }
}

/*******************************************************************/
/* Stack                                                           */
/*******************************************************************/

/*
char *sp1 = strdup("String 1\n");
char *sp2 = strdup("String 2\n");
char *sp3 = strdup("String 3\n");

PushStack(&stack,(void *)sp1);
PopStack(&stack,(void *)&sp,sizeof(sp));
*/

void PushStack(struct Rlist **liststart,void *item)

{ struct Rlist *rp;

/* Have to keep track of types personally */

if ((rp = (struct Rlist *)malloc(sizeof(struct Rlist))) == NULL)
   {
   CfLog(cferror,"Unable to allocate Rlist","malloc");
   FatalError("");
   }

rp->next = *liststart;
rp->item = item;
rp->type = CF_STACK;
*liststart = rp;
}

/*******************************************************************/

void PopStack(struct Rlist **liststart, void **item,size_t size)

{ struct Rlist *rp = *liststart;

if (*liststart == NULL)
   {
   FatalError("Attempt to pop from empty stack");
   }
 
*item = rp->item;

if (rp->next == NULL) /* only one left */
   {
   *liststart = (void *)NULL;
   }
else
   {
   *liststart = rp->next;
   }
 
free((char *)rp);
}

/*******************************************************************/

struct Rlist *SplitStringAsRList(char *string,char sep)

 /* Splits a string containing a separator like "," 
    into a linked list of separate items, */

{ struct Rlist *liststart = NULL;
  char format[9], *sp;
  char node[CF_MAXVARSIZE];
  
Debug("SplitStringAsRList(%s)\n",string);

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

   AppendRScalar(&liststart,node,CF_SCALAR);

   if (*sp == '\0')
      {
      break;
      }
   }

return liststart;
}
