#include <test.h>

#include <platform.h>
#include <alloc.h>
#include <string_expressions.h>
#include <string_lib.h>

static char *ForbiddenVarRefEval(ARG_UNUSED const char *varname,
                                 ARG_UNUSED VarRefType type,
                                 ARG_UNUSED void *param)
{
    fail();
}

static char *IdentityVarRefEval(const char *varname,
                                ARG_UNUSED VarRefType type,
                                ARG_UNUSED void *param)
{
    return xstrdup(varname);
}

static char *AppendAVarRefEval(const char *varname,
                               ARG_UNUSED VarRefType type,
                               ARG_UNUSED void *param)
{
    return StringConcatenate(2, "a", varname);
}

static char *DiscriminateVarTypesVarRefEval(const char *varname, VarRefType type,
                                            ARG_UNUSED void *param)
{
    if (type == VAR_REF_TYPE_SCALAR)
    {
        return StringConcatenate(3, "cozy(", varname, ")");
    }
    else
    {
        return StringConcatenate(3, "ugly{", varname, "}");
    }
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

static void test_literal(void)
{
    CheckParse("hello", "hello", ForbiddenVarRefEval, NULL);
}

static void test_var_naked(void)
{
    CheckParse("$(foo)", "foo", IdentityVarRefEval, NULL);
}

static void test_var_naked_two_level(void)
{
    CheckParse("$($(foo))", "aafoo", AppendAVarRefEval, NULL);
}

static void test_var_one_level(void)
{
    CheckParse("$(foo)x$(bar)y$(baz)", "fooxbarybaz", IdentityVarRefEval, NULL);
}

static void test_different_var_types(void)
{
    CheckParse("@{a$(b@(c)${d})@(e)}", "ugly{acozy(bugly{c}cozy(d))ugly{e}}", DiscriminateVarTypesVarRefEval, NULL);
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
        unit_test(test_different_var_types),
    };

    return run_tests(tests);
}
