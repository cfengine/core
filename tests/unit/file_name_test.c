#include "cf3.defs.h"

#include <setjmp.h>
#include <cmockery.h>

#include "files_names.h"


static void test_first_file_separator(void **state)
{
    const char *out;

    char *in1 = "/tmp/myfile";
    out = FirstFileSeparator(in1);
    assert_true(out == in1);

    char *in2 = "/tmp/myfile";
    out = FirstFileSeparator(in2);
    assert_true(out == in2);

    char *in3 = "c:\\tmp\\myfile";
    out = FirstFileSeparator(in3);
    assert_true(out == in3 + 2);

    char *in4 = "\\\\my\\windows\\share";
    out = FirstFileSeparator(in4);
    assert_true(out == in4 + 1);
}

static void test_get_parent_directory_copy(void **state)
{
    char *out;

#ifndef NT

    /* unix, will fail on windows because of IsFileSep */

    out = GetParentDirectoryCopy("/some/path/here");
    assert_string_equal(out, "/some/path");
    free(out);

    out = GetParentDirectoryCopy("/some/path/here/dir/");
    assert_string_equal(out, "/some/path/here/dir");
    free(out);

    out = GetParentDirectoryCopy("/some/path/here/dir/.");
    assert_string_equal(out, "/some/path/here/dir");
    free(out);

    out = GetParentDirectoryCopy("/some");
    assert_string_equal(out, "/");
    free(out);

#else  /* NT */

    /* windows, will fail on unix because of IsFileSep */

    out = GetParentDirectoryCopy("c:\\some\\path with space\\here and now");
    assert_string_equal(out, "c:\\some\\path with space");
    free(out);

    out = GetParentDirectoryCopy("c:\\some");
    assert_string_equal(out, "c:\\");
    free(out);

    out = GetParentDirectoryCopy("\\\\some\\path");
    assert_string_equal(out, "\\\\some");
    free(out);

    out = GetParentDirectoryCopy("\\\\some");
    assert_string_equal(out, "\\\\");
    free(out);

#endif  /* __MINGW32__ */
}


int main()
{
    const UnitTest tests[] =
        {
            unit_test(test_first_file_separator),
            unit_test(test_get_parent_directory_copy)
        };

    return run_tests(tests);
}
