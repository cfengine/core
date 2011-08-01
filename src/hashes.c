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
/* File: hashes.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

static void EditHashValue(char *scopeid,char *lval,void *rval);

/*******************************************************************/
/* Hashes                                                          */
/*******************************************************************/

void InitHashes(struct CfAssoc **table)

{ int i;

for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   table[i] = NULL;
   }
}


/******************************************************************/

void CopyHashes(struct CfAssoc **newhash,struct CfAssoc **oldhash)

{ int i;

for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   newhash[i] = CopyAssoc(oldhash[i]);
   }
}

/******************************************************************/

static void EditHashValue(char *scopeid,char *lval,void *rval)

{ int found, slot = GetHash(lval);
  int i = slot;
  struct Scope *ptr = GetScope(scopeid);
  struct CfAssoc *ap;

Debug("EditHashValue(%s,%s)\n",scopeid,lval);
  
if (CompareVariable(lval,ptr->hashtable[slot]) != 0)
   {
   /* Recover from hash collision */
   
   while (true)
      {
      i++;

      if (i >= CF_HASHTABLESIZE-1)
         {
         i = 0;
         }

      if (CompareVariable(lval,ptr->hashtable[i]) == 0)
         {
         found = true;
         break;
         }

      /* Removed autolookup in Unix environment variables -
         implement as getenv() fn instead */

      if (i == slot)
         {
         found = false;
         break;
         }
      }

   if (!found)
      {
      Debug("No such variable found %s.%s\n",scopeid,lval);
      return;
      }
   }

ap = ptr->hashtable[i];
ap->rval = rval;
}
   
/******************************************************************/

void DeleteHashes(struct CfAssoc **hashtable)

{ int i;

if (hashtable)
   {
   for (i = 0; i < CF_HASHTABLESIZE; i++)
      {
      if (hashtable[i] != NULL)
         {
	 DeleteAssoc(hashtable[i]);
         hashtable[i] = NULL;
         }
      }
   }
}

/*******************************************************************/

void PrintHashes(FILE *fp,struct CfAssoc **table,int html)

{ int i;

if (html)
   {
   fprintf(fp,"<table class=border width=600>\n");
   fprintf (fp,"<tr><th>id</th><th>dtype</th><th>rtype</th><th>identifier</th><th>Rvalue</th></tr>\n");         
   }
 
for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   if (table[i] != NULL)
      {
      if (html)
         {
         fprintf (fp,"<tr><td> %5d </td><th>%8s</th><td> %c</td><td> %s</td><td> ",i,CF_DATATYPES[table[i]->dtype],table[i]->rtype,table[i]->lval);
         ShowRval(fp,table[i]->rval,table[i]->rtype);
         fprintf(fp,"</td></tr>\n");         
         }
      else
         {
         fprintf (fp," %5d : %8s %c %s = ",i,CF_DATATYPES[table[i]->dtype],table[i]->rtype,table[i]->lval);
         ShowRval(fp,table[i]->rval,table[i]->rtype);
         fprintf(fp,"\n");
         }
      }
   }

if (html)
   {
   fprintf(fp,"</table>\n");
   }
}

/*******************************************************************/

int GetHash(const char *name)

{
return OatHash(name);
}

/*******************************************************************/

bool HashInsertElement(CfAssoc **hashtable, const char *element,
                       void *rval, char rtype, enum cfdatatype dtype)
{
int bucket = GetHash(element);
int i = bucket;

do
   {
   /* Collision -- this element already exists */
   if (CompareVariable(element, hashtable[i]) == 0)
      {
      return false;
      }

   /* Free bucket is found */
   if (hashtable[i] == NULL)
      {
      hashtable[i] = NewAssoc(element, rval, rtype, dtype);
      return true;
      }

   i = (i + 1) % CF_HASHTABLESIZE;
   }
while (i != bucket);

/* Hash table is full */
return false;
}

/*******************************************************************/

bool HashDeleteElement(CfAssoc **hashtable, const char *element)
{
int bucket = GetHash(element);
int i = bucket;

do
   {
   /* Element is found */
   if (hashtable[i] && strcmp(element, hashtable[i]->lval) == 0)
      {
      DeleteAssoc(hashtable[i]);
      hashtable[i] = NULL;
      return true;
      }

   i = (i + 1) % CF_HASHTABLESIZE;
   }
while (i != bucket);

/* Looped through whole hashtable and did not find needed element */
return false;
}

/*******************************************************************/

CfAssoc *HashLookupElement(CfAssoc **hashtable, const char *element)
{
int bucket = GetHash(element);
int i = bucket;

do
   {
   /* Element is found */
   if (CompareVariable(element, hashtable[i]) == 0)
      {
      return hashtable[i];
      }

   i = (i + 1) % CF_HASHTABLESIZE;
   }
while (i != bucket);

/* Looped through whole hashtable and did not find needed element */
return NULL;
}

/*******************************************************************/

void HashClear(CfAssoc **hashtable)
{
int i;
for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   if (hashtable[i] != NULL)
      {
      DeleteAssoc(hashtable[i]);
      hashtable[i] = NULL;
      }
   }
}

/*******************************************************************/

HashIterator HashIteratorInit(CfAssoc **hashtable)
{
return (HashIterator) { hashtable, 0 };
}

/*******************************************************************/

CfAssoc *HashIteratorNext(HashIterator *i)
{
while (i->bucket < CF_HASHTABLESIZE && i->hash[i->bucket] == NULL)
    i->bucket++;

if (i->bucket == CF_HASHTABLESIZE)
   {
   return NULL;
   }
else
   {
   return i->hash[i->bucket++];
   }
}
