#include "sequence.h"

#include "alloc.h"

#include <stdlib.h>
#include <assert.h>


static const size_t EXPAND_FACTOR = 2;


Sequence *SequenceCreate(size_t initialCapacity, void (ItemDestroy)(void *item))
{
Sequence *seq = xmalloc(sizeof(Sequence));

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


static void DestroyRange(Sequence *seq, size_t start, size_t end)
{
if (seq->ItemDestroy)
   {
   for (size_t i = start; i <= end; i++)
      {
      seq->ItemDestroy(seq->data[i]);
      }
   }
}


void SequenceDestroy(Sequence *seq)
{
assert(seq && "Attempted to destroy a null sequence");

if (seq->length > 0)
   {
   DestroyRange(seq, 0, seq->length - 1);
   }

free(seq->data);
free(seq);
}


static void ExpandIfNeccessary(Sequence *seq)
{
assert(seq->length <= seq->capacity);

if (seq->length == seq->capacity)
   {
   seq->capacity *= EXPAND_FACTOR;
   seq->data = xrealloc(seq->data, sizeof(void *) * seq->capacity);
   }
}


void SequenceAppend(Sequence *seq, void *item)
{
ExpandIfNeccessary(seq);

seq->data[seq->length] = item;
++(seq->length);
}


void SequenceRemoveRange(Sequence *seq, size_t start, size_t end)
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


void *SequenceLookup(Sequence *seq, const void *key, SequenceItemComparator Compare)
{
for (size_t i = 0; i < seq->length; i++)
   {
   if (Compare(key, seq->data[i]) == 0)
      {
      return seq->data[i];
      }
   }

return NULL;
}

static void Swap(void **l, void **r)
{
void *t = *l;
*l = *r;
*r = t;
}

// adopted from http://rosettacode.org/wiki/Sorting_algorithms/Quicksort#C
static void QuickSortRecursive(void **data, int n, SequenceItemComparator Compare, size_t maxterm)
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
   while (Compare(*l, pivot) < 0)
      {
      ++l;
      }
   while (Compare(*r, pivot) > 0)
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

QuickSortRecursive(data, r - data + 1, Compare, maxterm + 1);
QuickSortRecursive(l, data + n - l, Compare, maxterm + 1);
}


void SequenceSort(Sequence *seq, SequenceItemComparator Compare)
{
QuickSortRecursive(seq->data, seq->length, Compare, 0);
}
