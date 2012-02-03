#include "sequence.h"

#include "alloc.h"

#include <assert.h>


static const size_t EXPAND_FACTOR = 2;


Sequence *SequenceCreate(size_t initialCapacity)
{
Sequence *seq = xmalloc(sizeof(Sequence));

if (initialCapacity <= 0)
   {
   initialCapacity = 1;
   }

seq->capacity = initialCapacity;
seq->length = 0;
seq->data = xcalloc(sizeof(void *), initialCapacity);

return seq;
}

void SequenceDestroy(Sequence **seq, void (ItemDestroy)(void *item))
{
Sequence *seqp = *seq;
assert(seqp && "Attempted to destroy a null sequence");

if (ItemDestroy)
   {
   for (size_t i = 0; i < seqp->length; i++)
      {
      ItemDestroy(seqp->data[i]);
      }
   }

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
