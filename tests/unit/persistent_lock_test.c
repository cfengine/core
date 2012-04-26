#include "cf3.defs.h"
#include "cf3.extern.h"

#include "error.h"
#include "transaction.h"

#include <setjmp.h>
#include <cmockery.h>

static void test_lock_acquire_by_id(void **state)
{
    cfapi_errid errid;
    char *lock_id = "testlock";
    
    errid = AcquireLockByID(lock_id, 1);
    assert_int_equal(errid, CFERRID_SUCCESS);
    
    errid = AcquireLockByID(lock_id, 1);
    assert_int_equal(errid, CFERRID_LOCK_NOT_ACQUIRED);

    sleep(1);
    
    errid = AcquireLockByID(lock_id, 0);
    assert_int_equal(errid, CFERRID_SUCCESS);
}


int main()
{
    strlcpy(CFWORKDIR, "/tmp", sizeof(CFWORKDIR));
    mkdir("/tmp/state", 0755);
    unlink("/tmp/state/cf_lock.tcdb");
    unlink("/tmp/state/cf_lock.qdbm");

    const UnitTest tests[] =
      {
        unit_test(test_lock_acquire_by_id),
      };
    
    return run_tests(tests);
}
