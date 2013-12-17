#ifndef CFENGINE_RING_BUFFER_H
#define CFENGINE_RING_BUFFER_H

#include <platform.h>

typedef struct RingBuffer_ RingBuffer;
typedef struct RingBufferIterator_ RingBufferIterator;

RingBuffer *RingBufferNew(size_t capacity, void *(*copy)(const void *), void (*destroy)(void *));
void RingBufferDestroy(RingBuffer *buf);

void RingBufferAppend(RingBuffer *buf, void *item);
void RingBufferClear(RingBuffer *buf);

size_t RingBufferLength(const RingBuffer *buf);
bool RingBufferIsFull(const RingBuffer *buf);
const void *RingBufferHead(const RingBuffer *buf);
const void *RingBufferTail(const RingBuffer *buf);

RingBufferIterator *RingBufferIteratorNew(const RingBuffer *buf);
void RingBufferIteratorDestroy(RingBufferIterator *iter);
const void *RingBufferIteratorNext(RingBufferIterator *iter);


#endif
