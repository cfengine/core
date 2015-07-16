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

static void test_CommandArg0_bound(void)
{
    char dst[128];
    size_t zret;

    zret = CommandArg0_bound(dst, "", sizeof(dst));
    assert_string_equal(dst, "");
    assert_int_equal(zret, 0);
    zret = CommandArg0_bound(dst, " ", sizeof(dst));
    assert_string_equal(dst, "");
    assert_int_equal(zret, 0);
    zret = CommandArg0_bound(dst, " blah", sizeof(dst));
    assert_string_equal(dst, "");
    assert_int_equal(zret, 0);
    zret = CommandArg0_bound(dst, "blah", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);
    zret = CommandArg0_bound(dst, "blah blue", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);

    zret = CommandArg0_bound(dst, "\"\"", sizeof(dst));
    assert_string_equal(dst, "");
    assert_int_equal(zret, 0);
    zret = CommandArg0_bound(dst, "\"blah\"", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);
    zret = CommandArg0_bound(dst, "\"blah", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);
    zret = CommandArg0_bound(dst, "\"blah blue", sizeof(dst));
    assert_string_equal(dst, "blah blue");
    assert_int_equal(zret, 9);

    zret = CommandArg0_bound(dst, "\"\" blus", sizeof(dst));
    assert_string_equal(dst, "");
    assert_int_equal(zret, 0);
    zret = CommandArg0_bound(dst, "\"blah\" blue", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);
    zret = CommandArg0_bound(dst, "\"blah\"blue", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);

    zret = CommandArg0_bound(dst, "blah ", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);

    zret = CommandArg0_bound(dst, "\" \"", sizeof(dst));
    assert_string_equal(dst, " ");
    assert_int_equal(zret, 1);
    zret = CommandArg0_bound(dst, "\" \" ", sizeof(dst));
    assert_string_equal(dst, " ");
    assert_int_equal(zret, 1);

    zret = CommandArg0_bound(dst, "blah \"blue\"", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);

    zret = CommandArg0_bound(dst, "blah\"blue\"", sizeof(dst));
    assert_string_equal(dst, "blah\"blue\"");
    assert_int_equal(zret, 10);

    zret = CommandArg0_bound(dst, "blah\"blue", sizeof(dst));
    assert_string_equal(dst, "blah\"blue");
    assert_int_equal(zret, 9);

    /* TEST OVERFLOW */

    zret = CommandArg0_bound(dst, "", 0);
    assert_int_equal(zret, (size_t) -1);
    zret = CommandArg0_bound(dst, "blah", 0);
    assert_int_equal(zret, (size_t) -1);
    zret = CommandArg0_bound(dst, " ", 0);
    assert_int_equal(zret, (size_t) -1);
    zret = CommandArg0_bound(dst, "\"blah\"", 0);
    assert_int_equal(zret, (size_t) -1);

    zret = CommandArg0_bound(dst, "blah", 1);
    assert_int_equal(zret, (size_t) -1);
    zret = CommandArg0_bound(dst, "\"blah\"", 1);
    assert_int_equal(zret, (size_t) -1);

    zret = CommandArg0_bound(dst, "b", 1);
    assert_int_equal(zret, (size_t) -1);
    zret = CommandArg0_bound(dst, "\"b\"", 1);
    assert_int_equal(zret, (size_t) -1);

    zret = CommandArg0_bound(dst, "", 1);
    assert_int_equal(zret, 0);                         /* empty string fits */
    zret = CommandArg0_bound(dst, " ", 1);
    assert_int_equal(zret, 0);                         /* empty string fits */
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_str_to_service_policy),
        unit_test(test_double_from_string),
        unit_test(test_CommandArg0_bound),
    };

    return run_tests(tests);
}
