#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "conversion.h"

static void test_null_agent_type_to_string(void **state)
{
    assert_int_equal(Agent2Type(NULL), cf_noagent);
}

static void test_invalid_agent_type_to_string(void **state)
{
    assert_int_equal(Agent2Type("InvalidAgentType"), cf_noagent);
}

int main()
{
    const UnitTest tests[] = {
        unit_test(test_null_agent_type_to_string),
        unit_test(test_invalid_agent_type_to_string)
    };

    return run_tests(tests);
}

