#include <test.h>

#include <item_lib.h>
#include <cf3.defs.h>
#include <processes_select.c>

static void test_SplitProcLine_windows(void)
{

    char buf[CF_BUFSIZE];
    snprintf(buf, sizeof(buf), "%-20s %5s %s %s %8s %8s %-3s %s %s %5s %s",
             "USER", "PID", "%CPU", "%MEM", "VSZ", "RSS", "TTY", "STAT", "START", "TIME", "COMMAND");

    char *names[CF_PROCCOLS] = {0};
    int start[CF_PROCCOLS];
    int end[CF_PROCCOLS];

    GetProcessColumnNames(buf, names, start, end);

    const char *const proc_line = "NETWORK SERVICE        540  0.0  0.3     5092     11180 ?   ?    Apr28 00:00 C:\\Windows\\system32\\svchost.exe -k RPCSS -p";
    time_t pstime = time(NULL);

    char *column[CF_PROCCOLS] = {0};

    bool ret = SplitProcLine(proc_line, pstime, names, start, end, PCA_AllColumnsPresent, column);
    assert_true(ret);
    assert_string_equal(column[0], "NETWORK SERVICE");

    for (int i = 0; i < CF_PROCCOLS; i++)
    {
        free(names[i]);
        free(column[i]);
    }
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
          unit_test(test_SplitProcLine_windows),
    };

    return run_tests(tests);
}
