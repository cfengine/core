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
#include <threaded_deque.h>


#define EXPAND_FACTOR     2
#define DEFAULT_CAPACITY 16

/** @struct ThreadedDeque_
  @brief An implementation of a thread safe deque based on a circular array

  Can push left, push right and give various statistics about its contents,
  like amount of elements, capacity and if it is empty. Has the ability to
  block if deque is empty, waiting for new elements to be pushed.
  */
struct ThreadedDeque_ {
    pthread_mutex_t *lock;            /**< Thread lock for accessing data. */
    pthread_cond_t *cond_non_empty;   /**< Blocking condition if empty     */
    pthread_cond_t *cond_empty;       /**< Blocking condition if not empty */
    void (*ItemDestroy) (void *item); /**< Data-specific destroy function. */
    void **data;                      /**< Internal array of elements.     */
    size_t left;                      /**< Current position in deque.      */
    size_t right;                     /**< Current end of deque.           */
    size_t size;                      /**< Current size of deque.          */
    size_t capacity;                  /**< Current memory allocated.       */
};

static void DestroyRange(ThreadedDeque *deque, size_t start, size_t end);
static void ExpandIfNecessary(ThreadedDeque *deque);

ThreadedDeque *ThreadedDequeNew(size_t initial_capacity,
                                void (ItemDestroy) (void *item))
{
    ThreadedDeque *deque = xcalloc(1, sizeof(ThreadedDeque));

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
            "Failed to use error-checking mutexes for deque, "
            "falling back to normal ones (pthread_mutexattr_settype: %s)",
            GetErrorStrFromCode(ret));
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    }

    deque->lock = xmalloc(sizeof(pthread_mutex_t));
    ret = pthread_mutex_init(deque->lock, &attr);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize mutex (pthread_mutex_init: %s)",
            GetErrorStrFromCode(ret));
        pthread_mutexattr_destroy(&attr);
        free(deque->lock);
        free(deque);
        return NULL;
    }

    pthread_mutexattr_destroy(&attr);

    deque->cond_non_empty = xmalloc(sizeof(pthread_cond_t));
    ret = pthread_cond_init(deque->cond_non_empty, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize thread condition (pthread_cond_init: %s)",
            GetErrorStrFromCode(ret));
        free(deque->lock);
        free(deque->cond_non_empty);
        free(deque);
        return NULL;
    }

    deque->cond_empty = xmalloc(sizeof(pthread_cond_t));
    ret = pthread_cond_init(deque->cond_empty, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize thread condition "
            "(pthread_cond_init: %s)",
            GetErrorStrFromCode(ret));
        free(deque->lock);
        free(deque->cond_empty);
        free(deque->cond_non_empty);
        free(deque);
        return NULL;
    }

    deque->capacity = initial_capacity;
    deque->left = 0;
    deque->right = 0;
    deque->size = 0;
    deque->data = xmalloc(sizeof(void *) * initial_capacity);
    deque->ItemDestroy = ItemDestroy;

    return deque;
}

void ThreadedDequeDestroy(ThreadedDeque *deque)
{
    if (deque != NULL)
    {
        DestroyRange(deque, deque->left, deque->right);

        ThreadedDequeSoftDestroy(deque);
    }
}

void ThreadedDequeSoftDestroy(ThreadedDeque *deque)
{
    if (deque != NULL)
    {
        if (deque->lock != NULL)
        {
            pthread_mutex_destroy(deque->lock);
            free(deque->lock);
        }

        if (deque->cond_non_empty != NULL)
        {
            pthread_cond_destroy(deque->cond_non_empty);
            free(deque->cond_non_empty);
        }

        if (deque->cond_empty != NULL)
        {
            pthread_cond_destroy(deque->cond_empty);
            free(deque->cond_empty);
        }

        free(deque->data);
        free(deque);
    }
}

bool ThreadedDequePopLeft(ThreadedDeque *deque,
                          void **item,
                          int timeout)
{
    assert(deque != NULL);

    ThreadLock(deque->lock);

    if (deque->size == 0 && timeout != 0)
    {
        int res = 0;
        do {
            res = ThreadWait(deque->cond_non_empty, deque->lock, timeout);

            if (res != 0)
            {
                /* Lock is reacquired even when timed out, so it needs to be
                   released again. */
                ThreadUnlock(deque->lock);
                return false;
            }
        } while (deque->size == 0);
        // Reevaluate predicate to protect against spurious wakeups
    }

    bool ret = true;
    if (deque->size > 0)
    {
        size_t left = deque->left;
        *item = deque->data[left];

        deque->data[left++] = NULL;

        left %= deque->capacity;
        deque->left = left;
        deque->size--;
    } else {
        ret = false;
        *item = NULL;
    }

    if (deque->size == 0)
    {
        // Signals that the deque is empty for ThreadedDequeWaitEmpty
        pthread_cond_broadcast(deque->cond_empty);
    }

    ThreadUnlock(deque->lock);

    return ret;
}

bool ThreadedDequePopRight(ThreadedDeque *deque,
                           void **item,
                           int timeout)
{
    assert(deque != NULL);

    ThreadLock(deque->lock);

    if (deque->size == 0 && timeout != 0)
    {
        int res = 0;
        do {
            res = ThreadWait(deque->cond_non_empty, deque->lock, timeout);

            if (res != 0)
            {
                /* Lock is reacquired even when timed out, so it needs to be
                   released again. */
                ThreadUnlock(deque->lock);
                return false;
            }
        } while (deque->size == 0);
        // Reevaluate predicate to protect against spurious wakeups
    }

    bool ret = true;
    if (deque->size > 0)
    {
        size_t right = deque->right;
        right = right == 0 ? deque->capacity - 1 : right - 1;

        *item = deque->data[right];
        deque->data[right] = NULL;

        deque->right = right;
        deque->size--;
    } else {
        ret = false;
        *item = NULL;
    }

    if (deque->size == 0)
    {
        // Signals that the deque is empty for ThreadedDequeWaitEmpty
        pthread_cond_broadcast(deque->cond_empty);
    }

    ThreadUnlock(deque->lock);

    return ret;
}

size_t ThreadedDequePopLeftN(ThreadedDeque *deque,
                             void ***data_array,
                             size_t num,
                             int timeout)
{
    assert(deque != NULL);

    ThreadLock(deque->lock);

    if (deque->size == 0 && timeout != 0)
    {
        int res = 0;
        do {
            res = ThreadWait(deque->cond_non_empty, deque->lock, timeout);

            if (res != 0)
            {
                /* Lock is reacquired even when timed out, so it needs to be
                   released again. */
                ThreadUnlock(deque->lock);
                *data_array = NULL;
                return 0;
            }
        } while (deque->size == 0);
        // Reevaluate predicate to protect against spurious wakeups
    }

    size_t size = num < deque->size ? num : deque->size;
    void **data = NULL;

    if (size > 0)
    {
        data = xcalloc(size, sizeof(void *));
        size_t left = deque->left;

        for (size_t i = 0; i < size; i++)
        {
            data[i] = deque->data[left];
            deque->data[left++] = NULL;
            left %= deque->capacity;
        }

        deque->left = left;
        deque->size -= size;
    }

    if (deque->size == 0)
    {
        // Signals that the deque is empty for ThreadedDequeWaitEmpty
        pthread_cond_broadcast(deque->cond_empty);
    }

    *data_array = data;

    ThreadUnlock(deque->lock);

    return size;
}

size_t ThreadedDequePopRightN(ThreadedDeque *deque,
                              void ***data_array,
                              size_t num,
                              int timeout)
{
    assert(deque != NULL);

    ThreadLock(deque->lock);

    if (deque->size == 0 && timeout != 0)
    {
        int res = 0;
        do {
            res = ThreadWait(deque->cond_non_empty, deque->lock, timeout);

            if (res != 0)
            {
                /* Lock is reacquired even when timed out, so it needs to be
                   released again. */
                ThreadUnlock(deque->lock);
                *data_array = NULL;
                return 0;
            }
        } while (deque->size == 0);
        // Reevaluate predicate to protect against spurious wakeups
    }

    size_t size = num < deque->size ? num : deque->size;
    void **data = NULL;

    if (size > 0)
    {
        data = xcalloc(size, sizeof(void *));
        size_t right = deque->right;

        for (size_t i = 0; i < size; i++)
        {
            right = right == 0 ? deque->capacity - 1 : right - 1;
            data[i] = deque->data[right];
            deque->data[right] = NULL;
        }

        deque->right = right;
        deque->size -= size;
    }

    if (deque->size == 0)
    {
        // Signals that the deque is empty for ThreadedDequeWaitEmpty
        pthread_cond_broadcast(deque->cond_empty);
    }

    *data_array = data;

    ThreadUnlock(deque->lock);

    return size;
}

size_t ThreadedDequePushLeft(ThreadedDeque *deque, void *item)
{
    assert(deque != NULL);

    ThreadLock(deque->lock);

    ExpandIfNecessary(deque);

    deque->left = deque->left == 0 ? deque->capacity - 1 : deque->left - 1;
    deque->data[deque->left] = item;
    deque->size++;
    size_t const size = deque->size;
    pthread_cond_signal(deque->cond_non_empty);

    ThreadUnlock(deque->lock);

    return size;
}

size_t ThreadedDequePushRight(ThreadedDeque *deque, void *item)
{
    assert(deque != NULL);

    ThreadLock(deque->lock);

    ExpandIfNecessary(deque);

    deque->data[deque->right++] = item;
    deque->right %= deque->capacity;
    deque->size++;
    size_t const size = deque->size;

    pthread_cond_signal(deque->cond_non_empty);

    ThreadUnlock(deque->lock);

    return size;
}

size_t ThreadedDequeCount(ThreadedDeque const *deque)
{
    assert(deque != NULL);

    ThreadLock(deque->lock);
    size_t const count = deque->size;
    ThreadUnlock(deque->lock);

    return count;
}

size_t ThreadedDequeCapacity(ThreadedDeque const *deque)
{
    assert(deque != NULL);

    ThreadLock(deque->lock);
    size_t const capacity = deque->capacity;
    ThreadUnlock(deque->lock);

    return capacity;
}

bool ThreadedDequeIsEmpty(ThreadedDeque const *deque)
{
    assert(deque != NULL);

    ThreadLock(deque->lock);
    bool const empty = (deque->size == 0);
    ThreadUnlock(deque->lock);

    return empty;
}

bool ThreadedDequeWaitEmpty(ThreadedDeque const *deque, int timeout)
{
    assert(deque != NULL);

    ThreadLock(deque->lock);

    if (deque->size == 0)
    {
        ThreadUnlock(deque->lock);
        return true;
    }

    if (timeout == 0)
    {
        ThreadUnlock(deque->lock);
        return false;
    }

    do {
        int res = ThreadWait(deque->cond_empty, deque->lock, timeout);

        if (res != 0)
        {
            /* Lock is reacquired even when timed out, so it needs to be
               released again. */
            ThreadUnlock(deque->lock);
            return false;
        }
        // Reevaluate predicate to protect against spurious wakeups
    } while (deque->size != 0);

    ThreadUnlock(deque->lock);

    return true;
}

ThreadedDeque *ThreadedDequeCopy(ThreadedDeque *deque)
{
    assert(deque != NULL);

    ThreadLock(deque->lock);

    ThreadedDeque *new_deque = xmemdup(deque, sizeof(ThreadedDeque));
    new_deque->data = xmalloc(sizeof(void *) * deque->capacity);
    memcpy(new_deque->data, deque->data,
           sizeof(void *) * new_deque->capacity);

    ThreadUnlock(deque->lock);

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    // enables error checking for deadlocks
    int ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to use error-checking mutexes for deque, "
            "falling back to normal ones (pthread_mutexattr_settype: %s)",
            GetErrorStrFromCode(ret));
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    }

    new_deque->lock = xmalloc(sizeof(pthread_mutex_t));
    ret = pthread_mutex_init(new_deque->lock, &attr);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize mutex (pthread_mutex_init: %s)",
            GetErrorStrFromCode(ret));
        pthread_mutexattr_destroy(&attr);
        free(new_deque->lock);
        free(new_deque);
        return NULL;
    }

    new_deque->cond_non_empty = xmalloc(sizeof(pthread_cond_t));
    ret = pthread_cond_init(new_deque->cond_non_empty, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize thread condition "
            "(pthread_cond_init: %s)",
            GetErrorStrFromCode(ret));
        free(new_deque->lock);
        free(new_deque->cond_non_empty);
        free(new_deque);
        return NULL;
    }

    new_deque->cond_empty = xmalloc(sizeof(pthread_cond_t));
    ret = pthread_cond_init(new_deque->cond_empty, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize thread condition "
            "(pthread_cond_init: %s)",
            GetErrorStrFromCode(ret));
        free(new_deque->lock);
        free(new_deque->cond_empty);
        free(new_deque->cond_non_empty);
        free(new_deque);
        return NULL;
    }

    return new_deque;
}

/**
  @brief Destroys data in range.
  @warning Assumes that locks are acquired.
  @note If start == end, this means that all elements in deque will be
        destroyed. Since the internal array is circular, it will wrap around
        when reaching the array bounds.
  @param [in] deque  Pointer to struct.
  @param [in] start  Position to start destroying from.
  @param [in] end    First position to not destroy. Can be same as start to
                     destroy the entire allocated area.
  */
static void DestroyRange(ThreadedDeque *deque, size_t start, size_t end)
{
    assert(deque != NULL);
    if (start > deque->capacity || end > deque->capacity)
    {
        Log(LOG_LEVEL_DEBUG,
            "Failed to destroy ThreadedDeque, index greater than capacity: "
            "start = %zu, end = %zu, capacity = %zu",
            start, end, deque->capacity);
        return;
    }

    if ((deque->ItemDestroy != NULL) && deque->size > 0)
    {
        do
        {
            deque->ItemDestroy(deque->data[start]);
            start++;
            start %= deque->capacity;
        } while (start != end);
    }
}

/**
  @brief Either expands capacity of deque, or shifts tail to beginning.
  @warning Assumes that locks are acquired.
  @param [in] deque  Pointer to struct.
  */
static void ExpandIfNecessary(ThreadedDeque *deque)
{
    assert(deque != NULL);
    assert(deque->size <= deque->capacity);

    if (deque->size == deque->capacity)
    {
        if (deque->right <= deque->left)
        {
            size_t old_capacity = deque->capacity;

            deque->capacity *= EXPAND_FACTOR;
            deque->data = xrealloc(deque->data,
                                   sizeof(void *) * deque->capacity);

            /* Move the data that has wrapped around to the newly allocated
             * part of the deque, since we need a continuous block of memory.
             * Offset of new placement is `old_capacity`.
             */
            memmove(deque->data + old_capacity, deque->data,
                    sizeof(void *) * deque->right);

            deque->right += old_capacity;
        }
        else
        {
            deque->capacity *= EXPAND_FACTOR;
            deque->data = xrealloc(deque->data,
                                   sizeof(void *) * deque->capacity);
        }
    }
}
