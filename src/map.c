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
#include "map.h"
#include "alloc.h"
#include "array_map_priv.h"
#include "hash_map_priv.h"

/*
 * This associative array implementation uses array with linear search up to
 * TINY_LIMIT elements, and then converts into full-fledged hash table with open
 * addressing.
 *
 * There is a lot of small hash tables, both iterating and deleting them as a
 * hashtable takes a lot of time, especially given associative hash tables are
 * created and destroyed for each scope entered and left.
 */

struct Map_
{
    MapHashFn hash_fn;

    union
    {
        ArrayMap *arraymap;
        HashMap *hashmap;
    };
};

static unsigned IdentityHashFn(const void *ptr)
{
    return (unsigned)(uintptr_t)ptr;
}

static bool IdentityEqualFn(const void *p1, const void *p2)
{
    return p1 == p2;
}

static void NopDestroyFn(void *p1)
{
}

/*
 * hash_fn is used as a flag "this map uses ArrayMap still". We could have a
 * separate boolean flag for that, but given we have to store hash_fn somewhere
 * anyway, let's reuse it this way. Saves us 4-8 bytes for each map.
 */
static bool IsArrayMap(const Map *map)
{
    return map->hash_fn != NULL;
}

Map *MapNew(MapHashFn hash_fn,
            MapKeyEqualFn equal_fn,
            MapDestroyDataFn destroy_key_fn,
            MapDestroyDataFn destroy_value_fn)
{
    if (hash_fn == NULL)
    {
        hash_fn = &IdentityHashFn;
    }

    if (equal_fn == NULL)
    {
        equal_fn = &IdentityEqualFn;
    }

    if (destroy_key_fn == NULL)
    {
        destroy_key_fn = &NopDestroyFn;
    }

    if (destroy_value_fn == NULL)
    {
        destroy_value_fn = &NopDestroyFn;
    }

    Map *map = xcalloc(1, sizeof(Map));
    map->arraymap = ArrayMapNew(equal_fn, destroy_key_fn, destroy_value_fn);
    map->hash_fn = hash_fn;
    return map;
}

static void ConvertToHashMap(Map *map)
{
    HashMap *hashmap = HashMapNew(map->hash_fn,
                                  map->arraymap->equal_fn,
                                  map->arraymap->destroy_key_fn,
                                  map->arraymap->destroy_value_fn);

    /* We have to use internals of ArrayMap here, as we don't want to
       destroy the values in ArrayMapDestroy */

    for (int i = 0; i < map->arraymap->size; ++i)
    {
        HashMapInsert(hashmap,
                      map->arraymap->values[i].key,
                      map->arraymap->values[i].value);
    }

    free(map->arraymap->values);
    free(map->arraymap);

    map->hashmap = hashmap;
    map->hash_fn = NULL;
}

void MapInsert(Map *map, void *key, void *value)
{
    if (IsArrayMap(map))
    {
        if (ArrayMapInsert(map->arraymap, key, value))
        {
            return;
        }
        else
        {
            ConvertToHashMap(map);
        }
    }

    HashMapInsert(map->hashmap, key, value);
}

/*
 * The best we can get out of C type system. Caller should make sure that if
 * argument is const, it does not modify the result.
 */
static MapKeyValue *MapGetRaw(const Map *map, const void *key)
{
    if (IsArrayMap(map))
    {
        return ArrayMapGet((ArrayMap *)map->arraymap, key);
    }
    else
    {
        return HashMapGet((HashMap *)map->hashmap, key);
    }
}

bool MapHasKey(const Map *map, const void *key)
{
    return MapGetRaw(map, key) != NULL;
}

void *MapGet(Map *map, const void *key)
{
    MapKeyValue *kv = MapGetRaw(map, key);
    return kv ? kv->value : NULL;
}

bool MapRemove(Map *map, const void *key)
{
    if (IsArrayMap(map))
    {
        return ArrayMapRemove(map->arraymap, key);
    }
    else
    {
        return HashMapRemove(map->hashmap, key);
    }
}

void MapClear(Map *map)
{
    if (IsArrayMap(map))
    {
        return ArrayMapClear(map->arraymap);
    }
    else
    {
        return HashMapClear(map->hashmap);
    }
}

void MapDestroy(Map *map)
{
    if (IsArrayMap(map))
    {
        ArrayMapDestroy(map->arraymap);
    }
    else
    {
        HashMapDestroy(map->hashmap);
    }
    free(map);
}

/******************************************************************************/

MapIterator MapIteratorInit(Map *map)
{
    MapIterator i;
    if (IsArrayMap(map))
    {
        i.is_array = true;
        i.arraymap_iter = ArrayMapIteratorInit(map->arraymap);
    }
    else
    {
        i.is_array = false;
        i.hashmap_iter = HashMapIteratorInit(map->hashmap);
    }
    return i;
}

MapKeyValue *MapIteratorNext(MapIterator *i)
{
    if (i->is_array)
    {
        return ArrayMapIteratorNext(&i->arraymap_iter);
    }
    else
    {
        return HashMapIteratorNext(&i->hashmap_iter);
    }
}
