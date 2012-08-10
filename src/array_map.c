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
#include "array_map_priv.h"
#include "alloc.h"

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

bool ArrayMapInsert(ArrayMap *map, void *key, void *value)
{
    if (map->size == TINY_LIMIT)
    {
        return false;
    }

    for (int i = 0; i < map->size; ++i)
    {
        if (map->equal_fn(map->values[i].key, key))
        {
            map->destroy_key_fn(key);
            map->destroy_value_fn(map->values[i].value);
            map->values[i].value = value;
            return true;
        }
    }

    map->values[map->size++] = (MapKeyValue) { key, value };
    return true;
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

void ArrayMapDestroy(ArrayMap *map)
{
    ArrayMapClear(map);
    free(map->values);
    free(map);
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
