#include <test.h>

#include <policy.h>
#include <misc_lib.h>                                          /* xsnprintf */
#include <parser.h>


static bool TestParsePolicy(const char *filename)
{
    char path[PATH_MAX];
    xsnprintf(path, sizeof(path), "%s/%s", TESTDATADIR, filename);
    Policy *p = ParserParseFile(AGENT_TYPE_COMMON, path, PARSER_WARNING_ALL, PARSER_WARNING_ALL);
    bool res = (p != NULL);
    PolicyDestroy(p);
    return res;
}

void test_benchmark(void)
{
    assert_true(TestParsePolicy("benchmark.cf"));
}

void test_no_bundle_or_body_keyword(void)
{
    assert_false(TestParsePolicy("no_bundle_or_body_keyword.cf"));
}

void test_bundle_invalid_type(void)
{
    assert_false(TestParsePolicy("bundle_invalid_type.cf"));
}

void test_body_invalid_type(void)
{
    assert_false(TestParsePolicy("body_invalid_type.cf"));
}

void test_constraint_ifvarclass_invalid(void)
{
    assert_false(TestParsePolicy("constraint_ifvarclass_invalid.cf"));
}

void test_bundle_args_invalid_type(void)
{
    assert_false(TestParsePolicy("bundle_args_invalid_type.cf"));
}

void test_bundle_args_forgot_cp(void)
{
    assert_false(TestParsePolicy("bundle_args_forgot_cp.cf"));
}

void test_bundle_body_forgot_ob(void)
{
    assert_false(TestParsePolicy("bundle_body_forgot_ob.cf"));
}

void test_bundle_invalid_promise_type(void)
{
    assert_false(TestParsePolicy("bundle_invalid_promise_type.cf"));
}

void test_bundle_body_wrong_promise_type_token(void)
{
    assert_false(TestParsePolicy("bundle_body_wrong_promise_type_token.cf"));
}

void test_bundle_body_wrong_statement(void)
{
    assert_false(TestParsePolicy("bundle_body_wrong_statement.cf"));
}

void test_bundle_body_forgot_semicolon(void)
{
    assert_false(TestParsePolicy("bundle_body_forgot_semicolon.cf"));
}

void test_bundle_body_promiser_statement_contains_colon(void)
{
    assert_false(TestParsePolicy("bundle_body_promiser_statement_contains_colon.cf"));
}

void test_bundle_body_promiser_statement_missing_assign(void)
{
    assert_false(TestParsePolicy("bundle_body_promiser_statement_missing_assign.cf"));
}

void test_bundle_body_promisee_missing_arrow(void)
{
    assert_false(TestParsePolicy("bundle_body_promise_missing_arrow.cf"));
}

void test_bundle_body_promiser_wrong_constraint_token(void)
{
    assert_false(TestParsePolicy("bundle_body_promiser_wrong_constraint_token.cf"));
}

void test_bundle_body_promiser_unknown_constraint_id(void)
{
    assert_false(TestParsePolicy("bundle_body_promiser_unknown_constraint_id.cf"));
}

void test_body_edit_line_common_constraints(void)
{
    assert_true(TestParsePolicy("body_edit_line_common_constraints.cf"));
}

void test_body_edit_xml_common_constraints(void)
{
    assert_true(TestParsePolicy("body_edit_xml_common_constraints.cf"));
}

void test_promise_promiser_nonscalar(void)
{
    assert_false(TestParsePolicy("promise_promiser_nonscalar.cf"));
}

void test_bundle_body_promiser_forgot_colon(void)
{
    assert_false(TestParsePolicy("bundle_body_promiser_forgot_colon.cf"));
}

void test_bundle_body_promisee_no_colon_allowed(void)
{
    assert_false(TestParsePolicy("bundle_body_promisee_no_colon_allowed.cf"));
}

void test_bundle_body_forget_cb_eof(void)
{
    assert_false(TestParsePolicy("bundle_body_forget_cb_eof.cf"));
}

void test_bundle_body_forget_cb_body(void)
{
    assert_false(TestParsePolicy("bundle_body_forget_cb_body.cf"));
}

void test_bundle_body_forget_cb_bundle(void)
{
    assert_false(TestParsePolicy("bundle_body_forget_cb_bundle.cf"));
}

void test_body_selection_wrong_token(void)
{
    assert_false(TestParsePolicy("body_selection_wrong_token.cf"));
}

void test_body_selection_forgot_semicolon(void)
{
    assert_false(TestParsePolicy("body_selection_forgot_semicolon.cf"));
}

void test_body_selection_unknown_selection_id(void)
{
    assert_false(TestParsePolicy("body_selection_unknown_selection_id.cf"));
}

void test_body_body_forget_cb_eof(void)
{
    assert_false(TestParsePolicy("body_body_forget_cb_eof.cf"));
}

void test_body_body_forget_cb_body(void)
{
    assert_false(TestParsePolicy("body_body_forget_cb_body.cf"));
}

void test_body_body_forget_cb_bundle(void)
{
    assert_false(TestParsePolicy("body_body_forget_cb_bundle.cf"));
}

void test_rval_list_forgot_colon(void)
{
    assert_false(TestParsePolicy("rval_list_forgot_colon.cf"));
}

void test_rval_list_wrong_input_type(void)
{
    assert_false(TestParsePolicy("rval_list_wrong_input_type.cf"));
}

void test_rval_function_forgot_colon(void)
{
    assert_false(TestParsePolicy("rval_function_forgot_colon.cf"));
}

void test_rval_function_wrong_input_type(void)
{
    assert_false(TestParsePolicy("rval_function_wrong_input_type.cf"));
}

void test_rval_wrong_input_type(void)
{
    assert_false(TestParsePolicy("rval_wrong_input_type.cf"));
}

void test_rval_list_forgot_cb_semicolon(void)
{
    assert_false(TestParsePolicy("rval_list_forgot_cb_semicolon.cf"));
}

void test_rval_list_forgot_cb_colon(void)
{
    assert_false(TestParsePolicy("rval_list_forgot_cb_colon.cf"));
}

void test_rval_function_forgot_cp_semicolon(void)
{
    assert_false(TestParsePolicy("rval_function_forgot_cp_semicolon.cf"));
}

void test_rval_function_forgot_cp_colon(void)
{
    assert_false(TestParsePolicy("rval_function_forgot_cp_colon.cf"));
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
        unit_test(test_bundle_body_promiser_wrong_constraint_token),
        unit_test(test_bundle_body_promiser_unknown_constraint_id),
        unit_test(test_bundle_body_promiser_forgot_colon),
        unit_test(test_bundle_body_promisee_no_colon_allowed),
        unit_test(test_bundle_body_forget_cb_eof),
        unit_test(test_bundle_body_forget_cb_body),
        unit_test(test_bundle_body_forget_cb_bundle),
        unit_test(test_body_edit_line_common_constraints),
        unit_test(test_body_edit_xml_common_constraints),
        unit_test(test_bundle_body_promisee_missing_arrow),

        unit_test(test_body_invalid_type),
        unit_test(test_body_selection_wrong_token),
        unit_test(test_body_selection_forgot_semicolon),
        unit_test(test_body_selection_unknown_selection_id),
        unit_test(test_body_body_forget_cb_eof),
        unit_test(test_body_body_forget_cb_body),
        unit_test(test_body_body_forget_cb_bundle),

        unit_test(test_promise_promiser_nonscalar),

        unit_test(test_constraint_ifvarclass_invalid),

        unit_test(test_rval_list_forgot_colon),
        unit_test(test_rval_list_wrong_input_type),
        unit_test(test_rval_list_forgot_cb_semicolon),
        unit_test(test_rval_list_forgot_cb_colon),
        unit_test(test_rval_function_forgot_colon),
        unit_test(test_rval_function_wrong_input_type),
        unit_test(test_rval_function_forgot_cp_semicolon),
        unit_test(test_rval_function_forgot_cp_colon),
        unit_test(test_rval_wrong_input_type),

        unit_test(test_no_bundle_or_body_keyword)

    };

    return run_tests(tests);
}
