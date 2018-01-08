#include <test.h>

#include <files_names.h>
#include <file_lib.h>

static void test_first_file_separator(void)
{
    const char *out;

    const char *in = "/tmp/myfile";
    out = FirstFileSeparator(in);
    assert_true(out == in);

    in = "tmp/myfile";
    out = FirstFileSeparator(in);
    assert_true(out == in + 3);

    in = "c:/tmp/myfile";
    out = FirstFileSeparator(in);
    assert_true(out == in + 2);

    in = "\\\\my\\windows\\share";
    out = FirstFileSeparator(in);
    assert_true(out == in + 1);

}

static void test_get_parent_directory_copy(void)
{
    char *out;

#ifndef _WIN32

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

#else  /* _WIN32 */

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

#endif  /* _WIN32 */
}

static void test_delete_redundant_slashes(void)
{
    {
        char str[] = "///a//b////c/";
        DeleteRedundantSlashes(str);
        assert_string_equal(str, "/a/b/c/");
    }

    {
        char str[] = "a//b////c/";
        DeleteRedundantSlashes(str);
        assert_string_equal(str, "a/b/c/");
    }

    {
        char str[] = "/a/b/c/";
        DeleteRedundantSlashes(str);
        assert_string_equal(str, "/a/b/c/");
    }

    {
        char str[] = "a///b////c///";
        DeleteRedundantSlashes(str);
        assert_string_equal(str, "a/b/c/");
    }

    {
        char str[] = "a///b////c";
        DeleteRedundantSlashes(str);
        assert_string_equal(str, "a/b/c");
    }

    {
        char str[] = "a///b/c";
        DeleteRedundantSlashes(str);
        assert_string_equal(str, "a/b/c");
    }

    {
        char str[] = "alpha///beta/charlie///zeta";
        DeleteRedundantSlashes(str);
        assert_string_equal(str, "alpha/beta/charlie/zeta");
    }

    {
        char str[] = "////alpha///beta/charlie///zeta///";
        DeleteRedundantSlashes(str);
        assert_string_equal(str, "/alpha/beta/charlie/zeta/");
    }

    {
        char str[] = "/a";
        DeleteRedundantSlashes(str);
        assert_string_equal(str, "/a");
    }

    {
        char str[] = "/alpha";
        DeleteRedundantSlashes(str);
        assert_string_equal(str, "/alpha");
    }
}

static void test_join_paths(void)
{
    char joined[PATH_MAX] = { 0 };

#ifndef _WIN32
    /* unix, will fail on windows because of different file separator */

    strlcat(joined, "/tmp", PATH_MAX);
    JoinPaths(joined, PATH_MAX, "test");
    assert_string_equal(joined, "/tmp/test");

    JoinPaths(joined, PATH_MAX, "/test2");
    assert_string_equal(joined, "/tmp/test/test2");

    strlcat(joined, "/", PATH_MAX);
    JoinPaths(joined, PATH_MAX, "test3");
    assert_string_equal(joined, "/tmp/test/test2/test3");

    strlcat(joined, "/", PATH_MAX);
    JoinPaths(joined, PATH_MAX, "/test4");
    assert_string_equal(joined, "/tmp/test/test2/test3/test4");

    memset(joined, 0, PATH_MAX);
    JoinPaths(joined, PATH_MAX, "test5");
    assert_string_equal(joined, "test5");

    memset(joined, 0, PATH_MAX);
    JoinPaths(joined, PATH_MAX, "/test6");
    assert_string_equal(joined, "/test6");

    memset(joined, 0, PATH_MAX);
    strlcat(joined, "test6", PATH_MAX);
    JoinPaths(joined, PATH_MAX, "test7");
    assert_string_equal(joined, "test6/test7");

#else  /* _WIN32 */
    /* windows, will fail on unix because of different file separator */

    strlcat(joined, "C:\\tmp", PATH_MAX);
    JoinPaths(joined, PATH_MAX, "test");
    assert_string_equal(joined, "C:\\tmp\\test");

    JoinPaths(joined, PATH_MAX, "\\test2");
    assert_string_equal(joined, "C:\\tmp\\test\\test2");

    strlcat(joined, "\\", PATH_MAX);
    JoinPaths(joined, PATH_MAX, "test3");
    assert_string_equal(joined, "C:\\tmp\\test\\test2\\test3");

    strlcat(joined, "\\", PATH_MAX);
    JoinPaths(joined, PATH_MAX, "\\test4");
    assert_string_equal(joined, "C:\\tmp\\test\\test2\\test3\\test4");

    memset(joined, 0, PATH_MAX);
    JoinPaths(joined, PATH_MAX, "test5");
    assert_string_equal(joined, "test5");

    memset(joined, 0, PATH_MAX);
    JoinPaths(joined, PATH_MAX, "C:\\test6");
    assert_string_equal(joined, "C:\\test6");

    memset(joined, 0, PATH_MAX);
    strlcat(joined, "test6", PATH_MAX);
    JoinPaths(joined, PATH_MAX, "test7");
    assert_string_equal(joined, "test6\\test7");
#endif
}

static void test_get_absolute_path(void)
{
    char *abs_path = NULL;
    char expected[PATH_MAX] = { 0 };
    char orig[PATH_MAX] = { 0 };

#ifndef _WIN32
    /* unix, will fail on windows because of different file separator */

    abs_path = GetAbsolutePath("/tmp/test");
    assert_string_equal(abs_path, "/tmp/test");
    free(abs_path);

    abs_path = GetAbsolutePath("/tmp/test/../test2");
    assert_string_equal(abs_path, "/tmp/test2");
    free(abs_path);

    getcwd(expected, PATH_MAX);
    abs_path = GetAbsolutePath("test/test2");
    assert_true(IsAbsoluteFileName(abs_path));
    strlcat(expected, "/test/test2", PATH_MAX);
    assert_string_equal(abs_path, expected);
    free(abs_path);
    memset(expected, 0, PATH_MAX);

    getcwd(expected, PATH_MAX);
    abs_path = GetAbsolutePath("./test");
    assert_true(IsAbsoluteFileName(abs_path));
    strlcat(expected, "/test", PATH_MAX);
    assert_string_equal(abs_path, expected);
    free(abs_path);
    memset(expected, 0, PATH_MAX);

    getcwd(expected, PATH_MAX);
    abs_path = GetAbsolutePath("test/../test2");
    assert_true(IsAbsoluteFileName(abs_path));
    strlcat(expected, "/test2", PATH_MAX);
    assert_string_equal(abs_path, expected);
    free(abs_path);
    memset(expected, 0, PATH_MAX);

    getcwd(expected, PATH_MAX);
    strlcat(orig, expected, PATH_MAX);
    chdir("..");
    ChopLastNode(expected);
    abs_path = GetAbsolutePath("test/test2");
    assert_true(IsAbsoluteFileName(abs_path));
    strlcat(expected, "/test/test2", PATH_MAX);
    assert_string_equal(abs_path, expected);
    free(abs_path);
    memset(expected, 0, PATH_MAX);
    chdir(orig);
    memset(orig, 0, PATH_MAX);

#else  /* _WIN32 */
    /* windows, will fail on unix because of different file separator */

    abs_path = GetAbsolutePath("C:\\tmp\\test");
    assert_string_equal(abs_path, "C:\\tmp\\test");
    free(abs_path);

    abs_path = GetAbsolutePath("C:\\tmp\\test\\..\\test2");
    assert_string_equal(abs_path, "C:\\tmp\\test2");
    free(abs_path);

    getcwd(expected, PATH_MAX);
    abs_path = GetAbsolutePath("test\\test2");
    assert_true(IsAbsoluteFileName(abs_path));
    strlcat(expected, "\\test\\test2", PATH_MAX);
    assert_string_equal(abs_path, expected);
    free(abs_path);
    memset(expected, 0, PATH_MAX);

    getcwd(expected, PATH_MAX);
    abs_path = GetAbsolutePath(".\\test");
    assert_true(IsAbsoluteFileName(abs_path));
    strlcat(expected, "\\test", PATH_MAX);
    assert_string_equal(abs_path, expected);
    free(abs_path);
    memset(expected, 0, PATH_MAX);

    getcwd(expected, PATH_MAX);
    abs_path = GetAbsolutePath("test\\..\\test2");
    assert_true(IsAbsoluteFileName(abs_path));
    strlcat(expected, "\\test2", PATH_MAX);
    assert_string_equal(abs_path, expected);
    free(abs_path);
    memset(expected, 0, PATH_MAX);

    getcwd(expected, PATH_MAX);
    strlcat(orig, expected, PATH_MAX);
    chdir("..");
    ChopLastNode(expected);
    abs_path = GetAbsolutePath("test\\test2");
    assert_true(IsAbsoluteFileName(abs_path));
    strlcat(expected, "\\test\\test2", PATH_MAX);
    assert_string_equal(abs_path, expected);
    free(abs_path);
    memset(expected, 0, PATH_MAX);
    chdir(orig);
    memset(orig, 0, PATH_MAX);

#endif
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_first_file_separator),
        unit_test(test_get_parent_directory_copy),
        unit_test(test_delete_redundant_slashes),
        unit_test(test_join_paths),
        unit_test(test_get_absolute_path)
    };

    return run_tests(tests);
}
