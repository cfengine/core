#include <test.h>
#include <rb-tree.h>

#include <alloc.h>
#include <sequence.h>

#include <stdlib.h>

static void *_IntCopy(const void *_a)
{
    return xmemdup(_a, sizeof(int));
}

static int _IntCompare(const void *_a, const void *_b)
{
    const int *a = _a, *b = _b;
    return *a - *b;
}

static RBTree *IntTreeNew_(void)
{
    return RBTreeNew(_IntCopy, _IntCompare, free, _IntCopy, _IntCompare, free);
}

static void test_new_destroy(void)
{
    RBTree *t = IntTreeNew_();
    RBTreeDestroy(t);
}

static void test_put_overwrite(void)
{
    RBTree *t = IntTreeNew_();

    int a = 42;
    assert_false(RBTreePut(t, &a, &a));
    int *r = RBTreeGet(t, &a);
    assert_int_equal(a, *r);

    assert_true(RBTreePut(t, &a, &a));
    r = RBTreeGet(t, &a);
    assert_int_equal(a, *r);

    RBTreeDestroy(t);
}

static void test_put_remove(void)
{
    RBTree *t = IntTreeNew_();

    int a = 42;
    assert_false(RBTreePut(t, &a, &a));
    int *r = RBTreeGet(t, &a);
    assert_int_equal(a, *r);

    assert_true(RBTreeRemove(t, &a));
    r = RBTreeGet(t, &a);
    assert_true(r == NULL);

    assert_false(RBTreeRemove(t, &a));
    r = RBTreeGet(t, &a);
    assert_true(r == NULL);

    RBTreeDestroy(t);
}

static void test_put_remove_inorder(void)
{
    RBTree *t = IntTreeNew_();
    for (int i = 0; i < 20000; i++)
    {
        RBTreePut(t, &i, &i);
    }

    for (int i = 0; i < 20000; i++)
    {
        int *r = RBTreeGet(t, &i);
        assert_int_equal(i, *r);
    }

    for (int i = 0; i < 20000; i++)
    {
        RBTreeRemove(t, &i);
    }

    RBTreeDestroy(t);
}

static void test_iterate_empty(void)
{
    RBTree *t = IntTreeNew_();

    RBTreeIterator *it = RBTreeIteratorNew(t);
    void *r = NULL;
    while (RBTreeIteratorNext(it, &r, NULL))
    {
        fail();
    }
    assert_true(r == NULL);
    RBTreeIteratorDestroy(it);

    RBTreeDestroy(t);
}

static void test_iterate(void)
{
    RBTree *t = IntTreeNew_();

    for (int i = 0; i < 20; i++)
    {
        RBTreePut(t, &i, &i);
    }

    assert_int_equal(20, RBTreeSize(t));

    RBTreeIterator *it = RBTreeIteratorNew(t);
    for (int i = 0; i < 20; i++)
    {
        int *k = NULL;
        int *v = NULL;
        assert_true(RBTreeIteratorNext(it, (void **)&k, (void **)&v));
        assert_int_equal(i, *k);
        assert_int_equal(i, *v);
    }

    assert_false(RBTreeIteratorNext(it, NULL, NULL));
    RBTreeIteratorDestroy(it);

    RBTreeDestroy(t);
}

static void test_put_remove_random(void)
{
    Seq *nums = SeqNew(20000, free);
    srand(0);
    for (int i = 0; i < 20000; i++)
    {
        int k = rand() % 1000;
        SeqAppend(nums, xmemdup(&k, sizeof(int)));
    }

    RBTree *t = IntTreeNew_();
    for (size_t i = 0; i < SeqLength(nums); i++)
    {
        RBTreePut(t, SeqAt(nums, i), SeqAt(nums, i));
    }

    for (size_t i = 0; i < SeqLength(nums); i++)
    {
        int *k = SeqAt(nums, i);
        int *r = RBTreeGet(t, k);
        assert_int_equal(*k, *r);
    }

    for (size_t i = 0; i < SeqLength(nums); i++)
    {
        RBTreeRemove(t, SeqAt(nums, i));
    }

    SeqDestroy(nums);
    RBTreeDestroy(t);
}

static void test_clear(void)
{
    RBTree *t = IntTreeNew_();
    for (int i = 0; i < 20000; i++)
    {
        RBTreePut(t, &i, &i);
    }

    int k = 5;

    assert_true(RBTreeGet(t, &k) != NULL);
    assert_int_equal(20000, RBTreeSize(t));

    RBTreeClear(t);

    assert_true(RBTreeGet(t, &k) == NULL);
    assert_int_equal(0, RBTreeSize(t));

    for (int i = 0; i < 20000; i++)
    {
        RBTreePut(t, &i, &i);
    }

    assert_true(RBTreeGet(t, &k) != NULL);
    assert_int_equal(20000, RBTreeSize(t));

    RBTreeDestroy(t);
}

static void test_equal(void)
{
    RBTree *a = IntTreeNew_();
    RBTree *b = IntTreeNew_();
    for (int i = 0; i < 20000; i++)
    {
        RBTreePut(a, &i, &i);
        RBTreePut(b, &i, &i);
    }

    assert_true(RBTreeEqual(a, b));

    RBTreeDestroy(a);
    RBTreeDestroy(b);
}

static void test_copy(void)
{
    RBTree *a = IntTreeNew_();
    for (int i = 0; i < 20000; i++)
    {
        RBTreePut(a, &i, &i);
    }

    RBTree *b = RBTreeCopy(a, NULL, NULL);

    assert_true(RBTreeEqual(a, b));

    RBTreeDestroy(a);
    RBTreeDestroy(b);
}


int main()
{
    const UnitTest tests[] =
    {
        unit_test(test_new_destroy),
        unit_test(test_put_overwrite),
        unit_test(test_put_remove),
        unit_test(test_put_remove_inorder),
        unit_test(test_iterate_empty),
        unit_test(test_iterate),
        unit_test(test_put_remove_random),
        unit_test(test_clear),
        unit_test(test_equal),
        unit_test(test_copy),
    };

    PRINT_TEST_BANNER();
    return run_tests(tests);
}
