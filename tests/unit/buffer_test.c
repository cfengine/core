#include <test.h>

#include <alloc.h>
#include <stdlib.h>
#include <string.h>
#include <cmockery.h>
#include <buffer.h>

static void test_createBuffer(void)
{
    Buffer *buffer = BufferNew();

    assert_true(buffer != NULL);
    assert_true(buffer->buffer != NULL);
    assert_int_equal(buffer->mode, BUFFER_BEHAVIOR_CSTRING);
    assert_int_equal(buffer->capacity, DEFAULT_BUFFER_CAPACITY);
    assert_int_equal(buffer->used, 0);

    BufferDestroy(buffer);
}

static void test_createBufferFrom(void)
{
    char *data = xstrdup("this is some data");
    unsigned int dataLength = strlen(data);

    Buffer *buffer = BufferNewFrom(data, dataLength);

    assert_true(buffer != NULL);
    assert_true(buffer->buffer != NULL);
    assert_string_equal(data, buffer->buffer);
    assert_int_equal(buffer->mode, BUFFER_BEHAVIOR_CSTRING);
    assert_int_equal(buffer->used, dataLength);

    BufferDestroy(buffer);
    free (data);
}

static void test_destroyBuffer(void)
{
    Buffer *buffer = BufferNew();
    BufferDestroy(buffer);
    buffer = NULL;
    BufferDestroy(buffer);
}

static void test_setBuffer(void)
{
    char *element0 = xstrdup("element0");
    unsigned int element0size = strlen(element0);
    char *element1 = (char *)xmalloc(2 * DEFAULT_BUFFER_CAPACITY + 2);
    unsigned int element1size = 2 * DEFAULT_BUFFER_CAPACITY + 1;

    Buffer *buffer = BufferNew();
    assert_true(buffer != NULL);
    // Smaller than the allocated buffer
    BufferSet(buffer, element0, element0size);
    assert_int_equal(element0size, buffer->used);
    assert_int_equal(element0size, BufferSize(buffer));
    assert_string_equal(element0, buffer->buffer);
    assert_string_equal(element0, BufferData(buffer));
    // Larger than the allocated buffer
    for (int i = 0; i < element1size; ++i)
    {
        element1[i] = 'a';
    }
    element1[element1size] = '\0';
    BufferSet(buffer, element1, element1size);
    assert_int_equal(element1size, buffer->used);
    assert_string_equal(element1, buffer->buffer);
    assert_string_equal(element1, BufferData(buffer));

    /*
     * Boundary checks, BUFFER_SIZE-1, BUFFER_SIZE and BUFFER_SIZE+1
     */
    Buffer *bm1 = BufferNew();
    Buffer *be = BufferNew();
    Buffer *bp1 = BufferNew();
    char buffer_m1[DEFAULT_BUFFER_CAPACITY - 1];
    char buffer_0[DEFAULT_BUFFER_CAPACITY];
    char buffer_p1[DEFAULT_BUFFER_CAPACITY + 1];
    unsigned int bm1_size = DEFAULT_BUFFER_CAPACITY - 1;
    unsigned int be_size = DEFAULT_BUFFER_CAPACITY;
    unsigned int bp1_size = DEFAULT_BUFFER_CAPACITY + 1;
    for (int i = 0; i < DEFAULT_BUFFER_CAPACITY - 1; ++i)
    {
        buffer_m1[i] = 'c';
        buffer_0[i] = 'd';
        buffer_p1[i] = 'e';
    }
    /*
     * One shorter, that means the buffer remains the same size as before.
     */
    buffer_m1[DEFAULT_BUFFER_CAPACITY - 2] = '\0';
    BufferSet(bm1, buffer_m1, bm1_size);
    assert_int_equal(bm1->capacity, DEFAULT_BUFFER_CAPACITY);
    /*
     * Same size, it should allocate one more block
     */
    buffer_0[DEFAULT_BUFFER_CAPACITY - 1] = '\0';
    BufferSet(be, buffer_0, be_size);
    assert_int_equal(be->capacity, 2 * DEFAULT_BUFFER_CAPACITY);
    /*
     * 1 more, it should allocate one more block
     */
    buffer_p1[DEFAULT_BUFFER_CAPACITY] = '\0';
    BufferSet(bp1, buffer_p1, bp1_size);
    assert_int_equal(bp1->capacity, 2 * DEFAULT_BUFFER_CAPACITY);
    BufferSet(buffer, element0, 0);
    /*
     * Destroy the buffer and good night.
     */
    BufferDestroy(buffer);
    BufferDestroy(bm1);
    BufferDestroy(be);
    BufferDestroy(bp1);
    free(element0);
    free(element1);
}

static void test_zeroBuffer(void)
{
    char *element0 = xstrdup("element0");
    unsigned int element0size = strlen(element0);
    const char *element0pointer = NULL;

    Buffer *buffer = BufferNew();
    BufferSet(buffer, element0, element0size);
    element0pointer = buffer->buffer;
    assert_int_equal(element0size, buffer->used);
    assert_int_equal(element0size, BufferSize(buffer));
    BufferClear(buffer);
    assert_int_equal(DEFAULT_BUFFER_CAPACITY, buffer->capacity);
    assert_int_equal(0, buffer->used);
    assert_int_equal(0, BufferSize(buffer));
	const char *data = BufferData(buffer);
	assert_string_equal(data, "");
    assert_true(element0pointer == buffer->buffer);
    BufferDestroy(buffer);
    free(element0);
}

static void test_copyCompareBuffer(void)
{
    char *element0 = xstrdup("element0");
    unsigned int element0size = strlen(element0);
    char *element1 = xstrdup("element1");
    unsigned int element1size = strlen(element1);

    Buffer *buffer0 = NULL;
    Buffer *buffer1 = NULL;
    Buffer *buffer2 = NULL;

    buffer0 = BufferNew();
    buffer1 = BufferNew();
    assert_int_equal(0, BufferCompare(buffer0, buffer0));
    assert_int_equal(0, BufferCompare(buffer0, buffer1));
    buffer2 = BufferCopy(buffer0);
    assert_true(buffer2);
    assert_int_equal(0, BufferCompare(buffer0, buffer2));

    // Add some flavour
    BufferDestroy(buffer2);
    BufferSet(buffer0, element0, element0size);
    BufferSet(buffer1, element1, element1size);
    assert_int_equal(0, BufferCompare(buffer0, buffer0));
    assert_int_equal(-1, BufferCompare(buffer0, buffer1));
    assert_int_equal(1, BufferCompare(buffer1, buffer0));
    buffer2 = BufferCopy(buffer0);
    assert_int_equal(0, BufferCompare(buffer0, buffer2));

    // Destroy the buffers
    BufferDestroy(buffer0);
    BufferDestroy(buffer1);
    BufferDestroy(buffer2);

    free (element0);
    free (element1);
}

static void test_appendBuffer(void)
{
    char *element0 = xstrdup("element0");
    unsigned int element0size = strlen(element0);
    const char *element0pointer = NULL;
    char *element1 = xstrdup("element1");
    unsigned int element1size = strlen(element1);
    const char *element1pointer = NULL;
    char *element2 = (char *)xmalloc(2 * DEFAULT_BUFFER_CAPACITY + 2);
    unsigned int element2size = 2 * DEFAULT_BUFFER_CAPACITY + 1;

    Buffer *buffer = BufferNew();
    assert_true(buffer != NULL);
    // Initialize the buffer with a small string
    BufferAppend(buffer, element0, element0size);
    element0pointer = buffer->buffer;
    assert_int_equal(element0size, buffer->used);
    assert_int_equal(element0size, BufferSize(buffer));
    assert_string_equal(element0, buffer->buffer);
    assert_string_equal(element0, BufferData(buffer));
    assert_int_equal(DEFAULT_BUFFER_CAPACITY, buffer->capacity);
    // Attach a small string to it
    BufferAppend(buffer, element1, element1size);
    assert_int_equal(element0size + element1size, BufferSize(buffer));
    element1pointer = buffer->buffer;
    assert_true(element0pointer == element1pointer);
    assert_int_equal(buffer->used, element0size + element1size);
    assert_int_equal(BufferSize(buffer), element0size + element1size);
    char *shortAppend = NULL;
    shortAppend = (char *)xmalloc(element0size + element1size + 1);
    strcpy(shortAppend, element0);
    strcat(shortAppend, element1);
    assert_string_equal(shortAppend, buffer->buffer);
    assert_string_equal(shortAppend, BufferData(buffer));

    /*
     * Zero the string and start again.
     */
    BufferClear(buffer);
    BufferAppend(buffer, element0, element0size);
    assert_int_equal(element0size, BufferSize(buffer));
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
    BufferAppend(buffer, element2, element2size);
    assert_int_equal(element0size + element2size, BufferSize(buffer));
    assert_int_equal(buffer->used, element0size + element2size);
    assert_int_equal(BufferSize(buffer), element0size + element2size);
    char *longAppend = NULL;
    longAppend = (char *)xmalloc(element0size + element2size + 1);
    strcpy(longAppend, element0);
    strcat(longAppend, element2);
    assert_string_equal(longAppend, buffer->buffer);
    assert_string_equal(longAppend, BufferData(buffer));

    BufferClear(buffer);

    /*
     * Destroy the buffer and good night.
     */
    free(shortAppend);
    free(longAppend);
    BufferDestroy(buffer);
    free(element0);
    free(element1);
    free(element2);
}

static void test_append_boundaries(void)
{
    /*
     * Boundary checks, BUFFER_SIZE-1, BUFFER_SIZE and BUFFER_SIZE+1
     */
    char buffer_m1[DEFAULT_BUFFER_CAPACITY - 1];
    char buffer_0[DEFAULT_BUFFER_CAPACITY];
    char buffer_p1[DEFAULT_BUFFER_CAPACITY + 1];
    for (size_t i = 0; i < DEFAULT_BUFFER_CAPACITY - 1; ++i)
    {
        buffer_m1[i] = 'c';
        buffer_0[i] = 'd';
        buffer_p1[i] = 'e';
    }

    {
        const unsigned int bm1_size = DEFAULT_BUFFER_CAPACITY - 1;
        Buffer *bm1 = BufferNew();
        buffer_m1[DEFAULT_BUFFER_CAPACITY - 2] = '\0';
        BufferAppend(bm1, buffer_m1, bm1_size);
        assert_int_equal(strlen(buffer_m1), BufferSize(bm1));
        assert_int_equal(bm1->capacity, DEFAULT_BUFFER_CAPACITY);
        BufferDestroy(bm1);
    }

    {
        const unsigned int be_size = DEFAULT_BUFFER_CAPACITY;
        Buffer *be = BufferNew();
        buffer_0[DEFAULT_BUFFER_CAPACITY - 1] = '\0';
        BufferAppend(be, buffer_0, be_size);
        assert_int_equal(strlen(buffer_0), BufferSize(be));
        assert_int_equal(be->capacity, 2 * DEFAULT_BUFFER_CAPACITY);
        BufferDestroy(be);
    }

    {
        const unsigned int bp1_size = DEFAULT_BUFFER_CAPACITY + 1;
        Buffer *bp1 = BufferNew();
        buffer_p1[DEFAULT_BUFFER_CAPACITY] = '\0';
        BufferAppend(bp1, buffer_p1, bp1_size);
        assert_int_equal(strlen(buffer_p1), BufferSize(bp1));
        assert_int_equal(bp1->capacity, 2 * DEFAULT_BUFFER_CAPACITY);
        BufferDestroy(bp1);
    }
}

static void test_printf(void)
{
    char *char0 = xstrdup("char0");
    unsigned int char0size = strlen(char0);
    const char *char0pointer = NULL;
    char *char1 = xstrdup("char1");
    unsigned int char1size = strlen(char1);
    const char *char1pointer = NULL;
    char *char2 = (char *)xmalloc(2 * DEFAULT_BUFFER_CAPACITY + 2);
    unsigned int char2size = 2 * DEFAULT_BUFFER_CAPACITY + 1;
    int int0 = 123456789;
    char *int0char = xstrdup("123456789");
    unsigned int int0charsize = strlen(int0char);
    double double0 = 3.1415;
    char *double0char = xstrdup("3.1415");
    unsigned int double0charsize = strlen(double0char);
    char *char0int0char1double0 = xstrdup("char0 123456789 char1 3.1415");
    unsigned int char0int0char1double0size = strlen(char0int0char1double0);

    Buffer *buffer = BufferNew();
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
    // The buffer should grow
    assert_int_equal(char2size, BufferPrintf(buffer, "%s", char2));
    assert_string_equal(char2, buffer->buffer);
    assert_string_equal(char2, BufferData(buffer));
    assert_int_equal(char2size, buffer->used);
    assert_int_equal(char2size, BufferSize(buffer));
    /*
     * Boundary checks, BUFFER_SIZE-1, BUFFER_SIZE and BUFFER_SIZE+1
     */
    Buffer *bm1 = BufferNew();
    Buffer *be = BufferNew();
    Buffer *bp1 = BufferNew();
    /*
     * The sizes are different for printf. If we have a size of X, then the string
     * is of length X-1, and so forth.
     */
    char buffer_m1[DEFAULT_BUFFER_CAPACITY];
    char buffer_0[DEFAULT_BUFFER_CAPACITY + 1];
    char buffer_p1[DEFAULT_BUFFER_CAPACITY + 2];
    unsigned int bm1_size = DEFAULT_BUFFER_CAPACITY - 1;
    unsigned int be_size = DEFAULT_BUFFER_CAPACITY;
    unsigned int bp1_size = DEFAULT_BUFFER_CAPACITY + 1;
    /*
     * Make sure the buffers are filled with 0.
     */
    memset(buffer_m1, '\0', DEFAULT_BUFFER_CAPACITY);
    memset(buffer_0, '\0', DEFAULT_BUFFER_CAPACITY + 1);
    memset(buffer_p1, '\0', DEFAULT_BUFFER_CAPACITY + 2);
    /*
     * Write something to the buffers
     */
    memset(buffer_m1, 'c', DEFAULT_BUFFER_CAPACITY);
    memset(buffer_0, 'd', DEFAULT_BUFFER_CAPACITY + 1);
    memset(buffer_p1, 'e', DEFAULT_BUFFER_CAPACITY + 2);
    /*
     * One shorter, that means the buffer remains the same size as before.
     */
    buffer_m1[DEFAULT_BUFFER_CAPACITY - 1] = '\0';
    assert_int_equal(bm1_size, BufferPrintf(bm1, "%s", buffer_m1));
    assert_string_equal(buffer_m1, bm1->buffer);
    assert_int_equal(bm1->capacity, DEFAULT_BUFFER_CAPACITY);
    /*
     * Same size, it should allocate one more block.
     * This means retrying the operation.
     */
    buffer_0[DEFAULT_BUFFER_CAPACITY] = '\0';
    assert_int_equal(be_size, BufferPrintf(be, "%s", buffer_0));
    assert_string_equal(buffer_0, be->buffer);
    assert_int_equal(be->capacity, 2 * DEFAULT_BUFFER_CAPACITY);
    /*
     * 1 more, it should allocate one more block
     * This means retrying the operation.
     */
    buffer_p1[DEFAULT_BUFFER_CAPACITY + 1] = '\0';
    assert_int_equal(bp1_size, BufferPrintf(bp1, "%s", buffer_p1));
    assert_string_equal(buffer_p1, bp1->buffer);
    assert_int_equal(bp1->capacity, 2 * DEFAULT_BUFFER_CAPACITY);
    /*
     * Release the resources
     */
    BufferDestroy(buffer);
    BufferDestroy(bm1);
    BufferDestroy(be);
    BufferDestroy(bp1);
    free(char0);
    free(char1);
    free(char2);
    free(int0char);
    free(double0char);
    free(char0int0char1double0);
}

static int test_vprintf_helper(Buffer *buffer, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int result = 0;
    result = BufferVPrintf(buffer, fmt, ap);
    va_end(ap);
    return result;
}

static void test_vprintf(void)
{
    char *char0 = xstrdup("char0");
    unsigned int char0size = strlen(char0);
    const char *char0pointer = NULL;
    char *char1 = xstrdup("char1");
    unsigned int char1size = strlen(char1);
    const char *char1pointer = NULL;
    char *char2 = (char *)xmalloc(2 * DEFAULT_BUFFER_CAPACITY + 2);
    unsigned int char2size = 2 * DEFAULT_BUFFER_CAPACITY + 1;
    int int0 = 123456789;
    char *int0char = xstrdup("123456789");
    unsigned int int0charsize = strlen(int0char);
    double double0 = 3.1415;
    char *double0char = xstrdup("3.1415");
    unsigned int double0charsize = strlen(double0char);
    char *char0int0char1double0 = xstrdup("char0 123456789 char1 3.1415");
    unsigned int char0int0char1double0size = strlen(char0int0char1double0);

    Buffer *buffer = BufferNew();
    assert_true(buffer != NULL);
    /*
     * Print the first char and compare the result
     */
    assert_int_equal(char0size, test_vprintf_helper(buffer, "%s", char0));
    char0pointer = buffer->buffer;
    assert_string_equal(char0, buffer->buffer);
    assert_string_equal(char0, BufferData(buffer));
    assert_int_equal(char0size, buffer->used);
    assert_int_equal(char0size, BufferSize(buffer));
    /*
     * Overwrite the first char with the second one
     */
    assert_int_equal(char1size, test_vprintf_helper(buffer, "%s", char1));
    char1pointer = buffer->buffer;
    assert_string_equal(char1, buffer->buffer);
    assert_string_equal(char1, BufferData(buffer));
    assert_int_equal(char1size, buffer->used);
    assert_int_equal(char1size, BufferSize(buffer));
    assert_true(char0pointer == char1pointer);
    /*
     * Try the int now
     */
    assert_int_equal(int0charsize, test_vprintf_helper(buffer, "%d", int0));
    assert_string_equal(int0char, buffer->buffer);
    assert_string_equal(int0char, BufferData(buffer));
    assert_int_equal(int0charsize, buffer->used);
    assert_int_equal(int0charsize, BufferSize(buffer));
    /*
     * Try the double now
     */
    assert_int_equal(double0charsize, test_vprintf_helper(buffer, "%.4f", double0));
    assert_string_equal(double0char, buffer->buffer);
    assert_string_equal(double0char, BufferData(buffer));
    assert_int_equal(double0charsize, buffer->used);
    assert_int_equal(double0charsize, BufferSize(buffer));
    /*
     * Try the combination now
     */
    assert_int_equal(char0int0char1double0size, test_vprintf_helper(buffer, "%s %d %s %.4f", char0, int0, char1, double0));
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
    // The buffer should resize itself
    assert_int_equal(char2size, test_vprintf_helper(buffer, "%s", char2));
    assert_string_equal(char2, buffer->buffer);
    assert_string_equal(char2, BufferData(buffer));
    assert_int_equal(char2size, buffer->used);
    assert_int_equal(char2size, BufferSize(buffer));
    /*
     * Boundary checks, BUFFER_SIZE-1, BUFFER_SIZE and BUFFER_SIZE+1
     */
    Buffer *bm1 = BufferNew();
    Buffer *be = BufferNew();
    Buffer *bp1 = BufferNew();
    /*
     * The sizes are different for printf. If we have a size of X, then the string
     * is of length X-1, and so forth.
     */
    char buffer_m1[DEFAULT_BUFFER_CAPACITY];
    char buffer_0[DEFAULT_BUFFER_CAPACITY + 1];
    char buffer_p1[DEFAULT_BUFFER_CAPACITY + 2];
    unsigned int bm1_size = DEFAULT_BUFFER_CAPACITY - 1;
    unsigned int be_size = DEFAULT_BUFFER_CAPACITY;
    unsigned int bp1_size = DEFAULT_BUFFER_CAPACITY + 1;
    /*
     * Make sure the buffers are filled with 0.
     */
    memset(buffer_m1, '\0', DEFAULT_BUFFER_CAPACITY);
    memset(buffer_0, '\0', DEFAULT_BUFFER_CAPACITY + 1);
    memset(buffer_p1, '\0', DEFAULT_BUFFER_CAPACITY + 2);
    /*
     * Write something to the buffers
     */
    memset(buffer_m1, 'c', DEFAULT_BUFFER_CAPACITY);
    memset(buffer_0, 'd', DEFAULT_BUFFER_CAPACITY + 1);
    memset(buffer_p1, 'e', DEFAULT_BUFFER_CAPACITY + 2);
    /*
     * One shorter, that means the buffer remains the same size as before.
     */
    buffer_m1[DEFAULT_BUFFER_CAPACITY - 1] = '\0';
    assert_int_equal(bm1_size, test_vprintf_helper(bm1, "%s", buffer_m1));
    assert_string_equal(buffer_m1, bm1->buffer);
    assert_int_equal(bm1->capacity, DEFAULT_BUFFER_CAPACITY);
    /*
     * Same size, it should allocate one more block.
     * This means retrying the operation.
     */
    buffer_0[DEFAULT_BUFFER_CAPACITY] = '\0';
    assert_int_equal(be_size, test_vprintf_helper(be, "%s", buffer_0));
    assert_string_equal(buffer_0, be->buffer);
    assert_int_equal(be->capacity, 2 * DEFAULT_BUFFER_CAPACITY);
    /*
     * 1 more, it should allocate one more block
     * This means retrying the operation.
     */
    buffer_p1[DEFAULT_BUFFER_CAPACITY + 1] = '\0';
    assert_int_equal(bp1_size, test_vprintf_helper(bp1, "%s", buffer_p1));
    assert_string_equal(buffer_p1, bp1->buffer);
    assert_int_equal(bp1->capacity, 2 * DEFAULT_BUFFER_CAPACITY);
    /*
     * Release the resources
     */
    BufferDestroy(buffer);
    BufferDestroy(bm1);
    BufferDestroy(be);
    BufferDestroy(bp1);
    free(char0);
    free(char1);
    free(char2);
    free(int0char);
    free(double0char);
    free(char0int0char1double0);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_createBuffer),
        unit_test(test_createBufferFrom),
        unit_test(test_destroyBuffer),
        unit_test(test_zeroBuffer),
        unit_test(test_copyCompareBuffer),
        unit_test(test_setBuffer),
        unit_test(test_appendBuffer),
        unit_test(test_append_boundaries),
        unit_test(test_printf),
        unit_test(test_vprintf)
    };

    return run_tests(tests);
}

