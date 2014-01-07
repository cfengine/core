#include <test.h>

#include <item_lib.h>
#include <eval_context.h>

/* FIXME: MatchRegion is now internal function of cf-agent */
#if 0
static void test_match_region(void)
{
    EvalContext *ctx =EvalContextNew();
    Item *items = NULL;
    Item *begin, *end;

    PrependItem(&items, "third", NULL);
    assert_true(MatchRegion(ctx, "third", items, NULL, false));

    end = items;
    PrependItem(&items, "second", NULL);
    PrependItem(&items, "first", NULL);
    begin = items;

    assert_true(MatchRegion(ctx, "first", begin, end, false));
    assert_false(MatchRegion(ctx, "second", begin, end, false));
    assert_false(MatchRegion(ctx, "third", begin, end, false));

    assert_true(MatchRegion(ctx, "first\nsecond", begin, end, false));
    assert_false(MatchRegion(ctx, "first\nthird", begin, end, false));
    assert_false(MatchRegion(ctx, "second\nthird", begin, end, false));

    assert_false(MatchRegion(ctx, "first\nsecond\nthird", begin, end, false));

    assert_true(MatchRegion(ctx, "first", begin, NULL, false));
    assert_false(MatchRegion(ctx, "second", begin, NULL, false));
    assert_false(MatchRegion(ctx, "third", begin, NULL, false));

    assert_true(MatchRegion(ctx, "first\nsecond", begin, NULL, false));
    assert_false(MatchRegion(ctx, "first\nthird", begin, NULL, false));
    assert_false(MatchRegion(ctx, "second\nthird", begin, NULL, false));

    assert_true(MatchRegion(ctx, "first\nsecond\nthird", begin, NULL, false));
}
#endif

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
    };

    return run_tests(tests);
}

// STUBS
