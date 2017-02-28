#include <test.h>

#include <files_interfaces.h>
#include <misc_lib.h>                                          /* xsnprintf */
#include <file_lib.h>          // CfReadLine()


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
    xsnprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/files_interfaces_test.XXXXXX");
    mkdtemp(CFWORKDIR);
    xsnprintf(FILE_NAME, CF_BUFSIZE, "%s/cf_files_interfaces_test", CFWORKDIR);
    xsnprintf(FILE_NAME_CORRUPT, CF_BUFSIZE, "%s/cf_files_interfaces_test_corrupt", CFWORKDIR);
    xsnprintf(FILE_NAME_EMPTY, CF_BUFSIZE, "%s/cf_files_interfaces_test_empty", CFWORKDIR);
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
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

static void test_cfreadline_valid(void)
{
    CreateGarbage(FILE_NAME);
    FILE *fin = fopen(FILE_NAME, "r");

    //test with non-empty file and valid file pointer
    size_t bs = CF_BUFSIZE;
    char *b = xmalloc(bs);

    ssize_t read = CfReadLine(&b, &bs, fin);
    assert_true(read > 0);
    assert_string_equal(b, FILE_LINE);

    if (fin)
    {
        fclose(fin);
    }

    free(b);
}

static void test_cfreadline_corrupted(void)
{
    CreateCorruptedGarbage(FILE_NAME);
    FILE *fin = fopen(FILE_NAME, "r");

    size_t bs = CF_BUFSIZE;
    char *b = xmalloc(bs);

    //test with non-empty file and valid file pointer
    ssize_t read = CfReadLine(&b, &bs, fin);
    assert_true(read > 0);
    assert_string_not_equal(b, FILE_LINE);

    if (fin)
    {
        fclose(fin);
    }

    free(b);
}

int main()
{
    PRINT_TEST_BANNER();
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

