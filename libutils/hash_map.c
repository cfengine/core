/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <platform.h>
#include <hash_map_priv.h>
#include <alloc.h>

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

static unsigned int HashMapGetBucket(const HashMap *map, const void *key)
{
    return map->hash_fn(key, 0, HASHMAP_BUCKETS);
}

/**
 * @retval true if value was preexisting in the map and got replaced.
 */
bool HashMapInsert(HashMap *map, void *key, void *value)
{
    unsigned bucket = HashMapGetBucket(map, key);

    for (BucketListItem *i = map->buckets[bucket]; i != NULL; i = i->next)
    {
        if (map->equal_fn(i->value.key, key))
        {
            /* Replace the key with the new one despite those two being the
             * same, since the new key might be referenced somewhere inside
             * the new value. */
            map->destroy_key_fn(i->value.key);
            map->destroy_value_fn(i->value.value);
            i->value.key   = key;
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

/* Do not destroy value item */
static void FreeBucketListItemSoft(HashMap *map, BucketListItem *item)
{
    if (item->next)
    {
        FreeBucketListItemSoft(map, item->next);
    }

    map->destroy_key_fn(item->value.key);
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

void HashMapSoftDestroy(HashMap *map)
{
    if (map)
    {
        for (int i = 0; i < HASHMAP_BUCKETS; ++i)
        {
            if (map->buckets[i])
            {
                FreeBucketListItemSoft(map, map->buckets[i]);
            }
            map->buckets[i] = NULL;
        }

        free(map->buckets);
        free(map);
    }
}

void HashMapDestroy(HashMap *map)
{
    if (map)
    {
        HashMapClear(map);
        free(map->buckets);
        free(map);
    }
}

void HashMapPrintStats(const HashMap *hmap, FILE *f)
{
    size_t bucket_lengths[HASHMAP_BUCKETS] = { 0 };
    size_t num_el = 0;
    size_t num_buckets = 0;

    for (int i = 0; i < HASHMAP_BUCKETS; i++)
    {
        BucketListItem *b = hmap->buckets[i];
        if (b != NULL)
        {
            num_buckets++;
        }
        while (b != NULL)
        {
            num_el++;
            bucket_lengths[i]++;
            b = b->next;
        }

    }

    fprintf(f, "\tTotal number of buckets:     %5d\n", HASHMAP_BUCKETS);
    fprintf(f, "\tNumber of non-empty buckets: %5zu\n", num_buckets);
    fprintf(f, "\tTotal number of elements:    %5zu\n", num_el);
    fprintf(f, "\tAverage elements per non-empty bucket (load factor): %5.2f\n",
            (float) num_el / num_buckets);

    fprintf(f, "\tThe 10 longest buckets are: \n");
    for (int j = 0; j < 10; j++)
    {
        /* Find the maximum 10 times, zeroing it after printing it. */
        int longest_bucket_id = 0;
        for (int i = 0; i < HASHMAP_BUCKETS; i++)
        {
            if (bucket_lengths[i] > bucket_lengths[longest_bucket_id])
            {
                longest_bucket_id = i;
            }
        }
        fprintf(f, "\t\tbucket %5d with %zu elements\n",
                longest_bucket_id, bucket_lengths[longest_bucket_id]);
        bucket_lengths[longest_bucket_id] = 0;
    }
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
