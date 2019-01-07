/*
   Copyright 2019 Northern.tech AS

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

#ifndef CFENGINE_THREADED_QUEUE_H
#define CFENGINE_THREADED_QUEUE_H

#include <platform.h>

typedef struct ThreadedQueue_ ThreadedQueue;

/**
  @brief Creates a new thread safe queue with specified capacity.
  @param [in] initial_capacity Initial capacity, defaults to 1.
  @param [in] ItemDestroy Function used to destroy data elements.
  */
ThreadedQueue *ThreadedQueueNew(size_t initial_capacity,
                                void (ItemDestroy) (void *item));

/**
  @brief Destroys the queue and frees the memory it occupies.
  @warning ThreadedQueue should only be destroyed if all threads are joined
  @param [in] queue The queue to destroy.
  */
void ThreadedQueueDestroy(ThreadedQueue *queue);

/**
  @brief Frees the memory allocated for the data pointer and the struct itself.
  @param [in] queue The queue to free.
  */
void ThreadedQueueSoftDestroy(ThreadedQueue *queue);

/**
  @brief Returns and removes the first element of the queue.
  @note If queue is empty, blocks for `timeout` seconds or until signalled.
        If THREAD_WAIT_INDEFINITELY is specified, waits forever until signal
        is given. If it times out, it returns false.
  @param [in] queue The queue to pop from.
  @param [out] item The item at the first poisition in the queue.
  @param [in] timeout Timeout for blocking in seconds.
  @return true on success, false if timed out or queue was empty.
  */
bool ThreadedQueuePop(ThreadedQueue *queue, void **item, int timeout);

/**
  @brief Pops num elements from the queue into data_array, returns amount.
  @note If queue is empty, blocks for `timeout` seconds or until signalled.
        If THREAD_WAIT_INDEFINITELY is specified, waits forever until
        signalled. If it times out, it returns 0 and sets *data_array to NULL.
  @warning The pointer array will have to be freed manually.
  @param [in] queue The queue to pop from.
  @param [out] data_array Pointer to location to put popped elements.
  @param [in] timeout Timeout for blocking in seconds.
  @return Amount of elements popped.
  */
size_t ThreadedQueuePopN(ThreadedQueue *queue,
                         void ***data_array,
                         size_t num,
                         int timeout);

/**
  @brief Pushes a new item on top of the queue, returns current size.
  @param [in] queue The queue to push to.
  @param [in] item The item to push.
  @return Current amount of elements in the queue.
  */
size_t ThreadedQueuePush(ThreadedQueue *queue, void *item);

/**
  @brief Get current number of items in queue.
  @note On NULL queue, returns 0.
  @param [in] queue The queue.
  @return The amount of elements in the queue.
  */
size_t ThreadedQueueCount(ThreadedQueue const *queue);

/**
  @brief Get current capacity of queue.
  @note On NULL queue, returns 0.
  @param [in] queue The queue.
  @return The current capacity of the queue.
  */
size_t ThreadedQueueCapacity(ThreadedQueue const *queue);

/**
  @brief Checks if a queue is empty.
  @param [in] queue The queue.
  @return Returns true if queue is empty, false otherwise.
  */
bool ThreadedQueueIsEmpty(ThreadedQueue const *queue);

/**
  @brief Waits until queue is empty.
  @note Useful for situations where you want to wait before populating the
        queue. Timeout can be set to THREAD_BLOCK_INDEFINITELY to wait
        forever. Otherwise waits the amount of seconds specified.
  @param [in] queue The queue.
  @param [in] timeout Amount of seconds to wait before timing out.
  @return True if it successfully waited, false if it timed out.
  */
bool ThreadedQueueWaitEmpty(ThreadedQueue const *queue, int timeout);

/**
  @brief Create a shallow copy of a given queue.
  @note This makes a new queue pointing to the same memory as the old queue.
  @note Is only thread safe if original queue was also thread safe.
  @param [in] queue The queue.
  @return A new queue pointing to the same data.
  */
ThreadedQueue *ThreadedQueueCopy(ThreadedQueue *queue);

#endif
