#include <test.h>

#include <refcount.h>

// Simple initialization test
static void test_init_destroy_RefCount(void)
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

static void test_attach_detach_RefCount(void)
{
    /*
     * This test does not check for NULL pointers, otherwise asserts will
     * be triggered. Neither does it check for non-existent owners.
     */
    int data1 = 0xdeadbeef;
    int data2 = 0xbad00bad;
    int data3 = 0x55aaaa55;
    RefCount *refCount = NULL;

    // initialize the refcount
    RefCountNew(&refCount);
    assert_int_equal(0, refCount->user_count);
    assert_true(refCount->last == NULL);
    assert_true(refCount->users == NULL);

    // attach it to the first data
    RefCountAttach(refCount, &data1);
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // Attach the second data
    RefCountAttach(refCount, &data2);
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // Detach the first data
    RefCountDetach(refCount, &data1);
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // Attach the third data
    RefCountAttach(refCount, &data3);
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data3);

    // Attach the first data
    RefCountAttach(refCount, &data1);
    // Check the result
    assert_int_equal(3, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // Detach the third data
    RefCountDetach(refCount, &data3);
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // Detach the first data
    RefCountDetach(refCount, &data1);
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data2);

    /*
     * We cannot detach the last element because that will assert.
     * Whenever there is only one element the only thing is to destroy
     * the refcount.
     */

    // Destroy the refcount
    RefCountDestroy(&refCount);
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
    RefCountAttach(refCount, &data1);
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data1);

    // isShared should return false
    assert_false(RefCountIsShared(refCount));

    // Attach the second data
    RefCountAttach(refCount, &data2);
    // Check the result
    assert_int_equal(2, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous != NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // isShared should return true
    assert_true(RefCountIsShared(refCount));

    // Detach and try again
    RefCountDetach(refCount, &data1);
    // Check the result
    assert_int_equal(1, refCount->user_count);
    assert_true(refCount->last->next == NULL);
    assert_true(refCount->last->previous == NULL);
    assert_true(refCount->last->user == (void *)&data2);

    // isShared should return false
    assert_false(RefCountIsShared(refCount));

    // Try isShared with a NULL refCount
    assert_false(RefCountIsShared(NULL));

    // Destroy the refcount
    RefCountDestroy(&refCount);
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

    // initialize refcount2 as a copy of refcount1
    refCount2 = refCount1;

    // isEqual should return true
    assert_true(RefCountIsEqual(refCount1, refCount2));

    /* Initialize refcount2 on its own */
    RefCountNew(&refCount2);
    assert_int_equal(0, refCount2->user_count);
    assert_true(refCount2->last == NULL);
    assert_true(refCount2->users == NULL);

    // isEqual should return false
    assert_false(RefCountIsEqual(refCount1, refCount2));

    // Add one to refcount1
    RefCountAttach(refCount1, &data2);

    // isEqual should return false
    assert_false(RefCountIsEqual(refCount1, refCount2));

    // Add the same to refcount2
    RefCountAttach(refCount2, &data2);

    // isEqual should return false
    assert_false(RefCountIsEqual(refCount1, refCount2));

    // Try one NULL
    assert_false(RefCountIsEqual(refCount1, NULL));
    assert_false(RefCountIsEqual(NULL, refCount2));

    // Both NULL
    assert_false(RefCountIsEqual(NULL, NULL));

    // Destroy both refcounts
    RefCountDestroy(&refCount1);
    RefCountDestroy(&refCount2);
}

int main()
{
    const UnitTest tests[] = {
        unit_test(test_init_destroy_RefCount)
        , unit_test(test_attach_detach_RefCount)
        , unit_test(test_isSharedRefCount)
        , unit_test(test_isEqualRefCount)
    };

    return run_tests(tests);
}

