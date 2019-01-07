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
#include <stack.h>


#define EXPAND_FACTOR     2
#define DEFAULT_CAPACITY 16

/** @struct Stack_
  @brief A simple thread-safe stack data structure.

  Can push, pop, and copy. Also has functions for showing current stack size
  and capacity, and if a stack is empty. If the amount of pushed elements
  exceed the capacity, it will be multiplied by EXPAND_FACTOR and reallocated
  with the new capacity. When destroying the stack, destroys each element with
  the ItemDestroy function specified -- unless it is NULL -- and then proceeds
  to destroy the lock, before freeing the data array and the stack itself.
  */
struct Stack_ {
    pthread_mutex_t *lock;            /**< Thread lock for accessing data. */
    void (*ItemDestroy) (void *item); /**< Data-specific destroy function. */
    void **data;                      /**< Internal array of elements.     */
    size_t size;                      /**< Amount of elements in stack.    */
    size_t capacity;                  /**< Current memory allocated.       */
};

static void DestroyRange(Stack *stack, size_t start, size_t end);
static void ExpandIfNecessary(Stack *stack);

Stack *StackNew(size_t initial_capacity, void (ItemDestroy) (void *item))
{
    Stack *stack = xmalloc(sizeof(Stack));

    if (initial_capacity == 0)
    {
        initial_capacity = DEFAULT_CAPACITY;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    int ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to use error-checking mutexes for stack, "
            "falling back to normal ones (pthread_mutexattr_settype: %s)",
            GetErrorStrFromCode(ret));
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    }

    stack->lock = malloc(sizeof(pthread_mutex_t));
    ret = pthread_mutex_init(stack->lock, &attr);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize mutex (pthread_mutex_init: %s)",
            GetErrorStrFromCode(ret));
        pthread_mutexattr_destroy(&attr);
        free(stack);
        return NULL;
    }

    pthread_mutexattr_destroy(&attr);

    stack->capacity = initial_capacity;
    stack->size = 0;
    stack->data = xcalloc(initial_capacity, sizeof(void *));
    stack->ItemDestroy = ItemDestroy;

    return stack;
}

void StackDestroy(Stack *stack)
{
    if (stack != NULL)
    {
        ThreadLock(stack->lock);
        DestroyRange(stack, 0, stack->size);
        ThreadUnlock(stack->lock);

        StackSoftDestroy(stack);
    }
}

void StackSoftDestroy(Stack *stack)
{
    if (stack != NULL)
    {
        if (stack->lock != NULL)
        {
            pthread_mutex_destroy(stack->lock);
            free(stack->lock);
        }

        free(stack->data);
        free(stack);
    }
}

void *StackPop(Stack *stack)
{
    assert(stack != NULL);

    ThreadLock(stack->lock);

    size_t size = stack->size;
    void *item = NULL;

    if (size > 0)
    {
        size--;
        item = stack->data[size];

        stack->data[size] = NULL;
        stack->size = size;
    }

    ThreadUnlock(stack->lock);

    return item;
}

void StackPush(Stack *stack, void *item)
{
    assert(stack != NULL);

    ThreadLock(stack->lock);

    ExpandIfNecessary(stack);
    stack->data[stack->size++] = item;

    ThreadUnlock(stack->lock);
}

size_t StackPushReportCount(Stack *stack, void *item)
{
    assert(stack != NULL);

    ThreadLock(stack->lock);

    ExpandIfNecessary(stack);
    stack->data[stack->size++] = item;
    size_t size = stack->size;

    ThreadUnlock(stack->lock);

    return size;
}

size_t StackCount(Stack const *stack)
{
    assert(stack != NULL);

    ThreadLock(stack->lock);
    size_t count = stack->size;
    ThreadUnlock(stack->lock);

    return count;
}

size_t StackCapacity(Stack const *stack)
{
    assert(stack != NULL);

    ThreadLock(stack->lock);
    size_t capacity = stack->capacity;
    ThreadUnlock(stack->lock);

    return capacity;
}

bool StackIsEmpty(Stack const *stack)
{
    assert(stack != NULL);

    ThreadLock(stack->lock);
    bool const empty = (stack->size == 0);
    ThreadUnlock(stack->lock);

    return empty;
}

Stack *StackCopy(Stack const *stack)
{
    assert(stack != NULL);

    ThreadLock(stack->lock);

    Stack *new_stack = xmemdup(stack, sizeof(Stack));
    new_stack->data = xmalloc(sizeof(void *) * stack->capacity);
    memcpy(new_stack->data, stack->data, sizeof(void *) * stack->size);

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    int ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to use error-checking mutexes for stack, "
            "falling back to normal ones (pthread_mutexattr_settype: %s)",
            GetErrorStrFromCode(ret));
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    }

    new_stack->lock = malloc(sizeof(pthread_mutex_t));
    ret = pthread_mutex_init(new_stack->lock, &attr);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize mutex (pthread_mutex_init: %s)",
            GetErrorStrFromCode(ret));
        free(new_stack->lock);
        free(new_stack);
        new_stack = NULL;
    }

    pthread_mutexattr_destroy(&attr);
    ThreadUnlock(stack->lock);

    return new_stack;
}

/**
  @brief Destroys data in range.
  @note Assumes that locks are acquired.
  @param [in] stack Pointer to struct.
  @param [in] start Start position to destroy from.
  @param [in] end Where to stop.
  */
static void DestroyRange(Stack *stack, size_t start, size_t end)
{
    assert(stack != NULL);
    if (start > stack->capacity || end > stack->capacity)
    {
        return;
    }

    if (stack->ItemDestroy)
    {
        for (size_t i = start; i < end; i++)
        {
            stack->ItemDestroy(stack->data[i]);
        }
    }
}

/**
  @brief Expands capacity of stack.
  @note Assumes that locks are acquired.
  @param [in] stack Pointer to struct.
  */
static void ExpandIfNecessary(Stack *stack)
{
    assert(stack != NULL);
    assert(stack->size <= stack->capacity);

    if (stack->size == stack->capacity)
    {
        stack->capacity *= EXPAND_FACTOR;
        stack->data = xrealloc(stack->data, sizeof(void *) * stack->capacity);
    }
}
