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

void test_bundle_body_forgot_ob(void **state)
{
    assert_false(LoadPolicy("bundle_body_forgot_ob.cf"));
}

void test_bundle_invalid_promise_type(void **state)
{
    /* 
     * FIXME function to test this is not yet implemented
    */
    assert_true(LoadPolicy("bundle_invalid_promise_type.cf"));
}

void test_bundle_body_wrong_promise_type_token(void **state)
{
    assert_false(LoadPolicy("bundle_body_wrong_promise_type_token.cf"));
}

void test_bundle_body_wrong_statement(void **state)
{
    assert_false(LoadPolicy("bundle_body_wrong_statement.cf"));
}

void test_bundle_body_forgot_semicolon(void **state)
{
    assert_false(LoadPolicy("bundle_body_forgot_semicolon.cf"));
}

void test_bundle_body_promiser_statement_contains_colon(void **state)
{
    assert_false(LoadPolicy("bundle_body_promiser_statement_contains_colon.cf"));
}

void test_bundle_body_promiser_statement_missing_assign(void **state)
{
    assert_false(LoadPolicy("bundle_body_promiser_statement_missing_assign.cf"));
}

void test_bundle_body_promise_missing_arrow(void **state)
{
    assert_false(LoadPolicy("bundle_body_promise_missing_arrow.cf"));
}

void test_bundle_body_promiser_unknown_constraint_id(void **state)
{
    /* 
     * FIXME function to test this is not yet implemented
    */
    assert_true(LoadPolicy("bundle_body_promiser_unknown_constraint_id.cf"));
}

void test_promise_promiser_nonscalar(void **state)
{
    assert_false(LoadPolicy("promise_promiser_nonscalar.cf"));
}

void test_bundle_body_promiser_forgot_colon(void **state)
{
    assert_false(LoadPolicy("bundle_body_promiser_forgot_colon.cf"));
}

void test_bundle_body_promisee_no_colon_allowed(void **state)
{
    assert_false(LoadPolicy("bundle_body_promisee_no_colon_allowed.cf"));
}

void test_body_selection_wrong_token(void **state)
{
    assert_false(LoadPolicy("body_selection_wrong_token.cf"));
}

void test_body_selection_forgot_semicolon(void **state)
{
    assert_false(LoadPolicy("body_selection_forgot_semicolon.cf"));
}


void test_body_selection_unknown_selection_id(void **state)
{
    /* 
     * FIXME function to test this is not yet implemented
    */
    assert_true(LoadPolicy("body_selection_unknown_selection_id.cf"));
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_benchmark),

        unit_test(test_bundle_invalid_type),
        unit_test(test_bundle_args_invalid_type),
        unit_test(test_bundle_args_forgot_cp),
        unit_test(test_bundle_body_forgot_ob),
        unit_test(test_bundle_invalid_promise_type),
        unit_test(test_bundle_body_wrong_promise_type_token),
        unit_test(test_bundle_body_wrong_statement),
        unit_test(test_bundle_body_forgot_semicolon),
        unit_test(test_bundle_body_promiser_statement_contains_colon),
        unit_test(test_bundle_body_promiser_statement_missing_assign),
        unit_test(test_bundle_body_promise_missing_arrow),
        unit_test(test_bundle_body_promiser_unknown_constraint_id),
        unit_test(test_bundle_body_promiser_forgot_colon),
        unit_test(test_bundle_body_promisee_no_colon_allowed),

        unit_test(test_body_invalid_type),
        unit_test(test_body_selection_wrong_token),
        unit_test(test_body_selection_forgot_semicolon),
        unit_test(test_body_selection_unknown_selection_id),

        unit_test(test_promise_promiser_nonscalar),

        unit_test(test_constraint_ifvarclass_invalid)
    };

    return run_tests(tests);
}
