#include "test.h"

#include "generic_agent.h"
#include "mon.h"

static double GetCpuStat()
{
    double q, dq = -1.0;
    long total_time = 1;
    FILE *fp;
    long userticks = 0, niceticks = 0, systemticks = 0,
         idle = 0, iowait = 0, irq = 0, softirq = 0;
    char cpuname[CF_MAXVARSIZE], buf[CF_BUFSIZE];


    if ((fp = fopen("/proc/stat", "r")) == NULL)
    {
        return -1.0;
    }

    while (!feof(fp))
    {
        if (fgets(buf, sizeof(buf), fp) == NULL)
        {
            break;
        }

        if (sscanf(buf, "%s%ld%ld%ld%ld%ld%ld%ld", cpuname, &userticks,
                   &niceticks, &systemticks, &idle, &iowait, &irq,
                   &softirq) != 8)
        {
            continue;
        }

        total_time = (userticks + niceticks + systemticks + idle);

        q = 100.0 * (double) (total_time - idle);

        if (strcmp(cpuname, "cpu") == 0)
        {
            dq = q / (double) total_time;
            if ((dq > 100) || (dq < 0))
            {
                dq = 50;
            }
        }
    }

    fclose(fp);
    return dq;
}

void test_cpu_monitor(void)
{
    double cf_this[100];
    double dq1 = GetCpuStat();
    if (dq1 == -1.0)
    {
        assert_true(1);
        return;
    }
    MonCPUGatherData(cf_this);
    double dq2 = GetCpuStat();
    if (dq2 == -1.0)
    {
        assert_true(1);
        return;
    }
    double min = (double) (dq2<dq1?dq2:dq1);
    double max = (double) (dq2<dq1?dq1:dq2); 
    double lower = (min - (fabs(dq2 - dq1)) * 0.90);
    double upper = (max + (fabs(dq2 - dq1)) * 1.10);

    assert_true(cf_this[ob_cpuall]>=lower && cf_this[ob_cpuall]<=upper);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_cpu_monitor),
    };

    return run_tests(tests);
}
