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


#include <sequence.h>
#include <alloc.h>
#include <string_lib.h>


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

void SeqSet(Seq *seq, size_t index, void *item)
{
    assert(index < SeqLength(seq));
    if (seq->ItemDestroy)
    {
        seq->ItemDestroy(seq->data[index]);
    }
    seq->data[index] = item;
}

void SeqAppend(Seq *seq, void *item)
{
    ExpandIfNeccessary(seq);

    seq->data[seq->length] = item;
    ++(seq->length);
}

void SeqAppendOnce(Seq *seq, void *item, SeqItemComparator Compare)
{
    if (SeqLookup(seq, item, Compare) == NULL)
    {
        SeqAppend(seq, item);
    }
    else
    {
        /* swallow the item anyway */
        if (seq->ItemDestroy != NULL)
        {
            seq->ItemDestroy(item);
        }
    }
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

void *SeqBinaryLookup(Seq *seq, const void *key, SeqItemComparator Compare)
{
    ssize_t index = SeqBinaryIndexOf(seq, key, Compare);
    if (index == -1)
    {
        return NULL;
    }
    else
    {
        return seq->data[index];
    }
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

ssize_t SeqBinaryIndexOf(Seq *seq, const void *key, SeqItemComparator Compare)
{
    if (seq->length == 0)
    {
        return -1;
    }

    size_t low = 0;
    size_t high = seq->length;

    while (low < high)
    {
        // Invariant: low <= middle < high
        size_t middle = low + ((high - low) >> 1); // ">> 1" is division by 2.
        int result = Compare(key, seq->data[middle], NULL);
        if (result == 0)
        {
            return middle;
        }
        if (result > 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }

    // Not found.
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

Seq *SeqSoftSort(const Seq *seq, SeqItemComparator compare, void *user_data)
{
    size_t length = SeqLength(seq);
    if (length == 0)
    {
        return SeqNew(0, NULL);
    }

    Seq *sorted_seq = SeqGetRange(seq, 0, length - 1);
    SeqSort(sorted_seq, compare, user_data);
    return sorted_seq;
}

void SeqSoftRemoveRange(Seq *seq, size_t start, size_t end)
{
    assert(seq);
    assert(end < seq->length);
    assert(start <= end);

    size_t rest_len = seq->length - end - 1;

    if (rest_len > 0)
    {
        memmove(seq->data + start, seq->data + end + 1, sizeof(void *) * rest_len);
    }

    seq->length -= end - start + 1;
}

void SeqClear(Seq *seq)
{
    if (SeqLength(seq) > 0)
    {
        SeqRemoveRange(seq, 0, SeqLength(seq) - 1);
    }
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
    if (SeqLength(seq) == 0)
    {
        return;
    }

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

Seq *SeqGetRange(const Seq *seq, size_t start, size_t end)
{
    assert (seq);

    if ((start > end) || (start >= seq->length) || (end >= seq->length))
    {
        return NULL;
    }

    Seq *sub = SeqNew(end - start + 1, seq->ItemDestroy);

    for (size_t i = start; i <= end; i++)
    {
        assert(i < SeqLength(seq));
        SeqAppend(sub, SeqAt(seq, i));
    }

    return sub;
}

void SeqStringAddSplit(Seq *seq, const char *str, char delimiter)
{
    if (str) // TODO: remove this inconsistency, add assert(str)
    {
        const char *prev = str;
        const char *cur = str;

        while (*cur != '\0')
        {
            if (*cur == delimiter)
            {
                size_t len = cur - prev;
                if (len > 0)
                {
                    SeqAppend(seq, xstrndup(prev, len));
                }
                else
                {
                    SeqAppend(seq, xstrdup(""));
                }
                prev = cur + 1;
            }

            cur++;
        }

        if (cur > prev)
        {
            SeqAppend(seq, xstrndup(prev, cur - prev));
        }
    }
}

Seq *SeqStringFromString(const char *str, char delimiter)
{
    Seq *seq = SeqNew(10, &free);

    SeqStringAddSplit(seq, str, delimiter);

    return seq;
}

int SeqStringLength(Seq *seq)
{
    assert (seq);

    int total_length = 0;
    size_t seq_length = SeqLength(seq);
    for (size_t i = 0; i < seq_length; i++)
    {
        total_length += SafeStringLength(SeqAt(seq, i));
    }

    return total_length;
}

void SeqRemoveNulls(Seq *s)
{
    int length = SeqLength(s);
    int from = 0;
    int to = 0;
    while (from < length)
    {
        if (s->data[from] == NULL)
        {
            ++from; // Skip NULL elements
        }
        else
        {
            // Copy elements in place, DON'T use SeqSet, which will free()
            s->data[to] = s->data[from];
            ++from;
            ++to;
        }
    }
    s->length = to;
}

Seq *SeqFromArgv(int argc, const char *const *const argv)
{
    assert(argc > 0);
    assert(argv != NULL);
    assert(argv[0] != NULL);

    Seq *args = SeqNew(argc, NULL);
    for (int i = 0; i < argc; ++i)
    {
        SeqAppend(args, (void *)argv[i]); // Discards const
    }
    return args;
}
