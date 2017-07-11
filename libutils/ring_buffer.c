/*
   Copyright 2017 Northern.tech AS

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/
#include <ring_buffer.h>

#include <alloc.h>
#include <misc_lib.h>

struct RingBuffer_
{
    void *(*copy)(const void *data);
    void (*destroy)(void *data);

    void **data;
    size_t capacity;
    size_t end;
    size_t len;
};

struct RingBufferIterator_
{
    const RingBuffer *buf;
    size_t num_read;
};

RingBuffer *RingBufferNew(size_t capacity, void *(*copy)(const void *), void (*destroy)(void *))
{
    assert(capacity > 0);

    RingBuffer *buf = xmalloc(sizeof(RingBuffer));

    buf->copy = copy;
    buf->destroy = destroy;

    buf->data = xcalloc(capacity, sizeof(void *));
    buf->capacity = MAX(capacity, 1);
    buf->len = 0;
    buf->end = 0;

    return buf;
}

void RingBufferAppend(RingBuffer *buf, void *item)
{
    if (buf->data[buf->end] && buf->destroy)
    {
        buf->destroy(buf->data[buf->end]);
    }

    buf->data[buf->end] = buf->copy ? buf->copy(item) : item;
    buf->end = (buf->end + 1) % buf->capacity;

    if (buf->len < buf->capacity)
    {
        buf->len++;
    }
}

void RingBufferClear(RingBuffer *buf)
{
    if (buf->destroy)
    {
        for (size_t i = 0; i < buf->capacity; i++)
        {
            if (buf->data[i])
            {
                buf->destroy(buf->data[i]);
                buf->data[i] = NULL;
            }
        }
    }

    buf->end = 0;
    buf->len = 0;
}

size_t RingBufferLength(const RingBuffer *buf)
{
    return buf->len;
}

bool RingBufferIsFull(const RingBuffer *buf)
{
    return buf->len == buf->capacity;
}

const void *RingBufferHead(const RingBuffer *buf)
{
    if (RingBufferLength(buf) == 0)
    {
        return NULL;
    }

    if (buf->end == 0)
    {
        return buf->data[buf->capacity - 1];
    }
    else
    {
        return buf->data[buf->end - 1];
    }
}

RingBufferIterator *RingBufferIteratorNew(const RingBuffer *buf)
{
    RingBufferIterator *iter = xmalloc(sizeof(RingBufferIterator));

    iter->buf = buf;
    iter->num_read = 0;

    return iter;
}

void RingBufferDestroy(RingBuffer *buf)
{
    if (buf)
    {
        RingBufferClear(buf);
        free(buf->data);
        free(buf);
    }
}

void RingBufferIteratorDestroy(RingBufferIterator *iter)
{
    if (iter)
    {
        free(iter);
    }
}

const void *RingBufferIteratorNext(RingBufferIterator *iter)
{
    if ((iter->buf->len - iter->num_read) == 0)
    {
        return NULL;
    }

    size_t offset = iter->num_read;
    if (RingBufferIsFull(iter->buf))
    {
        offset = (iter->buf->end + iter->num_read) % iter->buf->capacity;
    }

    const void *data = iter->buf->data[offset];
    iter->num_read++;

    return data;
}
