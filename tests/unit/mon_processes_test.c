#include "test.h"
#include "systype.h"
#include "generic_agent.h"
#include "item_lib.h"
#include "mon.h"
#include <misc_lib.h>                                          /* xsnprintf */


static void tests_setup(void)
{
    xsnprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/mon_processes_test.XXXXXX");
    mkdtemp(CFWORKDIR);

    char buf[CF_BUFSIZE];
    xsnprintf(buf, CF_BUFSIZE, "%s/state", CFWORKDIR);
    mkdir(buf, 0755);
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

static bool GetSysUsers( int *userListSz, int *numRootProcs, int *numOtherProcs)
{
    FILE *fp;
    char user[CF_BUFSIZE];
    char vbuff[CF_BUFSIZE];
    char cbuff[CF_BUFSIZE];

#if defined(__sun)
    xsnprintf(cbuff, CF_BUFSIZE, "/bin/ps -eo user > %s/users.txt", CFWORKDIR);
#elif defined(_AIX)
    xsnprintf(cbuff, CF_BUFSIZE, "/bin/ps -N -eo user > %s/users.txt", CFWORKDIR);
#elif defined(__linux__)
    xsnprintf(cbuff, CF_BUFSIZE, "/bin/ps -eo user > %s/users.txt", CFWORKDIR);
#else
    assert_true(1);
    return false;
#endif

    Item *userList = NULL;
    system(cbuff);
    xsnprintf(cbuff, CF_BUFSIZE, "%s/users.txt", CFWORKDIR);
    if ((fp = fopen(cbuff, "r")) == NULL)
    {
        return false;
    }
    while (fgets(vbuff, CF_BUFSIZE, fp) != NULL)
    {
        sscanf(vbuff, "%s", user);

        if (strcmp(user, "USER") == 0)
        {
            continue;
        }

        if (!IsItemIn(userList, user))
        {
            PrependItem(&userList, user, NULL);
            (*userListSz)++;
        }

        if (strcmp(user, "root") == 0)
        {
            (*numRootProcs)++;
        }
        else
        {
            (*numOtherProcs)++;
        }
    }
    fclose(fp);
    return true;
}

void test_processes_monitor(void)
{
# ifdef __sun
  return; //redmine 6316
# endif
    double cf_this[100] = { 0.0 };
    MonProcessesGatherData(cf_this);
    MonProcessesGatherData(cf_this);
    MonProcessesGatherData(cf_this);
    int usr, rusr, ousr;

    usr = rusr = ousr = 0;
    bool res = GetSysUsers(&usr, &rusr, &ousr);
    if (res == false )
    {
        assert_true(1);
        return;
    }

    usr  = 3*usr;
    rusr = 3*rusr;
    ousr = 3*ousr;
    int upper = (int)((double)usr*1.10);
    int lower = (int)((double)usr*0.90);
    assert_in_range((long long)cf_this[ob_users], lower, upper);
}

int main()
{
    strcpy(CFWORKDIR, "data");

#if defined(__sun)
    VSYSTEMHARDCLASS = PLATFORM_CONTEXT_SOLARIS;
#elif defined(_AIX)
    VSYSTEMHARDCLASS = PLATFORM_CONTEXT_AIX;
#elif defined(__linux__)
    VSYSTEMHARDCLASS = PLATFORM_CONTEXT_LINUX;
#endif

    PRINT_TEST_BANNER();
    tests_setup();
    const UnitTest tests[] =
    {
        unit_test(test_processes_monitor),
    };

    int ret = run_tests(tests);
    tests_teardown();
    return ret;
}
