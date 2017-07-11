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
#include <array_map_priv.h>
#include <alloc.h>

/* FIXME: make configurable and move to map.c */
#define TINY_LIMIT 14

ArrayMap *ArrayMapNew(MapKeyEqualFn equal_fn,
                      MapDestroyDataFn destroy_key_fn,
                      MapDestroyDataFn destroy_value_fn)
{
    ArrayMap *map = xcalloc(1, sizeof(ArrayMap));
    map->equal_fn = equal_fn;
    map->destroy_key_fn = destroy_key_fn;
    map->destroy_value_fn = destroy_value_fn;
    map->values = xcalloc(1, sizeof(MapKeyValue) * TINY_LIMIT);
    return map;
}

int ArrayMapInsert(ArrayMap *map, void *key, void *value)
{
    if (map->size == TINY_LIMIT)
    {
        return 0;
    }

    for (int i = 0; i < map->size; ++i)
    {
        if (map->equal_fn(map->values[i].key, key))
        {
            /* Replace the key with the new one despite those two being the
             * same, since the new key might be referenced somewhere inside
             * the new value. */
            map->destroy_key_fn(map->values[i].key);
            map->destroy_value_fn(map->values[i].value);
            map->values[i].key   = key;
            map->values[i].value = value;
            return 1;
        }
    }

    map->values[map->size++] = (MapKeyValue) { key, value };
    return 2;
}

bool ArrayMapRemove(ArrayMap *map, const void *key)
{
    for (int i = 0; i < map->size; ++i)
    {
        if (map->equal_fn(map->values[i].key, key))
        {
            map->destroy_key_fn(map->values[i].key);
            map->destroy_value_fn(map->values[i].value);

            memmove(map->values + i, map->values + i + 1,
                    sizeof(MapKeyValue) * (map->size - i - 1));

            map->size--;
            return true;
        }
    }
    return false;
}

MapKeyValue *ArrayMapGet(const ArrayMap *map, const void *key)
{
    for (int i = 0; i < map->size; ++i)
    {
        if (map->equal_fn(map->values[i].key, key))
        {
            return map->values + i;
        }
    }
    return NULL;
}

void ArrayMapClear(ArrayMap *map)
{
    for (int i = 0; i < map->size; ++i)
    {
        map->destroy_key_fn(map->values[i].key);
        map->destroy_value_fn(map->values[i].value);
    }
    map->size = 0;
}

void ArrayMapSoftDestroy(ArrayMap *map)
{
    if (map)
    {
        for (int i = 0; i < map->size; ++i)
        {
            map->destroy_key_fn(map->values[i].key);
        }
        map->size = 0;

        free(map->values);
        free(map);
    }
}

void ArrayMapDestroy(ArrayMap *map)
{
    if (map)
    {
        ArrayMapClear(map);
        free(map->values);
        free(map);
    }
}

/******************************************************************************/

ArrayMapIterator ArrayMapIteratorInit(ArrayMap *map)
{
    return (ArrayMapIterator) { map, 0 };
}

MapKeyValue *ArrayMapIteratorNext(ArrayMapIterator *i)
{
    if (i->pos >= i->map->size)
    {
        return NULL;
    }
    else
    {
        return &i->map->values[i->pos++];
    }
}
