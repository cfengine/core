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

#ifndef CFENGINE_STACK_H
#define CFENGINE_STACK_H

#include <platform.h>

typedef struct Stack_ Stack;

/**
  @brief Creates a new stack with specified capacity.
  @param [in] initial_capacity Initial capacity, defaults to 1.
  @param [in] ItemDestroy Function used to destroy data elements.
  */
Stack *StackNew(size_t initial_capacity, void (*ItemDestroy) ());

/**
  @brief Destroys the stack and frees the memory it occupies.
  @param [in] stack The stack to destroy.
  @warning Stack should only be destroyed if all threads are joined
  */
void StackDestroy(Stack *stack);

/**
  @brief Frees the memory allocated for the data pointer and the struct itself.
  @param [in] stack The stack to free.
  */
void StackSoftDestroy(Stack *stack);

/**
  @brief Returns and removes the last element added to the stack.
  @note Will return NULL if stack is empty.
  @param [in] stack The stack to pop from.
  @return A pointer to the last data added.
  */
void *StackPop(Stack *stack);

/**
  @brief Adds a new item on top of the stack.
  @param [in] stack The stack to push to.
  @param [in] item The item to push.
  */
void StackPush(Stack *stack, void *item);

/**
  @brief Adds a new item on top of the stack and returns the current size.
  @param [in] stack The stack to push to.
  @param [in] item The item to push.
  @return The amount of elements in the stack.
  */
size_t StackPushReportCount(Stack *stack, void *item);

/**
  @brief Get current number of items in stack.
  @note On NULL stack, returns 0.
  @param [in] stack The stack.
  @return The amount of elements in the stack.
  */
size_t StackCount(Stack const *stack);

/**
  @brief Get current capacity of stack.
  @note On NULL stack, returns 0.
  @param [in] stack The stack.
  @return The current capacity of the stack.
  */
size_t StackCapacity(Stack const *stack);

/**
  @brief Create a shallow copy of a given stack.
  @note This makes a new stack pointing to the same memory as the old stack.
  @param [in] stack The stack.
  @return A new stack pointing to the same data.
  */
Stack *StackCopy(Stack const *stack);

/**
  @brief Checks if a stack is empty.
  @param [in] stack The stack.
  @return Returns true if stack is empty, false otherwise.
  */
bool StackIsEmpty(Stack const *stack);

#endif
