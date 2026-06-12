#include <test.h>

#include <files_properties.c> // Include .c file to test static functions

void test_ConsiderFile_path_separators(void)
{
    // A genuine directory entry is a single path component. One carrying a
    // separator (e.g. from a hostile server's directory listing) would escape
    // the directory once joined, see http://cwe.mitre.org/data/definitions/32.html
    assert_false(ConsiderFile("../etc", "dir", NULL));
    assert_false(ConsiderFile("a/b", "dir", NULL));
    assert_false(ConsiderFile("../../etc/cron.d/x", "dir", NULL));
    assert_false(ConsiderFile("/etc/passwd", "dir", NULL));
#ifdef _WIN32
    assert_false(ConsiderFile("a\\b", "dir", NULL));
#endif

    // Existing guards still hold.
    assert_false(ConsiderFile("", "dir", NULL));
    assert_false(ConsiderFile(".", "dir", NULL));
    assert_false(ConsiderFile("..", "dir", NULL));
    assert_false(ConsiderFile("...", "dir", NULL));

    // Ordinary names are still accepted.
    assert_true(ConsiderFile("promises.cf", "dir", NULL));
    assert_true(ConsiderFile("a.b", "dir", NULL));
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_ConsiderFile_path_separators),
    };

    return run_tests(tests);
}
