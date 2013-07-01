#include "test.h"

#include "cf3.defs.h"
#include "logging.h"

static void test_timestamp_regex(void)
{
    fflush(stdout);
    int pipe_fd[2];
    assert_int_equal(pipe(pipe_fd), 0);
    // Duplicate stdout.
    int stored_stdout = dup(1);
    assert_true(stored_stdout >= 0);
    // Make stdout point to the pipe.
    assert_int_equal(dup2(pipe_fd[1], 1), 1);
    Log(LOG_LEVEL_ERR, "Test string");
    fflush(stdout);
    // Restore stdout.
    assert_int_equal(dup2(stored_stdout, 1), 1);

    char buf[CF_BUFSIZE];
    FILE *pipe_read_end = fdopen(pipe_fd[0], "r");
    assert_true(pipe_read_end != NULL);
    assert_true(fgets(buf, sizeof(buf), pipe_read_end) != NULL);

    const char *errptr;
    int erroffset;
    pcre *regex = pcre_compile(LOGGING_TIMESTAMP_REGEX, PCRE_MULTILINE, &errptr, &erroffset, NULL);
    assert_true(regex != NULL);
    assert_true(pcre_exec(regex, NULL, buf, strlen(buf), 0, 0, NULL, 0) >= 0);

    fclose(pipe_read_end);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    close(stored_stdout);
    free(regex);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_timestamp_regex),
    };

    return run_tests(tests);
}
