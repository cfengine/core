#include <test.h>

#include <cf3.defs.h>
#include <matching.h>
#include <match_scope.h>
#include <eval_context.h>

static void test_full_text_match(void)
{
    EvalContext *ctx = EvalContextNew();
    assert_int_equal(FullTextMatch(ctx, "[a-z]*", "1234abcd6789"), 0);
    EvalContextDestroy(ctx);
}

static void test_full_text_match2(void)
{
    EvalContext *ctx = EvalContextNew();
    assert_int_not_equal(FullTextMatch(ctx, "[1-4]*[a-z]*.*", "1234abcd6789"), 0);
    EvalContextDestroy(ctx);
}

static void test_block_text_match(void)
{
    EvalContext *ctx = EvalContextNew();
    int start, end;

    assert_int_not_equal(BlockTextMatch(ctx, "#[^\n]*", "line 1:\nline2: # comment to end\nline 3: blablab", &start, &end),
                         0);

    assert_int_equal(start, 15);
    assert_int_equal(end, 31);
    EvalContextDestroy(ctx);
}

static void test_block_text_match2(void)
{
    EvalContext *ctx = EvalContextNew();
    int start, end;

    assert_int_not_equal(BlockTextMatch(ctx, "[a-z]+", "1234abcd6789", &start, &end), 0);
    assert_int_equal(start, 4);
    assert_int_equal(end, 8);
    EvalContextDestroy(ctx);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_full_text_match),
        unit_test(test_full_text_match2),
        unit_test(test_block_text_match),
        unit_test(test_block_text_match2),
    };

    return run_tests(tests);
}
