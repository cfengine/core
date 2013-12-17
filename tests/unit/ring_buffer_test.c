#include <test.h>

#include <ring_buffer.h>
#include <string_lib.h>

static void test_basic(void)
{
    static size_t CAPACITY = 5;
    static size_t FILL = 8;

    RingBuffer *buf = RingBufferNew(CAPACITY, NULL, free);

    assert_int_equal(0, RingBufferLength(buf));

    for (size_t i = 0; i < FILL; i++)
    {
        char *s = StringFromLong(i);
        RingBufferAppend(buf, s);

        if (i >= CAPACITY)
        {
            assert_true(RingBufferIsFull(buf));
            assert_int_equal(CAPACITY, RingBufferLength(buf));
        }
        else
        {
            assert_int_equal(i + 1, RingBufferLength(buf));
        }
    }

    RingBufferIterator *iter = RingBufferIteratorNew(buf);
    const char *s = NULL;

    int i = 3;
    while ((s = RingBufferIteratorNext(iter)))
    {
        assert_int_equal(i, StringToLong(s));
        i++;
    }

    assert_false(RingBufferIteratorNext(iter));
    RingBufferIteratorDestroy(iter);

    RingBufferClear(buf);
    assert_int_equal(0, RingBufferLength(buf));

    RingBufferDestroy(buf);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_basic),
    };

    return run_tests(tests);
}
