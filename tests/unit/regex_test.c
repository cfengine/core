#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmockery.h>

#include "cf3.defs.h"
#include "matching.h"

static void test_full_text_match(void **state)
{
    assert_int_equal(FullTextMatch("[a-z]*", "1234abcd6789"), 0);
}

static void test_full_text_match2(void **state)
{
    assert_int_not_equal(FullTextMatch("[1-4]*[a-z]*.*", "1234abcd6789"), 0);
}

static void test_block_text_match(void **state)
{
    int start, end;

    assert_int_not_equal(BlockTextMatch("#[^\n]*", "line 1:\nline2: # comment to end\nline 3: blablab", &start, &end),
                         0);

    assert_int_equal(start, 15);
    assert_int_equal(end, 31);
}

static void test_block_text_match2(void **state)
{
    int start, end;

    assert_int_not_equal(BlockTextMatch("[a-z]+", "1234abcd6789", &start, &end), 0);
    assert_int_equal(start, 4);
    assert_int_equal(end, 8);
}

int main()
{
    const UnitTest tests[] =
{
        unit_test(test_full_text_match),
        unit_test(test_full_text_match2),
        unit_test(test_block_text_match),
        unit_test(test_block_text_match2),
    };

    return run_tests(tests);
}
