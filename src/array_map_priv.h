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

#ifndef CFENGINE_ARRAY_MAP_PRIV_H
#define CFENGINE_ARRAY_MAP_PRIV_H

#include "map_common.h"

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

/*
 * Returns false if the limit of the array size has been reached.
 */
bool ArrayMapInsert(ArrayMap *map, void *key, void *value);

bool ArrayMapRemove(ArrayMap *map, const void *key);
MapKeyValue *ArrayMapGet(const ArrayMap *map, const void *key);
void ArrayMapClear(ArrayMap *map);
void ArrayMapDestroy(ArrayMap *map);

/******************************************************************************/

ArrayMapIterator ArrayMapIteratorInit(ArrayMap *map);
MapKeyValue *ArrayMapIteratorNext(ArrayMapIterator *i);

#endif
