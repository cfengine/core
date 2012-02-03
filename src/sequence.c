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

void SequenceDestroy(Sequence **seq)
{
Sequence *seqp = *seq;
assert(seqp && "Attempted to destroy a null sequence");

DestroyRange(seqp, 0, seqp->length - 1);

free(seqp->data);
free(seqp);

*seq = NULL;
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

void SequenceSort(Sequence *seq, __compar_fn_t Compare)
{
qsort(seq->data, seq->length, sizeof(void *), Compare);
}
