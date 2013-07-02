#include "test.h"


#include "item_lib.h"

static void test_match_region(void)
{
    Item *items = NULL;
    Item *begin, *end;

    PrependItem(&items, "third", NULL);
    assert_true(MatchRegion("third", items, NULL, false));

    end = items;
    PrependItem(&items, "second", NULL);
    PrependItem(&items, "first", NULL);
    begin = items;

    assert_true(MatchRegion("first", begin, end, false));
    assert_false(MatchRegion("second", begin, end, false));
    assert_false(MatchRegion("third", begin, end, false));

    assert_true(MatchRegion("first\nsecond", begin, end, false));
    assert_false(MatchRegion("first\nthird", begin, end, false));
    assert_false(MatchRegion("second\nthird", begin, end, false));

    assert_false(MatchRegion("first\nsecond\nthird", begin, end, false));

    assert_true(MatchRegion("first", begin, NULL, false));
    assert_false(MatchRegion("second", begin, NULL, false));
    assert_false(MatchRegion("third", begin, NULL, false));

    assert_true(MatchRegion("first\nsecond", begin, NULL, false));
    assert_false(MatchRegion("first\nthird", begin, NULL, false));
    assert_false(MatchRegion("second\nthird", begin, NULL, false));

    assert_true(MatchRegion("first\nsecond\nthird", begin, NULL, false));
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_match_region),
    };

    return run_tests(tests);
}

// STUBS
