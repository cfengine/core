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

#ifndef CFENGINE_MAP_H
#define CFENGINE_MAP_H

#include "hash_map_priv.h"
#include "array_map_priv.h"

/*
 * Map structure. Details are encapsulated.
 */
typedef struct Map_ Map;

Map *MapNew(MapHashFn hash_fn,
            MapKeyEqualFn equal_fn,
            MapDestroyDataFn destroy_key_fn,
            MapDestroyDataFn destroy_value_fn);

/*
 * Returns 'true' if the key was previously used in the map, otherwise 'false'.
 * If the key is in the map, value get replaced. Old value is destroyed.
 */
void MapInsert(Map *map, void *key, void *value);

/*
 * Returns whether the key is in the map.
 */
bool MapHasKey(const Map *map, const void *key);

/*
 * Returns the value if the key is in map, NULL otherwise. To distinguish
 * between NULL as a value and NULL as a lack of entry, use MapHasKey.
 */
void *MapGet(Map *map, const void *key);

/*
 * Remove key/value pair from the map. Returns 'true' if key was present in the
 * map.
 */
bool MapRemove(Map *map, const void *key);

/*
 * MapIterator i = MapIteratorInit(map);
 * MapKeyValue *item;
 * while ((item = MapIteratorNext(&i)))
 * {
 *     // do something with item->key, item->value
 * }
 */


typedef struct
{
    bool is_array;
    union
    {
        ArrayMapIterator arraymap_iter;
        HashMapIterator hashmap_iter;
    };
} MapIterator;

MapIterator MapIteratorInit(Map *map);

MapKeyValue *MapIteratorNext(MapIterator *i);

/*
 * Clear the whole map
 */
void MapClear(Map *map);

/*
 * Destroy the map object.
 */
void MapDestroy(Map *map);


#define TYPED_MAP_DECLARE(Prefix, KeyType, ValueType)                   \
    typedef struct                                                      \
    {                                                                   \
        Map *impl;                                                      \
    } Prefix##Map;                                                      \
                                                                        \
    Prefix##Map *Prefix##MapNew(void);                                  \
    void Prefix##MapInsert(const Prefix##Map *map, KeyType key, ValueType value); \
    bool Prefix##MapHasKey(const Prefix##Map *map, const KeyType key);  \
    ValueType Prefix##MapGet(const Prefix##Map *map, const KeyType key); \
    bool Prefix##MapRemove(const Prefix##Map *map, const KeyType key);  \
    void Prefix##MapClear(Prefix##Map *map);                            \
    void Prefix##MapDestroy(Prefix##Map *map);                   \

#define TYPED_MAP_DEFINE(Prefix, KeyType, ValueType, hash_fn, equal_fn, \
                         destroy_key_fn, destroy_value_fn)              \
                                                                        \
    Prefix##Map *Prefix##MapNew(void)                                   \
    {                                                                   \
        Prefix##Map *map = xcalloc(1, sizeof(Prefix##Map));             \
        map->impl = MapNew(hash_fn, equal_fn,                           \
                           destroy_key_fn, destroy_value_fn);           \
        return map;                                                     \
    }                                                                   \
                                                                        \
    void Prefix##MapInsert(const Prefix##Map *map, KeyType key, ValueType value) \
    {                                                                   \
        MapInsert(map->impl, key, value);                               \
    }                                                                   \
                                                                        \
    bool Prefix##MapHasKey(const Prefix##Map *map, const KeyType key)   \
    {                                                                   \
        return MapHasKey(map->impl, key);                               \
    }                                                                   \
                                                                        \
    ValueType Prefix##MapGet(const Prefix##Map *map, const KeyType key) \
    {                                                                   \
        return MapGet(map->impl, key);                                  \
    }                                                                   \
                                                                        \
    bool Prefix##MapRemove(const Prefix##Map *map, const KeyType key)   \
    {                                                                   \
        return MapRemove(map->impl, key);                               \
    }                                                                   \
                                                                        \
    void Prefix##MapClear(Prefix##Map *map)                             \
    {                                                                   \
        return MapClear(map->impl);                                     \
    }                                                                   \
                                                                        \
    void Prefix##MapDestroy(Prefix##Map *map)                           \
    {                                                                   \
        MapDestroy(map->impl);                                          \
        free(map);                                                      \
    }

#endif
