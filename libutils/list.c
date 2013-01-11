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

#include "list.h"

#define IsIteratorValid(iterator) \
    iterator->state != iterator->origin->state
#define ChangeListState(list) \
    list->state++

/*
 * Helper method to detach lists.
 */
static void ListDetach(List *list)
{
    int shared = RefCountIsShared(list->ref_count);
    if (shared) {
        /*
         * 1. Perform a deep copy (expensive!)
         * 2. Detach
         */
        ListNode *p = NULL, *q = NULL, *newList = NULL;
        for (p = list->list; p; p = p->next) {
            if (newList) {
                q->next = (ListNode *)malloc(sizeof(ListNode));
                q->next->previous = q;
                q->next->next = NULL;
                q = q->next;
                if (p->payload)
                    if (list->copy)
                        list->copy(p->payload, &q->payload);
                    else
                        q->payload = p->payload;
            } else {
                // First element
                newList = (ListNode *)malloc(sizeof(ListNode));
                newList->next = NULL;
                newList->previous = NULL;
                if (p->payload)
                    if (list->copy)
                        list->copy(p->payload, &newList->payload);
                    else
                        newList->payload = p->payload;
                q = newList;
            }
        }
        list->list = newList;
        // Ok, we have our own copy of the list. Now we detach.
        RefCountDetach(list->ref_count, list);
        list->ref_count = NULL;
        RefCountNew(&list->ref_count);
        RefCountAttach(list->ref_count, list);
    }
}

int ListNew(List **list, int (*compare)(void *, void *), void (*copy)(void *source, void **destination), void (*destroy)(void *))
{
    if (!list)
        return -1;
    *list = (List *)malloc(sizeof(List));
    if (!(*list))
        return -1;
    (*list)->list = NULL;
    (*list)->first = NULL;
    (*list)->last = NULL;
    (*list)->node_count = 0;
    (*list)->state = 0;
    (*list)->compare = compare;
    (*list)->destroy = destroy;
    (*list)->copy = copy;
    RefCountNew(&(*list)->ref_count);
    RefCountAttach((*list)->ref_count, (*list));
    return 0;
}

int ListDestroy(List **list)
{
    if (!list || !(*list))
        return 0;
    int shared = RefCountIsShared((*list)->ref_count);
    if (!shared) {
        // We are the only ones using the list, we can delete it.
        ListNode *node = NULL;
        ListNode *p = NULL;
        for (node = (*list)->first; node; node = node->next) {
            if (p)
                free(p);
            if ((*list)->destroy)
                (*list)->destroy(node->payload);
            p = node;
        }
        if (p)
            free(p);
    }
    RefCountDetach((*list)->ref_count, (*list));
    free((*list));
    *list = NULL;
    return 0;
}

int ListCopy(List *origin, List **destination)
{
    if (!origin || !destination)
        return -1;
    *destination = (List *)malloc(sizeof(List));
    (*destination)->list = origin->list;
    (*destination)->first = origin->first;
    (*destination)->last = origin->last;
    (*destination)->node_count = origin->node_count;
    (*destination)->state = origin->state;
    (*destination)->destroy = origin->destroy;
    (*destination)->copy = origin->copy;
    (*destination)->compare = origin->compare;
    int result = RefCountAttach(origin->ref_count, (*destination));
    if (result < 0) {
        free (*destination);
        return -1;
    }
    (*destination)->ref_count = origin->ref_count;
    return 0;
}

int ListPrepend(List *list, void *payload)
{
    ListNode *node = NULL;
    if (!list)
        return -1;
    ListDetach(list);
    node = (ListNode *)malloc(sizeof(ListNode));
    node->payload = payload;
    node->previous = NULL;
    if (list->list) {
        // We have elements
        node->next = list->list;
        list->list->previous = node;
    } else {
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
        return -1;
    ListDetach(list);
    node = (ListNode *)malloc(sizeof(ListNode));
    if (!node) {
        // This is unlikely in Linux but other Unixes actually return NULL
        return -1;
    }
    node->next = NULL;
    node->payload = payload;
    if (list->last) {
        // We have elements
        node->previous = list->last;
        list->last->next = node;
    } else {
        // First element
        node->previous = NULL;
        list->list = node;
        list->first = node;
    }
    list->last = node;
    list->node_count++;
    return 0;
}

int ListRemove(List *list, void *payload)
{
    if (!list)
        return -1;
    ListNode *node = NULL;
    int found = 0;
    for (node = list->list; node; node = node->next) {
        if (!node->payload)
            continue;
        if (list->compare) {
            if (!list->compare(node->payload, payload)) {
                found = 1;
                break;
            }
        } else {
            if (node->payload == payload) {
                found = 1;
                break;
            }
        }
    }
    if (!found)
        return -1;
    ListDetach(list);
    node = NULL;
    // We need to find the node again since we have a new list
    for (node = list->list; node; node = node->next) {
        if (list->compare) {
            if (!list->compare(node->payload, payload)) {
                found = 1;
                break;
            }
        } else {
            if (node->payload == payload) {
                found = 1;
                break;
            }
        }
    }
    /*
     * We found the node, we just need to change the pointers.
     */
    if (node->next && node->previous) {
        // Middle of the list
        node->next->previous = node->previous;
        node->previous->next = node->next;
    } else if (node->next) {
        // First element of the list
        list->list = node->next;
        list->first = node->next;
        node->next->previous = NULL;
    } else if (node->previous) {
        // Last element
        node->previous->next = NULL;
        list->last = node->previous;
    } else {
        // Single element
        list->list = NULL;
        list->first = NULL;
        list->last = NULL;
    }
    if (list->destroy && node->payload)
        list->destroy(node->payload);
    free(node);
    list->node_count--;
    ChangeListState(list);
    return 0;
}

// Number of elements on the list
int ListCount(List *list)
{
    if (!list)
        return -1;
    return list->node_count;
}

/*
 * Functions for iterators
 */
int ListIteratorGet(List *list, ListIterator **iterator)
{
    if (!list || !iterator)
        return -1;
    *iterator = (ListIterator *)malloc(sizeof(ListIterator));
    if (!(*iterator)) {
        // This is unlikely in Linux but other Unixes actually return NULL
        return -1;
    }
    (*iterator)->current = list->list;
    // Remaining only works in one direction, we need two variables for this.
    (*iterator)->origin = list;
    (*iterator)->state = list->state;
    return 0;
}

int ListIteratorDestroy(ListIterator **iterator)
{
    if (!iterator || !(*iterator))
        return 0;
    (*iterator)->current = NULL;
    free((*iterator));
    *iterator = NULL;
    return 0;
}

int ListIteratorFirst(ListIterator *iterator)
{
    if (!iterator)
        return -1;
    if (IsIteratorValid(iterator))
        // The list has moved forward, the iterator is invalid now
        return -1;
    iterator->current = iterator->origin->first;
    return 0;
}

int ListIteratorLast(ListIterator *iterator)
{
    if (!iterator)
        return -1;
    if (IsIteratorValid(iterator))
        // The list has moved forward, the iterator is invalid now
        return -1;
    iterator->current = iterator->origin->last;
    return 0;
}

int ListIteratorNext(ListIterator *iterator)
{
    if (!iterator)
        return -1;
    if (IsIteratorValid(iterator))
        // The list has moved forward, the iterator is invalid now
        return -1;
    // Ok, check if we are at the end
    if (iterator->current && iterator->current->next) {
        iterator->current = iterator->current->next;
    } else
        return -1;
    return 0;
}

int ListIteratorPrevious(ListIterator *iterator)
{
    if (!iterator)
        return -1;
    if (IsIteratorValid(iterator))
        // The list has moved forward, the iterator is invalid now
        return -1;
    // Ok, check if we are at the end
    if (iterator->current && iterator->current->previous) {
        iterator->current = iterator->current->previous;
    } else
        return -1;
    return 0;
}

void *ListIteratorData(const ListIterator *iterator)
{
    if (!iterator)
        return NULL;
    if (IsIteratorValid(iterator))
        // The list has moved forward, the iterator is invalid now
        return NULL;
    return iterator->current->payload;
}
