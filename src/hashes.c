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

/*
 * This associative array implementation uses array with linear search up to
 * TINY_LIMIT elements, and then converts into full-fledged hash table with open
 * addressing.
 *
 * There is a lot of small hash tables, both iterating and deleting them as a
 * hashtable takes a lot of time, especially given associative hash tables are
 * created and destroyed for each scope entered and left.
 */

#define HASH_ENTRY_DELETED ((CfAssoc*)-1)

#define TINY_LIMIT 14

typedef struct AssocArray
   {
   CfAssoc *values[TINY_LIMIT];
   short size;
   } AssocArray;

struct AssocHashTable
   {
   union
      {
      struct AssocArray array;
      struct CfAssoc **buckets;
      };
   bool huge;
   };

/******************************************************************/

AssocHashTable *HashInit(void)
{
return calloc(1, sizeof(AssocHashTable));
}

/******************************************************************/

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

static void HashConvertToHuge(AssocHashTable *hashtable)
{
CfAssoc **buckets = calloc(1, sizeof(CfAssoc *) * CF_HASHTABLESIZE);
int i;

for (i = 0; i < hashtable->array.size; ++i)
   {
   /* This is a stripped down HugeHashInsertElement: it will fail on duplicate
    * elements or nearly-full hash table, or table with HASH_ENTRY_DELETED */
   CfAssoc *assoc = hashtable->array.values[i];
   int bucket = GetHash(assoc->lval);

   for(;;)
      {
      if (buckets[bucket] == NULL)
         {
         buckets[bucket] = assoc;
         break;
      }
      bucket = (bucket + 1) % CF_HASHTABLESIZE;
      }
   }

hashtable->huge = true;
hashtable->buckets = buckets;
}

/*******************************************************************/

static bool HugeHashInsertElement(AssocHashTable *hashtable, const char *element,
                                  const void *rval, char rtype, enum cfdatatype dtype)
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

static bool TinyHashInsertElement(AssocHashTable *hashtable, const char *element,
                                  const void *rval, char rtype, enum cfdatatype dtype)
{
int i;

if (hashtable->array.size == TINY_LIMIT)
   {
   HashConvertToHuge(hashtable);
   return HugeHashInsertElement(hashtable, element, rval, rtype, dtype);
   }

for (i = 0; i < hashtable->array.size; ++i)
   {
   if (strcmp(hashtable->array.values[i]->lval, element) == 0)
      {
      return false;
      }
   }

hashtable->array.values[hashtable->array.size++] = NewAssoc(element, rval, rtype, dtype);
return true;
}

/*******************************************************************/

bool HashInsertElement(AssocHashTable *hashtable, const char *element,
                       const void *rval, char rtype, enum cfdatatype dtype)
{
if (hashtable->huge)
   {
   return HugeHashInsertElement(hashtable, element, rval, rtype, dtype);
   }
else
   {
   return TinyHashInsertElement(hashtable, element, rval, rtype, dtype);
   }
}

/*******************************************************************/

static bool HugeHashDeleteElement(AssocHashTable *hashtable, const char *element)
{
int bucket = GetHash(element);
int i = bucket;

if (!hashtable->buckets)
   {
   return false;
   }

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
      i = (i + 1) % CF_HASHTABLESIZE;
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

static bool TinyHashDeleteElement(AssocHashTable *hashtable, const char *element)
{
int i;
for (i = 0; i < hashtable->array.size; ++i)
   {
   if (strcmp(hashtable->array.values[i]->lval, element) == 0)
      {
      int j;
      DeleteAssoc(hashtable->array.values[i]);
      for (j = i; j < hashtable->array.size - 1; ++j)
         {
         hashtable->array.values[j] = hashtable->array.values[j + 1];
         }
      hashtable->array.size--;
      return true;
      }
   }
return false;
}

/*******************************************************************/

bool HashDeleteElement(AssocHashTable *hashtable, const char *element)
{
if (hashtable->huge)
   {
   return HugeHashDeleteElement(hashtable, element);
   }
else
   {
   return TinyHashDeleteElement(hashtable, element);
   }
}

/*******************************************************************/

static CfAssoc *HugeHashLookupElement(AssocHashTable *hashtable, const char *element)
{
int bucket = GetHash(element);
int i = bucket;

if (!hashtable->buckets)
   {
   return NULL;
   }

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
      i = (i + 1) % CF_HASHTABLESIZE;
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

static CfAssoc *TinyHashLookupElement(AssocHashTable *hashtable, const char *element)
{
int i;
for (i = 0; i < hashtable->array.size; ++i)
   {
   if (strcmp(hashtable->array.values[i]->lval, element) == 0)
      {
      return hashtable->array.values[i];
      }
   }
return NULL;
}

/*******************************************************************/

CfAssoc *HashLookupElement(AssocHashTable *hashtable, const char *element)
{
if (hashtable->huge)
   {
   return HugeHashLookupElement(hashtable, element);
   }
else
   {
   return TinyHashLookupElement(hashtable, element);
   }
}

/*******************************************************************/

static void TinyHashClear(AssocHashTable *hashtable)
{
int i;
for (i = 0; i < hashtable->array.size; ++i)
   {
   DeleteAssoc(hashtable->array.values[i]);
   }
}

/*******************************************************************/

static void HugeHashClear(AssocHashTable *hashtable)
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
free(hashtable->buckets);
}

/*******************************************************************/

void HashClear(AssocHashTable *hashtable)
{
if (hashtable->huge)
   {
   HugeHashClear(hashtable);
   }
else
   {
   TinyHashClear(hashtable);
   }
}

/*******************************************************************/

void HashFree(AssocHashTable *hashtable)
{
HashClear(hashtable);
free(hashtable);
}

/*******************************************************************/

HashIterator HashIteratorInit(AssocHashTable *hashtable)
{
return (HashIterator) { hashtable, 0 };
}

/*******************************************************************/

static CfAssoc *HugeHashIteratorNext(HashIterator *i)
{
CfAssoc **buckets = i->hashtable->buckets;

for(; i->pos < CF_HASHTABLESIZE; i->pos++)
   {
   if (buckets[i->pos] != NULL && buckets[i->pos] != HASH_ENTRY_DELETED)
      {
      break;
      }
   }

if (i->pos == CF_HASHTABLESIZE)
   {
   return NULL;
   }
else
   {
   return buckets[i->pos++];
   }
}

/*******************************************************************/

static CfAssoc *TinyHashIteratorNext(HashIterator *i)
{
if (i->pos >= i->hashtable->array.size)
   {
   return NULL;
   }
else
   {
   return i->hashtable->array.values[i->pos++];
   }
}

/*******************************************************************/

CfAssoc *HashIteratorNext(HashIterator *i)
{
if (i->hashtable->huge)
   {
   return HugeHashIteratorNext(i);
   }
else
   {
   return TinyHashIteratorNext(i);
   }
}
