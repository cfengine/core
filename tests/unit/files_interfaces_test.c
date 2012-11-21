#include "test.h"
#include "files_interfaces.h"

#define FILE_SIZE (sizeof(FILE_CONTENTS) - 1)
#define FILE_LINE "some garbage!"
#define FILE_CORRUPTED_LINE "some \0 , gar\0bage!"
#define LINE_SIZE (sizeof(FILE_LINE) - 1)

char CFWORKDIR[CF_BUFSIZE];
char FILE_NAME[CF_BUFSIZE];
char FILE_NAME_CORRUPT[CF_BUFSIZE];
char FILE_NAME_EMPTY[CF_BUFSIZE];

static void tests_setup(void)
{
    snprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/files_interfaces_test.XXXXXX");
    mkdtemp(CFWORKDIR);
    snprintf(FILE_NAME, CF_BUFSIZE, "%s/cf_files_interfaces_test", CFWORKDIR);
    snprintf(FILE_NAME_CORRUPT, CF_BUFSIZE, "%s/cf_files_interfaces_test_corrupt", CFWORKDIR);
    snprintf(FILE_NAME_EMPTY, CF_BUFSIZE, "%s/cf_files_interfaces_test_empty", CFWORKDIR);
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

static void CreateGarbage(const char *filename)
{
    FILE *fh = fopen(filename, "w");
    for(int i = 0; i < 10; ++i)
    {
        fwrite(FILE_LINE, 14, 1, fh);
    }
    fclose(fh);
}

static void CreateCorruptedGarbage(const char *filename)
{
    FILE *fh = fopen(filename, "w");
    for(int i = 0; i < 10; ++i)
    {
        fwrite(FILE_CORRUPTED_LINE, 18, 1, fh);
    }
    fclose(fh);
}

static void test_cfreadline_valid(void **state)
{
    int read = 0;
    char output[CF_BUFSIZE] = { 0 };
    FILE *fin;

    CreateGarbage(FILE_NAME);
    fin = fopen(FILE_NAME, "r");

    //test with non-empty file and valid file pointer
    read = CfReadLine(output, CF_BUFSIZE - 1, fin);
    assert_int_equal(read, true);
    assert_string_equal(output, FILE_LINE);

    if (fin)
    {
        fclose(fin);
    }
}

static void test_cfreadline_corrupted(void **state)
{
    int read = 0;
    char output[CF_BUFSIZE] = { 0 };
    FILE *fin;

    CreateCorruptedGarbage(FILE_NAME);
    fin = fopen(FILE_NAME, "r");

    //test with non-empty file and valid file pointer
    read = CfReadLine(output, CF_BUFSIZE - 1, fin);
    assert_int_equal(read, true);
    assert_string_not_equal(output, FILE_LINE);

    if (fin)
    {
        fclose(fin);
    }
}

int main()
{
    tests_setup();

    const UnitTest tests[] =
    {
        unit_test(test_cfreadline_valid),
        unit_test(test_cfreadline_corrupted),
    };

    PRINT_TEST_BANNER();
    int ret = run_tests(tests);

    tests_teardown();
    return ret;
}

