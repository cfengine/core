#include "cf3.defs.h"

#include "files_lib.h"

#include <setjmp.h>
#include <cmockery.h>
#include <stdarg.h>

#define FILE_CONTENTS "8aysd9a8ydhsdkjnaldn12lk\njndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljew\nnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1l\rkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1\r\nlkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn38aysd9a8ydhsdkjnaldn12lkjndl1jndljewnbfdhwjebfkjhbnkjdn1lkdjn1lkjn3"
#define FILE_SIZE (sizeof(FILE_CONTENTS) - 1)

char CFWORKDIR[CF_BUFSIZE];

char FILE_NAME[CF_BUFSIZE];
char FILE_NAME_EMPTY[CF_BUFSIZE];

static void tests_setup(void)
{
    snprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/files_lib_test.XXXXXX");
    mkdtemp(CFWORKDIR);

    snprintf(FILE_NAME, CF_BUFSIZE, "%s/cfengine_file_test", CFWORKDIR);
    snprintf(FILE_NAME_EMPTY, CF_BUFSIZE, "%s/cfengine_file_test_empty", CFWORKDIR);
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

void test_file_write(void **p)
{
    bool res = FileWriteOver(FILE_NAME, FILE_CONTENTS);
    assert_true(res);
}

void test_file_read_all(void **p)
{
    char *output;

    ssize_t bytes_read_exact_max = FileReadMax(&output, FILE_NAME, FILE_SIZE);
    assert_int_equal(bytes_read_exact_max, FILE_SIZE);
    assert_string_equal(output, FILE_CONTENTS);

    free(output);

    char *output2;

    ssize_t bytes_read_large_max = FileReadMax(&output2, FILE_NAME, FILE_SIZE * 10);
    assert_int_equal(bytes_read_large_max, FILE_SIZE);
    assert_string_equal(output2, FILE_CONTENTS);

    free(output2);
}

void test_file_read_truncate(void **p)
{
    char expected_output[FILE_SIZE + 1];
    char *output;

    strlcpy(expected_output, FILE_CONTENTS, FILE_SIZE);
    ssize_t bytes_read_corner_max = FileReadMax(&output, FILE_NAME, FILE_SIZE - 1);
    assert_int_equal(bytes_read_corner_max, FILE_SIZE - 1);
    assert_string_equal(output, expected_output);

    free(output);

    char *output2;
    strlcpy(expected_output, FILE_CONTENTS, 11);
    ssize_t bytes_read_ten = FileReadMax(&output2, FILE_NAME, 10);
    assert_int_equal(bytes_read_ten, 10);
    assert_string_equal(output2, expected_output);
    free(output2);

    char *output3;
    strlcpy(expected_output, FILE_CONTENTS, 2);
    ssize_t bytes_read_one = FileReadMax(&output3, FILE_NAME, 1);
    assert_int_equal(bytes_read_one, 1);
    assert_string_equal(output3, expected_output);
    free(output3);
}

void test_file_read_empty(void **p)
{
    int creat_fd = creat(FILE_NAME_EMPTY, 0600);
    assert_true(creat_fd > -1);

    int close_res = close(creat_fd);
    assert_int_equal(close_res, 0);

    char *output;
    ssize_t bytes_read = FileReadMax(&output, FILE_NAME_EMPTY, 100);
    assert_int_equal(bytes_read, 0);
    assert_true(output);
}

void test_file_read_invalid(void **p)
{
    char *output;
    ssize_t bytes_read = FileReadMax(&output, "nonexisting file", 100);
    assert_true(bytes_read == -1);
}

int main()
{
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
