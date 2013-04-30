#include "test.h"

#include <string.h>
#include "cmockery.h"
#include "cf-serverd-functions.c"


static void generateTestFile()
{
    FILE *fp = fopen("/tmp/test_file", "w+");

    assert_int_not_equal(fp, NULL);

    fprintf(fp, "<?xml version=\"1.0\" standalone='no'?>\n");
    fprintf(fp, "<!DOCTYPE service-group SYSTEM \"avahi-service.dtd\">\n");
    fprintf(fp, "<!-- This file has been automatically generated by cf-serverd. -->\n");
    fprintf(fp, "<service-group>\n");
#ifdef HAVE_NOVA
    fprintf(fp, "<name replace-wildcards=\"yes\" >CFEngine Enterprise %s Policy Hub on %s </name>\n", Version(), "%h");
#else
    fprintf(fp, "<name replace-wildcards=\"yes\" >CFEngine Community %s Policy Server on %s </name>\n", Version(), "%h");
#endif
    fprintf(fp, "<service>\n");
    fprintf(fp, "<type>_cfenginehub._tcp</type>\n");
    DetermineCfenginePort();
    fprintf(fp, "<port>%s</port>\n", STR_CFENGINEPORT);
    fprintf(fp, "</service>\n");
    fprintf(fp, "</service-group>\n");
    fclose(fp);
}

static void test_generateAvahiConfig(void)
{
    generateTestFile();
    assert_int_equal(GenerateAvahiConfig("/tmp/avahi_config"), 0);
    FILE *testfile = fopen("/tmp/test_file", "r+");
    assert_int_not_equal(testfile, NULL);
    FILE *optfile = fopen("/tmp/avahi_config", "r+");
    assert_int_not_equal(optfile, NULL);
    char buffer1[256], buffer2[256];

    while (!feof(testfile) && !feof(optfile))
    {
        memset(buffer1, 0, 256);
        memset(buffer2, 0 ,256);
        fgets(buffer1, 256, testfile);
        fgets(buffer2, 256, optfile);
        assert_int_equal(strcmp(buffer1, buffer2), 0);
    }

    fclose(testfile);
    fclose(optfile);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
          unit_test(test_generateAvahiConfig)
    };

    return run_tests(tests);
}
