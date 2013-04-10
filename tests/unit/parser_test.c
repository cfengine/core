#include "test.h"

#include "policy.h"
#include "parser.h"

static Policy *LoadPolicy(const char *filename)
{
    char path[1024];
    sprintf(path, "%s/%s", TESTDATADIR, filename);

    return ParserParseFile(path);
}

void test_benchmark(void **state)
{
    Policy *p = LoadPolicy("benchmark.cf");
    assert_true(p);
    PolicyDestroy(p);
}

void test_bundle_invalid_type(void **state)
{
    assert_false(LoadPolicy("bundle_invalid_type.cf"));
}

void test_body_invalid_type(void **state)
{
    assert_false(LoadPolicy("body_invalid_type.cf"));
}

void test_constraint_comment_nonscalar(void **state)
{
    Policy *p = LoadPolicy("constraint_comment_nonscalar.cf");
    assert_false(p);
}

void test_constraint_ifvarclass_invalid(void **state)
{
    Policy *p = LoadPolicy("constraint_ifvarclass_invalid.cf");
    assert_false(p);
}

void test_bundle_args_invalid_type(void **state)
{
    assert_false(LoadPolicy("bundle_args_invalid_type.cf"));
}

void test_bundle_args_forgot_cp(void **state)
{
    assert_false(LoadPolicy("bundle_args_forgot_cp.cf"));
}

void bundle_body_forgot_ob(void **state)
{
    assert_false(LoadPolicy("bundle_body_forgot_ob.cf"));
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_benchmark),
        unit_test(test_bundle_invalid_type),

        unit_test(test_body_invalid_type),
        unit_test(test_bundle_args_invalid_type),
        unit_test(test_bundle_args_forgot_cp),
        unit_test(test_constraint_ifvarclass_invalid)
        unit_test(bundle_body_forgot_ob),
    };

    return run_tests(tests);
}
