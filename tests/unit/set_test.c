#include "test.h"

#include "set.h"

void test_stringset_from_string(void **state)
{
    StringSet *s = StringSetFromString("one,two, three four", ',');

    assert_true(StringSetContains(s, "one"));
    assert_true(StringSetContains(s, "two"));
    assert_true(StringSetContains(s, " three four"));

    StringSetDestroy(s);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_stringset_from_string)
    };

    return run_tests(tests);
}
