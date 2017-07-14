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

#include <alloc.h>
#include <queue.h>
#include <refcount.h>

typedef struct QueueNode_ {
    void *data;                 /*!< Pointer to the stored element */
    struct QueueNode_ *next;     /*!< Next element in the queue or NULL */
    struct QueueNode_ *previous; /*!< Pointer to the previous element or NULL */
} QueueNode;

struct Queue_ {
    size_t node_count;      /*!< Number of elements in the queue */
    QueueItemDestroy *destroy;
    QueueNode *head; /*!< Pointer to the start of the queue */
    QueueNode *tail; /*!< Pointer to the end of the queue */
};

Queue *QueueNew(QueueItemDestroy *item_destroy)
{
    Queue *queue = xcalloc(1, sizeof(Queue));

    queue->destroy = item_destroy;

    return queue;
}

void QueueDestroy(Queue *q)
{
    if (q)
    {
        QueueNode *current = q->head;
        while (current)
        {
            QueueNode *next = current->next;
            if (q->destroy)
            {
                q->destroy(current->data);
            }
            free(current);
            current = next;
        }

        free(q);
    }
}

static QueueNode *QueueNodeNew(void *element)
{
    QueueNode *node = xmalloc(sizeof(QueueNode));

    node->data = element;
    node->previous = NULL;
    node->next = NULL;

    return node;
}

void QueueEnqueue(Queue *q, void *element)
{
    assert(q);

    QueueNode *node = QueueNodeNew(element);

    if (q->tail)
    {
        q->tail->next = node;
        node->previous = q->tail;
        q->tail = node;
    }
    else
    {
        q->tail = node;
        q->head = node;
    }

    ++q->node_count;
}

void *QueueDequeue(Queue *q)
{
    assert(q);

    QueueNode *node = q->head;
    void *data = node->data;
    q->head = node->next;
    if (q->head)
    {
        q->head->previous = NULL;
    }
    else
    {
        /* Empty queue */
        q->head = NULL;
        q->tail = NULL;
    }
    --q->node_count;
    /* Free the node */
    free(node);
    /* Return the data */
    return data;
}

void *QueueHead(Queue *q)
{
    assert(q);
    return q->head->data;
}

int QueueCount(const Queue *q)
{
    assert(q);
    return q->node_count;
}

bool QueueIsEmpty(const Queue *q)
{
    assert(q);
    return q->node_count == 0;
}
