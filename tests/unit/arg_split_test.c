#include "cf3.defs.h"
#include "cf3.extern.h"

#include <setjmp.h>
#include <cmockery.h>

static void test_split_empty(void **state)
{
char **s = ArgSplitCommand("");
assert_true(s);
assert_false(*s);
ArgFree(s);
}

static void test_split_easy(void **state)
{
char **s = ArgSplitCommand("zero one two");
assert_string_equal(s[0], "zero");
assert_string_equal(s[1], "one");
assert_string_equal(s[2], "two");
assert_false(s[3]);

ArgFree(s);
}

static void test_split_quoted_beginning(void **state)
{
char **s = ArgSplitCommand("\"quoted string\" atbeginning");
assert_string_equal(s[0], "quoted string");
assert_string_equal(s[1], "atbeginning");
assert_false(s[2]);
ArgFree(s);
}

static void test_split_quoted_end(void **state)
{
char **s = ArgSplitCommand("atend 'quoted string'");
assert_string_equal(s[0], "atend");
assert_string_equal(s[1], "quoted string");
assert_false(s[2]);
ArgFree(s);
}

static void test_split_quoted_middle(void **state)
{
char **s = ArgSplitCommand("at `quoted string` middle");
assert_string_equal(s[0], "at");
assert_string_equal(s[1], "quoted string");
assert_string_equal(s[2], "middle");
assert_false(s[3]);
ArgFree(s);
}

static void test_complex_quoting(void **state)
{
char **s = ArgSplitCommand("\"foo`'bar\"");
assert_string_equal(s[0], "foo`'bar");
assert_false(s[1]);
ArgFree(s);
}


static void test_arguments_resize_for_null(void **state)
{
/* This test checks that extending returned argument list for NULL terminator
 * works correctly */
char **s = ArgSplitCommand("0 1 2 3 4 5 6 7");
assert_string_equal(s[7], "7");
assert_false(s[8]);
ArgFree(s);
}

static void test_arguments_resize(void **state)
{
char **s = ArgSplitCommand("0 1 2 3 4 5 6 7 8");
assert_string_equal(s[7], "7");
assert_string_equal(s[8], "8");
assert_false(s[9]);
ArgFree(s);
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_split_empty),
   unit_test(test_split_easy),
   unit_test(test_split_quoted_beginning),
   unit_test(test_split_quoted_middle),
   unit_test(test_split_quoted_end),
   unit_test(test_complex_quoting),
   unit_test(test_arguments_resize_for_null),
   unit_test(test_arguments_resize),
   };

return run_tests(tests);
}
