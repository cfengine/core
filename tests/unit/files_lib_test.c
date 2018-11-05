#include <test.h>

#include <cf3.defs.h>
#include <files_lib.h>
#include <misc_lib.h>                                          /* xsnprintf */


#define FILE_CONTENTS "8aysd9a8ydhsdkjnaldn12lk\njndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljew\nnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1l\rkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1\r\nlkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn3"
#define FILE_SIZE (sizeof(FILE_CONTENTS) - 1)

char CFWORKDIR[CF_BUFSIZE];

char FILE_NAME[CF_BUFSIZE];
char FILE_NAME_EMPTY[CF_BUFSIZE];

static void tests_setup(void)
{
    xsnprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/files_lib_test.XXXXXX");
    mkdtemp(CFWORKDIR);

    xsnprintf(FILE_NAME, CF_BUFSIZE, "%s/cfengine_file_test", CFWORKDIR);
    xsnprintf(FILE_NAME_EMPTY, CF_BUFSIZE, "%s/cfengine_file_test_empty", CFWORKDIR);
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

void test_file_write(void)
{
    bool res = FileWriteOver(FILE_NAME, FILE_CONTENTS);
    assert_true(res);
}

void test_file_read_all(void)
{
    bool truncated;
    Writer *w = FileRead(FILE_NAME, FILE_SIZE, &truncated);
    assert_int_equal(StringWriterLength(w), FILE_SIZE);
    assert_string_equal(StringWriterData(w), FILE_CONTENTS);
    assert_int_equal(truncated, false);
    WriterClose(w);

    Writer *w2 = FileRead(FILE_NAME, FILE_SIZE * 10, &truncated);
    assert_int_equal(StringWriterLength(w2), FILE_SIZE);
    assert_string_equal(StringWriterData(w2), FILE_CONTENTS);
    assert_int_equal(truncated, false);
    WriterClose(w2);

    Writer *w3 = FileRead(FILE_NAME, FILE_SIZE * 10, NULL);
    assert_int_equal(StringWriterLength(w3), FILE_SIZE);
    assert_string_equal(StringWriterData(w3), FILE_CONTENTS);
    WriterClose(w3);
}

void test_file_read_truncate(void)
{
    char expected_output[FILE_SIZE + 1];

    bool truncated = false;
    Writer *w = FileRead(FILE_NAME, FILE_SIZE - 1, &truncated);
    assert_int_equal(StringWriterLength(w), FILE_SIZE - 1);
    strlcpy(expected_output, FILE_CONTENTS, FILE_SIZE);
    assert_string_equal(StringWriterData(w), expected_output);
    assert_int_equal(truncated, true);
    WriterClose(w);

    bool truncated2 = false;
    Writer *w2 = FileRead(FILE_NAME, 10, &truncated2);
    assert_int_equal(StringWriterLength(w2), 10);
    strlcpy(expected_output, FILE_CONTENTS, 11);
    assert_string_equal(StringWriterData(w2), expected_output);
    assert_int_equal(truncated2, true);
    WriterClose(w2);

    bool truncated3 = false;
    Writer *w3 = FileRead(FILE_NAME, 1, &truncated3);
    assert_int_equal(StringWriterLength(w3), 1);
    strlcpy(expected_output, FILE_CONTENTS, 2);
    assert_string_equal(StringWriterData(w3), expected_output);
    assert_int_equal(truncated3, true);
    WriterClose(w3);
}

void test_file_read_empty(void)
{
    int creat_fd = creat(FILE_NAME_EMPTY, 0600);
    assert_true(creat_fd > -1);

    int close_res = close(creat_fd);
    assert_int_equal(close_res, 0);

    bool truncated = true;
    Writer *w = FileRead(FILE_NAME_EMPTY, 100, &truncated);
    assert_int_equal(StringWriterLength(w), 0);
    assert_string_equal(StringWriterData(w), "");
    assert_int_equal(truncated, false);
    WriterClose(w);
}

void test_file_read_invalid(void)
{
    Writer *w = FileRead("nonexisting file", 100, NULL);
    assert_false(w);
}

int main()
{
    PRINT_TEST_BANNER();
    tests_setup();

    const UnitTest tests[] =
        {
            unit_test(test_file_write),
            unit_test(test_file_read_all),
            unit_test(test_file_read_truncate),
            unit_test(test_file_read_empty),
            unit_test(test_file_read_invalid),
        };

    int ret = run_tests(tests);

    tests_teardown();

    return ret;
}
