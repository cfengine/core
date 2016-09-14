#include <test.h>

#include <sysinfo.h>

static void test_uptime(void)
{
    /*
     * Assume we have been online at least one minute, and less than 5 years.
     * Should be good enough for everyone...
     */
    int uptime = GetUptimeMinutes(time(NULL));
    printf("Uptime: %.2f days\n", uptime / (60.0 * 24));
    assert_in_range(uptime, 1, 60*24*365*5);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_uptime)
    };

    return run_tests(tests);
}
