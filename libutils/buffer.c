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

#include "alloc.h"
#include "buffer.h"
#include "refcount.h"

static unsigned int general_memory_cap = DEFAULT_MEMORY_CAP;
unsigned int BufferGeneralMemoryCap()
{
    return general_memory_cap;
}

void BufferSetGeneralMemoryCap(unsigned int cap)
{
    /*
     * The cap cannot be set to zero, otherwise everything would fail.
     */
    if (cap == 0)
        return;
    general_memory_cap = cap;
}

Buffer *BufferNew(void)
{
    Buffer *buffer = (Buffer *)xmalloc(sizeof(Buffer));
    buffer->capacity = DEFAULT_BUFFER_SIZE;
    buffer->buffer = (char *)xmalloc(buffer->capacity);
    buffer->buffer[0] = '\0';
    buffer->mode = BUFFER_BEHAVIOR_CSTRING;
    buffer->used = 0;
    buffer->beginning = 0;
    buffer->end = 0;
    buffer->memory_cap = general_memory_cap;
    RefCountNew(&(buffer->ref_count));
    RefCountAttach(buffer->ref_count, buffer);
    return buffer;
}

Buffer* BufferNewFrom(const char *data, unsigned int length)
{
    /*
     * Are we going to go over the limit?
     */
    if (length > general_memory_cap)
    {
        return NULL;
    }
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
    buffer->beginning = 0;
    buffer->end = 0;
    buffer->memory_cap = general_memory_cap;
    RefCountNew(&(buffer->ref_count));
    RefCountAttach(buffer->ref_count, buffer);
    /*
     * We have a buffer that is large enough, copy the data.
     */
    unsigned int c = 0;
    unsigned int total = 0;
    for (c = 0; c < length; ++c)
    {
        buffer->buffer[c] = data[c];
        if ((data[c] == '\0') && (buffer->mode = BUFFER_BEHAVIOR_CSTRING))
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

int BufferDestroy(Buffer **buffer)
{
    // If already NULL don't bother.
    if (!buffer || !*buffer)
    {
        return 0;
    }
    /*
     * Here is how it goes, if we are shared then we cannot destroy the buffer
     * we simply detach from it. If we are not shared we need to destroy the buffer
     * and the RefCount.
     */
    if (RefCountIsShared((*buffer)->ref_count))
    {
        int result = 0;
        result = RefCountDetach((*buffer)->ref_count, *buffer);
        if (result < 0)
        {
            return -1;
        }
    }
    else
    {
        // We can destroy the buffer
        if ((*buffer)->buffer)
        {
            free ((*buffer)->buffer);
        }
        // Destroy the RefCount struct
        RefCountDestroy(&(*buffer)->ref_count);
    }
    free (*buffer);
    *buffer = NULL;
    return 0;
}

int BufferCopy(Buffer *source, Buffer **destination)
{
    // Basically we copy the link to the array and mark the attachment
    if (!source || !destination)
    {
        return -1;
    }
    *destination = (Buffer *)xmalloc(sizeof(Buffer));
    (*destination)->capacity = source->capacity;
    (*destination)->mode = source->mode;
    (*destination)->used = source->used;
    (*destination)->beginning = source->beginning;
    (*destination)->end = source->end;
    (*destination)->memory_cap = source->memory_cap;
    int elements = 0;
    elements = RefCountAttach(source->ref_count, (*destination));
    if (elements < 0)
    {
        return -1;
    }
    (*destination)->buffer = source->buffer;
    (*destination)->ref_count = source->ref_count;
    return 0;
}

int BufferCompare(Buffer *buffer1, Buffer *buffer2)
{
    /*
     * Quick safety check. If buffer1 is NULL, then it is immediately smaller than buffer2.
     * Same if buffer2 is NULL. If both are NULL, then they are equal.
     */
    if (!buffer1 && !buffer2)
    {
        return 0;
    }
    else if (!buffer1 && buffer2)
    {
        return -1;
    }
    else if (buffer1 && !buffer2)
    {
        return 1;
    }
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

int BufferSet(Buffer *buffer, char *bytes, unsigned int length)
{
    if (!buffer || !bytes)
    {
        return -1;
    }
    if (length > buffer->memory_cap)
    {
        return -1;
    }
    if (RefCountIsShared(buffer->ref_count))
    {
        char *new_buffer = NULL;
        new_buffer = (char *)xmalloc(buffer->capacity);
        /*
         * Make a local copy of the variables that are required to restore to normality.
         */
        RefCount *ref_count = buffer->ref_count;
        /*
         * We try to attach first, since it is more likely that Attach might fail than
         * detach.
         */
        int result = 0;
        buffer->ref_count = NULL;
        RefCountNew(&buffer->ref_count);
        result = RefCountAttach(buffer->ref_count, buffer);
        if (result < 0)
        {
            /*
             * Restore and signal the error.
             */
            free (new_buffer);
            RefCountDestroy(&buffer->ref_count);
            buffer->ref_count = ref_count;
            return -1;
        }
        /*
         * Detach. This operation might fail, although it is very rare.
         */
        result = RefCountDetach(ref_count, buffer);
        if (result < 0)
        {
            /*
             * The ref_count structure has not been modified, therefore
             * we can reuse it.
             * We need to destroy the other ref_count though.
             */
            free (new_buffer);
            RefCountDestroy(&buffer->ref_count);
            buffer->ref_count = ref_count;
            return -1;
        }
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
    return buffer->used;
}

int BufferAppend(Buffer *buffer, const char *bytes, unsigned int length)
{
    if (!buffer || !bytes)
    {
        return -1;
    }
    if (length + buffer->used > buffer->memory_cap)
    {
        return -1;
    }
    if (RefCountIsShared(buffer->ref_count))
    {
        char *new_buffer = NULL;
        new_buffer = (char *)xmalloc(buffer->capacity);
        /*
         * Make a local copy of the variables that are required to restore to normality.
         */
        RefCount *ref_count = buffer->ref_count;
        /*
         * We try to attach first, since it is more likely that Attach might fail than
         * detach.
         */
        int result = 0;
        buffer->ref_count = NULL;
        RefCountNew(&buffer->ref_count);
        result = RefCountAttach(buffer->ref_count, buffer);
        if (result < 0)
        {
            /*
             * Restore and signal the error.
             */
            free (new_buffer);
            RefCountDestroy(&buffer->ref_count);
            buffer->ref_count = ref_count;
            return -1;
        }
        /*
         * Detach. This operation might fail, although it is very rare.
         */
        result = RefCountDetach(ref_count, buffer);
        if (result < 0)
        {
            /*
             * The ref_count structure has not been modified, therefore
             * we can reuse it.
             * We need to destroy the other ref_count though.
             */
            free (new_buffer);
            RefCountDestroy(&buffer->ref_count);
            buffer->ref_count = ref_count;
            return -1;
        }
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
    unsigned int c = 0;
    unsigned int total = 0;
    for (c = 0; c < length; ++c)
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
    return buffer->used;
}

int BufferPrintf(Buffer *buffer, const char *format, ...)
{
    /*
     * We declare two lists, in case we need to reiterate over the list because the buffer was
     * too small.
     */
    va_list ap;
    va_list aq;
    va_start(ap, format);
    va_copy(aq, ap);
    if (!buffer || !format)
    {
        va_end(aq);
        va_end(ap);
        return -1;
    }
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
        /*
         * Make a local copy of the variables that are required to restore to normality.
         */
        RefCount *ref_count = buffer->ref_count;
        /*
         * We try to attach first, since it is more likely that Attach might fail than
         * detach.
         */
        int result = 0;
        buffer->ref_count = NULL;
        RefCountNew(&buffer->ref_count);
        result = RefCountAttach(buffer->ref_count, buffer);
        if (result < 0)
        {
            /*
             * Restore and signal the error.
             */
            free (new_buffer);
            RefCountDestroy(&buffer->ref_count);
            buffer->ref_count = ref_count;
            va_end(aq);
            va_end(ap);
            return -1;
        }
        /*
         * Detach. This operation might fail, although it is very rare.
         */
        result = RefCountDetach(ref_count, buffer);
        if (result < 0)
        {
            /*
             * The ref_count structure has not been modified, therefore
             * we can reuse it.
             * We need to destroy the other ref_count though.
             */
            free (new_buffer);
            RefCountDestroy(&buffer->ref_count);
            buffer->ref_count = ref_count;
            va_end(aq);
            va_end(ap);
            return -1;
        }
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
        if (printed > buffer->memory_cap)
        {
            /*
             * We would go over the memory_cap limit.
             */
            va_end(aq);
            va_end(ap);
            return -1;
        }
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
    if (!buffer || !format)
    {
        return -1;
    }
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
        /*
         * Make a local copy of the variables that are required to restore to normality.
         */
        RefCount *ref_count = buffer->ref_count;
        /*
         * We try to attach first, since it is more likely that Attach might fail than
         * detach.
         */
        int result = 0;
        buffer->ref_count = NULL;
        RefCountNew(&buffer->ref_count);
        result = RefCountAttach(buffer->ref_count, buffer);
        if (result < 0)
        {
            /*
             * Restore and signal the error.
             */
            free (new_buffer);
            RefCountDestroy(&buffer->ref_count);
            buffer->ref_count = ref_count;
            return -1;
        }
        /*
         * Detach. This operation might fail, although it is very rare.
         */
        result = RefCountDetach(ref_count, buffer);
        if (result < 0)
        {
            /*
             * The ref_count structure has not been modified, therefore
             * we can reuse it.
             * We need to destroy the other ref_count though.
             */
            free (new_buffer);
            RefCountDestroy(&buffer->ref_count);
            buffer->ref_count = ref_count;
            return -1;
        }
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
         * We use the copy of the list.
         */
        if (printed > buffer->memory_cap)
        {
            /*
             * We would go over the memory_cap limit.
             */
            return -1;
        }
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
    if (!buffer)
    {
        return;
    }
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
    if (!buffer)
    {
        return 0;
    }
    return buffer->used;
}

const char *BufferData(Buffer *buffer)
{
    if (!buffer)
    {
        return NULL;
    }
    return (const char *)buffer->buffer;
}

int BufferMode(Buffer *buffer)
{
    if (!buffer)
    {
        return -1;
    }
    return buffer->mode;
}

void BufferSetMode(Buffer *buffer, BufferBehavior mode)
{
    if (!buffer)
    {
        return;
    }
    if ((mode != BUFFER_BEHAVIOR_CSTRING) && (mode != BUFFER_BEHAVIOR_BYTEARRAY))
    {
        return;
    }
    /*
     * If we switch from BYTEARRAY mode to CSTRING then we need to adjust the
     * length to the first '\0'. This makes our life easier in the long run.
     */
    if (BUFFER_BEHAVIOR_CSTRING == mode)
    {
        unsigned int i = 0;
        for (i = 0; i < buffer->used; ++i)
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

unsigned int BufferMemoryCap(Buffer *buffer)
{
    if (!buffer)
    {
        return 0;
    }
    return buffer->memory_cap;
}

void BufferSetMemoryCap(Buffer *buffer, unsigned int cap)
{
    if (!buffer || !cap)
    {
        return;
    }
    buffer->memory_cap = cap;
}
