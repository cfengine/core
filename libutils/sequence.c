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

#include "sequence.h"

#include "alloc.h"

#include <stdlib.h>
#include <assert.h>

static const size_t EXPAND_FACTOR = 2;

Seq *SeqNew(size_t initialCapacity, void (ItemDestroy) (void *item))
{
    Seq *seq = xmalloc(sizeof(Seq));

    if (initialCapacity <= 0)
    {
        initialCapacity = 1;
    }

    seq->capacity = initialCapacity;
    seq->length = 0;
    seq->data = xcalloc(sizeof(void *), initialCapacity);
    seq->ItemDestroy = ItemDestroy;

    return seq;
}

static void DestroyRange(Seq *seq, size_t start, size_t end)
{
    if (seq->ItemDestroy)
    {
        for (size_t i = start; i <= end; i++)
        {
            seq->ItemDestroy(seq->data[i]);
        }
    }
}

void SeqDestroy(Seq *seq)
{
    if (seq && seq->length > 0)
    {
        DestroyRange(seq, 0, seq->length - 1);
    }
    SeqSoftDestroy(seq);
}

void SeqSoftDestroy(Seq *seq)
{
    if (seq)
    {
        free(seq->data);
        free(seq);
    }
}

static void ExpandIfNeccessary(Seq *seq)
{
    assert(seq->length <= seq->capacity);

    if (seq->length == seq->capacity)
    {
        seq->capacity *= EXPAND_FACTOR;
        seq->data = xrealloc(seq->data, sizeof(void *) * seq->capacity);
    }
}

void SeqAppend(Seq *seq, void *item)
{
    ExpandIfNeccessary(seq);

    seq->data[seq->length] = item;
    ++(seq->length);
}

void SeqAppendSeq(Seq *seq, const Seq *items)
{
    for (size_t i = 0; i < SeqLength(items); i++)
    {
        SeqAppend(seq, SeqAt(items, i));
    }
}

void SeqRemoveRange(Seq *seq, size_t start, size_t end)
{
    assert(seq);
    assert(start >= 0);
    assert(end < seq->length);
    assert(start <= end);

    DestroyRange(seq, start, end);

    size_t rest_len = seq->length - end - 1;

    if (rest_len > 0)
    {
        memmove(seq->data + start, seq->data + end + 1, sizeof(void *) * rest_len);
    }

    seq->length -= end - start + 1;
}

void SeqRemove(Seq *seq, size_t index)
{
    SeqRemoveRange(seq, index, index);
}

void *SeqLookup(Seq *seq, const void *key, SeqItemComparator Compare)
{
    for (size_t i = 0; i < seq->length; i++)
    {
        if (Compare(key, seq->data[i], NULL) == 0)
        {
            return seq->data[i];
        }
    }

    return NULL;
}

ssize_t SeqIndexOf(Seq *seq, const void *key, SeqItemComparator Compare)
{
    for (size_t i = 0; i < seq->length; i++)
    {
        if (Compare(key, seq->data[i], NULL) == 0)
        {
            return i;
        }
    }

    return -1;
}

static void Swap(void **l, void **r)
{
    void *t = *l;

    *l = *r;
    *r = t;
}

// adopted from http://rosettacode.org/wiki/Sorting_algorithms/Quicksort#C
static void QuickSortRecursive(void **data, int n, SeqItemComparator Compare, void *user_data, size_t maxterm)
{
    assert(maxterm < 1000);

    if (n < 2)
    {
        return;
    }

    void *pivot = data[n / 2];
    void **l = data;
    void **r = data + n - 1;

    while (l <= r)
    {
        while (Compare(*l, pivot, user_data) < 0)
        {
            ++l;
        }
        while (Compare(*r, pivot, user_data) > 0)
        {
            --r;
        }
        if (l <= r)
        {
            Swap(l, r);
            ++l;
            --r;
        }
    }

    QuickSortRecursive(data, r - data + 1, Compare, user_data, maxterm + 1);
    QuickSortRecursive(l, data + n - l, Compare, user_data, maxterm + 1);
}

void SeqSort(Seq *seq, SeqItemComparator Compare, void *user_data)
{
    QuickSortRecursive(seq->data, seq->length, Compare, user_data, 0);
}

void SeqSoftRemoveRange(Seq *seq, size_t start, size_t end)
{
    assert(seq);
    assert(start >= 0);
    assert(end < seq->length);
    assert(start <= end);

    size_t rest_len = seq->length - end - 1;

    if (rest_len > 0)
    {
        memmove(seq->data + start, seq->data + end + 1, sizeof(void *) * rest_len);
    }

    seq->length -= end - start + 1;
}

void SeqSoftRemove(Seq *seq, size_t index)
{
    SeqSoftRemoveRange(seq, index, index);
}

void SeqReverse(Seq *seq)
{
    for (size_t i = 0; i < (seq->length / 2); i++)
    {
        Swap(&seq->data[i], &seq->data[seq->length - 1 - i]);
    }
}

size_t SeqLength(const Seq *seq)
{
    assert(seq);
    return seq->length;
}

void SeqShuffle(Seq *seq, unsigned int seed)
{
    /* Store current random number state for being reset at the end of function */
    int rand_state = rand();

    srand(seed);

    for (size_t i = SeqLength(seq) - 1; i > 0; i--)
    {
        size_t j = rand() % (i + 1);

        Swap(seq->data + i, seq->data + j);
    }

    /* Restore previous random number state */
    srand(rand_state);
}
