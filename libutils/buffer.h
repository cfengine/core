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

#ifndef CFENGINE_BUFFER_H
#define CFENGINE_BUFFER_H

#include <stdarg.h>
#include "refcount.h"

/**
  @brief Buffer implementation

  The buffer structure acts as a byte container. It can contains any bytes and it is not restricted to
  C strings (by default it acts as a C String).

  If an error arises while doing something, we do everything we can to restore things to its previous state.
  Unfortunately not all errors are recoverable. Since we do not have a proper errno system, we just return -1.
  */

typedef enum
{
    BUFFER_BEHAVIOR_CSTRING, //<! CString compatibility mode. A '\0' would be interpreted as end of the string, regardless of the size.
    BUFFER_BEHAVIOR_BYTEARRAY, //<! Byte array mode. A '\0' has no meaning, only the size of the buffer is taken into consideration.
} BufferBehavior;

#define DEFAULT_BUFFER_SIZE     4096
#define DEFAULT_CHUNK_SIZE      4096
#define DEFAULT_LOW_WATERMARK   512

struct Buffer {
    char *buffer;
    int mode;
    unsigned int low_water_mark;
    unsigned int chunk_size;
    unsigned int capacity;
    unsigned int real_capacity;
    unsigned int used;
    unsigned int beginning; /*!< This is to be used in the future to trim characters in the front. */
    unsigned int end; /*!< This is to be used in the future to trim characters in the back. */
    RefCount *ref_count;
};
typedef struct Buffer Buffer;

/**
  @brief Buffer initialization routine.
  Initializes the internals of a buffer. By default it is initialized to emulate a C string, but that can be
  changed at run time if needed. The default size of the buffer is set to DEFAULT_BUFFER_SIZE (4096).
  @param buffer Buffer to be initialized.
  @return 0 if the initialization was successful, -1 otherwise.
  */
int BufferNew(Buffer **buffer);
/**
  @brief Destroys a buffer and frees the memory associated with it.
  @param buffer Buffer to be destroyed.
  @return 0 if the destruction was successful, -1 otherwise.
  */
int BufferDestroy(Buffer **buffer);
/**
  @brief Creates a shallow copy of the source buffer.
  @param source Source buffer.
  @param destination Destination buffer.
  @return 0 if the copy was successful, -1 otherwise.
  */
int BufferCopy(Buffer *source, Buffer **destination);
/**
  @brief Compares two buffers. Uses the same semantic as strcmp.
  @note If this is called with NULL pointers, it will crash. There is no way around it.
  @param buffer1
  @param buffer2
  @return -1 if buffer1 < buffer2, 0 if buffer1 == buffer2, +1 if buffer1 > buffer2
  */
int BufferEqual(Buffer *buffer1, Buffer *buffer2);
/**
  @brief Replaces the current content of the buffer with the given string.

  In CString mode the content of bytes is copied until length bytes have been copied or a '\0' is found, whatever
  happens first. In ByteArray mode length bytes are copied regardless of if there are '\0' or not.
  @note The content of the buffer are overwritten with the new content, it is not possible to access them afterwards.
  @note For complex data it is preferable to use Printf since that will make sure that all data is represented properly.
  @param buffer Buffer to be used.
  @param bytes Collection of bytes to be copied into the buffer.
  @param length Length of the collection of bytes.
  @return The number of bytes copied or -1 if there was an error.
  */
int BufferSet(Buffer *buffer, char *bytes, unsigned int length);
/**
  @brief Appends the collection of bytes at the end of the current buffer.

  As with BufferSet(Buffer *buffer, char *bytes, unsigned int length), CString mode appends until a '\0' is found or up to
  the specified length. The data is NULL terminated. In ByteArray mode length bytes are copied and '\0' are not taken into
  account.
  @note There is a big difference between CString mode and ByteArray mode. In CString mode the final '\0' character will be
  overwritten and replaced with the content. In ByteArray mode there is no '\0', and therefore the last character is not replaced.
  @param buffer The buffer to operate on.
  @param bytes The collection of bytes to be appended.
  @param length The length of the data.
  @return The number of bytes used or -1 in case of error.
  */
int BufferAppend(Buffer *buffer, char *bytes, unsigned int length);
/**
  @brief Stores complex data on the buffer.

  This function uses the same semantic and flags as printf. Internally it might or not call sprintf, so do not depend on obscure
  sprintf/printf behaviors.
  @note This function can be used both in CString mode and in ByteArray mode. The only difference is the presence of the final '\0'
  character.
  @param buffer
  @param format
  @return The number of bytes written to the buffer or -1 if there was not enough space to printf in the buffer. The caller should try again.
  */
int BufferPrintf(Buffer *buffer, const char *format, ...);
/**
  @brief Clears the buffer.

  Clearing the buffer does not mean destroying the data. The data might be still present after this function is called, although
  it might not be accessible. This function never fails.

  If a NULL pointer is given it will politely ignore the call.
  @note This function might trigger a deep copy and a memory allocation if the buffer is shared.
  @param buffer Buffer to clear.
  */
void BufferZero(Buffer *buffer);
/**
  @brief Returns the size of the buffer.
  @param buffer
  @return The size of the buffer, that is the number of bytes contained on it.
  */
unsigned int BufferSize(Buffer *buffer);
/**
  @brief Returns the current mode of operation of the buffer.
  @param buffer The buffer to operate on.
  @return The current mode of operation.
  */
int BufferMode(Buffer *buffer);
/**
  @brief Sets the operational mode of the buffer.

  Although there are no problems changing the operational mode once the buffer is in use, there might be some obscure side effects.
  The data will not be changed but the interpretation of it will, therefore it might be possible that some data is lost when switching
  from ByteArray mode to CString mode, since '\0' are allowed in ByteArray mode but not in CString mode.
  @param buffer The buffer to operate on.
  @param mode The new mode of operation.
  */
void BufferSetMode(Buffer *buffer, BufferBehavior mode);
/**
  @brief Provides a pointer to the internal data.

  This is a const pointer and it is not supposed to be used to write data to the buffer, doing so will lead to undefined behavior and
  most likely segmentation faults. Use the proper functions to write data to the buffer.
  @param buffer
  @return A const char pointer to the data contained on the buffer.
  */
const char *BufferData(Buffer *buffer);
/*
 * Advanced API below
 */
/**
  @brief Returns the low_water_mark.
  @param buffer Buffer
  @return The low_water_mark for this buffer.
  */
unsigned int BufferLowWaterMark(Buffer *buffer);
/**
  @brief Sets the low_water_mark

  The low_water_mark is the indicator for the buffer to allocate more memory. We use a lazy approach, where we tolerate using more memory
  than what the low_water_mark allows as long as we do not exceed the buffer size. Next time we use the buffer to add stuff to it we will
  trigger a memory allocation.
  @param buffer Buffer
  @param low_water_mark New low_water_mark
  */
void BufferSetLowWaterMark(Buffer *buffer, unsigned int low_water_mark);
/**
  @brief Returns the chunk_size.
  @param buffer Buffer
  @return The chunk_size for this buffer.
  */
unsigned int BufferChunkSize(Buffer *buffer);
/**
  @brief Sets the chunk_size

  The chunk_size is the amount of memory requested by a new memory allocation. If the buffer needs to allocate more memory, it will do it
  in chunks of "chunk_size". Consider increasing this value if you allocate large chunks of memory at a time to avoid multiple memory allocations.
  @param buffer Buffer
  @param low_water_mark New low_water_mark
  */
void BufferSetChunkSize(Buffer *buffer, unsigned int chunk_size);

#endif // CFENGINE_BUFFER_H
