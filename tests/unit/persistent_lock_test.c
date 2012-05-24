#include "cf3.defs.h"

#include "transaction.h"

#include <setjmp.h>
#include <cmockery.h>

static void test_lock_acquire_by_id(void **state)
{
    bool result;
    char *lock_id = "testlock1";
    
    result = AcquireLockByID(lock_id, 1);
    assert_true(result);
    
    result = AcquireLockByID(lock_id, 1);
    assert_false(result);

    sleep(1);
    
    result = AcquireLockByID(lock_id, 0);
    assert_true(result);
}

static void test_lock_invalidate(void **state)
{
    bool result;
    time_t lock_time;
    char *lock_id = "testlock2";
    
    result = AcquireLockByID(lock_id, 1);
    assert_true(result);

    lock_time = FindLockTime(lock_id);
    assert_true(lock_time > 0);
    
    result = InvalidateLockTime(lock_id);
    assert_true(result);

    lock_time = FindLockTime(lock_id);
    assert_int_equal(lock_time, 0);

    result = AcquireLockByID(lock_id, 1);
    assert_true(result);

    lock_time = FindLockTime(lock_id);
    assert_true(lock_time > 0);
}


int main()
{
    strlcpy(CFWORKDIR, "/tmp", sizeof(CFWORKDIR));
    cf_mkdir("/tmp/state", 0755);
    unlink("/tmp/state/cf_lock.tcdb");
    unlink("/tmp/state/cf_lock.qdbm");

    const UnitTest tests[] =
      {
        unit_test(test_lock_acquire_by_id),
        unit_test(test_lock_invalidate),
      };
    
    return run_tests(tests);
}
