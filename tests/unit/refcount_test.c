#include "test.h"

#include "refcount.h"

// Simple initialization test
static void test_initRefCount(void)
{
    RefCount *refCount = NULL;
    RefCountNew(&refCount);
    assert_int_equal(0, refCount->user_count);
    assert_true(refCount->last == NULL);
    assert_true(refCount->users == NULL);
}

// Simple deletion test
static void test_destroyRefCount(void)
{
    RefCount *refCount = NULL;
    RefCountNew(&refCount);
    assert_int_equal(0, refCount->user_count);
    assert_true(refCount->last == NULL);
    assert_true(refCount->users == NULL);

    // Now we destroy the refcount.
    RefCountDestroy(&refCount);
    assert_true(refCount == NULL);

    // Try to destroy a NULL refCount
    RefCountDestroy(&refCount);
}

static void test_attachRefCount(void)
{
    int data1 = 0xdeadbeef;
    int data2 = 0xbad00bad;
    RefCount *refCount = NULL;

    // initialize the refcount
    RefCountNew(&refCount);
    assert_int_equal(0, refCount->user_count);
    assert_true(refCount->last == NULL);
    assert_true(refCount->users == NULL);

    // attach it to the first data
    assert_int_equal(1, RefCountAttach(refCount, &data1));
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // Attach the second data
    assert_int_equal(2, RefCountAttach(refCount, &data2));
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // Try to attach to a NULL refCount
    assert_int_equal(-1, RefCountAttach(NULL, &data2));
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // Try to attach NULL data
    assert_int_equal(-1, RefCountAttach(refCount, NULL));
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // Try to attach with both NULL
    assert_int_equal(-1, RefCountAttach(NULL, NULL));
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data2);
}

static void test_detachRefCount(void)
{
    int data1 = 0xdeadbeef;
    int data2 = 0xbad00bad;
    int data3 = 0x55aaaa55;
    int dataNotFound = 0xaa5555aa;
    RefCount *refCount = NULL;

    // initialize the refcount
    RefCountNew(&refCount);
    assert_int_equal(0, refCount->user_count);
    assert_true(refCount->last == NULL);
    assert_true(refCount->users == NULL);

    // attach it to the first data
    assert_int_equal(1, RefCountAttach(refCount, &data1));
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // Attach the second data
    assert_int_equal(2, RefCountAttach(refCount, &data2));
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // Detach the first data
    assert_int_equal(1, RefCountDetach(refCount, &data1));
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // Attach the third data
    assert_int_equal(2, RefCountAttach(refCount, &data3));
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data3);

    // Attach the first data
    assert_int_equal(3, RefCountAttach(refCount, &data1));
    // Check the result
    assert_int_equal(3, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // Detach dataNotFound
    assert_int_equal(-1, RefCountDetach(refCount, &dataNotFound));
    // Check the result
    assert_int_equal(3, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // Detach NULL data
    assert_int_equal(-1, RefCountDetach(refCount, NULL));
    // Check the result
    assert_int_equal(3, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // Detach NULL refCount
    assert_int_equal(-1, RefCountDetach(NULL, &data1));
    // Check the result
    assert_int_equal(3, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // Detach both NULL
    assert_int_equal(-1, RefCountDetach(NULL, NULL));
    // Check the result
    assert_int_equal(3, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // Detach the third data
    assert_int_equal(2, RefCountDetach(refCount, &data3));
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // Detach the first data
    assert_int_equal(1, RefCountDetach(refCount, &data1));
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // Detach the second data, this is a NOP
    assert_int_equal(0, RefCountDetach(refCount, &data2));
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data2);
}

static void test_isSharedRefCount(void)
{
    int data1 = 0xdeadbeef;
    int data2 = 0xbad00bad;
    RefCount *refCount = NULL;

    // initialize the refcount
    RefCountNew(&refCount);
    assert_int_equal(0, refCount->user_count);
    assert_true(refCount->last == NULL);
    assert_true(refCount->users == NULL);

    // isShared should return false
    assert_false(RefCountIsShared(refCount));

    // attach it to the first data
    assert_int_equal(1, RefCountAttach(refCount, &data1));
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // isShared should return false
    assert_false(RefCountIsShared(refCount));

    // Attach the second data
    assert_int_equal(2, RefCountAttach(refCount, &data2));
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // isShared should return true
    assert_true(RefCountIsShared(refCount));

    // Detach and try again
    assert_int_equal(1, RefCountDetach(refCount, &data1));
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // isShared should return false
    assert_false(RefCountIsShared(refCount));

    // Try isShared with a NULL refCount
    assert_false(RefCountIsShared(NULL));
}

static void test_isEqualRefCount(void)
{
    int data2 = 0xbad00bad;
    RefCount *refCount1 = NULL;
    RefCount *refCount2 = NULL;

    // initialize refcount1
    RefCountNew(&refCount1);
    assert_int_equal(0, refCount1->user_count);
    assert_true(refCount1->last == NULL);
    assert_true(refCount1->users == NULL);

    // initialize refcount2
    RefCountNew(&refCount2);
    assert_int_equal(0, refCount2->user_count);
    assert_true(refCount2->last == NULL);
    assert_true(refCount2->users == NULL);

    // isEqual should return true
    assert_true(RefCountIsEqual(refCount1, refCount2));

    // Add one to refcount1
    assert_int_equal(1, RefCountAttach(refCount1, &data2));

    // isEqual should return false
    assert_false(RefCountIsEqual(refCount1, refCount2));

    // Add the same to refcount2
    assert_int_equal(1, RefCountAttach(refCount2, &data2));

    // isEqual should return true
    assert_true(RefCountIsEqual(refCount1, refCount2));

    // Try one NULL
    assert_false(RefCountIsEqual(refCount1, NULL));
    assert_false(RefCountIsEqual(NULL, refCount2));

    // Both NULL
    assert_true(RefCountIsEqual(NULL, NULL));
}

int main()
{
    const UnitTest tests[] = {
        unit_test(test_initRefCount)
        , unit_test(test_destroyRefCount)
        , unit_test(test_attachRefCount)
        , unit_test(test_detachRefCount)
        , unit_test(test_isSharedRefCount)
        , unit_test(test_isEqualRefCount)
    };

    return run_tests(tests);
}

