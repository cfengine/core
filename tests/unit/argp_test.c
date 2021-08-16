#include <test.h>
#include <argp.h>


static void test_argp_present(void)
{
    /* Check that argp is present on all supported platforms */
    int argc = 1;
    char *argv[] = { "program" };
    error_t err = argp_parse(0, argc, argv, 0, 0, 0);
    assert_int_equal(err, 0);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_argp_present),
    };

    return run_tests(tests);
}
