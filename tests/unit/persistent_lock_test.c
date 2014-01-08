#include <cf3.defs.h>

#include <locks.h>

#include <test.h>

static void tests_setup(void)
{
    OpenSSL_add_all_digests();
    /* FIXME: get rid of hardcoded filenames */
    snprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/persistent_lock_test.XXXXXX");
    mkdtemp(CFWORKDIR);

    char buf[CF_BUFSIZE];
    snprintf(buf, CF_BUFSIZE, "%s/state", CFWORKDIR);
    mkdir(buf, 0755);
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

int main()
{
    PRINT_TEST_BANNER();
    tests_setup();

    const UnitTest tests[] =
      {

      };
    
    int ret = run_tests(tests);

    tests_teardown();

    return ret;
}
