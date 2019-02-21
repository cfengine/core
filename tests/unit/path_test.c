#include <path.h>

#include <test.h>

static void test_path_getquoted(void)
{
    char *result = Path_GetQuoted(NULL);
    assert_true(result == NULL);

    result = Path_GetQuoted("no_need_to_quote/this");
    assert_string_equal(result, "no_need_to_quote/this");
    free(result);

    result = Path_GetQuoted("\"already/quoted\"");
    assert_string_equal(result, "\"already/quoted\"");
    free(result);

    result = Path_GetQuoted("needs some/quoting");
    assert_string_equal(result, "\"needs some/quoting\"");
    free(result);

    result = Path_GetQuoted("also&needs/quoting");
    assert_string_equal(result, "\"also&needs/quoting\"");
    free(result);

    result = Path_GetQuoted("also;needs/quoting");
    assert_string_equal(result, "\"also;needs/quoting\"");
    free(result);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_path_getquoted),
    };

    return run_tests(tests);
}
