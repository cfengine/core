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

#ifndef CFENGINE_ARRAY_MAP_PRIV_H
#define CFENGINE_ARRAY_MAP_PRIV_H

#include <map_common.h>

typedef struct
{
    MapKeyEqualFn equal_fn;
    MapDestroyDataFn destroy_key_fn;
    MapDestroyDataFn destroy_value_fn;
    MapKeyValue *values;
    short size;
} ArrayMap;

typedef struct
{
    ArrayMap *map;
    int pos;
} ArrayMapIterator;

ArrayMap *ArrayMapNew(MapKeyEqualFn equal_fn,
                      MapDestroyDataFn destroy_key_fn,
                      MapDestroyDataFn destroy_value_fn);

/**
 * @retval 0 if the limit of the array size has been reached,
 *           and no insertion took place.
 * @retval 1 if the key was found and the value was replaced.
 * @retval 2 if the key-value pair was not found and inserted as new.
 */
int ArrayMapInsert(ArrayMap *map, void *key, void *value);

bool ArrayMapRemove(ArrayMap *map, const void *key);
MapKeyValue *ArrayMapGet(const ArrayMap *map, const void *key);
void ArrayMapClear(ArrayMap *map);
void ArrayMapSoftDestroy(ArrayMap *map);
void ArrayMapDestroy(ArrayMap *map);

/******************************************************************************/

ArrayMapIterator ArrayMapIteratorInit(ArrayMap *map);
MapKeyValue *ArrayMapIteratorNext(ArrayMapIterator *i);

#endif
