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

#include <alloc.h>
#include <logging.h>
#include <mutex.h>
#include <pthread.h>
#include <threaded_queue.h>


#define EXPAND_FACTOR     2
#define DEFAULT_CAPACITY 16

/** @struct ThreadedQueue_
  @brief An implementation of a thread safe queue based on a circular array

  Can enqueue, dequeue and give various statistics about its contents, like
  amount of elements, capacity and if it is empty. Has the ability to block
  if queue is empty, waiting for new elements to be queued.
  */
struct ThreadedQueue_ {
    pthread_mutex_t *lock;            /**< Thread lock for accessing data. */
    pthread_cond_t *cond_non_empty;   /**< Blocking condition if empty     */
    pthread_cond_t *cond_empty;       /**< Blocking condition if not empty */
    void (*ItemDestroy) (void *item); /**< Data-specific destroy function. */
    void **data;                      /**< Internal array of elements.     */
    size_t head;                      /**< Current position in queue.      */
    size_t tail;                      /**< Current end of queue.           */
    size_t size;                      /**< Current size of queue.          */
    size_t capacity;                  /**< Current memory allocated.       */
};

static void DestroyRange(ThreadedQueue *queue, size_t start, size_t end);
static void ExpandIfNecessary(ThreadedQueue *queue);

ThreadedQueue *ThreadedQueueNew(size_t initial_capacity,
                                void (ItemDestroy) (void *item))
{
    ThreadedQueue *queue = xcalloc(1, sizeof(ThreadedQueue));

    if (initial_capacity == 0)
    {
        initial_capacity = DEFAULT_CAPACITY;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    // enables errorchecking for deadlocks
    int ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to use error-checking mutexes for queue, "
            "falling back to normal ones (pthread_mutexattr_settype: %s)",
            GetErrorStrFromCode(ret));
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    }

    queue->lock = xmalloc(sizeof(pthread_mutex_t));
    ret = pthread_mutex_init(queue->lock, &attr);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize mutex (pthread_mutex_init: %s)",
            GetErrorStrFromCode(ret));
        pthread_mutexattr_destroy(&attr);
        free(queue->lock);
        free(queue);
        return NULL;
    }

    pthread_mutexattr_destroy(&attr);

    queue->cond_non_empty = xmalloc(sizeof(pthread_cond_t));
    ret = pthread_cond_init(queue->cond_non_empty, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize thread condition (pthread_cond_init: %s)",
            GetErrorStrFromCode(ret));
        free(queue->lock);
        free(queue->cond_non_empty);
        free(queue);
        return NULL;
    }

    queue->cond_empty = xmalloc(sizeof(pthread_cond_t));
    ret = pthread_cond_init(queue->cond_empty, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize thread condition "
            "(pthread_cond_init: %s)",
            GetErrorStrFromCode(ret));
        free(queue->lock);
        free(queue->cond_empty);
        free(queue->cond_non_empty);
        free(queue);
        return NULL;
    }

    queue->capacity = initial_capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    queue->data = xmalloc(sizeof(void *) * initial_capacity);
    queue->ItemDestroy = ItemDestroy;

    return queue;
}

void ThreadedQueueDestroy(ThreadedQueue *queue)
{
    if (queue != NULL)
    {
        ThreadLock(queue->lock);
        DestroyRange(queue, queue->head, queue->tail);
        ThreadUnlock(queue->lock);

        ThreadedQueueSoftDestroy(queue);
    }
}

void ThreadedQueueSoftDestroy(ThreadedQueue *queue)
{
    if (queue != NULL)
    {
        if (queue->lock != NULL)
        {
            pthread_mutex_destroy(queue->lock);
            free(queue->lock);
        }

        if (queue->cond_non_empty != NULL)
        {
            pthread_cond_destroy(queue->cond_non_empty);
            free(queue->cond_non_empty);
        }

        if (queue->cond_empty != NULL)
        {
            pthread_cond_destroy(queue->cond_empty);
            free(queue->cond_empty);
        }

        free(queue->data);
        free(queue);
    }
}

bool ThreadedQueuePop(ThreadedQueue *queue, void **item, int timeout)
{
    assert(queue != NULL);

    ThreadLock(queue->lock);

    if (queue->size == 0 && timeout != 0)
    {
        int res = 0;
        do {
            res = ThreadWait(queue->cond_non_empty, queue->lock, timeout);

            if (res != 0)
            {
                /* Lock is reacquired even when timed out, so it needs to be
                   released again. */
                ThreadUnlock(queue->lock);
                return false;
            }
        } while (queue->size == 0);
        // Reevaluate predicate to protect against spurious wakeups
    }

    bool ret = true;
    if (queue->size > 0)
    {
        size_t head = queue->head;
        *item = queue->data[head];

        queue->data[head++] = NULL;

        head %= queue->capacity;
        queue->head = head;
        queue->size--;
    } else {
        ret = false;
        *item = NULL;
    }

    if (queue->size == 0)
    {
        // Signals that the queue is empty for ThreadedQueueWaitEmpty
        pthread_cond_broadcast(queue->cond_empty);
    }

    ThreadUnlock(queue->lock);

    return ret;
}

size_t ThreadedQueuePopN(ThreadedQueue *queue,
                         void ***data_array,
                         size_t num,
                         int timeout)
{
    assert(queue != NULL);

    ThreadLock(queue->lock);

    if (queue->size == 0 && timeout != 0)
    {
        int res = 0;
        do {
            res = ThreadWait(queue->cond_non_empty, queue->lock, timeout);

            if (res != 0)
            {
                /* Lock is reacquired even when timed out, so it needs to be
                   released again. */
                ThreadUnlock(queue->lock);
                *data_array = NULL;
                return 0;
            }
        } while (queue->size == 0);
        // Reevaluate predicate to protect against spurious wakeups
    }

    size_t size = num < queue->size ? num : queue->size;
    void **data = NULL;

    if (size > 0)
    {
        data = xcalloc(size, sizeof(void *));
        size_t head = queue->head;

        for (size_t i = 0; i < size; i++)
        {
            data[i] = queue->data[head];
            queue->data[head++] = NULL;
            head %= queue->capacity;
        }

        queue->head = head;
        queue->size -= size;
    }

    if (queue->size == 0)
    {
        // Signals that the queue is empty for ThreadedQueueWaitEmpty
        pthread_cond_broadcast(queue->cond_empty);
    }

    *data_array = data;

    ThreadUnlock(queue->lock);

    return size;
}

size_t ThreadedQueuePush(ThreadedQueue *queue, void *item)
{
    assert(queue != NULL);

    ThreadLock(queue->lock);

    ExpandIfNecessary(queue);
    queue->data[queue->tail++] = item;
    queue->size++;
    size_t const size = queue->size;
    pthread_cond_signal(queue->cond_non_empty);

    ThreadUnlock(queue->lock);

    return size;
}

size_t ThreadedQueueCount(ThreadedQueue const *queue)
{
    assert(queue != NULL);

    ThreadLock(queue->lock);
    size_t const count = queue->size;
    ThreadUnlock(queue->lock);

    return count;
}

size_t ThreadedQueueCapacity(ThreadedQueue const *queue)
{
    assert(queue != NULL);

    ThreadLock(queue->lock);
    size_t const capacity = queue->capacity;
    ThreadUnlock(queue->lock);

    return capacity;
}

bool ThreadedQueueIsEmpty(ThreadedQueue const *queue)
{
    assert(queue != NULL);

    ThreadLock(queue->lock);
    bool const empty = (queue->size == 0);
    ThreadUnlock(queue->lock);

    return empty;
}

bool ThreadedQueueWaitEmpty(ThreadedQueue const *queue, int timeout)
{
    assert(queue != NULL);
    bool ret = true;

    ThreadLock(queue->lock);

    if (queue->size != 0)
    {
        if (timeout != 0)
        {
            int res = 0;

            do {
                res = ThreadWait(queue->cond_empty, queue->lock, timeout);

                if (res != 0)
                {
                    /* Lock is reacquired even when timed out, so it needs to
                       be released again. */
                    ThreadUnlock(queue->lock);
                    return false;
                }
            } while (queue->size != 0);
            // Reevaluate predicate to protect against spurious wakeups
        }
        else
        {
            ret = false;
        }
    }

    ThreadUnlock(queue->lock);

    return ret;
}

ThreadedQueue *ThreadedQueueCopy(ThreadedQueue *queue)
{
    assert(queue != NULL);

    ThreadLock(queue->lock);

    ThreadedQueue *new_queue = xmemdup(queue, sizeof(ThreadedQueue));
    new_queue->data = xmalloc(sizeof(void *) * queue->capacity);
    memcpy(new_queue->data, queue->data,
           sizeof(void *) * new_queue->capacity);

    ThreadUnlock(queue->lock);

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    // enables error checking for deadlocks
    int ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to use error-checking mutexes for queue, "
            "falling back to normal ones (pthread_mutexattr_settype: %s)",
            GetErrorStrFromCode(ret));
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    }

    new_queue->lock = xmalloc(sizeof(pthread_mutex_t));
    ret = pthread_mutex_init(new_queue->lock, &attr);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize mutex (pthread_mutex_init: %s)",
            GetErrorStrFromCode(ret));
        pthread_mutexattr_destroy(&attr);
        free(new_queue->lock);
        free(new_queue);
        return NULL;
    }

    new_queue->cond_non_empty = xmalloc(sizeof(pthread_cond_t));
    ret = pthread_cond_init(new_queue->cond_non_empty, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize thread condition "
            "(pthread_cond_init: %s)",
            GetErrorStrFromCode(ret));
        free(new_queue->lock);
        free(new_queue->cond_non_empty);
        free(new_queue);
        return NULL;
    }

    new_queue->cond_empty = xmalloc(sizeof(pthread_cond_t));
    ret = pthread_cond_init(new_queue->cond_empty, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize thread condition "
            "(pthread_cond_init: %s)",
            GetErrorStrFromCode(ret));
        free(new_queue->lock);
        free(new_queue->cond_empty);
        free(new_queue->cond_non_empty);
        free(new_queue);
        return NULL;
    }

    return new_queue;
}

/**
  @brief Destroys data in range.
  @warning Assumes that locks are acquired.
  @note If start == end, this means that all elements in queue will be
        destroyed. Since the internal array is circular, it will wrap around
        when reaching the array bounds.
  @param [in] queue Pointer to struct.
  @param [in] start Position to start destroying from.
  @param [in] end First position to not destroy. Can be same as start.
  */
static void DestroyRange(ThreadedQueue *queue, size_t start, size_t end)
{
    assert(queue != NULL);
    if (start > queue->capacity || end > queue->capacity)
    {
        Log(LOG_LEVEL_DEBUG,
            "Failed to destroy ThreadedQueue, index greater than capacity: "
            "start = %zu, end = %zu, capacity = %zu",
            start, end, queue->capacity);
        return;
    }

    if ((queue->ItemDestroy != NULL) && queue->size > 0)
    {
        queue->ItemDestroy(queue->data[start]);

        // In case start == end, start at second element in range
        for (size_t i = start + 1; i != end; i++)
        {
            i %= queue->capacity;

            queue->ItemDestroy(queue->data[i]);
        }
    }
}

/**
  @brief Either expands capacity of queue, or shifts tail to beginning.
  @warning Assumes that locks are acquired.
  @param [in] queue Pointer to struct.
  */
static void ExpandIfNecessary(ThreadedQueue *queue)
{
    assert(queue != NULL);
    assert(queue->size <= queue->capacity);

    if (queue->size == queue->capacity)
    {
        if (queue->tail <= queue->head)
        {
            size_t old_capacity = queue->capacity;

            queue->capacity *= EXPAND_FACTOR;
            queue->data = xrealloc(queue->data,
                                   sizeof(void *) * queue->capacity);

            memmove(queue->data + old_capacity, queue->data,
                    sizeof(void *) * queue->tail);

            queue->tail += old_capacity;
        }
        else
        {
            queue->capacity *= EXPAND_FACTOR;
            queue->data = xrealloc(queue->data,
                                   sizeof(void *) * queue->capacity);
        }
    }

    queue->tail %= queue->capacity;
}
