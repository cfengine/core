#include <test.h>

#include <cmockery.h>
#include <conversion.h>

static void test_str_to_service_policy(void)
{
    assert_int_equal(ServicePolicyFromString("start"), SERVICE_POLICY_START);
    assert_int_equal(ServicePolicyFromString("restart"), SERVICE_POLICY_RESTART);
}

static void test_double_from_string(void)
{
    {
        double val;
        assert_true(DoubleFromString("1.2k", &val));
        assert_double_close(1200.0, val);
    }

    {
        double val;
        assert_false(DoubleFromString("abc", &val));
    }
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_str_to_service_policy),
        unit_test(test_double_from_string),
    };

    return run_tests(tests);
}
