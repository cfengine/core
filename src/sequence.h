/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_SEQUENCE_H
#define CFENGINE_SEQUENCE_H

#include "platform.h"

/**
  @brief Sequence data-structure.

  This is an array-list loosely modeled on GSequence. It is a managed array of void pointers and can be used to store
  arbitrary data. The array list will auto-expand by a factor of EXPAND_FACTOR (e.g. 2) when necessary, but not contract.
  Because sequence is content agnostic, it does not support the usual copy semantics found in other CFEngine structures,
  such as RList. Thus, appending an item to a Sequence may imply a transfer of ownership. Clients that require copy semantics
  should therefore make sure that elements are copied before they are appended. Some Sequence operations may remove some or
  all of the elements held. In order to do so safely, it's incumbent upon the client to supply the necessary item
  destructor to the Sequence constructor. If the item destructor argument is NULL, Sequence will not attempt to free
  the item memory held.
*/
typedef struct
{
    void **data;
    size_t length;
    size_t capacity;
    void (*ItemDestroy) (void *item);
} Sequence;

/**
  @brief Create a new Sequence
  @param [in] initial_capacity Size of initial buffer to allocate for item pointers.
  @param [in] ItemDestroy Optional item destructor to clean up memory when needed.
  @return A pointer to the created Sequence
  */
Sequence *SequenceCreate(size_t initial_capacity, void (*ItemDestroy) ());

/**
  @brief Destroy an existing Sequence
  @param [in] seq The Sequence to destroy.
  */
void SequenceDestroy(Sequence *seq);

/**
  @brief
  Function to compare two items in a Sequence.

  @retval -1 if the first argument is smaller than the second
  @retval 0 if the arguments are equal
  @retval 1 if the first argument is bigger than the second
  */
typedef int (*SequenceItemComparator) (const void *, const void *, void *user_data);

/**
  @brief Append a new item to the Sequence
  @param seq [in] The Sequence to append to.
  @param item [in] The item to append. Note that this item may be passed to the item destructor specified in the constructor.
  */
void SequenceAppend(Sequence *seq, void *item);

/**
  @brief Linearly searches through the sequence and return the first item considered equal to the specified key.
  @param seq [in] The Sequence to search.
  @param key [in] The item to compare against.
  @param compare [in] Comparator function to use. An item matches if the function returns 0.
  @returns A pointer to the found item, or NULL if not found.
  */
void *SequenceLookup(Sequence *seq, const void *key, SequenceItemComparator Compare);

/**
  @brief Linearly searches through the sequence and returns the index of the first matching object, or -1 if it doesn't exist.
  */
ssize_t SequenceIndexOf(Sequence *seq, const void *key, SequenceItemComparator Compare);

/**
  @brief Remove an inclusive range of items in the Sequence. A single item may be removed by specifiying start = end.
  @param seq [in] The Sequence to remove from.
  @param start [in] Index of the first element to remove
  @param end [in] Index of the last element to remove.
  */
void SequenceRemoveRange(Sequence *seq, size_t start, size_t end);

/**
  @brief Remove a single item in the sequence
  */
void SequenceRemove(Sequence *seq, size_t index);

/**
  @brief Sort a Sequence according to the given item comparator function
  @param compare [in] The comparator function used for sorting.
  @param user_data [in] Pointer passed to the comparator function
  */
void SequenceSort(Sequence *seq, SequenceItemComparator compare, void *user_data);

/**
  @brief Remove an inclusive range of item handles in the Sequence. A single item may be removed by specifiying start = end.
  @param seq [in] The Sequence to remove from.
  @param start [in] Index of the first element to remove
  @param end [in] Index of the last element to remove.
 */
void SequenceSoftRemoveRange(Sequence *seq, size_t start, size_t end);

/**
  @brief Remove a single item handle from the sequence
  */
void SequenceSoftRemove(Sequence *seq, size_t index);

/**
  @brief Reverses the order of the sequence
  */
void SequenceReverse(Sequence *seq);

#endif
