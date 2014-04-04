#include <test.h>

#include <stdint.h>
#include <stdlib.h>

#include <enterprise_extension.h>


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
    putenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR=nonexistingdir");

    assert_int_equal(extension_function(2, 3), 5);

    unsetenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR");
}

static void test_extension_function(void)
{
    putenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR=.libs");

    assert_int_equal(extension_function(2, 3), 6);

    unsetenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR");
}

static void test_extension_function_broken(void)
{
    putenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR=.libs");

    // This one should call the stub, even if the extension is available, because the
    // function signature is different.
    assert_int_equal(extension_function_broken(2, 3), 5);

    unsetenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR");
}

static void test_extension_function_version_mismatch()
{
    putenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR=.libs");
    putenv("CFENGINE_TEST_RETURN_VERSION=1.1.1");

    // This one should call the stub, even if the extension is available, because the
    // version is different.
    assert_int_equal(extension_function(2, 3), 5);

    unsetenv("CFENGINE_TEST_RETURN_VERSION");
    unsetenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR");
}

int main()
{
    putenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DO_CLOSE=1");
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_extension_function_stub),
        unit_test(test_extension_function),
        unit_test(test_extension_function_broken),
        unit_test(test_extension_function_version_mismatch),
    };

    return run_tests(tests);
}
