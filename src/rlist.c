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
/* File: rlist.c                                                             */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

static int IsRegexIn(struct Rlist *list,char *s);
static int CompareRlist(struct Rlist *list1, struct Rlist *list2);
static void DeleteRlistNoRef(struct Rlist *list);
static void ShowRlistState(FILE *fp,struct Rlist *list);

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

int IsStringIn(struct Rlist *list,char *s)

{ struct Rlist *rp;

if (s == NULL || list == NULL)
   {
   return false;
   }

for (rp = list; rp != NULL; rp=rp->next)
   {
   if (rp->type != CF_SCALAR)
      {
      continue;
      }

   if (strcmp(s,rp->item) == 0)
      {
      return true;
      }
   }

return false;
}

/*******************************************************************/

int IsIntIn(struct Rlist *list,int i)

{ struct Rlist *rp;
  char s[CF_SMALLBUF];

snprintf(s,CF_SMALLBUF-1,"%d",i);
  
if (list == NULL)
   {
   return false;
   }

for (rp = list; rp != NULL; rp=rp->next)
   {
   if (rp->type != CF_SCALAR)
      {
      continue;
      }

   if (strcmp(s,rp->item) == 0)
      {
      return true;
      }
   }

return false;
}

/*******************************************************************/

static int IsRegexIn(struct Rlist *list,char *regex)

{ struct Rlist *rp;

if (regex == NULL || list == NULL)
   {
   return false;
   }

for (rp = list; rp != NULL; rp=rp->next)
   {
   if (rp->type != CF_SCALAR)
      {
      continue;
      }

   if (FullTextMatch(regex,rp->item))
      {
      return true;
      }
   }

return false;
}

/*******************************************************************/

int IsInListOfRegex(struct Rlist *list,char *str)

{ struct Rlist *rp;

if (str == NULL || list == NULL)
   {
   return false;
   }

for (rp = list; rp != NULL; rp=rp->next)
   {
   if (rp->type != CF_SCALAR)
      {
      continue;
      }

   if (FullTextMatch(rp->item,str))
      {
      return true;
      }
   }

return false;
}

/*******************************************************************/

void *CopyRvalItem(void *item, char type)

{ struct Rlist *rp,*srp,*start = NULL;
  struct FnCall *fp;
  void *new,*rval;
  char rtype = CF_SCALAR;
  char naked[CF_BUFSIZE];
  
Debug("CopyRvalItem(%c)\n",type);

if (item == NULL)
   {
   switch (type)
      {
      case CF_SCALAR:
          return strdup("");

      case CF_LIST:
          return NULL;
      }
   }

naked[0] = '\0';

switch(type)
   {
   case CF_SCALAR:
       /* the rval is just a string */
       if ((new = strdup((char *)item)) ==  NULL)
          {
          CfOut(cf_error,"strdup","Memory allocation");
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
          if (IsNakedVar(rp->item,'@'))
             {
             GetNaked(naked,rp->item);

             if (GetVariable(CONTEXTID,naked,&rval,&rtype) != cf_notype)
                {
                switch (rtype)
                   {
                   case CF_LIST:
                       for (srp = (struct Rlist *)rval; srp != NULL; srp=srp->next)
                          {
                          AppendRlist(&start,srp->item,srp->type);
                          }
                       break;
                     
                   default:
                       AppendRlist(&start,rp->item,rp->type);
                       break;
                   }
                }
             else
                {
                AppendRlist(&start,rp->item,rp->type);
                }
             }
          else
             {             
             AppendRlist(&start,rp->item,rp->type);
             }
          }
       
       return start;
   }

//snprintf(output,CF_BUFSIZE,"Unknown type %c in CopyRvalItem - should not happen",type);
//FatalError(output);
return NULL;
}

/*******************************************************************/

int CompareRval(void *rval1, char rtype1, void *rval2, char rtype2)

{
if (rtype1 != rtype2)
   {
   return -1;
   }

switch (rtype1)
   {
   case CF_SCALAR:

       if (IsCf3VarString((char *)rval1) || IsCf3VarString((char *)rval2))
          {
          return -1; // inconclusive
          }

       if (strcmp(rval1,rval2) != 0)
          {
          return false;
          }
       
       break;
       
   case CF_LIST:
       return CompareRlist(rval1,rval2);
       
   case CF_FNCALL:
       return -1;       
   }

return true;
}

/*******************************************************************/

static int CompareRlist(struct Rlist *list1, struct Rlist *list2)

{ struct Rlist *rp1,*rp2;

for (rp1 = list1, rp2 = list2; rp1 != NULL && rp2!= NULL; rp1=rp1->next,rp2=rp2->next)
   {
   if (rp1->item && rp2->item)
      {
      struct Rlist *rc1,*rc2;
      
      if (rp1->type == CF_FNCALL || rp2->type == CF_FNCALL)
         {
         return -1; // inconclusive
         }

      rc1 = rp1;
      rc2 = rp2;

      // Check for list nesting with { fncall(), "x" ... }
      
      if (rp1->type == CF_LIST)
         {   
         rc1 = rp1->item;
         }
      
      if (rp2->type == CF_LIST)
         {
         rc2 = rp2->item;
         }

      if (IsCf3VarString(rc1->item) || IsCf3VarString(rp2->item))
         {
         return -1; // inconclusive
         }

      if (strcmp(rc1->item,rc2->item) != 0)
         {
         return false;
         }
      }
   else
      {
      return false;
      }
   }

return true;
}

/*******************************************************************/

struct Rlist *CopyRlist(struct Rlist *list)

{ struct Rlist *rp,*start = NULL;

Debug("CopyRlist()\n");
  
if (list == NULL)
   {
   return NULL;
   }

for (rp = list; rp != NULL; rp = rp->next)
   {
   AppendRlist(&start,rp->item,rp->type);  // allocates memory for objects
   }

return start;
}

/*******************************************************************/

void DeleteRlist(struct Rlist *list)

/* Delete an rlist and all its references */

{ struct Rlist *rl, *next;

if (list != NULL)
   {
   for(rl = list; rl != NULL; rl = next)
      {
      next = rl->next;
      
      if (rl->item != NULL)
         {
         DeleteRvalItem(rl->item,rl->type);
         }
      
      free(rl);
      }
   }
}

/*******************************************************************/

static void DeleteRlistNoRef(struct Rlist *list)
/* Delete a rlist, but not its references */

{ struct Rlist *rl, *next;

if(list != NULL)
   {
   for(rl = list; rl != NULL; rl = next)
      {
      next = rl->next;
      free(rl);
      }
   }
}

/*******************************************************************/

struct Rlist *IdempAppendRScalar(struct Rlist **start,void *item, char type)

{ char *scalar = item;

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

{ char *scalar = item;

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

{ char *scalar = item;

if (type != CF_SCALAR)
   {
   FatalError("Cannot append non-scalars to lists");
   }
 
return AppendRlist(start,scalar,type);
}

/*******************************************************************/

struct Rlist *PrependRScalar(struct Rlist **start,void *item, char type)

{ char *scalar = item;

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

switch(type)
   {
   case CF_SCALAR:
       Debug("Appending scalar to rval-list [%s]\n",(char *)item);
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
       Debug("Cannot append %c to rval-list [%s]\n",type,(char *)item);
       return NULL;
   }

if ((rp = (struct Rlist *)malloc(sizeof(struct Rlist))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Rlist");
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

ThreadLock(cft_lock);

if (type == CF_LIST)
   {
   rp->state_ptr = rp->item;
   }
else
   {
   rp->state_ptr = NULL;
   }

rp->next = NULL;

ThreadUnlock(cft_lock);

return rp;
}

/*******************************************************************/

struct Rlist *PrependRlist(struct Rlist **start,void *item, char type)

   /* heap memory for item must have already been allocated */
    
{ struct Rlist *rp,*lp = *start;
  struct FnCall *fp;

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

ThreadLock(cft_system);

if ((rp = (struct Rlist *)malloc(sizeof(struct Rlist))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Rlist");
   FatalError("");
   }

ThreadUnlock(cft_system);

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

ThreadLock(cft_lock);
*start = rp;
ThreadUnlock(cft_lock);
return rp;
}

/*******************************************************************/

struct Rlist *OrthogAppendRlist(struct Rlist **start,void *item, char type)

   /* Allocates new memory for objects - careful, could leak!  */
    
{ struct Rlist *rp,*lp;
  struct CfAssoc *cp;
  
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
   CfOut(cf_error,"malloc","Unable to allocate Rlist");
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


// This is item is in fact a struct CfAssoc pointing to a list

cp = (struct CfAssoc *)item;

// Note, we pad all iterators will a blank so the ptr arithmetic works
// else EndOfIteration will not see lists with only one element

lp = PrependRlist((struct Rlist **)&(cp->rval),CF_NULL_VALUE,CF_SCALAR);
rp->state_ptr = lp->next; // Always skip the null value
AppendRlist((struct Rlist **)&(cp->rval),CF_NULL_VALUE,CF_SCALAR);

rp->item = item;
rp->type = CF_LIST;
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

struct Rlist *ParseShownRlist(char *string)

{ struct Rlist *newlist = NULL,*splitlist,*rp;
  char value[CF_MAXVARSIZE];

/* Parse a string representation generated by ShowList and turn back into Rlist */
  
splitlist = SplitStringAsRList(string,',');

for (rp =  splitlist; rp != NULL; rp = rp->next)
   {
   sscanf(rp->item,"%*[{ '\"]%255[^'\"]",value);
   AppendRlist(&newlist,value,CF_SCALAR);
   }

DeleteRlist(splitlist);
return newlist;
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

int PrintRlist(char *buffer,int bufsize,struct Rlist *list)

{ struct Rlist *rp;

StartJoin(buffer,"{",bufsize);

for (rp = list; rp != NULL; rp=rp->next)
   {
   if(!JoinSilent(buffer,"'",bufsize))
      {
      EndJoin(buffer,"...TRUNCATED'}",bufsize);
      return false;
      }
   
   if(!PrintRval(buffer,bufsize,rp->item,rp->type))
      {
      EndJoin(buffer,"...TRUNCATED'}",bufsize);
      return false;
      }

   if(!JoinSilent(buffer,"'",bufsize))
      {
      EndJoin(buffer,"...TRUNCATED'}",bufsize);
      return false;
      }
   
   if (rp->next != NULL)
      {
      if(!JoinSilent(buffer,",",bufsize))
         {
         EndJoin(buffer,"...TRUNCATED}",bufsize);
         return false;
         }
      }
   }

EndJoin(buffer,"}",bufsize);

return true;
}

/*******************************************************************/

int GetStringListElement(char *strList, int index, char *outBuf, int outBufSz)

/** Takes a string-parsed list "{'el1','el2','el3',..}" and writes
 ** "el1" or "el2" etc. based on index (starting on 0) in outBuf.
 ** returns true on success, false otherwise.
 **/

{ char *sp,*elStart = strList,*elEnd;
  int elNum = 0;
  int minBuf;
  
memset(outBuf,0,outBufSz);

if (EMPTY(strList))
   {
   return false;
   }

if(strList[0] != '{')
   {
   return false;
   }

for(sp = strList; *sp != '\0'; sp++)
   {   
   if((sp[0] == '{' || sp[0] == ',') && sp[1] == '\'')
      {
      elStart = sp + 2;
      }
     
   else if((sp[0] == '\'') && sp[1] == ',' || sp[1] == '}')
      {
      elEnd = sp;
      
      if(elNum == index)
         {
         if(elEnd - elStart < outBufSz)
            {
            minBuf = elEnd - elStart;
            }
         else
            {
            minBuf = outBufSz - 1;
            }
         
         strncpy(outBuf,elStart,minBuf);
         
         break;
         }
      
      elNum++;
      }
   }

return true;
}

/*******************************************************************/

int StripListSep(char *strList, char *outBuf, int outBufSz)
{
  memset(outBuf,0,outBufSz);

  if(EMPTY(strList))
    {
    return false;
    }
  
  if(strList[0] != '{')
    {
    return false;
    }
  
  snprintf(outBuf,outBufSz,"%s",strList + 1);
  
  if(outBuf[strlen(outBuf) - 1] == '}')
    {
    outBuf[strlen(outBuf) - 1] = '\0';
    }

  return true;
}


/*******************************************************************/

int PrintRval(char *buffer,int bufsize,void *rval,char type)

{
if (rval == NULL)
   {
   return 0;
   }

switch (type)
   {
   case CF_SCALAR:
       return JoinSilent(buffer,(char *)rval,bufsize);
       break;
       
   case CF_LIST:
       return PrintRlist(buffer,bufsize,(struct Rlist *)rval);
       break;
       
   case CF_FNCALL:
       PrintFnCall(buffer,bufsize,(struct FnCall *)rval);
       break;

   case CF_NOPROMISEE:
       // fprintf(fp,"(no-one)");
       break;
   }

return true;
}

/*******************************************************************/

static void ShowRlistState(FILE *fp,struct Rlist *list)

{
ShowRval(fp,list->state_ptr->item,list->type);
}

/*******************************************************************/

void ShowRval(FILE *fp,void *rval,char type)

{
char buf[CF_BUFSIZE];

if (rval == NULL)
   {
   return;
   }

switch (type)
   {
   case CF_SCALAR:
       EscapeQuotes((char *)rval,buf,sizeof(buf));
       fprintf(fp,"%s",buf);
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

{ struct Rlist *clist, *next = NULL;

Debug("DeleteRvalItem(%c)",type);

if (DEBUG)
   {
   ShowRval(stdout,rval,type);
   }

Debug("\n");

if (rval == NULL)
   {
   Debug("DeleteRval NULL\n");
   return;
   }

switch(type)
   {
   case CF_SCALAR:

       ThreadLock(cft_lock);
       free((char *)rval);
       ThreadUnlock(cft_lock);
       break;

   case CF_LIST:
     
       /* rval is now a list whose first item is clist->item */

     for(clist = (struct Rlist *)rval; clist != NULL; clist = next)
       {

       next = clist->next;

       if (clist->item)
          {
          DeleteRvalItem(clist->item,clist->type);
          }

       free(clist);
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

/*********************************************************************/

void DeleteRlistEntry(struct Rlist **liststart,struct Rlist *entry)
 
{ struct Rlist *rp, *sp;

if (entry != NULL)
   {
   if (entry->item != NULL)
      {
      free(entry->item);
      }

   sp = entry->next;

   if (entry == *liststart)
      {
      *liststart = sp;
      }
   else
      {
      for (rp = *liststart; rp->next != entry; rp=rp->next)
         {
         }

      rp->next = sp;
      }

   free((char *)entry);
   }
}


/*******************************************************************/

struct Rlist *AppendRlistAlien(struct Rlist **start,void *item)

   /* Allocates new memory for objects - careful, could leak!  */
    
{ struct Rlist *rp,*lp = *start;

if ((rp = (struct Rlist *)malloc(sizeof(struct Rlist))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Rlist");
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
rp->type = CF_SCALAR;

ThreadLock(cft_lock);

rp->next = NULL;

ThreadUnlock(cft_lock);
return rp;
}

/*******************************************************************/

struct Rlist *PrependRlistAlien(struct Rlist **start,void *item)

   /* Allocates new memory for objects - careful, could leak!  */
    
{ struct Rlist *rp;

ThreadLock(cft_lock); 

if ((rp = (struct Rlist *)malloc(sizeof(struct Rlist))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Rlist");
   FatalError("");
   }

rp->next = *start;
*start = rp;
ThreadUnlock(cft_lock);

rp->item = item;
rp->type = CF_SCALAR;
return rp;
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
   CfOut(cf_error,"malloc","Unable to allocate Rlist");
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
    into a linked list of separate items, supports
    escaping separators, e.g. \, */

{ struct Rlist *liststart = NULL;
  char *sp;
  char node[CF_MAXVARSIZE];
  int maxlen = strlen(string);
  
Debug("SplitStringAsRList(%s)\n",string);

if (string == NULL)
   {
   return NULL;
   }

for (sp = string; *sp != '\0'; sp++)
   {
   if (*sp == '\0' || sp > string+maxlen)
      {
      break;
      }

   memset(node,0,CF_MAXVARSIZE);

   sp += SubStrnCopyChr(node,sp,CF_MAXVARSIZE,sep);

   AppendRScalar(&liststart,node,CF_SCALAR);
   }

return liststart;
}

/*******************************************************************/

struct Rlist *SplitRegexAsRList(char *string,char *regex,int max,int blanks)

 /* Splits a string containing a separator like "," 
    into a linked list of separate items, */

{ struct Rlist *liststart = NULL;
  char *sp;
  char node[CF_MAXVARSIZE];
  int start,end;
  int delta, count = 0;

if (string == NULL)
   {
   return NULL;
   }

Debug("\n\nSplit \"%s\" with regex \"%s\" (up to maxent %d)\n\n",string,regex,max);
  
sp = string;
  
while ((count < max) && BlockTextMatch(regex,sp,&start,&end))
   {
   if (end == 0)
      {
      break;
      }

   delta = end - start;
   memset(node,0,CF_MAXVARSIZE);
   strncpy(node,sp,start);

   if (blanks || strlen(node) > 0)
      {
      AppendRScalar(&liststart,node,CF_SCALAR);
      count++;
      }
   
   sp += end;
   }

if (count < max)
   {
   memset(node,0,CF_MAXVARSIZE);
   strncpy(node,sp,CF_MAXVARSIZE-1);
   
   if (blanks || strlen(node) > 0)
      {
      AppendRScalar(&liststart,node,CF_SCALAR);
      }
   }

return liststart;
}

/*****************************************************************************/

int PrependPackageItem(struct CfPackageItem **list,char *name,char *version,char *arch,struct Attributes a,struct Promise *pp)

{ struct CfPackageItem *pi;

if (strlen(name) == 0 || strlen(version) == 0 || strlen(arch) == 0)
   {
   return false;
   }

CfOut(cf_verbose,""," -> Package (%s,%s,%s) found",name,version,arch);

if ((pi = (struct CfPackageItem *)malloc(sizeof(struct CfPackageItem))) == NULL)
   {
   CfOut(cf_error,"malloc","Can't allocate new package\n");
   return false;
   }

if (list)
   {
   pi->next = *list;
   }
else
   {
   pi->next = NULL;
   }

pi->name = strdup(name);
pi->version = strdup(version);
pi->arch = strdup(arch);
*list = pi;

/* Finally we need these for later schedule exec, once this iteration context has gone */

pi->pp = DeRefCopyPromise("this",pp);
return true;
}


/*****************************************************************************/

int PrependListPackageItem(struct CfPackageItem **list,char *item,struct Attributes a,struct Promise *pp)

{ char name[CF_MAXVARSIZE];
  char arch[CF_MAXVARSIZE];
  char version[CF_MAXVARSIZE];
  char vbuff[CF_MAXVARSIZE];

strncpy(vbuff,ExtractFirstReference(a.packages.package_list_name_regex,item),CF_MAXVARSIZE-1);
sscanf(vbuff,"%s",name); /* trim */

strncpy(vbuff,ExtractFirstReference(a.packages.package_list_version_regex,item),CF_MAXVARSIZE-1);
sscanf(vbuff,"%s",version); /* trim */

if (a.packages.package_list_arch_regex)
   {
   strncpy(vbuff,ExtractFirstReference(a.packages.package_list_arch_regex,item),CF_MAXVARSIZE-1);
   sscanf(vbuff,"%s",arch); /* trim */
   }
else
   {
   strncpy(arch,"default",CF_MAXVARSIZE-1);
   }

if (strcmp(name,"CF_NOMATCH") == 0 || strcmp(version,"CF_NOMATCH") == 0 || strcmp(arch,"CF_NOMATCH") == 0)
   {
   return false;
   }

Debug(" -? Package line \"%s\"\n",item);
Debug(" -?      with name \"%s\"\n",name);
Debug(" -?      with version \"%s\"\n",version);
Debug(" -?      with architecture \"%s\"\n",arch);

return PrependPackageItem(list,name,version,arch,a,pp);
}
