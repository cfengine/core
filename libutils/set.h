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

#ifndef CFENGINE_SET_H
#define CFENGINE_SET_H

#include <map.h>
#include <buffer.h>
#include <json.h>

typedef Map Set;
typedef MapIterator SetIterator;

Set *SetNew(MapHashFn element_hash_fn,
            MapKeyEqualFn element_equal_fn,
            MapDestroyDataFn element_destroy_fn);
void SetDestroy(Set *set);

void SetAdd(Set *set, void *element);
void SetJoin(Set *set, Set *otherset);
bool SetContains(const Set *set, const void *element);
bool SetRemove(Set *set, const void *element);
void SetClear(Set *set);
size_t SetSize(const Set *set);

bool SetIsEqual(const Set *set1, const Set *set2);

SetIterator SetIteratorInit(Set *set);
void *SetIteratorNext(SetIterator *i);

#define TYPED_SET_DECLARE(Prefix, ElementType)                          \
    typedef struct                                                      \
    {                                                                   \
        Set *impl;                                                      \
    } Prefix##Set;                                                      \
                                                                        \
    typedef SetIterator Prefix##SetIterator;                            \
                                                                        \
    Prefix##Set *Prefix##SetNew(void);                                  \
    void Prefix##SetAdd(const Prefix##Set *set, ElementType element);   \
    void Prefix##SetJoin(const Prefix##Set *set, const Prefix##Set *otherset); \
    bool Prefix##SetContains(const Prefix##Set *Set, const ElementType element);  \
    bool Prefix##SetRemove(const Prefix##Set *Set, const ElementType element);  \
    void Prefix##SetClear(Prefix##Set *set);                            \
    size_t Prefix##SetSize(const Prefix##Set *set);                     \
    bool Prefix##SetIsEqual(const Prefix##Set *set1, const Prefix##Set *set2); \
    void Prefix##SetDestroy(Prefix##Set *set);                          \
    Prefix##SetIterator Prefix##SetIteratorInit(Prefix##Set *set);      \
    ElementType Prefix##SetIteratorNext(Prefix##SetIterator *iter);     \

#define TYPED_SET_DEFINE(Prefix, ElementType, hash_fn, equal_fn, destroy_fn) \
                                                                        \
    Prefix##Set *Prefix##SetNew(void)                                   \
    {                                                                   \
        Prefix##Set *set = xcalloc(1, sizeof(Prefix##Set));             \
        set->impl = SetNew(hash_fn, equal_fn, destroy_fn);              \
        return set;                                                     \
    }                                                                   \
                                                                        \
    void Prefix##SetAdd(const Prefix##Set *set, ElementType element)    \
    {                                                                   \
        SetAdd(set->impl, (void *)element);                             \
    }                                                                   \
                                                                        \
    void Prefix##SetJoin(const Prefix##Set *set, const Prefix##Set *otherset) \
    {                                                                   \
        SetJoin(set->impl, otherset->impl);                             \
    }                                                                   \
                                                                        \
    bool Prefix##SetContains(const Prefix##Set *set, const ElementType element)   \
    {                                                                   \
        return SetContains(set->impl, element);                         \
    }                                                                   \
                                                                        \
    bool Prefix##SetRemove(const Prefix##Set *set, const ElementType element)   \
    {                                                                   \
        return SetRemove(set->impl, element);                           \
    }                                                                   \
                                                                        \
    void Prefix##SetClear(Prefix##Set *set)                             \
    {                                                                   \
        SetClear(set->impl);                                            \
    }                                                                   \
                                                                        \
    size_t Prefix##SetSize(const Prefix##Set *set)                      \
    {                                                                   \
        return SetSize(set->impl);                                      \
    }                                                                   \
                                                                        \
    bool Prefix##SetIsEqual(const Prefix##Set *set1, const Prefix##Set *set2) \
    {                                                                   \
        return SetIsEqual(set1->impl, set2->impl);                      \
    }                                                                   \
                                                                        \
    void Prefix##SetDestroy(Prefix##Set *set)                           \
    {                                                                   \
        if (set)                                                        \
        {                                                               \
            SetDestroy(set->impl);                                      \
            free(set);                                                  \
        }                                                               \
    }                                                                   \
                                                                        \
    Prefix##SetIterator Prefix##SetIteratorInit(Prefix##Set *set)       \
    {                                                                   \
        return SetIteratorInit(set->impl);                              \
    }                                                                   \
                                                                        \
    ElementType Prefix##SetIteratorNext(Prefix##SetIterator *iter)      \
    {                                                                   \
        return SetIteratorNext(iter);                                   \
    }                                                                   \


TYPED_SET_DECLARE(String, char *)

void StringSetAddSplit(StringSet *set, const char *str, char delimiter);
StringSet *StringSetFromString(const char *str, char delimiter);
Buffer *StringSetToBuffer(StringSet *set, const char delimiter);
JsonElement *StringSetToJson(const StringSet *set);

#endif
