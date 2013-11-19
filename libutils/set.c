/*
   Copyright (C) CFEngine AS

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

#include <set.h>

#include <alloc.h>
#include <string_lib.h>
#include <buffer.h>

TYPED_SET_DEFINE(String, char *, (MapHashFn)&StringHash, (MapKeyEqualFn)&StringSafeEqual, &free)

Set *SetNew(MapHashFn element_hash_fn,
            MapKeyEqualFn element_equal_fn,
            MapDestroyDataFn element_destroy_fn)
{
    return MapNew(element_hash_fn, element_equal_fn, element_destroy_fn, NULL);
}

void SetDestroy(Set *set)
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

size_t SetSize(const Set *set)
{
    return MapSize(set);
}

bool SetIsEqual(const Set *set1, const Set *set2)
{
    return MapContainsSameKeys(set1, set2);
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

Buffer *StringSetToBuffer(StringSet *set, const char *delimiter)
{
    Buffer *buf = BufferNew();

    StringSetIterator it = StringSetIteratorInit(set);
    const char *element = NULL;
    int pos = 0;
    int size = StringSetSize(set);

    while ((element = StringSetIteratorNext(&it)))
    {
        BufferAppend(buf, element, strlen(element));
        if (pos < size-1)
        {
            BufferAppend(buf, delimiter, strlen(delimiter));
        }

        pos++;
    }

    return buf;
}

StringSet *StringSetFromString(const char *str, char delimiter)
{
    StringSet *set = StringSetNew();

    char delimiters[2] = { 0 };
    delimiters[0] = delimiter;

    if (NULL != str)
    {
        char *copy = xstrdup(str);
        char *curr = NULL;

        while ((curr = strsep(&copy, delimiters)))
        {
            StringSetAdd(set, xstrdup(curr));
        }

        free(copy);
    }

    return set;
}
