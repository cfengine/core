#include <test.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <wait.h>

/*
 * This test checks for different types of IPC to check that redirection of
 * STDIN will work.
 * This test is only significant in Linux, because this is only used by the
 * hub process that only runs on Linux.
 */
static char redirection[] = "/tmp/redirectionXXXXXX";
static char *path = NULL;
static char *message = "This is the message to be written by our helper";
static char message_back[128];
static void test_fd_redirection(void)
{
    assert_true(path != NULL);
    /*
     * Create a file for writing, then start a new process and wait for it
     * to write to the file.
     * Check the contents of the file.
     */
    int fd = -1;
    fd = mkstemp(redirection);
    assert_int_not_equal(-1, fd);
    /* Start the new process */
    int pid = 0;
    pid = fork();
    assert_int_not_equal(-1, pid);
    if (pid == 0)
    {
        /* Child */
        dup2(fd, STDIN_FILENO);
        char *argv[] = { path, message, NULL };
        char *envp[] = { NULL };
        execve(path, argv, envp);
        exit(-1);
    }
    else
    {
        /* Parent */
        int status = 0;
        int options = 0;
        /* Wait for the child to be done */
        assert_int_equal(waitpid(pid, &status, options), pid);
        /* Did it exit correctly? */
        assert_true(WIFEXITED(status));
        assert_int_equal(WEXITSTATUS(status), 0);
        /* Rewind the file so we can check the message */
        lseek(fd, 0, SEEK_SET);
        /* Read back the message */
        assert_int_equal(strlen(message), read(fd, message_back, strlen(message)));
        /* Compare it */
        assert_string_equal(message, message_back);
    }
    close (fd);
}

void tests_teardown()
{
    unlink (redirection);
}

int main(int argc, char **argv)
{
    PRINT_TEST_BANNER();
    /* Find the proper path for our helper */
    char *base = dirname(argv[0]);
    char *helper = "/redirection_test_stub";
    sprintf(path, "%s%s", base, helper);
    const UnitTest tests[] =
    {
        unit_test(test_fd_redirection)
    };
    tests_teardown();
    return run_tests(tests);
}

