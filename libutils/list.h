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

#ifndef CFENGINE_LIST_H
#define CFENGINE_LIST_H

#include <stdlib.h>
#include <refcount.h>

/**
  @brief Double linked list implementation.

  A linked list is to be used when data needs to be stored and then sequentially processed, not for quick
  lookups. Prime examples of bad usage of a linked list is for instance having a list of files and then
  iterating over the list to find a particular file based on the name. A Map will be much better for this type
  of usage. A good usage for a linked list is to store a list of files that need to be changed sequentially.
  For instance a list of files that need to change permissions.

  A linked list should not be used when fast retrieval is important, for that it is better to use a Map.
  The implementation of the list is kept intentionally opaque, so we can change it in the future. It provides a
  reference counted list with copy on write. In order for this to work the user needs to provide at least a copy
  function. The List is used at its full potential if the user provides three helper functions:
  - copy(void *source, void **destination)
  - compare(void *a, void *b)
  - destroy(void *element)

  It is highly recommend to at least implement the copy function, since that is the key for the copy on write implementation.
  The destroy function is recommended to avoid leaking memory whenever an element is destroyed. Notice that the
  list never copies the elements, so as long as the user still has access to the elements this function does not
  need to be implemented and the user then needs to delete the elements afterwards.

  The compare function is a nice to have function. It is used when the user wants to remove elements, in order to
  find the right element to delete. If this function is not provided, then the list will try to match the pointer.
  These three helper functions can assume that they will never be called with NULL pointers.

  A special note on the copy function. The copy function is analog to the copy constructor in C++, it takes a properly
  constructed element and produces a newly created element that is a copy of the previous one. The copy function
  needs to allocate memory for the new element and then fill it with the proper data. It should be avoid moving
  pointers around, it should copy the content to the new element so the new element is not tied to the previous
  element.

  The list can have many iterators, but only one mutable iterator at any given time. The difference between a normal
  iterator and a mutable iterator is the fact that with normal iterators only additions can be performed to the list
  without invalidating the iterator, while the mutable iterator allows any kind of change. Be aware that removing from
  the list, either by using remove or via the mutable iterator will invalidate all the normal iterators.

  Simple way to iterate over the list:

  ListIterator *i = ListIteratorGet(list);
  int r = 0;
  for (r = ListIteratorFirst(i); r == 0; r = ListIteratorNext(i))
  {
      MyData = ListIteratorData(i);
      ...
      Do something with the data.
      ...
  }
  */
typedef struct List List;
typedef struct ListMutableIterator ListMutableIterator;
typedef struct ListIterator ListIterator;
/**
  @brief Initialization of a linked list.
  @param compare Compare functions for the elements of the list. Same semantic as strcmp.
  @param copy Copies one element into a new element.
  @param destroy Destroys an element.
  @return A fully initialized list ready to be used or -1 in case of error.
  */
List *ListNew(int (*compare)(const void *, const void *), void (*copy)(const void *source, void **destination), void (*destroy)(void *));
/**
  @brief Destroy a linked list.
  @param list List to be destroyed. It can be a NULL pointer.
  @return 0 if destroyed, -1 otherwise.
  */
int ListDestroy(List **list);
/**
  @brief Performs a shallow copy of a linked list.
  A shallow copy is a copy that does not copy the elements but only the list structure.
  This is done by internal reference counting. If any of the lists is modified afterwards
  then a deep copy of the list is triggered and all the elements are copied.
  @param origin Original list to be copied.
  @param destination List to be copied to.
  @return 0 if copied, -1 otherwise.
  @remark If no copy function is provided, then this function returns -1.
  */
int ListCopy(List *origin, List **destination);
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
int ListPrepend(List *list, void *payload);
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
int ListAppend(List *list, void *payload);
/**
  @brief Removes an element from the linked list.
  Removes the first element that matches the payload. It starts looking from the beginning of the list.
  Notice that this might trigger a deep copy of the list. This only happens if the list was
  copied before.
  @param list Linked list.
  @param payload Data to be removed.
  @return 0 if removed, -1 otherwise.
  */
int ListRemove(List *list, void *payload);
/**
  @brief Returns the number of elements on a given linked list.
  @param list Linked list.
  @return The number of elements on the list.
  */
int ListCount(const List *list);

/**
  @brief Gets an iterator for a given linked list.

  This iterator will be invalid if data is removed from the list. It will still be valid
  after a new addition though.
  @note After creation the iterator will be pointing to the first item of the list.
  @param list Linked list
  @param iterator Iterator.
  @return A fully initialized iterator or NULL in case of error.
  */
ListIterator *ListIteratorGet(const List *list);
/**
  @brief Releases the memory associated with an iterator.

  This function exists only to free the memory associated with the iterator, there is no strong
  connection between the iterator and the list.
  @note It is not possible to get an iterator for an empty list.
  @param iterator Iterator.
  @return 0 if released, -1 otherwise.
  */
int ListIteratorDestroy(ListIterator **iterator);
/**
  @brief Moves the iterator to the first element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int ListIteratorFirst(ListIterator *iterator);
/**
  @brief Moves the iterator to the last element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int ListIteratorLast(ListIterator *iterator);
/**
  @brief Moves the iterator to the next element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int ListIteratorNext(ListIterator *iterator);
/**
  @brief Moves the iterator to the previous element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int ListIteratorPrevious(ListIterator *iterator);
/**
  @brief Returns the data associated with the current element.
  @param iterator Iterator.
  @return Pointer to the data or NULL if it was not possible.
  */
void *ListIteratorData(const ListIterator *iterator);
/**
  @brief Checks if the iterator has a next element
  @return True if it has a next element or False if not.
  */
bool ListIteratorHasNext(const ListIterator *iterator);
/**
  @brief Checks if the iterator has a previous element
  @return True if it has a previous element or False if not.
  */
bool ListIteratorHasPrevious(const ListIterator *iterator);

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
ListMutableIterator *ListMutableIteratorGet(List *list);
/**
  @brief Releases the memory associated with an iterator.

  This function has to be called for mutable iterators, otherwise the list will think there is already one mutable
  iterator and the creation of new mutable iterators will not be possible.
  @note It is not possible to get an iterator for an empty list.
  @param iterator Iterator.
  @return 0 if released, -1 otherwise.
  */
int ListMutableIteratorRelease(ListMutableIterator **iterator);
/**
  @brief Moves the iterator to the first element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int ListMutableIteratorFirst(ListMutableIterator *iterator);
/**
  @brief Moves the iterator to the last element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int ListMutableIteratorLast(ListMutableIterator *iterator);
/**
  @brief Moves the iterator to the next element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int ListMutableIteratorNext(ListMutableIterator *iterator);
/**
  @brief Moves the iterator to the previous element of the list.
  @param iterator Iterator.
  @return 0 if it was possible to move, -1 otherwise.
  */
int ListMutableIteratorPrevious(ListMutableIterator *iterator);
/**
  @brief Returns the data associated with the current element.
  @param iterator Iterator.
  @return Pointer to the data or NULL if it was not possible.
  */
void *ListMutableIteratorData(const ListMutableIterator *iterator);
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
int ListMutableIteratorRemove(ListMutableIterator *iterator);
/**
  @brief Prepends element on front of the element pointed by the iterator.

  An important clarification, the iterator still points to the same element after this operation. It is up to the user
  to move the iterator to the prepended element.

  All the light operators are still valid after this operation.
  @param iterator Iterator
  @param payload Element to be prepended.
  @return 0 if prepended, -1 in case of error.
  */
int ListMutableIteratorPrepend(ListMutableIterator *iterator, void *payload);
/**
  @brief Appends element after the element pointed by the iterator.

  An important clarification, the iterator still points to the same element after this operation. It is up to the user
  to move the iterator to the appended element.

  All the light operators are still valid after this operation.
  @param iterator Iterator
  @param payload Element to be appended.
  @return 0 if appended, -1 in case of error.
  */
int ListMutableIteratorAppend(ListMutableIterator *iterator, void *payload);
/**
  @brief Checks if the iterator has a next element
  @return True if it has a next element or False if not.
  */
bool ListMutableIteratorHasNext(const ListMutableIterator *iterator);
/**
  @brief Checks if the iterator has a previous element
  @return True if it has a previous element or False if not.
  */
bool ListMutableIteratorHasPrevious(const ListMutableIterator *iterator);

#endif // CFENGINE_LIST_H
