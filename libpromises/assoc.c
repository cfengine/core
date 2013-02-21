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

#include "assoc.h"

#include "hashes.h"
#include "rlist.h"

CfAssoc *NewAssoc(const char *lval, Rval rval, DataType dt)
{
    CfAssoc *ap;

    ap = xmalloc(sizeof(CfAssoc));

/* Make a private copy because promises are ephemeral in expansion phase */

    ap->lval = xstrdup(lval);
    ap->rval = RvalCopy(rval);
    ap->dtype = dt;

    return ap;
}

/*******************************************************************/

void DeleteAssoc(CfAssoc *ap)
{
    if (ap == NULL)
    {
        return;
    }

    CfDebug(" ----> Delete variable association %s\n", ap->lval);

    free(ap->lval);
    RvalDestroy(ap->rval);

    free(ap);

}

/*******************************************************************/

CfAssoc *CopyAssoc(CfAssoc *old)
{
    if (old == NULL)
    {
        return NULL;
    }

    return NewAssoc(old->lval, old->rval, old->dtype);
}

/*******************************************************************/

CfAssoc *AssocNewReference(const char *lval, Rval rval, DataType dtype)
{
    CfAssoc *ap = NULL;

    ap = xmalloc(sizeof(CfAssoc));

    ap->lval = xstrdup(lval);
    ap->rval = rval;
    ap->dtype = dtype;

    return ap;
}

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

typedef struct
{
    CfAssoc *values[TINY_LIMIT];
    short size;
} AssocArray;

struct AssocHashTable_
{
    union
    {
        AssocArray array;
        CfAssoc **buckets;
    };
    bool huge;
};

/******************************************************************/

AssocHashTable *HashInit(void)
{
    return xcalloc(1, sizeof(AssocHashTable));
}

/******************************************************************/

void HashCopy(AssocHashTable *newhash, AssocHashTable *oldhash)
{
    AssocHashTableIterator i = HashIteratorInit(oldhash);
    CfAssoc *assoc;

    while ((assoc = HashIteratorNext(&i)))
    {
        HashInsertElement(newhash, assoc->lval, assoc->rval, assoc->dtype);
    }
}

/*******************************************************************/

static void HashConvertToHuge(AssocHashTable *hashtable)
{
    CfAssoc **buckets = xcalloc(1, sizeof(CfAssoc *) * CF_HASHTABLESIZE);
    int i;

    for (i = 0; i < hashtable->array.size; ++i)
    {
        /* This is a stripped down HugeHashInsertElement: it will fail on duplicate
         * elements or nearly-full hash table, or table with HASH_ENTRY_DELETED */
        CfAssoc *assoc = hashtable->array.values[i];
        int bucket = GetHash(assoc->lval, CF_HASHTABLESIZE);

        for (;;)
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

static bool HugeHashInsertElement(AssocHashTable *hashtable, const char *element, Rval rval, DataType dtype)
{
    int bucket = GetHash(element, CF_HASHTABLESIZE);
    int i = bucket;

    do
    {
        /* Free bucket is found */
        if (hashtable->buckets[i] == NULL || hashtable->buckets[i] == HASH_ENTRY_DELETED)
        {
            hashtable->buckets[i] = NewAssoc(element, rval, dtype);
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

static bool TinyHashInsertElement(AssocHashTable *hashtable, const char *element, Rval rval, DataType dtype)
{
    int i;

    if (hashtable->array.size == TINY_LIMIT)
    {
        HashConvertToHuge(hashtable);
        return HugeHashInsertElement(hashtable, element, rval, dtype);
    }

    for (i = 0; i < hashtable->array.size; ++i)
    {
        if (strcmp(hashtable->array.values[i]->lval, element) == 0)
        {
            return false;
        }
    }

    /* Do not inline NewAssoc into a assignment -- NewAssoc calls CopyRvalItem,
       which can call GetVariable (OMG), which calls HashLookupElement, so we
       need to be sure hash table is in consistent state while calling
       NewAssoc. If NewAssoc is in the right-hand side of the assignment, then
       compiler is free to choose the order of increment and NewAssoc call, so
       HashLookupElement might end up reading values by NULL pointer. Long-term
       solution is to fix CopyRvalItem. */

    CfAssoc *a = NewAssoc(element, rval, dtype);

    hashtable->array.values[hashtable->array.size++] = a;
    return true;
}

/*******************************************************************/

bool HashInsertElement(AssocHashTable *hashtable, const char *element, Rval rval, DataType dtype)
{
    if (hashtable->huge)
    {
        return HugeHashInsertElement(hashtable, element, rval, dtype);
    }
    else
    {
        return TinyHashInsertElement(hashtable, element, rval, dtype);
    }
}

/*******************************************************************/

static bool HugeHashDeleteElement(AssocHashTable *hashtable, const char *element)
{
    int bucket = GetHash(element, CF_HASHTABLESIZE);
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
    int bucket = GetHash(element, CF_HASHTABLESIZE);
    int i = bucket;

    do
    {
        /* End of allocated chunk */
        if (hashtable->buckets[i] == NULL)
        {
            return NULL;
        }

        if (hashtable->buckets[i] != HASH_ENTRY_DELETED && strcmp(element, hashtable->buckets[i]->lval) == 0)
        {
            return hashtable->buckets[i];
        }

        i = (i + 1) % CF_HASHTABLESIZE;
    }
    while (i != bucket);

/* Looped through whole hashtable */
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
    hashtable->array.size = 0;
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
    memset(hashtable->buckets, 0, sizeof(CfAssoc *) * CF_HASHTABLESIZE);
}

/*******************************************************************/

void HashClear(AssocHashTable *hashtable)
{
    if (hashtable->huge)
    {
        HugeHashClear(hashtable);
        free(hashtable->buckets);
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

AssocHashTableIterator HashIteratorInit(AssocHashTable *hashtable)
{
    return (AssocHashTableIterator) { hashtable, 0 };
}

/*******************************************************************/

static CfAssoc *HugeHashIteratorNext(AssocHashTableIterator *i)
{
    CfAssoc **buckets = i->hashtable->buckets;

    for (; i->pos < CF_HASHTABLESIZE; i->pos++)
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

static CfAssoc *TinyHashIteratorNext(AssocHashTableIterator *i)
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

CfAssoc *HashIteratorNext(AssocHashTableIterator *i)
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

/*******************************************************************/

void HashToList(Scope *sp, Rlist **list)
{
    if (sp == NULL)
    {
        return;
    }

    AssocHashTableIterator i = HashIteratorInit(sp->hashtable);
    CfAssoc *assoc;

    while ((assoc = HashIteratorNext(&i)))
    {
        RlistPrependScalar(list, assoc->lval);
    }
}
