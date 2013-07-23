#include <stdint.h>

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
    // Crude way of reverting to the stub function. Rename the library to something else.
    rename(ENTERPRISE_LIBRARY_NAME, ENTERPRISE_LIBRARY_NAME ".disabled");
    assert_int_equal(extension_function(2, 3), 5);
    rename(ENTERPRISE_LIBRARY_NAME ".disabled", ENTERPRISE_LIBRARY_NAME);
}

static void test_extension_function(void)
{
    assert_int_equal(extension_function(2, 3), 6);
}

static void test_extension_function_broken(void)
{
    // This one should call the stub, even if the extension is available, because the
    // function signature is different.
    assert_int_equal(extension_function_broken(2, 3), 5);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_extension_function_stub),
        unit_test(test_extension_function),
        unit_test(test_extension_function_broken),
    };

    return run_tests(tests);
}
