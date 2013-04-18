#include "test.h"

#include "policy.h"
#include "parser.h"

static Policy *LoadPolicy(const char *filename)
{
    char path[1024];
    sprintf(path, "%s/%s", TESTDATADIR, filename);

    return ParserParseFile(path);
}

void test_benchmark(void)
{
    Policy *p = LoadPolicy("benchmark.cf");
    assert_true(p);
    PolicyDestroy(p);
}

void test_bundle_invalid_type(void)
{
    assert_false(LoadPolicy("bundle_invalid_type.cf"));
}

void test_body_invalid_type(void)
{
    assert_false(LoadPolicy("body_invalid_type.cf"));
}

void test_constraint_ifvarclass_invalid(void)
{
    Policy *p = LoadPolicy("constraint_ifvarclass_invalid.cf");
    assert_false(p);
}

void test_bundle_args_invalid_type(void)
{
    assert_false(LoadPolicy("bundle_args_invalid_type.cf"));
}

void test_bundle_args_forgot_cp(void)
{
    assert_false(LoadPolicy("bundle_args_forgot_cp.cf"));
}

void test_bundle_body_forgot_ob(void)
{
    assert_false(LoadPolicy("bundle_body_forgot_ob.cf"));
}

void test_bundle_invalid_promise_type(void)
{
    assert_false(LoadPolicy("bundle_invalid_promise_type.cf"));
}

void test_bundle_body_wrong_promise_type_token(void)
{
    assert_false(LoadPolicy("bundle_body_wrong_promise_type_token.cf"));
}

void test_bundle_body_wrong_statement(void)
{
    assert_false(LoadPolicy("bundle_body_wrong_statement.cf"));
}

void test_bundle_body_forgot_semicolon(void)
{
    assert_false(LoadPolicy("bundle_body_forgot_semicolon.cf"));
}

void test_bundle_body_promiser_statement_contains_colon(void)
{
    assert_false(LoadPolicy("bundle_body_promiser_statement_contains_colon.cf"));
}

void test_bundle_body_promiser_statement_missing_assign(void)
{
    assert_false(LoadPolicy("bundle_body_promiser_statement_missing_assign.cf"));
}

void test_bundle_body_promisee_missing_arrow(void)
{
    assert_false(LoadPolicy("bundle_body_promise_missing_arrow.cf"));
}

void test_bundle_body_promiser_wrong_constraint_token(void)
{
    assert_false(LoadPolicy("bundle_body_promiser_wrong_constraint_token.cf"));
}

void test_bundle_body_promiser_unknown_constraint_id(void)
{
    assert_false(LoadPolicy("bundle_body_promiser_unknown_constraint_id.cf"));
}

void test_body_edit_line_common_constraints(void)
{
    assert_true(LoadPolicy("body_edit_line_common_constraints.cf"));
}

void test_body_edit_xml_common_constraints(void)
{
    assert_true(LoadPolicy("body_edit_xml_common_constraints.cf"));
}

void test_promise_promiser_nonscalar(void)
{
    assert_false(LoadPolicy("promise_promiser_nonscalar.cf"));
}

void test_bundle_body_promiser_forgot_colon(void)
{
    assert_false(LoadPolicy("bundle_body_promiser_forgot_colon.cf"));
}

void test_bundle_body_promisee_no_colon_allowed(void)
{
    assert_false(LoadPolicy("bundle_body_promisee_no_colon_allowed.cf"));
}

void test_bundle_body_forget_cb_eof(void)
{
    assert_false(LoadPolicy("bundle_body_forget_cb_eof.cf"));
}

void test_bundle_body_forget_cb_body(void)
{
    assert_false(LoadPolicy("bundle_body_forget_cb_body.cf"));
}

void test_bundle_body_forget_cb_bundle(void)
{
    assert_false(LoadPolicy("bundle_body_forget_cb_bundle.cf"));
}

void test_body_selection_wrong_token(void)
{
    assert_false(LoadPolicy("body_selection_wrong_token.cf"));
}

void test_body_selection_forgot_semicolon(void)
{
    assert_false(LoadPolicy("body_selection_forgot_semicolon.cf"));
}

void test_body_selection_unknown_selection_id(void)
{
    assert_false(LoadPolicy("body_selection_unknown_selection_id.cf"));
}

void test_body_body_forget_cb_eof(void)
{
    assert_false(LoadPolicy("body_body_forget_cb_eof.cf"));
}

void test_body_body_forget_cb_body(void)
{
    assert_false(LoadPolicy("body_body_forget_cb_body.cf"));
}

void test_body_body_forget_cb_bundle(void)
{
    assert_false(LoadPolicy("body_body_forget_cb_bundle.cf"));
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
        unit_test(test_bundle_body_promisee_missing_arrow),
        unit_test(test_bundle_body_promiser_wrong_constraint_token),
        unit_test(test_bundle_body_promiser_unknown_constraint_id),
        unit_test(test_bundle_body_promiser_forgot_colon),
        unit_test(test_bundle_body_promisee_no_colon_allowed),
        unit_test(test_bundle_body_forget_cb_eof),
        unit_test(test_bundle_body_forget_cb_body),
        unit_test(test_bundle_body_forget_cb_bundle),
        unit_test(test_body_edit_line_common_constraints),
        unit_test(test_body_edit_xml_common_constraints),

        unit_test(test_body_invalid_type),
        unit_test(test_body_selection_wrong_token),
        unit_test(test_body_selection_forgot_semicolon),
        unit_test(test_body_selection_unknown_selection_id),
        unit_test(test_body_body_forget_cb_eof),
        unit_test(test_body_body_forget_cb_body),
        unit_test(test_body_body_forget_cb_bundle),

        unit_test(test_promise_promiser_nonscalar),

        unit_test(test_constraint_ifvarclass_invalid)
    };

    return run_tests(tests);
}
