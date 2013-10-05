#include <stdint.h>
#include <stdlib.h>

#include <enterprise_extension.h>

#include <test.h>

ENTERPRISE_FUNC_2ARG_DECLARE(int64_t, extension_function, int32_t, short_int, int64_t, long_int);
ENTERPRISE_FUNC_2ARG_DECLARE(int64_t, extension_function_broken, int32_t, short_int, int64_t, long_int);

ENTERPRISE_FUNC_2ARG_DEFINE_STUB(int64_t, extension_function, int32_t, short_int, int64_t, long_int)
{
    return short_int + long_int;
}

ENTERPRISE_FUNC_2ARG_DEFINE_STUB(int64_t, extension_function_broken, int32_t, short_int, int64_t, long_int)
{
    return short_int + long_int;
}

static void test_extension_function_stub(void)
{
    setenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR", "nonexistingdir", 1);

    assert_int_equal(extension_function(2, 3), 5);

    unsetenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR");
}

static void test_extension_function(void)
{
    setenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR", ".libs", 1);

    assert_int_equal(extension_function(2, 3), 6);

    unsetenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR");
}

static void test_extension_function_broken(void)
{
    setenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR", ".libs", 1);

    // This one should call the stub, even if the extension is available, because the
    // function signature is different.
    assert_int_equal(extension_function_broken(2, 3), 5);

    unsetenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR");
}

static void test_extension_function_version_mismatch()
{
    setenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR", ".libs", 1);
    setenv("CFENGINE_TEST_RETURN_VERSION", "1.1.1", 1);

    // This one should call the stub, even if the extension is available, because the
    // version is different.
    assert_int_equal(extension_function(2, 3), 5);

    unsetenv("CFENGINE_TEST_RETURN_VERSION");
    unsetenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR");
}

static void test_real_extension_library()
{
    // This makes an assumption about your directory structure that may not always be correct.
    setenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR", "../../../enterprise/enterprise-plugin/.libs", 1);

    // Crude way to make the test pass if you built Community only. It assumes that if you ran autogen.sh, then
    // you have built enterprise and the test should be attempted.
    struct stat statbuf;
    if (stat("../../../enterprise/configure", &statbuf) == -1)
    {
        printf("%s: Can't find enterprise build. Assuming you haven't built it and skipping test...\n", __FUNCTION__);
        return;
    }

    void *handle = enterprise_library_open();
    assert_true(handle != NULL);
    enterprise_library_close(handle);

    unsetenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR");
}

static void test_disabled_extension_library()
{
    // This makes an assumption about your directory structure that may not always be correct.
    setenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR", "../../../enterprise/enterprise-plugin/.libs", 1);

    extension_libraries_disable();
    void *handle = enterprise_library_open();
    assert_true(handle == NULL);

    unsetenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR");
}

int main()
{
    setenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DO_CLOSE", "1", 1);
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_extension_function_stub),
        unit_test(test_extension_function),
        unit_test(test_extension_function_broken),
        unit_test(test_extension_function_version_mismatch),
        unit_test(test_real_extension_library),
        unit_test(test_disabled_extension_library),
    };

    return run_tests(tests);
}
