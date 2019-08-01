/*
   Copyright 2018 Northern.tech AS

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

#ifndef CFENGINE_SEQUENCE_H
#define CFENGINE_SEQUENCE_H

#include <platform.h>

/**
  @brief Sequence data-structure.

  This is an array-list loosely modeled on GSequence. It is a managed array of
  void pointers and can be used to store arbitrary data. The array list will
  auto-expand by a factor of EXPAND_FACTOR (e.g. 2) when necessary, but not
  contract.  Because sequence is content agnostic, it does not support the
  usual copy semantics found in other CFEngine structures, such as
  RList. Thus, appending an item to a Sequence may imply a transfer of
  ownership. Clients that require copy semantics should therefore make sure
  that elements are copied before they are appended. Some Sequence operations
  may remove some or all of the elements held. In order to do so safely, it's
  incumbent upon the client to supply the necessary item destructor to the
  Sequence constructor. If the item destructor argument is NULL, Sequence will
  not attempt to free the item memory held.
*/
typedef struct
{
    void **data;
    size_t length;
    size_t capacity;
    void (*ItemDestroy) (void *item);
} Seq;

static inline void *SeqAt(const Seq *seq, int i)
{
    return seq->data[i];
}

/**
  @brief Length of the sequence.
  @note On NULL sequence return size 0.
  @param seq [in] sequence.
  @return Sequence length.
  */
size_t SeqLength(const Seq *seq);

/**
  @brief Create a new Sequence
  @param [in] initial_capacity Size of initial buffer to allocate for item pointers.
  @param [in] ItemDestroy Optional item destructor to clean up memory when needed.
  @return A pointer to the created Sequence
  */
Seq *SeqNew(size_t initial_capacity, void (*ItemDestroy) ());

/**
  @brief Destroy an existing Sequence
  @param [in] seq The Sequence to destroy.
  */
void SeqDestroy(Seq *seq);

/**
  @brief Destroy an existing Sequence without destroying its items.
  @param [in] seq The Sequence to destroy.
  */
void SeqSoftDestroy(Seq *seq);

/**
  @brief
  Function to compare two items in a Sequence.

  @retval -1 if the first argument is smaller than the second
  @retval 0 if the arguments are equal
  @retval 1 if the first argument is bigger than the second
  */
typedef int (*SeqItemComparator) (const void *, const void *, void *user_data);

void SeqSet(Seq *set, size_t index, void *item);

/**
  @brief Append a new item to the Sequence
  @param seq [in] The Sequence to append to.
  @param item [in] The item to append. Note that this item may be passed to the item destructor specified in the constructor.
  */
void SeqAppend(Seq *seq, void *item);

/**
  @brief Append a new item to the Sequence if it's not already present in the Sequence.
  @note  This calls SeqLookup() and thus linearly searches through the sequence.
  @param seq [in] The Sequence to append to.
  @param item [in] The item to append. Note that this item will be passed to the item destructor specified in the constructor.
                   Either immediately if the same item (according to Compare()) is found in the Sequence or once the Sequence
                   is destroyed with SeqDestroy().
  */
void SeqAppendOnce(Seq *seq, void *item, SeqItemComparator Compare);

/**
 * @brief Append a sequence to this sequence. Only copies pointers.
 * @param seq Sequence to append to
 * @param items Sequence to copy pointers from.
 */
void SeqAppendSeq(Seq *seq, const Seq *items);

/**
  @brief Linearly searches through the sequence and return the first item considered equal to the specified key.
  @param seq [in] The Sequence to search.
  @param key [in] The item to compare against.
  @param compare [in] Comparator function to use. An item matches if the function returns 0.
  @returns A pointer to the found item, or NULL if not found.
  */
void *SeqLookup(Seq *seq, const void *key, SeqItemComparator Compare);

/**
 * @brief Performs a binary search looking for the item matching the given key.
 *        It is the programmer's responsibility to make sure that the sequence is already sorted.
 * @param seq [in] The Sequence to search.
 * @param key [in] The item to compare against.
 * @param compare [in] Comparator function to use (return value has strcmp semantics).
 * @returns A pointer to the found item, or NULL if not found.
 */
void *SeqBinaryLookup(Seq *seq, const void *key, SeqItemComparator Compare);

/**
  @brief Linearly searches through the sequence and returns the index of the first matching object, or -1 if it doesn't exist.
  */
ssize_t SeqIndexOf(Seq *seq, const void *key, SeqItemComparator Compare);

/**
 * @brief Performs a binary search looking for the item matching the given key.
 *        It is the programmer's responsibility to make sure that the sequence is already sorted.
 * @param seq [in] The Sequence to search.
 * @param key [in] The item to compare against.
 * @param compare [in] Comparator function to use (return value has strcmp semantics).
 * @returns The index of the item, or -1 if it is not found.
 */
ssize_t SeqBinaryIndexOf(Seq *seq, const void *key, SeqItemComparator Compare);

/**
  @brief Remove an inclusive range of items in the Sequence. A single item may be removed by specifying start = end.
  @param seq [in] The Sequence to remove from.
  @param start [in] Index of the first element to remove
  @param end [in] Index of the last element to remove.
  */
void SeqRemoveRange(Seq *seq, size_t start, size_t end);

/**
  @brief Remove a single item in the sequence
  */
void SeqRemove(Seq *seq, size_t index);

/**
  @brief Sort a Sequence according to the given item comparator function
  @param compare [in] The comparator function used for sorting.
  @param user_data [in] Pointer passed to the comparator function
  */
void SeqSort(Seq *seq, SeqItemComparator compare, void *user_data);

/**
  @brief Returns a soft copy of the sequence sorted according to the given item comparator function.
  @param compare [in] The comparator function used for sorting.
  @param user_data [in] Pointer passed to the comparator function
  */
Seq *SeqSoftSort(const Seq *seq, SeqItemComparator compare, void *user_data);

/**
  @brief Remove an inclusive range of item handles in the Sequence. A single item may be removed by specifying start = end.
  @param seq [in] The Sequence to remove from.
  @param start [in] Index of the first element to remove
  @param end [in] Index of the last element to remove.
 */
void SeqSoftRemoveRange(Seq *seq, size_t start, size_t end);

/**
  @brief Remove a single item handle from the sequence
  */
void SeqSoftRemove(Seq *seq, size_t index);

/**
  @brief Reverses the order of the sequence
  */
void SeqReverse(Seq *seq);

/**
 * @brief Shuffle the sequence by randomly switching positions of the pointers
 * @param seq
 * @param seed Seed value for the PRNG
 */
void SeqShuffle(Seq *seq, unsigned int seed);

/**
 * @brief Remove all elements in sequence
 * @param seq
 */
void SeqClear(Seq *seq);

/**
  @brief Get soft copy of sequence according to specified range
  @param [in] seq Sequence select from
  @param [in] start Start index of sub sequence.
  @param [in] end End index which will be included into.
  @return A pointer to sub sequence, NULL on error.
  */
Seq *SeqGetRange(const Seq *seq, size_t start, size_t end);

/**
  @brief Create a new Sequence from splitting a string on a fixed delimiter
  @param [in] str String to split.
  @param [in] delimiter The delimiter, a fixed string.
  @return A pointer to the always created Sequence
  */
Seq *SeqStringFromString(const char *str, char delimiter);


/**
 * @brief Return the total string length of a sequence of strings
 */
int SeqStringLength(Seq *seq);

void SeqRemoveNulls(Seq *s);

/**
 * @brief Convert the argc + argv in main function to a sequence of strings
 *
 * Does not copy, edit or free the elements in argv.
 * Shallow copy. SeqDestroy is safe, it will not free the contents of argv.
 *
 */
Seq *SeqFromArgv(int argc, const char *const *argv);

#endif
