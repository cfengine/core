#include "test.h"
#include "sequence.h"

#include "alloc.h"

static Seq *SequenceCreateRange(size_t initialCapacity, size_t start, size_t end)
{
    Seq *seq = SeqNew(initialCapacity, free);

    for (size_t i = start; i <= end; i++)
    {
        size_t *item = xmalloc(sizeof(size_t));

        *item = i;
        SeqAppend(seq, item);
    }

    return seq;
}

static void test_create_destroy(void)
{
    Seq *seq = SeqNew(5, NULL);

    SeqDestroy(seq);
}

static void test_append(void)
{
    Seq *seq = SeqNew(2, free);

    for (size_t i = 0; i < 1000; i++)
    {
        SeqAppend(seq, xstrdup("snookie"));
    }

    assert_int_equal(seq->length, 1000);

    for (size_t i = 0; i < 1000; i++)
    {
        assert_string_equal(seq->data[i], "snookie");
    }

    SeqDestroy(seq);
}

static int CompareNumbers(const void *a, const void *b, void *_user_data)
{
    return *(size_t *) a - *(size_t *) b;
}

static void test_lookup(void)
{
    Seq *seq = SequenceCreateRange(10, 0, 9);

    size_t *key = xmalloc(sizeof(size_t));

    *key = 5;

    size_t *result = SeqLookup(seq, key, CompareNumbers);
    assert_int_equal(*result, *key);

    *key = 17;
    result = SeqLookup(seq, key, CompareNumbers);
    assert_int_equal(result, NULL);

    SeqDestroy(seq);
    free(key);
}

static void test_index_of(void)
{
    Seq *seq = SequenceCreateRange(10, 0, 9);

    size_t *key = xmalloc(sizeof(size_t));

    *key = 5;

    ssize_t index = SeqIndexOf(seq, key, CompareNumbers);
    assert_int_equal(index, 5);

    *key = 17;
    index = SeqIndexOf(seq, key, CompareNumbers);
    assert_true(index == -1);

    SeqDestroy(seq);
    free(key);
}

static void test_sort(void)
{
    Seq *seq = SeqNew(5, NULL);

    size_t one = 1;
    size_t two = 2;
    size_t three = 3;
    size_t four = 4;
    size_t five = 5;

    SeqAppend(seq, &three);
    SeqAppend(seq, &two);
    SeqAppend(seq, &five);
    SeqAppend(seq, &one);
    SeqAppend(seq, &four);

    SeqSort(seq, CompareNumbers, NULL);

    assert_int_equal(seq->data[0], &one);
    assert_int_equal(seq->data[1], &two);
    assert_int_equal(seq->data[2], &three);
    assert_int_equal(seq->data[3], &four);
    assert_int_equal(seq->data[4], &five);

    SeqDestroy(seq);
}

static void test_remove_range(void)
{

    Seq *seq = SequenceCreateRange(10, 0, 9);

    SeqRemoveRange(seq, 3, 9);
    assert_int_equal(seq->length, 3);
    assert_int_equal(*(size_t *) seq->data[0], 0);
    assert_int_equal(*(size_t *) seq->data[1], 1);
    assert_int_equal(*(size_t *) seq->data[2], 2);

    SeqDestroy(seq);
    seq = SequenceCreateRange(10, 0, 9);

    SeqRemoveRange(seq, 0, 2);
    assert_int_equal(seq->length, 7);
    assert_int_equal(*(size_t *) seq->data[0], 3);

    SeqDestroy(seq);

    seq = SequenceCreateRange(10, 0, 9);

    SeqRemoveRange(seq, 5, 5);
    assert_int_equal(seq->length, 9);
    assert_int_equal(*(size_t *) seq->data[5], 6);

    SeqDestroy(seq);
}

static void test_remove(void)
{

    Seq *seq = SequenceCreateRange(10, 0, 9);

    SeqRemove(seq, 5);

    assert_int_equal(seq->length, 9);
    assert_int_equal(*(size_t *) seq->data[5], 6);

    SeqDestroy(seq);
    seq = SequenceCreateRange(10, 0, 9);

    SeqRemove(seq, 0);
    assert_int_equal(seq->length, 9);
    assert_int_equal(*(size_t *) seq->data[0], 1);

    SeqDestroy(seq);

    seq = SequenceCreateRange(10, 0, 9);

    SeqRemove(seq, 9);
    assert_int_equal(seq->length, 9);
    assert_int_equal(*(size_t *) seq->data[8], 8);

    SeqDestroy(seq);
}

static void test_reverse(void)
{
    {
        Seq *seq = SequenceCreateRange(2, 0, 1);
        assert_int_equal(0, *(size_t *)seq->data[0]);
        assert_int_equal(1, *(size_t *)seq->data[1]);
        SeqReverse(seq);
        assert_int_equal(1, *(size_t *)seq->data[0]);
        assert_int_equal(0, *(size_t *)seq->data[1]);
        SeqDestroy(seq);
    }

    {
        Seq *seq = SequenceCreateRange(3, 0, 2);
        SeqReverse(seq);
        assert_int_equal(2, *(size_t *)seq->data[0]);
        assert_int_equal(1, *(size_t *)seq->data[1]);
        assert_int_equal(0, *(size_t *)seq->data[2]);
        SeqDestroy(seq);
    }
}

static void test_len(void)
{
    Seq *seq = SeqNew(5, NULL);

    size_t one = 1;
    size_t two = 2;
    size_t three = 3;
    size_t four = 4;
    size_t five = 5;

    SeqAppend(seq, &three);
    SeqAppend(seq, &two);
    SeqAppend(seq, &five);
    SeqAppend(seq, &one);
    SeqAppend(seq, &four);

    assert_int_equal(SeqLength(seq),5);

    SeqDestroy(seq);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_create_destroy),
        unit_test(test_append),
        unit_test(test_lookup),
        unit_test(test_index_of),
        unit_test(test_sort),
        unit_test(test_remove_range),
        unit_test(test_remove),
        unit_test(test_reverse),
        unit_test(test_len)
    };

    return run_tests(tests);
}
