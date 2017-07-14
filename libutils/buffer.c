/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
#include <pcre_wrap.h>
#include <string_lib.h>

Buffer *BufferNewWithCapacity(unsigned int initial_capacity)
{
    Buffer *buffer = xmalloc(sizeof(Buffer));

    buffer->capacity = initial_capacity;
    buffer->buffer = xmalloc(buffer->capacity);
    buffer->buffer[0] = '\0';
    buffer->mode = BUFFER_BEHAVIOR_CSTRING;
    buffer->used = 0;

    return buffer;
}

Buffer *BufferNew(void)
{
    return BufferNewWithCapacity(DEFAULT_BUFFER_CAPACITY);
}

static void ExpandIfNeeded(Buffer *buffer, unsigned int needed)
{
    if (needed >= buffer->capacity)
    {
        size_t new_capacity = UpperPowerOfTwo(needed + 1);
        buffer->buffer = xrealloc(buffer->buffer, new_capacity);
        buffer->capacity = new_capacity;
    }
}

Buffer* BufferNewFrom(const char *data, unsigned int length)
{
    Buffer *buffer = BufferNewWithCapacity(length + 1);
    BufferAppend(buffer, data, length);

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
    assert(source);
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

void BufferSet(Buffer *buffer, const char *bytes, unsigned int length)
{
    assert(buffer);
    assert(bytes);

    BufferClear(buffer);

    BufferAppend(buffer, bytes, length);
}

char *BufferGet(Buffer *buffer)
{
    assert(buffer);
    buffer->unsafe = true;
    return buffer->buffer;
}

void BufferAppendString(Buffer *buffer, const char *str)
{
    size_t len = strlen(str);
    ExpandIfNeeded(buffer, buffer->used + len + 1);
    memcpy(buffer->buffer + buffer->used, str, len);
    buffer->used += len;
    buffer->buffer[buffer->used] = '\0';
}

void BufferTrimToMaxLength(Buffer *buffer, unsigned int max)
{
    if (buffer->used > max)
    {
        buffer->used = max;
        // no need to call ExpandIfNeeded
        buffer->buffer[buffer->used] = '\0';
    }
}

void BufferAppend(Buffer *buffer, const char *bytes, unsigned int length)
{
    assert(buffer);
    assert(bytes);

    if (length == 0)
    {
        return;
    }

    switch (buffer->mode)
    {
    case BUFFER_BEHAVIOR_CSTRING:
        {
            size_t actual_length = strnlen(bytes, length);
            ExpandIfNeeded(buffer, buffer->used + actual_length + 1);
            memcpy(buffer->buffer + buffer->used, bytes, actual_length);
            buffer->used += actual_length;
            buffer->buffer[buffer->used] = '\0';
        }
        break;

    case BUFFER_BEHAVIOR_BYTEARRAY:
        ExpandIfNeeded(buffer, buffer->used + length);
        memcpy(buffer->buffer + buffer->used, bytes, length);
        buffer->used += length;
        break;
    }
}

void BufferAppendChar(Buffer *buffer, char byte)
{
    if (buffer->used < (buffer->capacity - 1))
    {
        buffer->buffer[buffer->used] = byte;
        buffer->used++;

        if (buffer->mode == BUFFER_BEHAVIOR_CSTRING)
        {
            buffer->buffer[buffer->used] = '\0';
        }
    }
    else
    {
        BufferAppend(buffer, &byte, 1);
    }
}

void BufferAppendF(Buffer *buffer, const char *format, ...)
{
    assert(buffer);
    assert(format);

    va_list ap;
    va_list aq;
    va_start(ap, format);
    va_copy(aq, ap);

    int printed = vsnprintf(buffer->buffer + buffer->used, buffer->capacity - buffer->used, format, aq);
    if (printed >= (buffer->capacity - buffer->used))
    {
        /*
         * Allocate a larger buffer and retry.
         * Now is when having a copy of the list pays off :-)
         */
        ExpandIfNeeded(buffer, buffer->used + printed);

        printed = vsnprintf(buffer->buffer + buffer->used, buffer->capacity - buffer->used, format, ap);
        buffer->used += printed;
    }
    else
    {
        buffer->used += printed;
    }
    va_end(aq);
    va_end(ap);
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
        ExpandIfNeeded(buffer, printed);

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

// NB! Make sure to sanitize format if taken from user input
int BufferVPrintf(Buffer *buffer, const char *format, va_list ap)
{
    assert(buffer);
    assert(format);
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
        ExpandIfNeeded(buffer, printed);
        buffer->used = 0;
        printed = vsnprintf(buffer->buffer, buffer->capacity, format, ap);
        buffer->used = printed;
    }
    else
    {
        buffer->used = printed;
    }
    va_end(aq);
    return printed;
}

// returns NULL on success, otherwise an error string
const char* BufferSearchAndReplace(Buffer *buffer, const char *pattern, const char *substitute, const char *options)
{
    assert(buffer);
    assert(pattern);
    assert(substitute);
    assert(options);

    int err;

    pcre_wrap_job *job = pcre_wrap_compile(pattern, substitute, options, &err);
    if (job == NULL)
    {
        return pcre_wrap_strerror(err);
    }

    size_t length = BufferSize(buffer);
    char *result;
    if (0 > (err = pcre_wrap_execute(job, (char*)BufferData(buffer), length, &result, &length)))
    {
        return pcre_wrap_strerror(err);
    }

    BufferSet(buffer, result, length);
    free(result);
    pcre_wrap_free_job(job);

    return NULL;
}

void BufferClear(Buffer *buffer)
{
    assert(buffer);
    buffer->used = 0;
    buffer->buffer[0] = '\0';
}

unsigned int BufferSize(const Buffer *buffer)
{
    assert(buffer);
    return buffer ? buffer->used : 0;
}

const char *BufferData(const Buffer *buffer)
{
    assert(buffer);
    return buffer ? buffer->buffer : NULL;
}

void BufferCanonify(Buffer *buffer)
{
    assert(buffer);
    if (buffer         != NULL &&
        buffer->buffer != NULL)
    {
        CanonifyNameInPlace(buffer->buffer);
    }
}

BufferBehavior BufferMode(const Buffer *buffer)
{
    assert(buffer);
    return buffer ? buffer->mode : BUFFER_BEHAVIOR_BYTEARRAY;
}

void BufferSetMode(Buffer *buffer, BufferBehavior mode)
{
    assert(buffer);
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

Buffer* BufferFilter(Buffer *buffer, BufferFilterFn filter, const bool invert)
{
    assert(buffer);

    Buffer *filtered = BufferNew();
    for (unsigned int i = 0; i < buffer->used; ++i)
    {
        bool test = (*filter)(buffer->buffer[i]);
        if (invert)
        {
            test = !test;
        }

        if (test)
        {
            BufferAppendChar(filtered, buffer->buffer[i]);
        }
    }

    return filtered;
}

void BufferRewrite(Buffer *buffer, BufferFilterFn filter, const bool invert)
{
    assert(buffer);

    Buffer *rewrite = BufferFilter(buffer, filter, invert);
    BufferSet(buffer, BufferData(rewrite), BufferSize(rewrite));
    BufferDestroy(rewrite);
}

unsigned BufferCapacity(const Buffer *buffer)
{
    assert(buffer);
    return buffer ? buffer->capacity : 0;
}
