/*
   Copyright 2018 Northern.tech AS

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

#include <set.h>

#include <alloc.h>
#include <string_lib.h>
#include <buffer.h>

TYPED_SET_DEFINE(String, char *,
                 StringHash_untyped, StringSafeEqual_untyped, free)

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
    assert(set != NULL);
    MapInsert(set, element, element);
}

bool SetContains(const Set *set, const void *element)
{
    assert(set != NULL);
    return MapHasKey(set, element);
}

bool SetRemove(Set *set, const void *element)
{
    assert(set != NULL);
    return MapRemove(set, element);
}

void SetClear(Set *set)
{
    assert(set != NULL);
    MapClear(set);
}

size_t SetSize(const Set *set)
{
    assert(set != NULL);
    return MapSize(set);
}

bool SetIsEqual(const Set *set1, const Set *set2)
{
    assert(set1 != NULL);
    assert(set2 != NULL);
    return MapContainsSameKeys(set1, set2);
}

SetIterator SetIteratorInit(Set *set)
{
    assert(set != NULL);
    return MapIteratorInit(set);
}

void *SetIteratorNext(SetIterator *i)
{
    MapKeyValue *kv = MapIteratorNext(i);
    return kv ? kv->key : NULL;
}

void SetJoin(Set *set, Set *otherset, SetElementCopyFn copy_function)
{
    assert(set != NULL);
    assert(otherset != NULL);
    if (set == otherset)
        return;

    SetIterator si = SetIteratorInit(otherset);
    void *ptr = NULL;

    for (ptr = SetIteratorNext(&si); ptr != NULL; ptr = SetIteratorNext(&si))
    {
        if (copy_function != NULL)
        {
            ptr = copy_function(ptr);
        }
        SetAdd(set, ptr);
    }
}

Buffer *StringSetToBuffer(StringSet *set, const char delimiter)
{
    assert(set != NULL);

    Buffer *buf = BufferNew();
    StringSetIterator it = StringSetIteratorInit(set);
    const char *element = NULL;
    int pos = 0;
    int size = StringSetSize(set);
    char minibuf[2];

    minibuf[0] = delimiter;
    minibuf[1] = '\0';

    while ((element = StringSetIteratorNext(&it)))
    {
        BufferAppend(buf, element, strlen(element));
        if (pos < size-1)
        {
            BufferAppend(buf, minibuf, sizeof(char));
        }

        pos++;
    }

    return buf;
}

void StringSetAddSplit(StringSet *set, const char *str, char delimiter)
{
    assert(set != NULL);
    if (str) // TODO: remove this inconsistency, add assert(str)
    {
        const char *prev = str;
        const char *cur = str;

        while (*cur != '\0')
        {
            if (*cur == delimiter)
            {
                size_t len = cur - prev;
                if (len > 0)
                {
                    StringSetAdd(set, xstrndup(prev, len));
                }
                else
                {
                    StringSetAdd(set, xstrdup(""));
                }
                prev = cur + 1;
            }

            cur++;
        }

        if (cur > prev)
        {
            StringSetAdd(set, xstrndup(prev, cur - prev));
        }
    }
}

StringSet *StringSetFromString(const char *str, char delimiter)
{
    StringSet *set = StringSetNew();

    StringSetAddSplit(set, str, delimiter);

    return set;
}

JsonElement *StringSetToJson(const StringSet *set)
{
    assert(set != NULL);

    JsonElement *arr = JsonArrayCreate(StringSetSize(set));
    StringSetIterator it = StringSetIteratorInit((StringSet *)set);
    const char *el = NULL;

    while ((el = StringSetIteratorNext(&it)))
    {
        JsonArrayAppendString(arr, el);
    }

    return arr;
}
