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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_BUFFERLIST_H
#define CFENGINE_BUFFERLIST_H

#include <stdlib.h>
#include "refcount.h"
#include "buffer.h"
#include "list.h"

/**
  @brief Buffer list implementation.

  This data structure is a specialization of List to be used only with Buffer structures.
  Please consult the List documentation for more general information.
  */
typedef struct BufferList BufferList;
typedef struct BufferListMutableIterator BufferListMutableIterator;
typedef struct BufferListIterator BufferListIterator;
/**
  @brief Initialization of a buffer list.
  @return A fully initialized list ready to be used or -1 in case of error.
  */
BufferList *BufferListNew();
/**
  @brief Destroy a buffer list.
  @param list List to be destroyed. It can be a NULL pointer.
  @return 0 if destroyed, -1 otherwise.
  */
int BufferListDestroy(BufferList **list);
/**
  @brief Performs a shallow copy of a buffer list.
  A shallow copy is a copy that does not copy the elements but only the list structure.
  This is done by internal reference counting. If any of the lists is modified afterwards
  then a deep copy of the list is triggered and all the elements are copied.
  @param origin Original list to be copied.
  @param destination List to be copied to.
  @return 0 if copied, -1 otherwise.
  */
int BufferListCopy(BufferList *origin, BufferList **destination);
/**
  @brief Adds an element to the beginning of the list.
  Notice that we do not copy the element, so if the original element is free'd there will be
  a dangling pointer.
  We used to change the state of the list after adding an element, but now we don't do it.
  The reason is because adding an element is not destructive, no iterators will be affected
  by this, while removing it is destructive. An iterator might be pointing to the dark side
  of the moon after a removal operation.
  If the list is shared this will trigger a deep copy of the list.
  @param list Linked list.
  @param payload Data to be added.
  @return 0 if prepended, -1 otherwise.
  */
int BufferListPrepend(BufferList *list, Buffer *payload);
/**
  @brief Adds an element to the end of the list.
  Notice that we do not copy the element, so if the original element is free'd there will be
  a dangling pointer.
  We used to change the state of the list after adding an element, but now we don't do it.
  The reason is because adding an element is not destructive, no iterators will be affected
  by this, while removing it is destructive. An iterator might be pointing to the dark side
  of the moon after a removal operation.
  If the list is shared this will trigger a deep copy of the list.
  @param list Linked list.
  @param payload Data to be added.
  @return 0 if appended, -1 otherwise.
  */
int BufferListAppend(BufferList *list, Buffer *payload);
/**
  @brief Removes an element from the buffer list.
  Removes the first element that matches the payload. It starts looking from the beginning of the list.
  Notice that this might trigger a deep copy of the list. This only happens if the list was
  copied before.
  @param list Buffer list.
  @param payload Data to be removed.
  @return 0 if removed, -1 otherwise.
  */
int BufferListRemove(BufferList *list, Buffer *payload);
/**
  @brief Returns the number of elements on a given linked list.
  @param list Buffer list.
  @return The number of elements on the list.
  */
int BufferListCount(BufferList *list);

/**
  @brief Gets an iterator for a given buffer list.

  This iterator will be invalid if data is removed from the list. It will still be valid
  after a new addition though.
  @note After creation the iterator will be pointing to the first item of the list.
  @param list Linked list
  @param iterator Iterator.
  @return A fully initialized iterator or NULL in case of error.
  */
BufferListIterator *BufferListIteratorGet(BufferList *list);
/**
  @brief Releases the memory associated with an iterator.

  This function exists only to free the memory associated with the iterator, there is no strong
  connection between the iterator and the list.
  @note It is not possible to get an iterator for an empty list.
  @param iterator Iterator.
  @return 0 if released, -1 otherwise.
  */
int BufferListIteratorDestroy(BufferListIterator **iterator);
/**
  @brief Moves the iterator to the first element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int BufferListIteratorFirst(BufferListIterator *iterator);
/**
  @brief Moves the iterator to the last element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int BufferListIteratorLast(BufferListIterator *iterator);
/**
  @brief Moves the iterator to the next element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int BufferListIteratorNext(BufferListIterator *iterator);
/**
  @brief Moves the iterator to the previous element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int BufferListIteratorPrevious(BufferListIterator *iterator);
/**
  @brief Returns the data associated with the current element.
  @param iterator Iterator.
  @return Pointer to the data or NULL if it was not possible.
  */
Buffer *BufferListIteratorData(const BufferListIterator *iterator);
/**
  @brief Checks if the iterator has a next element
  @return True if it has a next element or False if not.
  */
bool BufferListIteratorHasNext(const BufferListIterator *iterator);
/**
  @brief Checks if the iterator has a previous element
  @return True if it has a previous element or False if not.
  */
bool BufferListIteratorHasPrevious(const BufferListIterator *iterator);

/**
  @brief Creates a new mutable iterator.

  A mutable iterator can be used for the same kind of operations as a normal iterator, but it can also be
  used to add and remove elements while iterating over the list. Any removal operation will invalidate all
  the normal iterators though.

  Since there can be only one mutable iterator for a list at any given time, the creation of a second iterator
  will fail.
  @note After creation the iterator will be pointing to the first item of the list.
  @param iterator Iterator to be initialized.
  @return A fully initialized iterator or NULL in case of error.
  */
BufferListMutableIterator *BufferListMutableIteratorGet(BufferList *list);
/**
  @brief Releases the memory associated with an iterator.

  This function has to be called for mutable iterators, otherwise the list will think there is already one mutable
  iterator and the creation of new mutable iterators will not be possible.
  @note It is not possible to get an iterator for an empty list.
  @param iterator Iterator.
  @return 0 if released, -1 otherwise.
  */
int BufferListMutableIteratorRelease(BufferListMutableIterator **iterator);
/**
  @brief Moves the iterator to the first element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int BufferListMutableIteratorFirst(BufferListMutableIterator *iterator);
/**
  @brief Moves the iterator to the last element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int BufferListMutableIteratorLast(BufferListMutableIterator *iterator);
/**
  @brief Moves the iterator to the next element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int BufferListMutableIteratorNext(BufferListMutableIterator *iterator);
/**
  @brief Moves the iterator to the previous element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int BufferListMutableIteratorPrevious(BufferListMutableIterator *iterator);
/**
  @brief Returns the data associated with the current element.
  @param iterator Iterator.
  @return Pointer to the data or NULL if it was not possible.
  */
Buffer *BufferListMutableIteratorData(const BufferListMutableIterator *iterator);
/**
  @brief Removes the current element from the list.

  After this operation all the normal iterators are invalid. The iterator might be pointing to the next element,
  or in the case of removing the last element, to the previous element.

  Although it is supported to remove elements from the list bypassing the iterator, i.e. calling ListRemove, this
  might bring unintended side effects. The iterator might be moved to another element, therefore special care must
  be taken when mixing removal both from the list and from the iterator.
  @param iterator Iterator
  @return 0 if removed, -1 otherwise.
  */
int BufferListMutableIteratorRemove(BufferListMutableIterator *iterator);
/**
  @brief Prepends element on front of the element pointed by the iterator.

  An important clarification, the iterator still points to the same element after this operation. It is up to the user
  to move the iterator to the prepended element.

  All the light operators are still valid after this operation.
  @param iterator Iterator
  @param payload Element to be prepended.
  @return 0 if prepended, -1 in case of error.
  */
int BufferListMutableIteratorPrepend(BufferListMutableIterator *iterator, Buffer *payload);
/**
  @brief Appends element after the element pointed by the iterator.

  An important clarification, the iterator still points to the same element after this operation. It is up to the user
  to move the iterator to the appended element.

  All the light operators are still valid after this operation.
  @param iterator Iterator
  @param payload Element to be appended.
  @return 0 if appended, -1 in case of error.
  */
int BufferListMutableIteratorAppend(BufferListMutableIterator *iterator, Buffer *payload);
/**
  @brief Checks if the iterator has a next element
  @return True if it has a next element or False if not.
  */
bool BufferListMutableIteratorHasNext(const BufferListMutableIterator *iterator);
/**
  @brief Checks if the iterator has a previous element
  @return True if it has a previous element or False if not.
  */
bool BufferListMutableIteratorHasPrevious(const BufferListMutableIterator *iterator);

#endif // CFENGINE_BUFFERLIST_H
