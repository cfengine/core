#include <test.h>

#include <ring_buffer.h>
#include <string_lib.h>
#include <alloc.h>

static void test_excess(void)
{
    static const size_t CAPACITY = 5;
    static const size_t FILL = 8;

    RingBuffer *buf = RingBufferNew(CAPACITY, NULL, free);

    assert_int_equal(0, RingBufferLength(buf));
    assert_false(RingBufferHead(buf));

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

        assert_string_equal(s, RingBufferHead(buf));
    }

    RingBufferIterator *iter = RingBufferIteratorNew(buf);
    const char *s = NULL;

    int i = FILL - CAPACITY;
    while ((s = RingBufferIteratorNext(iter)))
    {
        assert_int_equal(i, StringToLongExitOnError(s));
        i++;
    }

    assert_false(RingBufferIteratorNext(iter));
    RingBufferIteratorDestroy(iter);

    RingBufferClear(buf);
    assert_int_equal(0, RingBufferLength(buf));
    assert_false(RingBufferHead(buf));

    RingBufferDestroy(buf);
}

static void test_shortage(void)
{
    static const size_t CAPACITY = 5;

    RingBuffer *buf = RingBufferNew(CAPACITY, NULL, free);

    assert_false(RingBufferHead(buf));
    RingBufferAppend(buf, xstrdup("hello"));
    assert_string_equal("hello", RingBufferHead(buf));
    RingBufferAppend(buf, xstrdup("world"));
    assert_string_equal("world", RingBufferHead(buf));

    assert_int_equal(2, RingBufferLength(buf));

    RingBufferIterator *iter = RingBufferIteratorNew(buf);
    assert_string_equal("hello", RingBufferIteratorNext(iter));
    assert_string_equal("world", RingBufferIteratorNext(iter));
    assert_false(RingBufferIteratorNext(iter));
    RingBufferIteratorDestroy(iter);

    RingBufferClear(buf);
    assert_int_equal(0, RingBufferLength(buf));
    assert_false(RingBufferHead(buf));

    RingBufferDestroy(buf);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_excess),
        unit_test(test_shortage)
    };

    return run_tests(tests);
}
