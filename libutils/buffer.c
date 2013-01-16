/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "buffer.h"
#include "refcount.h"

int BufferNew(Buffer **buffer)
{
    if (!buffer)
    {
        return -1;
    }
    *buffer = (Buffer *)malloc(sizeof(Buffer));
    if (!(*buffer))
    {
        return -1;
    }
    (*buffer)->capacity = DEFAULT_BUFFER_SIZE;
    (*buffer)->buffer = (char *)malloc((*buffer)->capacity);
    if (!(*buffer)->buffer)
    {
        free (*buffer);
        return -1;
    }
    (*buffer)->mode = BUFFER_BEHAVIOR_CSTRING;
    (*buffer)->used = 0;
    (*buffer)->beginning = 0;
    (*buffer)->end = 0;
    RefCountNew(&(*buffer)->ref_count);
    RefCountAttach((*buffer)->ref_count, (*buffer));
    return 0;
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
    *destination = (Buffer *)malloc(sizeof(Buffer));
    if (!(*destination))
    {
        return -1;
    }
    (*destination)->capacity = source->capacity;
    (*destination)->mode = source->mode;
    (*destination)->used = source->used;
    (*destination)->beginning = source->beginning;
    (*destination)->end = source->end;
    int elements = 0;
    elements = RefCountAttach(source->ref_count, (*destination));
    if (elements < 0)
    {
        return -1;
    }
    (*destination)->ref_count = source->ref_count;
    return 0;
}

int BufferEqual(Buffer *buffer1, Buffer *buffer2)
{
    /*
     * Rules for comparison:
     * 1. First check the refcount elements, if they are the same
     * then the elements are the same.
     * 2. Look at the mode, even if the content is the same the interpretation might be different.
     * 3. Look at the content. For CString mode we stop at the first '\0'.
     */
    if (RefCountIsEqual(buffer1->ref_count, buffer2->ref_count))
    {
        return 1;
    }
    if (buffer1->mode != buffer2->mode)
    {
        return 0;
    }
    int mode = buffer1->mode;
    if (buffer1->used == buffer2->used)
    {
        int i = 0;
        int equal = 1;
        for (i = 0; (i < buffer1->used); i++)
        {
            if (buffer1->buffer[i] != buffer2->buffer[i])
            {
                equal = 0;
                break;
            }
            if (('\0' == buffer1->buffer[i]) && (mode == BUFFER_BEHAVIOR_CSTRING))
            {
                break;
            }
        }
        return equal;
    }
    return 0;
}

int BufferSet(Buffer *buffer, char *bytes, unsigned int length)
{
    if (!buffer || !bytes)
    {
        return -1;
    }
    if (RefCountIsShared(buffer->ref_count))
    {
        char *new_buffer = NULL;
        new_buffer = (char *)malloc(buffer->capacity);
        if (new_buffer == NULL)
        {
            /*
             * Memory allocations do fail from time to time, even in Linux.
             * Luckily we have not modified a thing, so just return -1.
             */
            return -1;
        }
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
            if ((buffer->buffer[i] == '\0') && (buffer->mode == CString))
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
    if (length > buffer->capacity)
    {
        char *p = NULL;
        unsigned int required_blocks = (length / DEFAULT_BUFFER_SIZE) + 1;
        p = (char *)realloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_SIZE);
        if (p == NULL)
        {
            /*
             * Allocation failed. We have the data, although in a detached state.
             * Just return -1 and be done with it.
             */
            return -1;
        }
        buffer->capacity = required_blocks * DEFAULT_BUFFER_SIZE;
        buffer->buffer = p;
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
        if ((buffer->mode == BUFFER_BEHAVIOR_CSTRING) && (bytes[i] == '\0'))
        {
            break;
        }
        ++total;
    }
    if (buffer->mode == BUFFER_BEHAVIOR_CSTRING)
    {
        buffer->buffer[buffer->used] = '\0';
    }
    return buffer->used;
}

int BufferAppend(Buffer *buffer, char *bytes, unsigned int length)
{
    if (!buffer || !bytes)
    {
        return -1;
    }
    if (RefCountIsShared(buffer->ref_count))
    {
        char *new_buffer = NULL;
        new_buffer = (char *)malloc(buffer->capacity);
        if (new_buffer == NULL)
        {
            /*
             * Memory allocations do fail from time to time, even in Linux.
             * Luckily we have not modified a thing, so just return -1.
             */
            return -1;
        }
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
            p[i] = buffer->buffer[i];
            if ((buffer->mode == BUFFER_BEHAVIOR_CSTRING) && (buffer->buffer[i] == '\0'))
            {
                break;
            }
            ++used;
        }
        if (buffer->mode == BUFFER_BEHAVIOR_CSTRING)
        {
            /*
             * Allocation failed. We have the data, although in a detached state.
             * Just return -1 and be done with it.
             */
            return -1;
        }
        buffer->capacity = required_blocks * DEFAULT_BUFFER_SIZE;
        buffer->buffer = p;
    }
    /*
     * We have a buffer that is large enough, copy the data.
     */
    unsigned int c = 0;
    unsigned int total = 0;
    for (c = 0; c < length; ++c)
    {
        buffer->buffer[c] = bytes[c - beginning];
        if ((buffer->mode == BUFFER_BEHAVIOR_CSTRING) && (bytes[i - beginning] == '\0'))
        {
            break;
        }
        ++total;
    }
    if (buffer->mode == BUFFER_BEHAVIOR_CSTRING)
    {
        buffer->buffer[buffer->used] = '\0';
    }
    return buffer->used;
}

int BufferPrintf(Buffer *buffer, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    if (!buffer || !format)
    {
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
        new_buffer = (char *)malloc(buffer->capacity);
        if (new_buffer == NULL)
        {
            /*
             * Memory allocations do fail from time to time, even in Linux.
             * Luckily we have not modified a thing, so just return -1.
             */
            va_end(ap);
            return -1;
        }
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
            if ((buffer->buffer[i] == '\0') && (buffer->mode == CString))
            {
                break;
            }
            ++used;
        }
        buffer->buffer = new_buffer;
        buffer->used = used;
    }
    printed = vsnprintf(buffer->buffer, buffer->capacity, format, ap);
    if (printed > buffer->capacity)
    {
        /*
         * Allocate a larger buffer and retry.
         * Don't forget to signal by returning 0.
         */
        unsigned int required_blocks = (printed / DEFAULT_BUFFER_SIZE) + 1;
        void *p = NULL;
        p = realloc(buffer->buffer, required_blocks * DEFAULT_BUFFER_SIZE);
        if (p == NULL)
        {
            va_end(ap);
            return -1;
        }
        buffer->buffer = (char *)p;
        buffer->capacity = required_blocks * DEFAULT_BUFFER_SIZE;
        buffer->used = 0;
        printed = 0;
    }
    else
    {
        buffer->used = printed;
    }
    va_end(ap);
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
        buffer->buffer = (char *)malloc(buffer->capacity);
        RefCountNew(&buffer->ref_count);
        RefCountAttach(buffer->ref_count, buffer);
    }
    buffer->used = 0;
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
    buffer->mode = mode;
}
