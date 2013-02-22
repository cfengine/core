#include "test.h"

#include "path_lib.h"

void TestParse(const char *in, const char *out, bool absolute)
{
    Path *p = PathFromString(in);
    assert_true(PathIsAbsolute(p) == absolute);
    char *s = PathToString(p);
    assert_string_equal(out, s);
    free(s);
    PathDestroy(p);
}

static void test_from_to_string(void **state)
{
    TestParse("", "", false);
    TestParse("/", "/", true);
    TestParse("abc", "abc", false);
    TestParse("/abc", "/abc", true);
    TestParse("abc/def", "abc/def", false);
    TestParse("/abc/def", "/abc/def", true);
    TestParse("./abc/def", "./abc/def", false);
    TestParse("abc/def/", "abc/def", false);
    TestParse("///abc/////def//", "/abc/def", true);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_from_to_string),
    };

    return run_tests(tests);
}

