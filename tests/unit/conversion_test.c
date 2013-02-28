#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "conversion.h"

static void str_to_service_policy(void **state)
{
    assert_int_equal(ServicePolicyFromString("start"), SERVICE_POLICY_START);
    assert_int_equal(ServicePolicyFromString("restart"), SERVICE_POLICY_RESTART);
}

int main()
{
    const UnitTest tests[] = {
        unit_test(str_to_service_policy)
    };

    return run_tests(tests);
}
