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

#include <platform.h>

#ifndef CFENGINE_QUEUE_H
#define CFENGINE_QUEUE_H

/**
  @brief Reference counted Queue implementation.

  This queue implementation is reference counted in order to make it easier to
  copy it. For the reference count implementation to work properly it is needed
  to pass a copy function.
  The elements are stored as pointers, therefore it is important not to free the
  elements after adding them to the queue.
  When the queue is destroyed, the destroy function will be called for each
  element in order to free all the memory. If the queue is reference counted,
  then no elements will be destroyed because the queue will simply detach.
  Notice that if no destroy function is passed, then free() is called on each
  pointer, therefore for simple types it is acceptable to set destroy to NULL
  (as long as elements are allocated on the heap).
  */

typedef struct Queue Queue;

/**
  @brief Creates a new Queue.
  @param copy Function to copy elements.
  @param destroy Function to destroy elements.
  @return A fully constructed Queue or NULL in case of problems.
  */
Queue *QueueNew(void (*copy)(const void *source, void **destination), void (*destroy)(void *));
/**
  @brief Destroys a Queue.
  @param queue  Queue to be destroyed.
  */
void QueueDestroy(Queue **queue);
/**
  @brief Copies a queue to a new queue.
  @param origin Original queue.
  @param destination Destination queue.
  @return 0 if successful, -1 in case of error.
  */
int QueueCopy(Queue *origin, Queue **destination);
/**
  @brief Enqueues an element.
  @param queue Queue to operate.
  @param element Element to enqueue.
  @return 0 if enqueued, -1 in any other case.
  */
int QueueEnqueue(Queue *queue, void *element);
/**
  @brief Dequeues the first element and returns it.
  @param queue Queue to operate.
  @return The first element in the queue.
  */
void *QueueDequeue(Queue *queue);
/**
  @brief Returns a pointer to the first element in the queue without dequeueing it.
  @param queue Queue to operate on.
  @return A pointer to the first element in the queue or NULL in case of error.
  */
void *QueueHead(Queue *queue);
/**
  @brief Number of elements in the queue.
  @param queue Queue to operate on.
  @return The number of elements in the queue or -1 in case of error.
  */
int QueueCount(const Queue *queue);
/**
  @brief Whether the queue is empty or not.
  @param queue Queue to operate on.
  @return True if the queue is empty and false in any other case.
  */
bool QueueIsEmpty(const Queue *queue);

#endif // CFENGINE_QUEUE_H
