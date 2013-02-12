#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "conversion.h"

static void str_to_service_policy(void **state)
{
    assert_int_equal(Str2ServicePolicy("start"), cfsrv_start);
    assert_int_equal(Str2ServicePolicy("restart"), cfsrv_restart);
}

int main()
{
    const UnitTest tests[] = {
        unit_test(str_to_service_policy)
    };

    return run_tests(tests);
}
