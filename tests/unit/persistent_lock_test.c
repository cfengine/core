#include "cf3.defs.h"

#include "transaction.h"

#include <setjmp.h>
#include <cmockery.h>

static void tests_setup(void)
{
    snprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/persistent_lock_test.XXXXXX");
    mkdtemp(CFWORKDIR);

    char buf[CF_BUFSIZE];
    snprintf(buf, CF_BUFSIZE, "%s/state", CFWORKDIR);
    mkdir(buf, 0755);
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

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
    tests_setup();

    const UnitTest tests[] =
      {
        unit_test(test_lock_acquire_by_id),
        unit_test(test_lock_invalidate),
      };
    
    int ret = run_tests(tests);

    tests_teardown();

    return ret;
}
