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
#include <queue.h>
#include <refcount.h>

struct QueueNode {
    void *data;                 /*!< Pointer to the stored element */
    struct QueueNode *next;     /*!< Next element in the queue or NULL */
    struct QueueNode *previous; /*!< Pointer to the previous element or NULL */
};

struct Queue {
    int node_count;      /*!< Number of elements in the queue */
    RefCount *ref_count; /*!< Refcount element to simplify copying */
    void (*copy)(const void *source, void **destination); /*!< Copies one element */
    void (*destroy)(void *element); /*!< Destroys an element */
    struct QueueNode *head; /*!< Pointer to the start of the queue */
    struct QueueNode *tail; /*!< Pointer to the end of the queue */
};

/*
 * Detaches a queue from its data. In order to do this, it first creates a copy
 * of the data and then detaches from the reference count.
 * After it creates a new reference count and attaches to it.
 */
static int QueueDetach(Queue *queue)
{
    /* We cannot detach without a copy function */
    if (!queue || !queue->copy)
    {
        return -1;
    }

    struct QueueNode *copy_start = NULL;
    struct QueueNode *copy_current = NULL;
    RefCount *old_ref_count = queue->ref_count;
    RefCountNew(&queue->ref_count);
    RefCountAttach(queue->ref_count, queue);

    for (struct QueueNode * current = queue->head;
         current;
         current = current->next)
    {
        struct QueueNode *p = xcalloc(1, sizeof(struct QueueNode));
        queue->copy(current->data, &p->data);

        if (NULL == copy_start)
        {
            copy_start = p;
            copy_current = copy_start;
        }
        else
        {
            copy_current->next = p;
            p->previous = copy_current;
            copy_current = p;
        }
    };
    /* Now it is time to detach and use the new queue instead of the old one. */
    RefCountDetach(old_ref_count, queue);
    queue->head = copy_start;
    queue->tail = copy_current;
    return 0;
}

/*
 * This method is only used when copying a structure, therefore we only need
 * to check the original structure since the copy will be overwritten anyways.
 * This method only makes sure that the copy is pointing to the original data
 * and that it attaches to the right RefCount structure.
 */
static int QueueAttach(Queue *original, Queue *copy)
{
    /* We avoid attaching if no copy function is found, otherwise we will not be able to detach */
    if (!original || !copy || !original->copy)
    {
        return -1;
    }
    copy->ref_count = original->ref_count;
    copy->head = original->head;
    copy->tail = original->tail;
    copy->node_count = original->node_count;
    RefCountAttach(original->ref_count, copy);
    return 0;
}

Queue *QueueNew(void (*copy)(const void *source, void **destination), void (*destroy)(void *))
{
    if (!copy)
    {
        return NULL;
    }
    Queue *queue = xcalloc(1, sizeof(Queue));
    queue->copy = copy;
    queue->destroy = destroy;
    RefCountNew(&queue->ref_count);
    RefCountAttach(queue->ref_count, queue);
    return queue;
}

void QueueDestroy(Queue **queue)
{
    if (!queue || !*queue)
    {
        return;
    }
    /* If the queue is shared, then just detach */
    if (RefCountIsShared((*queue)->ref_count))
    {
        /*
         * Shared, we detach and move on.
         * Since in any case we are destroying, we ignore the error because the
         * only errors are caused by either a NULL pointer or owner not found.
         */
        RefCountDetach((*queue)->ref_count, (*queue));
    }
    else
    {
        /* We need to destroy the queue */
        struct QueueNode *current = (*queue)->head;
        while (current)
        {
            struct QueueNode *next = current->next;
            if ((*queue)->destroy)
            {
                (*queue)->destroy(current->data);
            }
            free(current);
            current = next;
        }
    }
    /* Destroy the container */
    free(*queue);
    *queue = NULL;
}

int QueueCopy(Queue *origin, Queue **destination)
{
    if (!origin || !destination)
    {
        return -1;
    }
    *destination = xcalloc(1, sizeof(Queue));
    (*destination)->copy = origin->copy;
    (*destination)->destroy = origin->destroy;
    if (origin->node_count > 0)
    {
        /* Attach it to our data */
        if (QueueAttach(origin, *destination) < 0)
        {
            free(*destination);
            *destination = NULL;
            return -1;
        }
    }
    else
    {
        /* This is equivalent to creating a new queue */
        RefCountNew(&(*destination)->ref_count);
        RefCountAttach((*destination)->ref_count, (*destination));
    }
    return 0;
}

int QueueEnqueue(Queue *queue, void *element)
{
    if (!queue || !element)
    {
        return -1;
    }
    struct QueueNode *node = xcalloc(1, sizeof(struct QueueNode));
    node->data = element;
    /* Check if shared, if so detach first */
    if (RefCountIsShared(queue->ref_count))
    {
        if (QueueDetach(queue) < 0)
        {
            return -1;
        }
    }
    /* Now adjust the queue */
    if (queue->tail)
    {
        queue->tail->next = node;
        node->previous = queue->tail;
        queue->tail = node;
    }
    else
    {
        /* first element */
        queue->tail = node;
        queue->head = node;
    }
    ++queue->node_count;
    return 0;
}

void *QueueDequeue(Queue *queue)
{
    if (!queue || (queue->node_count < 1))
    {
        return NULL;
    }
    /* Check if shared, if so detach first */
    if (RefCountIsShared(queue->ref_count))
    {
        if (QueueDetach(queue) < 0)
        {
            return NULL;
        }
    }
    /* Fix the list pointers and counters */
    struct QueueNode *node = queue->head;
    void *data = node->data;
    queue->head = node->next;
    if (queue->head)
    {
        queue->head->previous = NULL;
    }
    else
    {
        /* Empty queue */
        queue->head = NULL;
        queue->tail = NULL;
    }
    --queue->node_count;
    /* Free the node */
    free(node);
    /* Return the data */
    return data;
}

void *QueueHead(Queue *queue)
{
    if (!queue || (queue->node_count < 1))
    {
        return NULL;
    }
    return queue->head->data;
}

int QueueCount(const Queue *queue)
{
    return queue ? queue->node_count : -1;
}

bool QueueIsEmpty(const Queue *queue)
{
    return !queue || (queue->node_count == 0);
}
