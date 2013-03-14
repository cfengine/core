#include "test.h"

#include "platform.h"
#include "alloc.h"
#include "string_expressions.h"
#include "string_lib.h"

static char *ForbiddenVarRefEval(const char *varname, void *param)
{
    fail();
}

static char *IdentityVarRefEval(const char *varname, void *param)
{
    return xstrdup(varname);
}

static char *AppendAVarRefEval(const char *varname, void *param)
{
    return StringConcatenate(2, "a", varname);
}

static void CheckParse(const char *string_expression, const char *expected_output, VarRefEvaluator evaluator, void *param)
{
    StringParseResult res = ParseStringExpression(string_expression, 0, strlen(string_expression));
    assert_true(res.result);
    char *eval_result = EvalStringExpression(res.result, evaluator, param);
    assert_string_equal(expected_output, eval_result);
    free(eval_result);
    FreeStringExpression(res.result);
}

static void test_literal(void **state)
{
    CheckParse("hello", "hello", ForbiddenVarRefEval, NULL);
}

static void test_var_naked(void **state)
{
    CheckParse("$(foo)", "foo", IdentityVarRefEval, NULL);
}

static void test_var_naked_two_level(void **state)
{
    CheckParse("$($(foo))", "aafoo", AppendAVarRefEval, NULL);
}

static void test_var_one_level(void **state)
{
    CheckParse("$(foo)x$(bar)y$(baz)", "fooxbarybaz", IdentityVarRefEval, NULL);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_literal),
        unit_test(test_var_naked),
        unit_test(test_var_naked_two_level),
        unit_test(test_var_one_level),
    };

    return run_tests(tests);
}
