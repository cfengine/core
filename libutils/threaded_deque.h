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

#ifndef CFENGINE_THREADED_DEQUE_H
#define CFENGINE_THREADED_DEQUE_H

#include <platform.h>

typedef struct ThreadedDeque_ ThreadedDeque;

/**
  @brief Creates a new thread safe deque with specified capacity.
  @param [in] initial_capacity  Initial capacity, defaults to 1.
  @param [in] ItemDestroy       Function used to destroy data elements.

  @return A new ThreadedDeque on success, NULL if pthread_mutex_init or
          pthread_cond_init fails.
  */
ThreadedDeque *ThreadedDequeNew(size_t initial_capacity,
                                void (ItemDestroy) (void *item));

/**
  @brief Destroys the deque and frees the memory it occupies.
  @warning ThreadedDeque should only be destroyed if all threads are joined
  @param [in] deque  The deque to destroy.
  */
void ThreadedDequeDestroy(ThreadedDeque *deque);

/**
  @brief Frees the memory allocated for the pointers and the struct.
  @param [in] deque  The deque to free.
  */
void ThreadedDequeSoftDestroy(ThreadedDeque *deque);

/**
  @brief Returns and removes the leftmost element of the deque.
  @note If deque is empty, blocks for `timeout` seconds or until signalled.
        If THREAD_WAIT_INDEFINITELY is specified, waits forever until signal
        is given. If it times out, it returns false.
  @param [in]  deque    The deque to pop from.
  @param [out] item     The item at the first poisition in the deque.
  @param [in]  timeout  Timeout for blocking in seconds.

  @return true on success, false if timed out or deque was empty.
  */
bool ThreadedDequePopLeft(ThreadedDeque *deque,
                          void **item,
                          int timeout);

/**
  @brief Returns and removes the rightmost element of the deque.
  @note If deque is empty, blocks for `timeout` seconds or until signalled.
        If THREAD_WAIT_INDEFINITELY is specified, waits forever until signal
        is given. If it times out, it returns false.
  @param [in]  deque    The deque to pop from.
  @param [out] item     The item at the first poisition in the deque.
  @param [in]  timeout  Timeout for blocking in seconds.

  @return true on success, false if timed out or deque was empty.
  */
bool ThreadedDequePopRight(ThreadedDeque *deque,
                           void **item,
                           int timeout);

/**
  @brief Pops num elements from the left in deque into data_array.
  @note If deque is empty, blocks for `timeout` seconds or until signalled.
        If THREAD_WAIT_INDEFINITELY is specified, waits forever until
        signalled. If it times out, it returns 0 and sets *data_array to NULL.
  @warning The pointer array will have to be freed manually.
  @param [in]  deque       The deque to pop from.
  @param [out] data_array  Pointer to location to put popped elements.
  @param [in]  timeout     Timeout for blocking in seconds.

  @return Amount of elements popped.
  */
size_t ThreadedDequePopLeftN(ThreadedDeque *deque,
                             void ***data_array,
                             size_t num,
                             int timeout);

/**
  @brief Pops num elements from the right in deque into data_array.
  @note If deque is empty, blocks for `timeout` seconds or until signalled.
        If THREAD_WAIT_INDEFINITELY is specified, waits forever until
        signalled. If it times out, it returns 0 and sets *data_array to NULL.
  @warning The pointer array will have to be freed manually.
  @param [in]  deque       The deque to pop from.
  @param [out] data_array  Pointer to location to put popped elements.
  @param [in]  timeout     Timeout for blocking in seconds.

  @return Amount of elements popped.
  */
size_t ThreadedDequePopRightN(ThreadedDeque *deque,
                              void ***data_array,
                              size_t num,
                              int timeout);

/**
  @brief Pushes item to left end of the deque, returns current size.
  @param [in] deque    The deque to push to.
  @param [in] item     The item to push.

  @return Current amount of elements in the deque.
  */
size_t ThreadedDequePushLeft(ThreadedDeque *deque, void *item);

/**
  @brief Pushes item to right end of the deque, returns current size.
  @param [in] deque    The deque to push to.
  @param [in] item     The item to push.

  @return Current amount of elements in the deque.
  */
size_t ThreadedDequePushRight(ThreadedDeque *deque, void *item);

/**
  @brief Get current number of items in deque.
  @note On NULL deque, returns 0.
  @param [in] deque  The deque.

  @return The amount of elements in the deque.
  */
size_t ThreadedDequeCount(ThreadedDeque const *deque);

/**
  @brief Get current capacity of deque.
  @note On NULL deque, returns 0.
  @param [in] deque  The deque.

  @return The current capacity of the deque.
  */
size_t ThreadedDequeCapacity(ThreadedDeque const *deque);

/**
  @brief Checks if a deque is empty.
  @param [in] deque  The deque.

  @return Returns true if deque is empty, false otherwise.
  */
bool ThreadedDequeIsEmpty(ThreadedDeque const *deque);

/**
  @brief Waits until deque is empty.
  @note Useful for situations where you want to wait before populating the
        deque. Timeout can be set to THREAD_BLOCK_INDEFINITELY to wait
        forever. Otherwise waits the amount of seconds specified.
  @param [in] deque    The deque.
  @param [in] timeout  Amount of seconds to wait before timing out.

  @return True if it successfully waited, false if it timed out.
  */
bool ThreadedDequeWaitEmpty(ThreadedDeque const *deque, int timeout);

/**
  @brief Create a shallow copy of a given deque.
  @note This makes a new deque pointing to the same memory as the old deque.
  @note Is only thread safe if original deque was also thread safe.
  @param [in] deque  The deque.

  @return A new deque pointing to the same data.
  */
ThreadedDeque *ThreadedDequeCopy(ThreadedDeque *deque);

#endif // CFENGINE_THREADED_DEQUE_H
