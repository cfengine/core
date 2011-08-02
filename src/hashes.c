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

#define HASH_ENTRY_DELETED ((CfAssoc*)-1)

struct AssocHashTable
   {
   struct CfAssoc *buckets[CF_HASHTABLESIZE];
   };

/******************************************************************/

AssocHashTable *HashInit(void)
{
return calloc(1, sizeof(AssocHashTable));
}

/******************************************************************/

/* Call only on empty newhash */
void HashCopy(struct AssocHashTable *newhash, struct AssocHashTable *oldhash)

{
HashIterator i = HashIteratorInit(oldhash);
CfAssoc *assoc;

while ((assoc = HashIteratorNext(&i)))
   {
   HashInsertElement(newhash, assoc->lval, assoc->rval, assoc->rtype, assoc->dtype);
   }
}

/*******************************************************************/

int GetHash(const char *name)

{
return OatHash(name);
}

/*******************************************************************/

bool HashInsertElement(AssocHashTable *hashtable, const char *element,
                       void *rval, char rtype, enum cfdatatype dtype)
{
int bucket = GetHash(element);
int i = bucket;

do
   {
   /* Free bucket is found */
   if (hashtable->buckets[i] == NULL || hashtable->buckets[i] == HASH_ENTRY_DELETED)
      {
      hashtable->buckets[i] = NewAssoc(element, rval, rtype, dtype);
      return true;
      }

   /* Collision -- this element already exists */
   if (strcmp(element, hashtable->buckets[i]->lval) == 0)
      {
      return false;
      }

   i = (i + 1) % CF_HASHTABLESIZE;
   }
while (i != bucket);

/* Hash table is full */
return false;
}

/*******************************************************************/

bool HashDeleteElement(AssocHashTable *hashtable, const char *element)
{
int bucket = GetHash(element);
int i = bucket;

do
   {
   /* End of allocated chunk */
   if (hashtable->buckets[i] == NULL)
      {
      break;
      }

   /* Keep looking */
   if (hashtable->buckets[i] == HASH_ENTRY_DELETED)
      {
      continue;
      }

   /* Element is found */
   if (strcmp(element, hashtable->buckets[i]->lval) == 0)
      {
      DeleteAssoc(hashtable->buckets[i]);
      hashtable->buckets[i] = NULL;
      return true;
      }

   i = (i + 1) % CF_HASHTABLESIZE;
   }
while (i != bucket);

/* Either looped through hashtable or found a NULL */
return false;
}

/*******************************************************************/

CfAssoc *HashLookupElement(AssocHashTable *hashtable, const char *element)
{
int bucket = GetHash(element);
int i = bucket;

do
   {
   /* End of allocated chunk */
   if (hashtable->buckets[i] == NULL)
      {
      break;
      }

   /* Keep looking */
   if (hashtable->buckets[i] == HASH_ENTRY_DELETED)
      {
      continue;
      }

   /* Element is found */
   if (strcmp(element, hashtable->buckets[i]->lval) == 0)
      {
      return hashtable->buckets[i];
      }

   i = (i + 1) % CF_HASHTABLESIZE;
   }
while (i != bucket);

/* Either looped through hashtable or found a NULL */
return NULL;
}

/*******************************************************************/

static void HashClearInt(AssocHashTable *hashtable)
{
int i;
for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   if (hashtable->buckets[i] != NULL)
      {
      if (hashtable->buckets[i] != HASH_ENTRY_DELETED)
         {
         DeleteAssoc(hashtable->buckets[i]);
         }
      }
   }
}

/*******************************************************************/

void HashClear(AssocHashTable *hashtable)
{
HashClearInt(hashtable);
memset(hashtable->buckets, 0, sizeof(CF_HASHTABLESIZE * sizeof(CfAssoc *)));
}

/*******************************************************************/

void HashFree(AssocHashTable *hashtable)
{
HashClearInt(hashtable);
free(hashtable);
}

/*******************************************************************/

HashIterator HashIteratorInit(AssocHashTable *hashtable)
{
return (HashIterator) { hashtable->buckets, 0 };
}

/*******************************************************************/

CfAssoc *HashIteratorNext(HashIterator *i)
{
for(; i->bucket < CF_HASHTABLESIZE; i->bucket++)
   {
   if (i->hash[i->bucket] != NULL && i->hash[i->bucket] != HASH_ENTRY_DELETED)
      {
      break;
      }
   }

if (i->bucket == CF_HASHTABLESIZE)
   {
   return NULL;
   }
else
   {
   return i->hash[i->bucket++];
   }
}
