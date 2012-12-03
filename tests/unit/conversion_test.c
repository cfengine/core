#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "conversion.h"

static void test_null_agent_type_to_string(void **state)
{
    assert_int_equal(Agent2Type(NULL), AGENT_TYPE_NOAGENT);
}

static void test_invalid_agent_type_to_string(void **state)
{
    assert_int_equal(Agent2Type("InvalidAgentType"), AGENT_TYPE_NOAGENT);
}

static void str_to_service_policy(void **state)
{
    assert_int_equal(Str2ServicePolicy("start"), cfsrv_start);
    assert_int_equal(Str2ServicePolicy("restart"), cfsrv_restart);
}

int main()
{
    const UnitTest tests[] = {
        unit_test(test_null_agent_type_to_string),
        unit_test(test_invalid_agent_type_to_string),
        unit_test(str_to_service_policy)
    };

    return run_tests(tests);
}

