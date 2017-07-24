#include "test.h"
#include "systype.h"
#include "generic_agent.h"
#include "item_lib.h"
#include "mon.h"
#include <logging.h>                                   /* LogSetGlobalLevel */
#include <misc_lib.h>                                          /* xsnprintf */
#include <known_dirs.h>

char CFWORKDIR[CF_BUFSIZE];

static void tests_setup(void)
{
    xsnprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/mon_processes_test.XXXXXX");
    mkdtemp(CFWORKDIR);

    char buf[CF_BUFSIZE];
    xsnprintf(buf, CF_BUFSIZE, "%s", GetStateDir());
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

    /*
     * The best would be to ask only "user" field from ps, but we are asking
     * for "user,pid". The reason is that we try to mimic cf-monitord's
     * behaviour, else a different number of users might be detected by the
     * test, as printing "user,pid" truncates the user column.  TODO fix the
     * ps command to use only "-o user" in both mon_processes.c and this test.
     */
#if defined(__sun)
    xsnprintf(cbuff, CF_BUFSIZE, "/bin/ps -eo user,pid > %s/users.txt", CFWORKDIR);
    /* Skip test, solaris 9 is red. See CFE-1560, fixed in master branch. */
    return false;
#elif defined(_AIX)
    xsnprintf(cbuff, CF_BUFSIZE, "/bin/ps -N -eo user,pid > %s/users.txt", CFWORKDIR);
#elif defined(__hpux)
    xsnprintf(cbuff, CF_BUFSIZE, "UNIX95=1  /bin/ps -eo user,pid > %s/users.txt", CFWORKDIR);
    /* SKIP on HP-UX since cf-monitord doesn't count processes correctly! */
    return false;
#else
    xsnprintf(cbuff, CF_BUFSIZE, "ps -eo user,pid > %s/users.txt", CFWORKDIR);
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
        int ret = sscanf(vbuff, " %s ", user);

        if (ret != 1 ||
            strcmp(user, "") == 0 ||
            strcmp(user, "USER") == 0 ||
            isdigit(user[0]))
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

    if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
    {
        char *s = ItemList2CSV(userList);
        Log(LOG_LEVEL_DEBUG, "Users in the process table detected from the test: (%s)", s);
        free(s);
    }

    return true;
}

void test_processes_monitor(void)
{
    double cf_this[100] = { 0.0 };
    MonProcessesGatherData(cf_this);
    MonProcessesGatherData(cf_this);
    MonProcessesGatherData(cf_this);

    int usr, rusr, ousr;
    usr = rusr = ousr = 0;

    bool res = GetSysUsers(&usr, &rusr, &ousr);
    if (!res)
    {
        Log(LOG_LEVEL_NOTICE, "TEST SKIPPED!");
        return;
    }

    usr  = 3*usr;
    rusr = 3*rusr;
    ousr = 3*ousr;

    Log(LOG_LEVEL_NOTICE, "Counted %d/3 different users on the process table,"
        " while CFEngine counted %f/3", usr, cf_this[ob_users]);
    Log(LOG_LEVEL_NOTICE, "This is a non-deterministic test,"
        " the two numbers should be *about* the same since the 'ps'"
        " commands run very close to each other");

    int upper = (int) ((double) usr*1.10);
    int lower = (int) ((double) usr*0.90);
    assert_in_range((long long) cf_this[ob_users], lower, upper);
}

int main()
{
    LogSetGlobalLevel(LOG_LEVEL_DEBUG);
    strcpy(CFWORKDIR, "data");

#if defined(__sun)
    VSYSTEMHARDCLASS = PLATFORM_CONTEXT_SOLARIS;
    VPSHARDCLASS = PLATFORM_CONTEXT_SOLARIS;
#elif defined(_AIX)
    VSYSTEMHARDCLASS = PLATFORM_CONTEXT_AIX;
    VPSHARDCLASS = PLATFORM_CONTEXT_AIX;
#elif defined(__linux__)
    VSYSTEMHARDCLASS = PLATFORM_CONTEXT_LINUX;
    VPSHARDCLASS = PLATFORM_CONTEXT_LINUX;
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
