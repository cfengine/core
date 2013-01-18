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

/*
 * Helper method to detach buffers.
 */
static void BufferDetach(Buffer *buffer)
{
    int shared = RefCountIsShared(buffer->ref_count);
    if (shared)
    {
        /*
         * We only detach from the main buffer and not copy.
         * We set the pointer to NULL and let the caller take care of the situation.
         */
        buffer->buffer = NULL;
        buffer->used = 0;
        buffer->capacity = 0;
        buffer->real_capacity = 0;
        // Ok, we have our own copy of the buffer. Now we detach.
        RefCountDetach(buffer->ref_count, buffer);
        buffer->ref_count = NULL;
        RefCountNew(&buffer->ref_count);
        RefCountAttach(buffer->ref_count, buffer);
    }
}

int BufferNew(Buffer **buffer)
{
    if (!buffer)
    {
        return -1;
    }
    *buffer = (Buffer *)malloc(sizeof(Buffer));
    (*buffer)->capacity = DEFAULT_BUFFER_SIZE;
    (*buffer)->low_water_mark = DEFAULT_LOW_WATERMARK;
    (*buffer)->buffer = (char *)malloc((*buffer)->capacity);
    if (!(*buffer))
    {
        return -1;
    }
    (*buffer)->real_capacity = (*buffer)->capacity - (*buffer)->low_water_mark;
    (*buffer)->mode = BUFFER_BEHAVIOR_CSTRING;
    (*buffer)->chunk_size = DEFAULT_CHUNK_SIZE;
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
    (*destination)->low_water_mark = source->low_water_mark;
    (*destination)->real_capacity = source->real_capacity;
    (*destination)->chunk_size = source->chunk_size;
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
    /*
     * The algorithm goes like this:
     * 1. Detach if shared.
     * 2. Check if we need more space and resize the buffer if needed
     * 3. Copy the data into the buffer
     * Point 1 is tricky, since we might have enough space on the buffer
     * but we move over the low_water_mark. In that case we do not allocate
     * more memory, but next time we add something we do it.
     * Notice that we delay the detaching until we know we have enough memory to
     * avoid losing data.
     * The memory allocation is not the most efficient, but it is consistent with
     * the model. The user should modify the allocation values if needed.
     */
    unsigned int pre_used = buffer->used;
    unsigned int pre_capacity = buffer->capacity;
    unsigned int pre_chunk_size = buffer->chunk_size;
    char *pre_data = buffer->buffer;
    RefCount *ref_count = buffer->ref_count;
    BufferDetach(buffer);
    char *p = NULL;
    unsigned int capacity = buffer->capacity;
    unsigned int chunk_size = buffer->chunk_size;
    int allocated = 0;
    while (length > capacity)
    {
        /*
         * This case is easy, we just need to add more memory.
         */
        if (p)
        {
            free (p);
        }
        p = (char *)malloc(capacity + chunk_size);
        if (!p)
        {
            /*
             * We couldn't allocate more memory, therefore we signal the error.
             * We reset everything to how it was before the detach.
             */
            RefCountDestroy(&buffer->ref_count);
            if (RefCountAttach(ref_count, buffer) < 0)
            {
                /*
                 * Too much, we cannot set things back.
                 * Should we abort? exit?
                 */
                return -1;
            }
            buffer->ref_count = ref_count;
            buffer->buffer = pre_data;
            buffer->capacity = pre_capacity;
            buffer->chunk_size = pre_chunk_size;
            buffer->used = pre_used;
            return -1;
        }
        capacity += chunk_size;
        allocated = 1;
    }
    if (allocated)
    {
        buffer->buffer = p;
        buffer->capacity = capacity;
        buffer->real_capacity = buffer->capacity - buffer->low_water_mark;
    }
    /*
     * We should have enough memory now, either because we allocated it or
     * because we already had enough.
     */
    buffer->used = 0;
    unsigned int i = 0;
    for (i = 0; i < length; ++i)
    {
        buffer->buffer[i] = bytes[i];
        if ((buffer->mode == BUFFER_BEHAVIOR_CSTRING) && (bytes[i] == '\0'))
        {
            break;
        }
        buffer->used++;
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
    /*
     * 1. Detach if shared.
     * 2. Check if we need more space and resize our internal buffer.
     * 3. Append the data at the end.
     */
    unsigned int pre_used = buffer->used;
    unsigned int pre_capacity = buffer->capacity;
    unsigned int pre_chunk_size = buffer->chunk_size;
    char *pre_data = buffer->buffer;
    RefCount *ref_count = buffer->ref_count;
    BufferDetach(buffer);
    char *p = NULL;
    int allocated = 0;
    unsigned int capacity = buffer->capacity;
    unsigned int chunk_size = buffer->chunk_size;
    while (length + pre_used > capacity)
    {
        /*
         * This case is easy, we just need to add more memory.
         */
        if (p)
        {
            free (p);
        }
        p = (char *)malloc(capacity + chunk_size);
        if (!p)
        {
            /*
             * We couldn't allocate more memory, therefore we signal the error.
             * We set everything back to normal.
             */
            RefCountDestroy(&buffer->ref_count);
            if (RefCountAttach(ref_count, buffer) < 0)
            {
                /*
                 * Too much, we cannot set things back.
                 * Should we abort? exit?
                 */
                return -1;
            }
            buffer->ref_count = ref_count;
            buffer->buffer = pre_data;
            buffer->capacity = pre_capacity;
            buffer->chunk_size = pre_chunk_size;
            buffer->used = pre_used;
            return -1;
        }
        capacity += chunk_size;
        allocated = 1;
    }
    unsigned int i = 0;
    if (allocated)
    {
        /*
         * Copy the old data.
         * Although we know that if somebody used the proper functions there will be no '\0'
         * embedded into the array, somebody could have modified the array without calling
         * the proper functions. Therefore we check if there are embedded '\0' and stop copying.
         */
        unsigned int used = 0;
        for (i = 0; i < buffer->used; ++i)
        {
            p[i] = buffer->buffer[i];
            if ((buffer->mode == BUFFER_BEHAVIOR_CSTRING) && (buffer->buffer[i] == '\0'))
            {
                /*
                 * This is an error, fix the variables and return -1.
                 * In theory we could have checked this before allocating memory
                 * but in most cases this check is supefluous.
                 */
                RefCountDestroy(&buffer->ref_count);
                if (RefCountAttach(ref_count, buffer) < 0)
                {
                    /*
                     * Too much, we cannot set things back.
                     * Should we abort? exit?
                     */
                    return -1;
                }
                buffer->ref_count = ref_count;
                buffer->buffer = pre_data;
                buffer->capacity = pre_capacity;
                buffer->chunk_size = pre_chunk_size;
                buffer->used = pre_used;
                free (p);
                return -1;
            }
            used++;
        }
        if (buffer->mode == BUFFER_BEHAVIOR_CSTRING)
        {
            p[used] = '\0';
        }
        buffer->buffer = p;
        buffer->used = used;
        buffer->capacity = capacity;
        buffer->real_capacity = buffer->capacity - buffer->low_water_mark;
    }
    /*
     * There is no else case for the previous if condition because there is nothing to do.
     * No need to copy the data since we are using the same buffer.
     * Our detach process does not copy the data since we most likely are going to throw it
     * away anyway. The detach process just sets the capacity to 0 and makes sure buffer is NULL.
     * Therefore when we detach we are going to allocate memory, this path will only be used
     * when we do not detach and when we already have enough memory.
     */
    unsigned int beginning = buffer->used;
    for (i = beginning; i < beginning + length; ++i)
    {
        buffer->buffer[i] = bytes[i - beginning];
        if ((buffer->mode == BUFFER_BEHAVIOR_CSTRING) && (bytes[i - beginning] == '\0'))
        {
            break;
        }
        buffer->used++;
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
    /*
     * Printf is different from both Set and Append. We don't know how big a buffer we need,
     * therefore we detach and start with a clean sheet and allocate more if needed.
     */
    unsigned int capacity = buffer->capacity;
    char *pre_data = buffer->buffer;
    unsigned int pre_used = buffer->used;
    RefCount *ref_count = buffer->ref_count;
    int printed = 0;
    BufferDetach(buffer);
    if (0 == buffer->capacity)
    {
        /*
         * The only way to find out if we were detached is by looking at the new capacity.
         * If we were shared, then no memory has been allocated and our capacity is 0.
         * Detach does not allocate memory, therefore we allocate the memory.
         */
        buffer->buffer = (char *)malloc(capacity);
        if (!buffer->buffer)
        {
            /*
             * Reset to previous state and signal the error.
             */
            RefCountDestroy(&buffer->ref_count);
            if (RefCountAttach(ref_count, buffer) < 0)
            {
                /*
                 * Too much, we cannot set things back.
                 * Should we abort? exit?
                 */
                va_end(ap);
                return -1;
            }
            buffer->ref_count = ref_count;
            buffer->buffer = pre_data;
            buffer->capacity = capacity;
            buffer->used = pre_used;
            va_end(ap);
            return -1;
        }
        buffer->capacity = capacity;
        buffer->real_capacity = buffer->capacity - buffer->low_water_mark;
    }
    buffer->used = 0;
    /*
     * We will rely on vsnprintf to do the job for us. We know how big a buffer we have and
     * we know that if vsnprintf returns more than that, then we need to do it again.
     * Meaning we will return -1 and the user will have to call us again. We will allocate more
     * memory for next try.
     * There is a clear reason why we do not retry ourselves: the va_list. It is not clear what
     * happens if we try to traverse the list again, therefore we avoid the problem by just
     * simply signaling the error.
     */
    printed = vsnprintf(buffer->buffer, buffer->capacity, format, ap);
    if (printed > buffer->capacity)
    {
        /*
         * Tough love, discard the buffer and allocate more memory.
         */
        printed = -1;
        free (buffer->buffer);
        buffer->buffer = (char *)malloc(buffer->capacity + buffer->chunk_size);
        if (!buffer->buffer)
        {
            /*
             * Reset to previous state and signal the error.
             */
            RefCountDestroy(&buffer->ref_count);
            if (RefCountAttach(ref_count, buffer) < 0)
            {
                /*
                 * Too much, we cannot set things back.
                 * Should we abort? exit?
                 */
                va_end(ap);
                return -1;
            }
            buffer->ref_count = ref_count;
            buffer->buffer = pre_data;
            buffer->capacity = capacity;
            buffer->used = pre_used;
            va_end(ap);
            return -1;
        }
        buffer->capacity += buffer->chunk_size;
        buffer->real_capacity = buffer->capacity - buffer->low_water_mark;
        buffer->used = 0;
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
     * 1. Detach if shared
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

/*
 * Advanced API below
 */
unsigned int BufferLowWaterMark(Buffer *buffer)
{
    if (!buffer)
    {
        return 0;
    }
    return buffer->low_water_mark;
}

void BufferSetLowWaterMark(Buffer *buffer, unsigned int low_water_mark)
{
    if (!buffer || (buffer->low_water_mark >= buffer->capacity))
    {
        return;
    }
    buffer->low_water_mark = low_water_mark;
    buffer->real_capacity = buffer->capacity - buffer->low_water_mark;
}

unsigned int BufferChunkSize(Buffer *buffer)
{
    if (!buffer)
    {
        return 0;
    }
    return buffer->chunk_size;
}

void BufferSetChunkSize(Buffer *buffer, unsigned int chunk_size)
{
    if (!buffer || !chunk_size)
    {
        return;
    }
    buffer->chunk_size = chunk_size;
}
