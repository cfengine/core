#include <setjmp.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "cmockery.h"
#include "buffer.h"

static void test_createBuffer(void **state)
{
    Buffer *buffer = NULL;
    assert_int_equal(0, BufferNew(&buffer));
    assert_true(buffer != NULL);
    assert_true(buffer->buffer != NULL);
    assert_int_equal(buffer->mode, BUFFER_BEHAVIOR_CSTRING);
    assert_int_equal(buffer->low_water_mark, DEFAULT_LOW_WATERMARK);
    assert_int_equal(buffer->chunk_size, DEFAULT_CHUNK_SIZE);
    assert_int_equal(buffer->capacity, DEFAULT_BUFFER_SIZE);
    assert_int_equal(buffer->real_capacity, DEFAULT_BUFFER_SIZE - DEFAULT_LOW_WATERMARK);
    assert_int_equal(buffer->used, 0);
    assert_int_equal(buffer->beginning, 0);
    assert_int_equal(buffer->end, 0);
    assert_true(buffer->ref_count != NULL);
    assert_int_equal(buffer->ref_count->user_count, 1);
    assert_int_equal(-1, BufferNew(NULL));
}

static void test_destroyBuffer(void **state)
{
    Buffer *buffer = NULL;
    assert_int_equal(0, BufferNew(&buffer));
    assert_int_equal(0, BufferDestroy(&buffer));
    assert_true(buffer == NULL);
    assert_int_equal(0, BufferDestroy(NULL));
}

static void test_setBuffer(void **state)
{
    char element0[] = "element0";
    unsigned int element0size = strlen(element0);
    const char *element0pointer = NULL;
    char element1[2 * DEFAULT_BUFFER_SIZE + 2];
    unsigned int element1size = 2 * DEFAULT_BUFFER_SIZE + 1;
    const char *element1pointer = NULL;

    Buffer *buffer = NULL;
    assert_int_equal(0, BufferNew(&buffer));
    assert_true(buffer != NULL);
    // Smaller than the allocated buffer
    assert_int_equal(element0size, BufferSet(buffer, element0, element0size));
    element0pointer = buffer->buffer;
    assert_int_equal(element0size, buffer->used);
    assert_int_equal(element0size, BufferSize(buffer));
    assert_string_equal(element0, buffer->buffer);
    assert_string_equal(element0, BufferData(buffer));
    assert_int_equal(DEFAULT_BUFFER_SIZE, buffer->capacity);
    // Larger than the allocated buffer
    int i = 0;
    for (i = 0; i < element1size; ++i)
        element1[i] = 'a';
    element1[element1size] = '\0';
    assert_int_equal(element1size, BufferSet(buffer, element1, element1size));
    element1pointer = buffer->buffer;
    assert_true(element0pointer != element1pointer);
    assert_int_equal(element1size, buffer->used);
    assert_string_equal(element1, buffer->buffer);
    assert_string_equal(element1, BufferData(buffer));
    assert_int_equal(DEFAULT_BUFFER_SIZE * 3, buffer->capacity);
    // Negative cases
    assert_int_equal(-1, BufferSet(NULL, element0, element0size));
    assert_int_equal(-1, BufferSet(NULL, NULL, element0size));
    assert_int_equal(-1, BufferSet(buffer, NULL, element0size));
    assert_int_equal(0, BufferSet(buffer, element0, 0));
    /*
     * Destroy the buffer and good night.
     */
    assert_int_equal(0, BufferDestroy(&buffer));
    assert_true(buffer == NULL);
}

static void test_zeroBuffer(void **state)
{
    char element0[] = "element0";
    unsigned int element0size = strlen(element0);
    const char *element0pointer = NULL;

    Buffer *buffer = NULL;
    assert_int_equal(0, BufferNew(&buffer));
    assert_int_equal(element0size, BufferSet(buffer, element0, element0size));
    element0pointer = buffer->buffer;
    assert_int_equal(element0size, buffer->used);
    assert_int_equal(element0size, BufferSize(buffer));
    BufferZero(buffer);
    assert_int_equal(DEFAULT_BUFFER_SIZE, buffer->capacity);
    assert_int_equal(0, buffer->used);
    assert_int_equal(0, BufferSize(buffer));
    assert_true(element0pointer == buffer->buffer);
    BufferZero(NULL);
    assert_int_equal(0, BufferDestroy(&buffer));
}

static void test_copyEqualBuffer(void **state)
{
    char element0[] = "element0";
    unsigned int element0size = strlen(element0);
    char element1[] = "element1";
    unsigned int element1size = strlen(element1);

    Buffer *buffer0 = NULL;
    Buffer *buffer1 = NULL;
    Buffer *buffer2 = NULL;

    // Empty buffers, all empty buffers are the same
    assert_int_equal(0, BufferNew(&buffer0));
    assert_int_equal(0, BufferNew(&buffer1));
    assert_true(BufferEqual(buffer0, buffer0));
    assert_true(BufferEqual(buffer0, buffer1));
    assert_int_equal(0, BufferCopy(buffer0, &buffer2));
    assert_true(BufferEqual(buffer0, buffer2));

    // Add some flavour
    assert_int_equal(0, BufferDestroy(&buffer2));
    assert_int_equal(element0size, BufferSet(buffer0, element0, element0size));
    assert_int_equal(element1size, BufferSet(buffer1, element1, element1size));
    assert_true(BufferEqual(buffer0, buffer0));
    assert_false(BufferEqual(buffer0, buffer1));
    assert_int_equal(0, BufferCopy(buffer0, &buffer2));
    assert_true(BufferEqual(buffer0, buffer2));

    // Destroy the buffers
    assert_int_equal(0, BufferDestroy(&buffer0));
    assert_int_equal(0, BufferDestroy(&buffer1));
    assert_int_equal(0, BufferDestroy(&buffer2));
}

static void test_appendBuffer(void **state)
{
    char element0[] = "element0";
    unsigned int element0size = strlen(element0);
    const char *element0pointer = NULL;
    char element1[] = "element1";
    unsigned int element1size = strlen(element1);
    const char *element1pointer = NULL;
    char element2[2 * DEFAULT_BUFFER_SIZE + 2];
    unsigned int element2size = 2 * DEFAULT_BUFFER_SIZE + 1;
    const char *element2pointer = NULL;

    Buffer *buffer = NULL;
    assert_int_equal(0, BufferNew(&buffer));
    assert_true(buffer != NULL);
    // Initialize the buffer with a small string
    assert_int_equal(element0size, BufferAppend(buffer, element0, element0size));
    element0pointer = buffer->buffer;
    assert_int_equal(element0size, buffer->used);
    assert_int_equal(element0size, BufferSize(buffer));
    assert_string_equal(element0, buffer->buffer);
    assert_string_equal(element0, BufferData(buffer));
    assert_int_equal(DEFAULT_BUFFER_SIZE, buffer->capacity);
    // Attach a small string to it
    assert_int_equal(element0size + element1size, BufferAppend(buffer, element1, element1size));
    element1pointer = buffer->buffer;
    assert_true(element0pointer == element1pointer);
    assert_int_equal(buffer->used, element0size + element1size);
    assert_int_equal(BufferSize(buffer), element0size + element1size);
    char *shortAppend = NULL;
    shortAppend = (char *)malloc(element0size + element1size + 1);
    strcpy(shortAppend, element0);
    strcat(shortAppend, element1);
    assert_string_equal(shortAppend, buffer->buffer);
    assert_string_equal(shortAppend, BufferData(buffer));

    /*
     * Zero the string and start again.
     */
    BufferZero(buffer);
    assert_int_equal(element0size, BufferAppend(buffer, element0, element0size));
    element0pointer = buffer->buffer;
    assert_int_equal(element0size, buffer->used);
    assert_int_equal(element0size, BufferSize(buffer));
    assert_string_equal(element0, buffer->buffer);
    assert_string_equal(element0, BufferData(buffer));

    /*
     * Larger than the allocated buffer, this means we will allocate more memory
     * copy stuff into the new buffer and all that.
     */
    int i = 0;
    for (i = 0; i < element2size; ++i)
        element2[i] = 'a';
    element2[element2size] = '\0';
    assert_int_equal(element0size + element2size, BufferAppend(buffer, element2, element2size));
    element2pointer = buffer->buffer;
    assert_true(element0pointer != element2pointer);
    assert_int_equal(buffer->used, element0size + element2size);
    assert_int_equal(BufferSize(buffer), element0size + element2size);
    char *longAppend = NULL;
    longAppend = (char *)malloc(element0size + element2size + 1);
    strcpy(longAppend, element0);
    strcat(longAppend, element2);
    assert_string_equal(longAppend, buffer->buffer);
    assert_string_equal(longAppend, BufferData(buffer));
    /*
     * Destroy the buffer and good night.
     */
    free(shortAppend);
    free(longAppend);
    assert_int_equal(0, BufferDestroy(&buffer));
    assert_true(buffer == NULL);
}

static void test_printf(void **state)
{
    char char0[] = "char0";
    unsigned int char0size = strlen(char0);
    const char *char0pointer = NULL;
    char char1[] = "char1";
    unsigned int char1size = strlen(char1);
    const char *char1pointer = NULL;
    char char2[2 * DEFAULT_BUFFER_SIZE + 2];
    unsigned int char2size = 2 * DEFAULT_BUFFER_SIZE + 1;
    int int0 = 123456789;
    char int0char[] = "123456789";
    unsigned int int0charsize = strlen(int0char);
    double double0 = 3.1415;
    char double0char[] = "3.1415";
    unsigned int double0charsize = strlen(double0char);
    char char0int0char1double0[] = "char0 123456789 char1 3.1415";
    unsigned int char0int0char1double0size = strlen(char0int0char1double0);

    Buffer *buffer = NULL;
    assert_int_equal(0, BufferNew(&buffer));
    assert_true(buffer != NULL);
    /*
     * Print the first char and compare the result
     */
    assert_int_equal(char0size, BufferPrintf(buffer, "%s", char0));
    char0pointer = buffer->buffer;
    assert_string_equal(char0, buffer->buffer);
    assert_string_equal(char0, BufferData(buffer));
    assert_int_equal(char0size, buffer->used);
    assert_int_equal(char0size, BufferSize(buffer));
    /*
     * Overwrite the first char with the second one
     */
    assert_int_equal(char1size, BufferPrintf(buffer, "%s", char1));
    char1pointer = buffer->buffer;
    assert_string_equal(char1, buffer->buffer);
    assert_string_equal(char1, BufferData(buffer));
    assert_int_equal(char1size, buffer->used);
    assert_int_equal(char1size, BufferSize(buffer));
    assert_true(char0pointer == char1pointer);
    /*
     * Try the int now
     */
    assert_int_equal(int0charsize, BufferPrintf(buffer, "%d", int0));
    assert_string_equal(int0char, buffer->buffer);
    assert_string_equal(int0char, BufferData(buffer));
    assert_int_equal(int0charsize, buffer->used);
    assert_int_equal(int0charsize, BufferSize(buffer));
    /*
     * Try the double now
     */
    assert_int_equal(double0charsize, BufferPrintf(buffer, "%.4f", double0));
    assert_string_equal(double0char, buffer->buffer);
    assert_string_equal(double0char, BufferData(buffer));
    assert_int_equal(double0charsize, buffer->used);
    assert_int_equal(double0charsize, BufferSize(buffer));
    /*
     * Try the combination now
     */
    assert_int_equal(char0int0char1double0size, BufferPrintf(buffer, "%s %d %s %.4f", char0, int0, char1, double0));
    assert_string_equal(char0int0char1double0, buffer->buffer);
    assert_string_equal(char0int0char1double0, BufferData(buffer));
    assert_int_equal(char0int0char1double0size, buffer->used);
    assert_int_equal(char0int0char1double0size, BufferSize(buffer));
    /*
     * Finally, try something larger than the default buffer and see if we get the right return value.
     */
    unsigned int i = 0;
    for (i = 0; i < char2size; ++i)
        char2[i] = 'a';
    char2[char2size] = '\0';
    // The first time, the buffer is too small.
    assert_int_equal(-1, BufferPrintf(buffer, "%s", char2));
    // The second time the buffer is too small also.
    assert_int_equal(-1, BufferPrintf(buffer, "%s", char2));
    // The third time there is enough space
    assert_int_equal(char2size, BufferPrintf(buffer, "%s", char2));
    assert_string_equal(char2, buffer->buffer);
    assert_string_equal(char2, BufferData(buffer));
    assert_int_equal(char2size, buffer->used);
    assert_int_equal(char2size, BufferSize(buffer));
}

static void test_advancedAPI(void **state)
{
    Buffer *buffer = NULL;
    assert_int_equal(0, BufferNew(&buffer));
    assert_true(buffer != NULL);
    assert_true(buffer->buffer != NULL);
    assert_int_equal(buffer->mode, BUFFER_BEHAVIOR_CSTRING);
    assert_int_equal(buffer->low_water_mark, DEFAULT_LOW_WATERMARK);
    assert_int_equal(buffer->chunk_size, DEFAULT_CHUNK_SIZE);
    assert_int_equal(buffer->capacity, DEFAULT_BUFFER_SIZE);
    assert_int_equal(buffer->real_capacity, DEFAULT_BUFFER_SIZE - DEFAULT_LOW_WATERMARK);
    assert_int_equal(buffer->used, 0);
    /*
     * We have a properly initialized buffer, let's play with the advanced API and change some values here and
     * there.
     * 1. chunk_size since it does not affect any other values.
     * 2. low_water_mark since it affects real_capacity.
     */
    assert_int_equal(DEFAULT_CHUNK_SIZE, BufferChunkSize(buffer));
    assert_int_equal(0, BufferChunkSize(NULL));
    BufferSetChunkSize(NULL, 0);
    BufferSetChunkSize(buffer, 0);
    assert_int_equal(DEFAULT_CHUNK_SIZE, buffer->chunk_size);
    BufferSetChunkSize(NULL, 8192);
    BufferSetChunkSize(buffer, 2*DEFAULT_CHUNK_SIZE);
    assert_int_equal(2*DEFAULT_CHUNK_SIZE, buffer->chunk_size);

    assert_int_equal(DEFAULT_LOW_WATERMARK, BufferLowWaterMark(buffer));
    assert_int_equal(0, BufferLowWaterMark(NULL));
    BufferSetLowWaterMark(NULL, 0);
    BufferSetLowWaterMark(buffer, 0);
    assert_int_equal(0, buffer->low_water_mark);
    assert_int_equal(buffer->capacity, buffer->real_capacity);
    BufferSetLowWaterMark(buffer, 1024);
    assert_int_equal(1024, buffer->low_water_mark);
    assert_int_equal(DEFAULT_BUFFER_SIZE - 1024, buffer->real_capacity);
    /*
     * Destroy the buffer and good night.
     */
    assert_int_equal(0, BufferDestroy(&buffer));
    assert_true(buffer == NULL);
}

int main()
{
    const UnitTest tests[] = {
        unit_test(test_createBuffer)
        , unit_test(test_destroyBuffer)
        , unit_test(test_zeroBuffer)
        , unit_test(test_copyEqualBuffer)
        , unit_test(test_setBuffer)
        , unit_test(test_appendBuffer)
        , unit_test(test_printf)
        , unit_test(test_advancedAPI)
    };

    return run_tests(tests);
}

