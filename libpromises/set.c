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

#include "set.h"


Set *SetNew(MapHashFn element_hash_fn,
            MapKeyEqualFn element_equal_fn,
            MapDestroyDataFn element_destroy_fn)
{
    return MapNew(element_hash_fn, element_equal_fn, element_destroy_fn, NULL);
}

void SetDestroy(void *set)
{
    MapDestroy(set);
}

void SetAdd(Set *set, void *element)
{
    MapInsert(set, element, element);
}

bool SetContains(const Set *set, const void *element)
{
    return MapHasKey(set, element);
}

bool SetRemove(Set *set, const void *element)
{
    return MapRemove(set, element);
}

void SetClear(Set *set)
{
    MapClear(set);
}

SetIterator SetIteratorInit(Set *set)
{
    return MapIteratorInit(set);
}

void *SetIteratorNext(SetIterator *i)
{
    MapKeyValue *kv = MapIteratorNext(i);
    return kv ? kv->key : NULL;
}
