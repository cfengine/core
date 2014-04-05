#include <test.h>

#include <sysinfo.h>

static void test_uptime(void)
{
    /*
     * Assume we have been online at least five minutes, and less than two years.
     * If two years is not long enough, stop watching that uptime counter and
     * reboot the machine, dammit! :-)
     */
    int uptime = GetUptimeMinutes(time(NULL));
    printf("Uptime: %.2f days\n", uptime / (60.0 * 24));
    assert_in_range(uptime, 5, 60*24*365*2);
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
