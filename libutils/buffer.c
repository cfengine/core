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

Buffer *BufferNew(void)
{
    Buffer *buffer = (Buffer *)xmalloc(sizeof(Buffer));

    buffer->capacity = DEFAULT_BUFFER_SIZE;
    buffer->buffer = (char *)xmalloc(buffer->capacity);
    buffer->buffer[0] = '\0';
    buffer->mode = BUFFER_BEHAVIOR_CSTRING;
    buffer->used = 0;
    RefCountNew(&(buffer->ref_count));
    RefCountAttach(buffer->ref_count, buffer);

    return buffer;
}

Buffer* BufferNewFrom(const char *data, unsigned int length)
{
    Buffer *buffer = (Buffer *)xmalloc(sizeof(Buffer));
    buffer->capacity = DEFAULT_BUFFER_SIZE;
    buffer->buffer = (char *)xmalloc(buffer->capacity);
    /*
     * Check if we have enough space, otherwise create a larger buffer
     */
    if (length >= buffer->capacity)
    {
        unsigned int required_blocks = (length / DEFAULT_BUFFER_SIZE) + 1;
        buffer->buffer = (char *)xrealloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_SIZE);
        buffer->capacity = required_blocks * DEFAULT_BUFFER_SIZE;
        buffer->used = 0;
    }
    buffer->mode = BUFFER_BEHAVIOR_CSTRING;
    buffer->used = 0;
    RefCountNew(&(buffer->ref_count));
    RefCountAttach(buffer->ref_count, buffer);
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
        /*
         * Here is how it goes, if we are shared then we cannot destroy the buffer
         * we simply detach from it. If we are not shared we need to destroy the buffer
         * and the RefCount.
         */
        if (RefCountIsShared(buffer->ref_count))
        {
            RefCountDetach(buffer->ref_count, buffer);
        }
        else
        {
            free(buffer->buffer);
            RefCountDestroy(&(buffer->ref_count));
        }

        free(buffer);
    }
}

Buffer *BufferCopy(const Buffer *source)
{
    Buffer *copy = xmalloc(sizeof(Buffer));

    copy->capacity = source->capacity;
    copy->mode = source->mode;
    copy->used = source->used;
    RefCountAttach(source->ref_count, copy);
    copy->buffer = source->buffer;
    copy->ref_count = source->ref_count;

    return copy;
}

int BufferCompare(const Buffer *buffer1, const Buffer *buffer2)
{
    assert(buffer1);
    assert(buffer2);

    /*
     * Rules for comparison:
     * 1. First check the refcount elements, if they are the same
     * then the elements are the same.
     * 2. Check the content
     * 2.1. If modes are different, check until the first '\0'
     * 2.2. If sizes are different, check until the first buffer ends.
     */
    if (RefCountIsEqual(buffer1->ref_count, buffer2->ref_count))
    {
        return 0;
    }
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

    if (RefCountIsShared(buffer->ref_count))
    {
        char *new_buffer = NULL;
        new_buffer = (char *)xmalloc(buffer->capacity);
        RefCount *ref_count = buffer->ref_count;
        buffer->ref_count = NULL;
        RefCountNew(&buffer->ref_count);
        RefCountAttach(buffer->ref_count, buffer);
        RefCountDetach(ref_count, buffer);
        /*
         * Ok, now we need to take care of the buffer.
         * We copy the data so we have the same data in case of error.
         */
        unsigned int i = 0;
        unsigned int used = 0;
        for (i = 0; i < buffer->used; ++i)
        {
            new_buffer[i] = buffer->buffer[i];
            if ((buffer->buffer[i] == '\0') && (buffer->mode == BUFFER_BEHAVIOR_CSTRING))
            {
                break;
            }
            ++used;
        }
        buffer->buffer = new_buffer;
        buffer->used = used;
    }
    /*
     * Check if we have enough space, otherwise create a larger buffer
     */
    if (length >= buffer->capacity)
    {
        unsigned int required_blocks = (length / DEFAULT_BUFFER_SIZE) + 1;
        buffer->buffer = (char *)xrealloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_SIZE);
        buffer->capacity = required_blocks * DEFAULT_BUFFER_SIZE;
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

void BufferAppend(Buffer *buffer, const char *bytes, unsigned int length)
{
    if (RefCountIsShared(buffer->ref_count))
    {
        char *new_buffer = NULL;
        new_buffer = (char *)xmalloc(buffer->capacity);
        RefCount *ref_count = buffer->ref_count;
        buffer->ref_count = NULL;
        RefCountNew(&buffer->ref_count);
        RefCountAttach(buffer->ref_count, buffer);
        RefCountDetach(ref_count, buffer);
        /*
         * Ok, now we need to take care of the buffer.
         */
        unsigned int used = 0;
        for (unsigned int i = 0; i < buffer->used; ++i)
        {
            new_buffer[i] = buffer->buffer[i];
            if ((buffer->buffer[i] == '\0') && (buffer->mode == BUFFER_BEHAVIOR_CSTRING))
            {
                break;
            }
            ++used;
        }
        buffer->buffer = new_buffer;
        buffer->used = used;
    }
    /*
     * Check if we have enough space, otherwise create a larger buffer
     */
    if (buffer->used + length >= buffer->capacity)
    {
        unsigned int required_blocks = ((buffer->used + length)/ DEFAULT_BUFFER_SIZE) + 1;
        buffer->buffer = (char *)xrealloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_SIZE);
        buffer->capacity = required_blocks * DEFAULT_BUFFER_SIZE;
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
    int printed = 0;
    /*
     * We don't know how big of a buffer we will need. It might be that we have enough space
     * or it might be that we don't have enough space. Unfortunately, we cannot reiterate over
     * a va_list, so our only solution is to tell the caller to retry the call. We signal this
     * by returning zero. Before doing that we increase the buffer to a suitable size.
     * The tricky part is the implicit sharing and the reference counting, if we are not shared then
     * everything is easy, however if we are shared then we need a different strategy.
     */
    if (RefCountIsShared(buffer->ref_count))
    {
        char *new_buffer = NULL;
        new_buffer = (char *)xmalloc(buffer->capacity);
        RefCount *ref_count = buffer->ref_count;
        buffer->ref_count = NULL;
        RefCountNew(&buffer->ref_count);
        RefCountAttach(buffer->ref_count, buffer);
        RefCountDetach(ref_count, buffer);
        /*
         * Ok, now we need to take care of the buffer.
         */
        unsigned int i = 0;
        unsigned int used = 0;
        for (i = 0; i < buffer->used; ++i)
        {
            new_buffer[i] = buffer->buffer[i];
            if ((buffer->buffer[i] == '\0') && (buffer->mode == BUFFER_BEHAVIOR_CSTRING))
            {
                break;
            }
            ++used;
        }
        buffer->buffer = new_buffer;
        buffer->used = used;
    }
    printed = vsnprintf(buffer->buffer, buffer->capacity, format, aq);
    if (printed >= buffer->capacity)
    {
        /*
         * Allocate a larger buffer and retry.
         * Now is when having a copy of the list pays off :-)
         */
        unsigned int required_blocks = (printed / DEFAULT_BUFFER_SIZE) + 1;
        buffer->buffer = (char *)xrealloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_SIZE);
        buffer->capacity = required_blocks * DEFAULT_BUFFER_SIZE;
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
    int printed = 0;
    /*
     * We don't know how big of a buffer we will need. It might be that we have enough space
     * or it might be that we don't have enough space. Unfortunately, we cannot reiterate over
     * a va_list, so our only solution is to tell the caller to retry the call. We signal this
     * by returning zero. Before doing that we increase the buffer to a suitable size.
     * The tricky part is the implicit sharing and the reference counting, if we are not shared then
     * everything is easy, however if we are shared then we need a different strategy.
     */
    if (RefCountIsShared(buffer->ref_count))
    {
        char *new_buffer = NULL;
        new_buffer = (char *)xmalloc(buffer->capacity);
        RefCount *ref_count = buffer->ref_count;
        buffer->ref_count = NULL;
        RefCountNew(&buffer->ref_count);
        RefCountAttach(buffer->ref_count, buffer);
        RefCountDetach(ref_count, buffer);
        /*
         * Ok, now we need to take care of the buffer.
         */
        unsigned int i = 0;
        unsigned int used = 0;
        for (i = 0; i < buffer->used; ++i)
        {
            new_buffer[i] = buffer->buffer[i];
            if ((buffer->buffer[i] == '\0') && (buffer->mode == BUFFER_BEHAVIOR_CSTRING))
            {
                break;
            }
            ++used;
        }
        buffer->buffer = new_buffer;
        buffer->used = used;
    }
    printed = vsnprintf(buffer->buffer, buffer->capacity, format, aq);
    if (printed >= buffer->capacity)
    {
        unsigned int required_blocks = (printed / DEFAULT_BUFFER_SIZE) + 1;
        buffer->buffer = (char *)xrealloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_SIZE);
        buffer->capacity = required_blocks * DEFAULT_BUFFER_SIZE;
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

void BufferZero(Buffer *buffer)
{
    /*
     * 1. Detach if shared, allocate a new buffer
     * 2. Mark used as zero.
     */
    if (RefCountIsShared(buffer->ref_count))
    {
        RefCountDetach(buffer->ref_count, buffer);
        buffer->buffer = (char *)xmalloc(buffer->capacity);
        RefCountNew(&buffer->ref_count);
        RefCountAttach(buffer->ref_count, buffer);
    }
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
