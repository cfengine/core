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

#include <alloc.h>
#include <bufferlist.h>

struct BufferListMutableIterator {
    ListMutableIterator *iterator; 
};
struct BufferList {
    List *list;
};
struct BufferListIterator {
    ListIterator *iterator;
};

/*
 * Helper methods to compare, copy and destroy buffers.
 */
static int compare(const void *a, const void *b)
{
    Buffer *aa = (Buffer *)a;
    Buffer *bb = (Buffer *)b;
    return BufferCompare(aa, bb);
}

static void copy(const void *o, void **d)
{
    Buffer *origin = (Buffer *)o;
    Buffer **destination = (Buffer **)d;
    BufferCopy(origin, destination);
}

static void destroy(void *e)
{
    Buffer *element = (Buffer *)e;
    BufferDestroy(&element);
}

BufferList *BufferListNew()
{
    BufferList *list = NULL;
    list = (BufferList *)xmalloc(sizeof(BufferList));
    list->list = ListNew(compare, copy, destroy);
    return list;
}

int BufferListDestroy(BufferList **list)
{
    if (!list || !(*list))
    {
        return 0;
    }
    int result = ListDestroy(&(*list)->list);
    free(*list);
    *list = NULL;
    return result;
}

int BufferListCopy(BufferList *origin, BufferList **destination)
{
    if (!origin || !destination)
        return -1;
    *destination = (BufferList *)xmalloc(sizeof(BufferList));
    int result = 0;
    result = ListCopy(origin->list, &(*destination)->list);
    if (result < 0)
    {
        free (*destination);
    }
    return result;
}

int BufferListPrepend(BufferList *list, Buffer *payload)
{
    if (!list)
    {
        return -1;
    }
    return ListPrepend(list->list, (void *)payload);
}

int BufferListAppend(BufferList *list, Buffer *payload)
{
    if (!list)
    {
        return -1;
    }
    return ListAppend(list->list, (void *)payload);
}

int BufferListRemove(BufferList *list, Buffer *payload)
{
    if (!list || !payload)
        return -1;
    return ListRemove(list->list, (void *)payload);
}

// Number of elements on the list
int BufferListCount(const BufferList *list)
{
    if (!list)
    {
        return -1;
    }
    return ListCount(list->list);
}

/*
 * Functions for iterators
 */
BufferListIterator *BufferListIteratorGet(const BufferList *list)
{
    if (!list)
    {
        return NULL;
    }
    BufferListIterator *iterator = NULL;
    iterator = (BufferListIterator *)xmalloc(sizeof(BufferListIterator));
    iterator->iterator = ListIteratorGet(list->list);
    if (!iterator->iterator)
    {
        free (iterator);
        return NULL;
    }
    return iterator;
}

int BufferListIteratorDestroy(BufferListIterator **iterator)
{
    if (!iterator || !(*iterator))
    {
        return 0;
    }
    int result = 0;
    result = ListIteratorDestroy(&(*iterator)->iterator);
    free((*iterator));
    *iterator = NULL;
    return result;
}

int BufferListIteratorFirst(BufferListIterator *iterator)
{
    return iterator ? ListIteratorFirst(iterator->iterator) : -1;
}

int BufferListIteratorLast(BufferListIterator *iterator)
{
    return iterator ? ListIteratorLast(iterator->iterator) : -1;
}

int BufferListIteratorNext(BufferListIterator *iterator)
{
    return iterator ? ListIteratorNext(iterator->iterator) : -1;
}

int BufferListIteratorPrevious(BufferListIterator *iterator)
{
    return iterator ? ListIteratorPrevious(iterator->iterator) : -1;
}

Buffer *BufferListIteratorData(const BufferListIterator *iterator)
{
    return iterator ? (Buffer *)ListIteratorData(iterator->iterator) : NULL;
}

bool BufferListIteratorHasNext(const BufferListIterator *iterator)
{
    return iterator ? ListIteratorHasNext(iterator->iterator) : false;
}

bool BufferListIteratorHasPrevious(const BufferListIterator *iterator)
{
    return iterator ? ListIteratorHasPrevious(iterator->iterator) : false;
}

/*
 * Mutable iterator operations
 */
BufferListMutableIterator *BufferListMutableIteratorGet(BufferList *list)
{
    if (!list)
    {
        return NULL;
    }
    BufferListMutableIterator *iterator = NULL;
    iterator = (BufferListMutableIterator *)xmalloc(sizeof(BufferListMutableIterator));
    iterator->iterator = ListMutableIteratorGet(list->list);
    if (!iterator->iterator)
    {
        free (iterator);
        return NULL;
    }
    return iterator;
}

int BufferListMutableIteratorRelease(BufferListMutableIterator **iterator)
{
    int result = 0;
    if (iterator && *iterator) 
    {
        result = ListMutableIteratorRelease(&(*iterator)->iterator);
        free (*iterator);
        *iterator = NULL;
    }
    return result;
}

int BufferListMutableIteratorFirst(BufferListMutableIterator *iterator)
{
    return iterator ? ListMutableIteratorFirst(iterator->iterator) : -1;
}

int BufferListMutableIteratorLast(BufferListMutableIterator *iterator)
{
    return iterator ? ListMutableIteratorLast(iterator->iterator) : -1;
}

int BufferListMutableIteratorNext(BufferListMutableIterator *iterator)
{
    return iterator ? ListMutableIteratorNext(iterator->iterator) : -1;
}

int BufferListMutableIteratorPrevious(BufferListMutableIterator *iterator)
{
    return iterator ? ListMutableIteratorPrevious(iterator->iterator) : -1;
}

Buffer *BufferListMutableIteratorData(const BufferListMutableIterator *iterator)
{
    return iterator ? (Buffer *)ListMutableIteratorData(iterator->iterator) : NULL;
}

int BufferListMutableIteratorRemove(BufferListMutableIterator *iterator)
{
    return iterator ? ListMutableIteratorRemove(iterator->iterator) : -1;
}

int BufferListMutableIteratorPrepend(BufferListMutableIterator *iterator, Buffer *payload)
{
    return iterator ? ListMutableIteratorPrepend(iterator->iterator, (void *)payload) : -1;
}

int BufferListMutableIteratorAppend(BufferListMutableIterator *iterator, Buffer *payload)
{
    return iterator ? ListMutableIteratorAppend(iterator->iterator, (void *)payload) : -1;
}

bool BufferListMutableIteratorHasNext(const BufferListMutableIterator *iterator)
{
    return iterator ? ListMutableIteratorHasNext(iterator->iterator) : false;
}

bool BufferListMutableIteratorHasPrevious(const BufferListMutableIterator *iterator)
{
    return iterator ? ListMutableIteratorHasPrevious(iterator->iterator) : false;
}
