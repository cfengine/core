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

#include "platform.h"
#include "hash_map_priv.h"
#include "alloc.h"

/* FIXME: make configurable and move to map.c */
#define HASHMAP_BUCKETS 8192

HashMap *HashMapNew(MapHashFn hash_fn, MapKeyEqualFn equal_fn,
                    MapDestroyDataFn destroy_key_fn,
                    MapDestroyDataFn destroy_value_fn)
{
    HashMap *map = xcalloc(1, sizeof(HashMap));
    map->hash_fn = hash_fn;
    map->equal_fn = equal_fn;
    map->destroy_key_fn = destroy_key_fn;
    map->destroy_value_fn = destroy_value_fn;
    map->buckets = xcalloc(1, sizeof(BucketListItem *) * HASHMAP_BUCKETS);
    return map;
}

static unsigned HashMapGetBucket(const HashMap *map, const void *key)
{
    return map->hash_fn(key) % HASHMAP_BUCKETS;
}

bool HashMapInsert(HashMap *map, void *key, void *value)
{
    unsigned bucket = HashMapGetBucket(map, key);

    for (BucketListItem *i = map->buckets[bucket]; i != NULL; i = i->next)
    {
        if (map->equal_fn(i->value.key, key))
        {
            map->destroy_key_fn(key);
            map->destroy_value_fn(i->value.value);
            i->value.value = value;
            return true;
        }
    }

    BucketListItem *i = xcalloc(1, sizeof(BucketListItem));
    i->value.key = key;
    i->value.value = value;
    i->next = map->buckets[bucket];
    map->buckets[bucket] = i;

    return false;
}

bool HashMapRemove(HashMap *map, const void *key)
{
    unsigned bucket = HashMapGetBucket(map, key);

    /*
     * prev points to a previous "next" pointer to rewrite it in case value need
     * to be deleted
     */

    for (BucketListItem **prev = &map->buckets[bucket];
         *prev != NULL;
         prev = &((*prev)->next))
    {
        BucketListItem *cur = *prev;
        if (map->equal_fn(cur->value.key, key))
        {
            map->destroy_key_fn(cur->value.key);
            map->destroy_value_fn(cur->value.value);
            *prev = cur->next;
            free(cur);
            return true;
        }
    }

    return false;
}

MapKeyValue *HashMapGet(const HashMap *map, const void *key)
{
    unsigned bucket = HashMapGetBucket(map, key);

    for (BucketListItem *cur = map->buckets[bucket];
         cur != NULL;
         cur = cur->next)
    {
        if (map->equal_fn(cur->value.key, key))
        {
            return &cur->value;
        }
    }

    return NULL;
}

static void FreeBucketListItem(HashMap *map, BucketListItem *item)
{
    if (item->next)
    {
        FreeBucketListItem(map, item->next);
    }

    map->destroy_key_fn(item->value.key);
    map->destroy_value_fn(item->value.value);
    free(item);
}

void HashMapClear(HashMap *map)
{
    for (int i = 0; i < HASHMAP_BUCKETS; ++i)
    {
        if (map->buckets[i])
        {
            FreeBucketListItem(map, map->buckets[i]);
        }
        map->buckets[i] = NULL;
    }
}

void HashMapDestroy(HashMap *map)
{
    HashMapClear(map);
    free(map->buckets);
    free(map);
}

/******************************************************************************/

HashMapIterator HashMapIteratorInit(HashMap *map)
{
    return (HashMapIterator) { map, map->buckets[0], 0 };
}

MapKeyValue *HashMapIteratorNext(HashMapIterator *i)
{
    while (i->cur == NULL)
    {
        if (++i->bucket >= HASHMAP_BUCKETS)
        {
            return NULL;
        }

        i->cur = i->map->buckets[i->bucket];
    }

    MapKeyValue *ret = &i->cur->value;
    i->cur = i->cur->next;
    return ret;
}
