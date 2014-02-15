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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <alloc.h>
#include <buffer.h>
#include <refcount.h>
#include <misc_lib.h>

Buffer *BufferNewWithCapacity(unsigned int initial_capacity)
{
    Buffer *buffer = (Buffer *)xmalloc(sizeof(Buffer));

    buffer->capacity = initial_capacity;
    buffer->buffer = (char *)xmalloc(buffer->capacity);
    buffer->buffer[0] = '\0';
    buffer->mode = BUFFER_BEHAVIOR_CSTRING;
    buffer->used = 0;

    return buffer;
}

Buffer *BufferNew(void)
{
    return BufferNewWithCapacity(DEFAULT_BUFFER_CAPACITY);
}

Buffer* BufferNewFrom(const char *data, unsigned int length)
{
    Buffer *buffer = (Buffer *)xmalloc(sizeof(Buffer));
    buffer->capacity = DEFAULT_BUFFER_CAPACITY;
    buffer->buffer = (char *)xmalloc(buffer->capacity);
    /*
     * Check if we have enough space, otherwise create a larger buffer
     */
    if (length >= buffer->capacity)
    {
        unsigned int required_blocks = (length / DEFAULT_BUFFER_CAPACITY) + 1;
        buffer->buffer = (char *)xrealloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_CAPACITY);
        buffer->capacity = required_blocks * DEFAULT_BUFFER_CAPACITY;
        buffer->used = 0;
    }
    buffer->mode = BUFFER_BEHAVIOR_CSTRING;
    buffer->used = 0;

    /*
     * We have a buffer that is large enough, copy the data.
     */
    unsigned int total = 0;
    for (unsigned int c = 0; c < length; ++c)
    {
        buffer->buffer[c] = data[c];
        if ((data[c] == '\0') && (buffer->mode == BUFFER_BEHAVIOR_CSTRING))
        {
            break;
        }
        ++total;
    }
    buffer->used = total;
    if (buffer->mode == BUFFER_BEHAVIOR_CSTRING)
    {
        buffer->buffer[buffer->used] = '\0';
    }
    return buffer;
}

void BufferDestroy(Buffer *buffer)
{
    if (buffer)
    {
        free(buffer->buffer);
        free(buffer);
    }
}

char *BufferClose(Buffer *buffer)
{
    char *detached = buffer->buffer;
    free(buffer);

    return detached;
}

Buffer *BufferCopy(const Buffer *source)
{
    return BufferNewFrom(source->buffer, source->used);
}

int BufferCompare(const Buffer *buffer1, const Buffer *buffer2)
{
    assert(buffer1);
    assert(buffer2);

    /*
     * Rules for comparison:
     * 2. Check the content
     * 2.1. If modes are different, check until the first '\0'
     * 2.2. If sizes are different, check until the first buffer ends.
     */
    if (buffer1->mode == buffer2->mode)
    {
        if (buffer1->mode == BUFFER_BEHAVIOR_CSTRING)
        {
            /*
             * C String comparison
             */
            return strcmp(buffer1->buffer, buffer2->buffer);
        }
        else
        {
            /*
             * BUFFER_BEHAVIOR_BYTEARRAY
             * Byte by byte comparison
             */
            unsigned int i = 0;
            if (buffer1->used < buffer2->used)
            {
                for (i = 0; i < buffer1->used; ++i)
                {
                    if (buffer1->buffer[i] < buffer2->buffer[i])
                    {
                        return -1;
                    }
                    else if (buffer1->buffer[i] > buffer2->buffer[i])
                    {
                        return 1;
                    }
                }
                return -1;
            }
            else if (buffer1->used == buffer2->used)
            {
                for (i = 0; i < buffer1->used; ++i)
                {
                    if (buffer1->buffer[i] < buffer2->buffer[i])
                    {
                        return -1;
                    }
                    else if (buffer1->buffer[i] > buffer2->buffer[i])
                    {
                        return 1;
                    }
                }
            }
            else
            {
                for (i = 0; i < buffer2->used; ++i)
                {
                    if (buffer1->buffer[i] < buffer2->buffer[i])
                    {
                        return -1;
                    }
                    else if (buffer1->buffer[i] > buffer2->buffer[i])
                    {
                        return 1;
                    }
                }
                return 1;
            }
        }
    }
    else
    {
        /*
         * Mixed comparison
         * Notice that every BYTEARRAY was born as a CSTRING.
         * When we switch back to CSTRING we adjust the length to
         * match the first '\0'.
         */
        unsigned int i = 0;
        if (buffer1->used < buffer2->used)
        {
            for (i = 0; i < buffer1->used; ++i)
            {
                if (buffer1->buffer[i] < buffer2->buffer[i])
                {
                    return -1;
                }
                else if (buffer1->buffer[i] > buffer2->buffer[i])
                {
                    return 1;
                }
            }
            return -1;
        }
        else if (buffer1->used == buffer2->used)
        {
            for (i = 0; i < buffer1->used; ++i)
            {
                if (buffer1->buffer[i] < buffer2->buffer[i])
                {
                    return -1;
                }
                else if (buffer1->buffer[i] > buffer2->buffer[i])
                {
                    return 1;
                }
            }
        }
        else
        {
            for (i = 0; i < buffer2->used; ++i)
            {
                if (buffer1->buffer[i] < buffer2->buffer[i])
                {
                    return -1;
                }
                else if (buffer1->buffer[i] > buffer2->buffer[i])
                {
                    return 1;
                }
            }
            return 1;
        }
    }
    /*
     * We did all we could and the buffers seems to be equal.
     */
    return 0;
}

void BufferSet(Buffer *buffer, char *bytes, unsigned int length)
{
    assert(buffer);
    assert(bytes);

    /*
     * Check if we have enough space, otherwise create a larger buffer
     */
    if (length >= buffer->capacity)
    {
        unsigned int required_blocks = (length / DEFAULT_BUFFER_CAPACITY) + 1;
        buffer->buffer = (char *)xrealloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_CAPACITY);
        buffer->capacity = required_blocks * DEFAULT_BUFFER_CAPACITY;
        buffer->used = 0;
    }
    /*
     * We have a buffer that is large enough, copy the data.
     */
    unsigned int c = 0;
    unsigned int total = 0;
    for (c = 0; c < length; ++c)
    {
        buffer->buffer[c] = bytes[c];
        if ((bytes[c] == '\0') && (buffer->mode = BUFFER_BEHAVIOR_CSTRING))
        {
            break;
        }
        ++total;
    }
    buffer->used = total;
    if (buffer->mode == BUFFER_BEHAVIOR_CSTRING)
    {
        buffer->buffer[buffer->used] = '\0';
    }
}

char *BufferGet(Buffer *buffer)
{
    assert(buffer);
    buffer->unsafe = true;
    return buffer->buffer;
}

void BufferAppend(Buffer *buffer, const char *bytes, unsigned int length)
{
    /*
     * Check if we have enough space, otherwise create a larger buffer
     */
    if (buffer->used + length >= buffer->capacity)
    {
        unsigned int required_blocks = ((buffer->used + length)/ DEFAULT_BUFFER_CAPACITY) + 1;
        buffer->buffer = (char *)xrealloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_CAPACITY);
        buffer->capacity = required_blocks * DEFAULT_BUFFER_CAPACITY;
    }
    /*
     * We have a buffer that is large enough, copy the data.
     */
    unsigned int total = 0;
    for (unsigned int c = 0; c < length; ++c)
    {
        buffer->buffer[c + buffer->used] = bytes[c];
        if ((bytes[c] == '\0') && (buffer->mode = BUFFER_BEHAVIOR_CSTRING))
        {
            break;
        }
        ++total;
    }
    buffer->used += total;
    if (buffer->mode == BUFFER_BEHAVIOR_CSTRING)
    {
        buffer->buffer[buffer->used] = '\0';
    }
}

void BufferAppendChar(Buffer *buffer, char byte)
{
    // TODO: can probably be optimized
    BufferAppend(buffer, &byte, 1);
}

int BufferPrintf(Buffer *buffer, const char *format, ...)
{
    assert(buffer);
    assert(format);

    /*
     * We declare two lists, in case we need to reiterate over the list because the buffer was
     * too small.
     */
    va_list ap;
    va_list aq;
    va_start(ap, format);
    va_copy(aq, ap);

    /*
     * We don't know how big of a buffer we will need. It might be that we have enough space
     * or it might be that we don't have enough space. Unfortunately, we cannot reiterate over
     * a va_list, so our only solution is to tell the caller to retry the call. We signal this
     * by returning zero. Before doing that we increase the buffer to a suitable size.
     * The tricky part is the implicit sharing and the reference counting, if we are not shared then
     * everything is easy, however if we are shared then we need a different strategy.
     */
    int printed = vsnprintf(buffer->buffer, buffer->capacity, format, aq);
    if (printed >= buffer->capacity)
    {
        /*
         * Allocate a larger buffer and retry.
         * Now is when having a copy of the list pays off :-)
         */
        unsigned int required_blocks = (printed / DEFAULT_BUFFER_CAPACITY) + 1;
        buffer->buffer = (char *)xrealloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_CAPACITY);
        buffer->capacity = required_blocks * DEFAULT_BUFFER_CAPACITY;
        buffer->used = 0;
        printed = vsnprintf(buffer->buffer, buffer->capacity, format, ap);
        buffer->used = printed;
    }
    else
    {
        buffer->used = printed;
    }
    va_end(aq);
    va_end(ap);
    return printed;
}

int BufferVPrintf(Buffer *buffer, const char *format, va_list ap)
{
    va_list aq;
    va_copy(aq, ap);

    /*
     * We don't know how big of a buffer we will need. It might be that we have enough space
     * or it might be that we don't have enough space. Unfortunately, we cannot reiterate over
     * a va_list, so our only solution is to tell the caller to retry the call. We signal this
     * by returning zero. Before doing that we increase the buffer to a suitable size.
     * The tricky part is the implicit sharing and the reference counting, if we are not shared then
     * everything is easy, however if we are shared then we need a different strategy.
     */

    int printed = vsnprintf(buffer->buffer, buffer->capacity, format, aq);
    if (printed >= buffer->capacity)
    {
        unsigned int required_blocks = (printed / DEFAULT_BUFFER_CAPACITY) + 1;
        buffer->buffer = (char *)xrealloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_CAPACITY);
        buffer->capacity = required_blocks * DEFAULT_BUFFER_CAPACITY;
        buffer->used = 0;
        printed = vsnprintf(buffer->buffer, buffer->capacity, format, ap);
        buffer->used = printed;
    }
    else
    {
        buffer->used = printed;
    }
    return printed;
}

void BufferClear(Buffer *buffer)
{
    buffer->used = 0;
	buffer->buffer[0] = '\0';
}

unsigned int BufferSize(Buffer *buffer)
{
    return buffer->used;
}

const char *BufferData(Buffer *buffer)
{
    return buffer->buffer;
}

BufferBehavior BufferMode(Buffer *buffer)
{
    return buffer->mode;
}

void BufferSetMode(Buffer *buffer, BufferBehavior mode)
{
    assert(mode == BUFFER_BEHAVIOR_CSTRING || mode == BUFFER_BEHAVIOR_BYTEARRAY);

    /*
     * If we switch from BYTEARRAY mode to CSTRING then we need to adjust the
     * length to the first '\0'. This makes our life easier in the long run.
     */
    if (BUFFER_BEHAVIOR_CSTRING == mode)
    {
        for (unsigned int i = 0; i < buffer->used; ++i)
        {
            if (buffer->buffer[i] == '\0')
            {
                buffer->used = i;
                break;
            }
        }
    }
    buffer->mode = mode;
}
