/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#ifndef CFENGINE_HASH_MAP_PRIV_H
#define CFENGINE_HASH_MAP_PRIV_H

#include <map_common.h>

typedef struct BucketListItem_
{
    MapKeyValue value;
    struct BucketListItem_ *next;
} BucketListItem;

typedef struct
{
    MapHashFn hash_fn;
    MapKeyEqualFn equal_fn;
    MapDestroyDataFn destroy_key_fn;
    MapDestroyDataFn destroy_value_fn;
    BucketListItem **buckets;
} HashMap;

typedef struct
{
    HashMap *map;
    BucketListItem *cur;
    int bucket;
} HashMapIterator;

HashMap *HashMapNew(MapHashFn hash_fn, MapKeyEqualFn equal_fn,
                    MapDestroyDataFn destroy_key_fn,
                    MapDestroyDataFn destroy_value_fn);

bool HashMapInsert(HashMap *map, void *key, void *value);
bool HashMapRemove(HashMap *map, const void *key);
MapKeyValue *HashMapGet(const HashMap *map, const void *key);
void HashMapClear(HashMap *map);
void HashMapSoftDestroy(HashMap *map);
void HashMapDestroy(HashMap *map);
void HashMapPrintStats(const HashMap *hmap, FILE *f);

/******************************************************************************/

HashMapIterator HashMapIteratorInit(HashMap *m);
MapKeyValue *HashMapIteratorNext(HashMapIterator *i);

#endif
