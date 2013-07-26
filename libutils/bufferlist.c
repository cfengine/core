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
  versions of CFEngine, the applicable Commerical Open Source License
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
int BufferListCount(BufferList *list)
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
BufferListIterator *BufferListIteratorGet(BufferList *list)
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
    if (!iterator)
    {
        return -1;
    }
    return ListIteratorFirst(iterator->iterator);
}

int BufferListIteratorLast(BufferListIterator *iterator)
{
    if (!iterator)
    {
        return -1;
    }
    return ListIteratorLast(iterator->iterator);
}

int BufferListIteratorNext(BufferListIterator *iterator)
{
    if (!iterator)
    {
        return -1;
    }
    return ListIteratorNext(iterator->iterator);
}

int BufferListIteratorPrevious(BufferListIterator *iterator)
{
    if (!iterator)
    {
        return -1;
    }
    return ListIteratorPrevious(iterator->iterator);
}

Buffer *BufferListIteratorData(const BufferListIterator *iterator)
{
    if (!iterator)
    {
        return NULL;
    }
    return (Buffer *)ListIteratorData(iterator->iterator);
}

bool BufferListIteratorHasNext(const BufferListIterator *iterator)
{
    if (!iterator)
    {
        return false;
    }
    return ListIteratorHasNext(iterator->iterator);
}

bool BufferListIteratorHasPrevious(const BufferListIterator *iterator)
{
    if (!iterator)
    {
        return false;
    }
    return ListIteratorHasPrevious(iterator->iterator);
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
    if (!iterator)
        return -1;
    return ListMutableIteratorFirst(iterator->iterator);
}

int BufferListMutableIteratorLast(BufferListMutableIterator *iterator)
{
    if (!iterator)
        return -1;
    return ListMutableIteratorLast(iterator->iterator);
}

int BufferListMutableIteratorNext(BufferListMutableIterator *iterator)
{
    if (!iterator)
        return -1;
    return ListMutableIteratorNext(iterator->iterator);
}

int BufferListMutableIteratorPrevious(BufferListMutableIterator *iterator)
{
    if (!iterator)
        return -1;
    return ListMutableIteratorPrevious(iterator->iterator);
}

Buffer *BufferListMutableIteratorData(const BufferListMutableIterator *iterator)
{
    if (!iterator)
        return NULL;
    return (Buffer *)ListMutableIteratorData(iterator->iterator);
}

int BufferListMutableIteratorRemove(BufferListMutableIterator *iterator)
{
    if (!iterator)
        return -1;
    return ListMutableIteratorRemove(iterator->iterator);
}

int BufferListMutableIteratorPrepend(BufferListMutableIterator *iterator, Buffer *payload)
{
    if (!iterator)
        return -1;
    return ListMutableIteratorPrepend(iterator->iterator, (void *)payload);
}

int BufferListMutableIteratorAppend(BufferListMutableIterator *iterator, Buffer *payload)
{
    if (!iterator)
        return -1;
    return ListMutableIteratorAppend(iterator->iterator, (void *)payload);
}

bool BufferListMutableIteratorHasNext(const BufferListMutableIterator *iterator)
{
    if (!iterator)
    {
        return false;
    }
    return ListMutableIteratorHasNext(iterator->iterator);
}

bool BufferListMutableIteratorHasPrevious(const BufferListMutableIterator *iterator)
{
    if (!iterator)
    {
        return false;
    }
    return ListMutableIteratorHasPrevious(iterator->iterator);
}
