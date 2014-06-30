#include <test.h>

#include <cf3.defs.h>
#include <locks.h>
#include <misc_lib.h>                                          /* xsnprintf */


static void tests_setup(void)
{
    OpenSSL_add_all_digests();
    /* FIXME: get rid of hardcoded filenames */
    xsnprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/persistent_lock_test.XXXXXX");
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
