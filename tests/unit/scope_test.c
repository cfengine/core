#include "test.h"
#include "scope.h"

static void test_name_join(void **state)
{
    {
        char buf[CF_MAXVARSIZE] = { 0 };
        JoinScopeName(NULL, "sys", buf);
        assert_string_equal("sys", buf);
    }

    {
        char buf[CF_MAXVARSIZE] = { 0 };
        JoinScopeName("ns", "b", buf);
        assert_string_equal("ns:b", buf);
    }
}

static void test_name_split(void **state)
{
    {
        char ns[CF_MAXVARSIZE] = { 0 };
        char b[CF_MAXVARSIZE] = { 0 };
        SplitScopeName("sys", ns, b);
        assert_string_equal("", ns);
        assert_string_equal("sys", b);
    }

    {
        char ns[CF_MAXVARSIZE] = { 0 };
        char b[CF_MAXVARSIZE] = { 0 };
        SplitScopeName("ns:b", ns, b);
        assert_string_equal("ns", ns);
        assert_string_equal("b", b);
    }
}

int main()
{
    const UnitTest tests[] =
{
        unit_test(test_name_split),
        unit_test(test_name_join),
    };

    PRINT_TEST_BANNER();
    return run_tests(tests);
}
