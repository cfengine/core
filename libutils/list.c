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

#include <assert.h>
#include <alloc.h>
#include <list.h>

struct ListNode {
    void *payload;
    struct ListNode *next;
    struct ListNode *previous;
};
typedef struct ListNode ListNode;
struct ListMutableIterator {
    int valid;
    ListNode *current;
    List *origin;
};
struct List {
    // Number of nodes
    int node_count;
    // Incremental number that keeps track of the state of the list, only used for light iterators
    unsigned int state;
    // Nodes
    ListNode *list;
    // Link to the first element
    ListNode *first;
    // Link to the last element
    ListNode *last;
    // This function is used to compare two elements
    int (*compare)(const void *a, const void *b);
    // This function is used whenever there is need to perform a deep copy
    void (*copy)(const void *source, void **destination);
    // This function can be used to destroy the elements at destruction time
    void (*destroy)(void *element);
    // Reference counting
    RefCount *ref_count;
    // Mutable iterator.
    ListMutableIterator *iterator;
};
struct ListIterator {
    ListNode *current;
    List *origin;
    unsigned int state;
};

#define IsIteratorValid(iterator) \
    iterator->state != iterator->origin->state
#define IsMutableIteratorValid(iterator) \
    iterator->valid == 0
#define ChangeListState(list) \
    list->state++

/*
 * Helper method to detach lists.
 */
static void ListDetach(List *list)
{
    int shared = RefCountIsShared(list->ref_count);
    if (shared)
    {
        /*
         * 1. Perform a deep copy (expensive!)
         * 2. Detach
         */
        ListNode *p = NULL, *q = NULL, *newList = NULL, *first = NULL, *last = NULL;
        for (p = list->list; p; p = p->next)
        {
            if (newList)
            {
                q->next = (ListNode *)xmalloc(sizeof(ListNode));
                q->next->previous = q;
                q->next->next = NULL;
                q = q->next;
                last = q;
                if (p->payload)
                {
                    if (list->copy)
                    {
                        list->copy(p->payload, &q->payload);
                    }
                    else
                    {
                        q->payload = p->payload;
                    }
                }
            }
            else
            {
                // First element
                newList = (ListNode *)xmalloc(sizeof(ListNode));
                newList->next = NULL;
                newList->previous = NULL;
                first = newList;
                last = newList;
                if (p->payload)
                {
                    if (list->copy)
                    {
                        list->copy(p->payload, &newList->payload);
                    }
                    else
                    {
                        newList->payload = p->payload;
                    }
                }
                q = newList;
            }
        }
        list->list = newList;
        list->first = first;
        list->last = last;
        // Ok, we have our own copy of the list. Now we detach.
        RefCountDetach(list->ref_count, list);
        list->ref_count = NULL;
        RefCountNew(&list->ref_count);
        RefCountAttach(list->ref_count, list);
    }
}

List *ListNew(int (*compare)(const void *, const void *), void (*copy)(const void *, void **), void (*destroy)(void *))
{
    List *list = NULL;
    list = (List *)xmalloc(sizeof(List));
    list->list = NULL;
    list->first = NULL;
    list->last = NULL;
    list->node_count = 0;
    list->iterator = NULL;
    list->state = 0;
    list->compare = compare;
    list->destroy = destroy;
    list->copy = copy;
    RefCountNew(&list->ref_count);
    RefCountAttach(list->ref_count, list);
    return list;
}

int ListDestroy(List **list)
{
    if (!list || !(*list))
    {
        return 0;
    }
    int shared = RefCountIsShared((*list)->ref_count);
    if (shared)
    {
        /*
         * We just detach from the list.
         */
        RefCountDetach((*list)->ref_count, (*list));
    }
    else
    {
        // We are the only ones using the list, we can delete it.
        ListNode *node = NULL;
        ListNode *p = NULL;
        for (node = (*list)->first; node; node = p)
        {
            if ((*list)->destroy)
                (*list)->destroy(node->payload);
            p = node->next;
            free(node);
        }
        RefCountDestroy(&(*list)->ref_count);
    }
    free((*list));
    *list = NULL;
    return 0;
}

int ListCopy(List *origin, List **destination)
{
    if (!origin || !destination)
        return -1;
    /*
     * The first thing we check is the presence of a copy function.
     * Without that function we need to abort the operation.
     */
    if (!origin->copy)
    {
        return -1;
    }
    *destination = (List *)xmalloc(sizeof(List));
    (*destination)->list = origin->list;
    (*destination)->first = origin->first;
    (*destination)->last = origin->last;
    (*destination)->node_count = origin->node_count;
    (*destination)->state = origin->state;
    (*destination)->destroy = origin->destroy;
    (*destination)->copy = origin->copy;
    (*destination)->compare = origin->compare;
    /*
     * We do not copy iterators.
     */
    (*destination)->iterator = NULL;
    /* We have a copy function, we can perform a shallow copy. */
    RefCountAttach(origin->ref_count, (*destination));
    (*destination)->ref_count = origin->ref_count;
    return 0;
}

int ListPrepend(List *list, void *payload)
{
    ListNode *node = NULL;
    if (!list)
    {
        return -1;
    }
    ListDetach(list);
    node = (ListNode *)xmalloc(sizeof(ListNode));
    node->payload = payload;
    node->previous = NULL;
    if (list->list)
    {
        // We have elements
        node->next = list->list;
        list->list->previous = node;
    }
    else
    {
        // First element
        node->next = NULL;
        list->last = node;
    }
    list->list = node;
    list->first = node;
    list->node_count++;
    return 0;
}

int ListAppend(List *list, void *payload)
{
    ListNode *node = NULL;
    if (!list)
    {
        return -1;
    }
    ListDetach(list);
    node = (ListNode *)xmalloc(sizeof(ListNode));
    node->next = NULL;
    node->payload = payload;
    if (list->last)
    {
        // We have elements
        node->previous = list->last;
        list->last->next = node;
    }
    else
    {
        // First element
        node->previous = NULL;
        list->list = node;
        list->first = node;
    }
    list->last = node;
    list->node_count++;
    return 0;
}

/*
 * We split the code into several helper functions.
 * These functions are not exported to the outside world
 * since it does not make sense for them to be used in other
 * places.
 */
static int ListFindNode(List *list, void *payload)
{
    if (!list)
    {
        return -1;
    }
    ListNode *node = NULL;
    int found = 0;
    for (node = list->list; node; node = node->next)
    {
        if (!node->payload)
            continue;
        if (list->compare)
        {
            if (!list->compare(node->payload, payload))
            {
                found = 1;
                break;
            }
        }
        else
        {
            if (node->payload == payload)
            {
                found = 1;
                break;
            }
        }
    }
    return found;
}
static void ListUpdateListState(List *list)
{
    list->node_count--;
    ChangeListState(list);
}

int ListRemove(List *list, void *payload)
{
    if (!list || !payload)
        return -1;
    ListNode *node = NULL;
    /*
     * This is a complicated matter. We could detach the list before
     * we know that we have a new node, but that will mean that we
     * might have copied the whole list without real reasons. On the
     * other hand, it saves us a whole traversal of the list if we
     * just do it.
     */
    int found = ListFindNode(list, payload);
    if (!found)
        return -1;
    found = 0;
    ListDetach(list);
    node = NULL;
    /*
     * We need to find the node again since we have a new list.
     * In theory we don't have to worry about the existence of the node,
     * since the list has not changed, it might have been copied but
     * it is still the same as before.
     */
    for (node = list->list; node; node = node->next) 
	{
        if (list->compare) 
		{
            if (!list->compare(node->payload, payload)) 
			{
                found = 1;
                break;
            }
        } 
		else 
		{
            if (node->payload == payload) 
			{
                found = 1;
                break;
            }
        }
    }
    /*
     * This is nearly impossible, so we will only assert it.
     */
    assert(node);
    assert(found == 1);
    /*
     * Before deleting the node we have to update the mutable iterator.
     * We might need to advance it!
     */
    if (list->iterator)
    {
        if (list->iterator->current == node) 
        {
            /*
             * So lucky, it is the same node!
             * Move the iterator so it is not dangling.
             * Rules for moving:
             * 1. Move forward.
             * 2. if not possible, move backward.
             * 3. If not possible, then invalidate the iterator.
             */
            if (list->iterator->current->next)
            {
                list->iterator->current = list->iterator->current->next;
            }
            else if (list->iterator->current->previous)
            {
                list->iterator->current = list->iterator->current->previous;
            }
            else
            {
                list->iterator->valid = 0;
            }
        }
    }
    /*
     * Now, remove the node from the list and delete it.
     */
    if (node->next && node->previous) 
    {
        // Middle of the list
        node->next->previous = node->previous;
        node->previous->next = node->next;
    }
    else if (node->next)
    {
        // First element of the list
        list->list = node->next;
        list->first = node->next;
        node->next->previous = NULL;
    }
    else if (node->previous)
    {
        // Last element
        node->previous->next = NULL;
        list->last = node->previous;
    }
    else
    {
        // Single element
        list->list = NULL;
        list->first = NULL;
        list->last = NULL;
    }
    if (list->destroy && node->payload) 
    {
        list->destroy(node->payload);
    }
    else
    {
        free (node->payload);
    }
    free(node);
    ListUpdateListState(list);
    return 0;
}

// Number of elements on the list
int ListCount(const List *list)
{
    return list ? list->node_count : -1;
}

/*
 * Functions for iterators
 */
ListIterator *ListIteratorGet(const List *list)
{
    if (!list)
    {
        return NULL;
    }
    // You cannot get an iterator for an empty list.
    if (!list->first)
	{
        return NULL;
	}
    ListIterator *iterator = NULL;
    iterator = (ListIterator *)xmalloc(sizeof(ListIterator));
    iterator->current = list->list;
    // Remaining only works in one direction, we need two variables for this.
    iterator->origin = (List *)list;
    iterator->state = list->state;
    return iterator;
}

int ListIteratorDestroy(ListIterator **iterator)
{
    if (!iterator || !(*iterator))
    {
        return 0;
    }
    (*iterator)->current = NULL;
    free((*iterator));
    *iterator = NULL;
    return 0;
}

int ListIteratorFirst(ListIterator *iterator)
{
    if (!iterator)
    {
        return -1;
    }
    if (IsIteratorValid(iterator))
    {
        // The list has moved forward, the iterator is invalid now
        return -1;
    }
    iterator->current = iterator->origin->first;
    return 0;
}

int ListIteratorLast(ListIterator *iterator)
{
    if (!iterator)
    {
        return -1;
    }
    if (IsIteratorValid(iterator))
    {
        // The list has moved forward, the iterator is invalid now
        return -1;
    }
    iterator->current = iterator->origin->last;
    return 0;
}

int ListIteratorNext(ListIterator *iterator)
{
    if (!iterator)
    {
        return -1;
    }
    if (IsIteratorValid(iterator))
    {
        // The list has moved forward, the iterator is invalid now
        return -1;
    }
    // Ok, check if we are at the end
    if (iterator->current && iterator->current->next)
    {
        iterator->current = iterator->current->next;
    }
    else
    {
        return -1;
    }
    return 0;
}

int ListIteratorPrevious(ListIterator *iterator)
{
    if (!iterator)
    {
        return -1;
    }
    if (IsIteratorValid(iterator))
    {
        // The list has moved forward, the iterator is invalid now
        return -1;
    }
    // Ok, check if we are at the end
    if (iterator->current && iterator->current->previous)
    {
        iterator->current = iterator->current->previous;
    }
    else
    {
        return -1;
    }
    return 0;
}

void *ListIteratorData(const ListIterator *iterator)
{
    if (!iterator)
    {
        return NULL;
    }
    if (IsIteratorValid(iterator))
    {
        // The list has moved forward, the iterator is invalid now
        return NULL;
    }
    return iterator->current->payload;
}

bool ListIteratorHasNext(const ListIterator *iterator)
{
    if (!iterator)
    {
        return false;
    }
    if (IsIteratorValid(iterator))
    {
        // The list has moved forward, the iterator is invalid now
        return false;
    }
    if (iterator->current->next)
    {
        return true;
    }
    return false;
}

bool ListIteratorHasPrevious(const ListIterator *iterator)
{
    if (!iterator)
    {
        return false;
    }
    if (IsIteratorValid(iterator))
    {
        // The list has moved forward, the iterator is invalid now
        return false;
    }
    if (iterator->current->previous)
    {
        return true;
    }
    return false;
}

/*
 * Mutable iterator operations
 */
ListMutableIterator *ListMutableIteratorGet(List *list)
{
    if (!list)
    {
        return NULL;
    }
    if (list->iterator)
    {
        // Only one iterator at a time
        return  NULL;
    }
    // You cannot get an iterator for an empty list.
    if (!list->first)
    {
        return  NULL;
    }
    ListMutableIterator *iterator = NULL;
    iterator = (ListMutableIterator *)xmalloc(sizeof(ListMutableIterator));
    iterator->current = list->first;
    iterator->origin = list;
    iterator->valid = 1;
    list->iterator = iterator;
    return iterator;
}

int ListMutableIteratorRelease(ListMutableIterator **iterator)
{
    if (iterator && *iterator) {
        (*iterator)->origin->iterator = NULL;
        free (*iterator);
        *iterator = NULL;
    }
    return 0;
}

int ListMutableIteratorFirst(ListMutableIterator *iterator)
{
    if (!iterator)
        return -1;
    if (IsMutableIteratorValid(iterator))
        return -1;
    iterator->current = iterator->origin->first;
    return 0;
}

int ListMutableIteratorLast(ListMutableIterator *iterator)
{
    if (!iterator)
        return -1;
    if (IsMutableIteratorValid(iterator))
        return -1;
    iterator->current = iterator->origin->last;
    return 0;
}

int ListMutableIteratorNext(ListMutableIterator *iterator)
{
    if (!iterator)
        return -1;
    if (IsMutableIteratorValid(iterator))
        return -1;
    if (!iterator->current->next)
        return -1;
    iterator->current = iterator->current->next;
    return 0;
}

int ListMutableIteratorPrevious(ListMutableIterator *iterator)
{
    if (!iterator)
        return -1;
    if (IsMutableIteratorValid(iterator))
        return -1;
    if (!iterator->current->previous)
        return -1;
    iterator->current = iterator->current->previous;
    return 0;
}

void *ListMutableIteratorData(const ListMutableIterator *iterator)
{
    if (!iterator)
        return NULL;
    if (IsMutableIteratorValid(iterator))
        return NULL;
    return (void *)iterator->current->payload;
}

int ListMutableIteratorRemove(ListMutableIterator *iterator)
{
    if (!iterator)
        return -1;
    if (IsMutableIteratorValid(iterator))
        return -1;
    ListDetach(iterator->origin);
    /*
     * Removing an element is not as simple as it sounds. We need to inform the list
     * and make sure we move out of the way.
     */
    ListNode *node = NULL;
    if (iterator->current->next) {
        /*
         * We are not the last element, therefore we proceed as normal.
         */
        node = iterator->current->next;
    } else {
        /*
         * We might be the last element or the only element on the list.
         * If we are the only element we do not destroy the element otherwise the iterator
         * would become invalid.
         */
        if (iterator->current->previous) {
            /*
             * last element
             */
            node = iterator->current->previous;
        } else
            return -1;
    }
    /*
     * Now, remove the node from the list and delete it.
     */
    if (iterator->current->next && iterator->current->previous)
    {
        // Middle of the list
        iterator->current->next->previous = iterator->current->previous;
        iterator->current->previous->next = iterator->current->next;
    }
    else if (iterator->current->next)
    {
        // First element of the list
        iterator->origin->list = iterator->current->next;
        iterator->origin->first = iterator->current->next;
        iterator->current->next->previous = NULL;
    }
    else if (iterator->current->previous)
    {
        // Last element
        iterator->current->previous->next = NULL;
        iterator->origin->last = iterator->current->previous;
    }
    if (iterator->origin->destroy && iterator->current->payload)
    {
         iterator->origin->destroy(iterator->current->payload);
    }
    else
    {
        free (iterator->current->payload);
    }
    free(iterator->current);
    iterator->current = node;
    ListUpdateListState(iterator->origin);
    return 0;
}

int ListMutableIteratorPrepend(ListMutableIterator *iterator, void *payload)
{
    if (!iterator)
        return -1;
    if (IsMutableIteratorValid(iterator))
        return -1;
    ListNode *node = NULL;
    node = (ListNode *)xmalloc(sizeof(ListNode));
    ListDetach(iterator->origin);
    node->payload = payload;
    if (iterator->current->previous) {
        node->previous = iterator->current->previous;
        node->next = iterator->current;
        iterator->current->previous->next = node;
        iterator->current->previous = node;
    } else {
        // First element
        node->previous = NULL;
        node->next = iterator->current;
        iterator->current->previous = node;
        iterator->origin->first = node;
        iterator->origin->list = node;
    }
    iterator->origin->node_count++;
    return 0;
}

int ListMutableIteratorAppend(ListMutableIterator *iterator, void *payload)
{
    if (!iterator)
        return -1;
    if (IsMutableIteratorValid(iterator))
        return -1;
    ListNode *node = NULL;
    node = (ListNode *)xmalloc(sizeof(ListNode));
    ListDetach(iterator->origin);
    node->next = NULL;
    node->payload = payload;
    if (iterator->current->next) {
        node->next = iterator->current->next;
        node->previous = iterator->current;
        iterator->current->next->previous = node;
        iterator->current->next = node;
    } else {
        // Last element
        node->next = NULL;
        node->previous = iterator->current;
        iterator->current->next = node;
        iterator->origin->last = node;
    }
    iterator->origin->node_count++;
    return 0;
}

bool ListMutableIteratorHasNext(const ListMutableIterator *iterator)
{
    return iterator && iterator->current->next;
}

bool ListMutableIteratorHasPrevious(const ListMutableIterator *iterator)
{
    return iterator && iterator->current->previous;
}
