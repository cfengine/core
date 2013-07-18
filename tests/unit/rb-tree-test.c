#include "test.h"
#include "rb-tree.h"

#include "alloc.h"
#include "sequence.h"

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

static void test_iterate(void)
{
    Seq *nums = SeqNew(20, free);
    srand(0);
    for (int i = 0; i < 20; i++)
    {
        int k = rand() % 1000;
        SeqAppend(nums, xmemdup(&k, sizeof(int)));
    }

    RBTree *t = IntTreeNew_();
    for (size_t i = 0; i < SeqLength(nums); i++)
    {
        RBTreePut(t, SeqAt(nums, i), SeqAt(nums, i));
    }

    RBTreeIterator *it = RBTreeIteratorNew(t);
    int last = -1;
    void *_r = NULL;
    int i = 0;
    while (RBTreeIteratorNext(it, &_r, NULL))
    {
        int *r = _r;
        assert_true(*r >= last);
        last = *r;
        i++;
    }
    RBTreeIteratorDestroy(it);

    assert_int_equal(i, 20);

    SeqDestroy(nums);
    RBTreeDestroy(t);
}

static void test_put_remove_random(void)
{
    Seq *nums = SeqNew(20, free);
    srand(0);
    for (int i = 0; i < 20; i++)
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


int main()
{
    const UnitTest tests[] =
    {
        unit_test(test_new_destroy),
        unit_test(test_put_overwrite),
        unit_test(test_put_remove),
        unit_test(test_put_remove_inorder),
        unit_test(test_iterate),
        unit_test(test_put_remove_random)
    };

    PRINT_TEST_BANNER();
    return run_tests(tests);
}
