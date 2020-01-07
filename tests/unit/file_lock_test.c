#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <file_lib.h>

/**
 * This file contains file-locking tests that need to fork() a child process
 * which totally confuses the unit test framework used by the other tests.
 */

#define TEMP_DIR "/tmp/file_lib_test"
#define TEST_FILE "file_lib_test.txt"

static bool failure = false;

#define assert_int_equal(expr1, expr2) \
    if ((expr1) != (expr2))            \
    { \
        fprintf(stderr, "FAIL: "#expr1" != "#expr2" [%d != %d] (%s:%d)\n", (expr1), (expr2), __FILE__, __LINE__); \
        failure = true; \
    }

#define assert_true(expr) \
    if (!(expr)) \
    { \
        fprintf(stderr, "FAIL: "#expr" is FALSE (%s:%d)\n", __FILE__, __LINE__); \
        failure = true; \
    }

#define assert_false(expr) \
    if ((expr)) \
    { \
        fprintf(stderr, "FAIL: "#expr" is TRUE (%s:%d)\n", __FILE__, __LINE__); \
        failure = true; \
    }

static void clear_tempfiles()
{
    unlink(TEMP_DIR "/" TEST_FILE);
    rmdir(TEMP_DIR);
}

int main()
{
    atexit(&clear_tempfiles);
    mkdir(TEMP_DIR, 0755);

    /* TEST CASE 1 -- excl. lock in parent, try excl. lock in child */
    FileLock lock = EMPTY_FILE_LOCK;
    int fd = open(TEMP_DIR "/" TEST_FILE, O_CREAT | O_RDWR, 0644);
    lock.fd = fd;

    /* lock trying to wait */
    assert_int_equal(ExclusiveFileLock(&lock, true), 0);

    /* FD should not be changed */
    assert_int_equal(lock.fd, fd);

    /* we would be able to acquire the lock (we already have it) */
    assert_true(ExclusiveFileLockCheck(&lock));

    pid_t pid = fork();
    if (pid == 0)
    {
        /* child */
        failure = false;

        /* FDs are inherited, fcntl() locks are not */

        /* the lock is held by the parent process */
        assert_false(ExclusiveFileLockCheck(&lock));

        /* try to lock without waiting */
        assert_int_equal(ExclusiveFileLock(&lock, false), -1);

        /* should not affect parent's FD */
        close(fd);

        _exit(failure ? 1 : 0);
    }
    else
    {
        /* parent */
        int status;
        int ret = waitpid(pid, &status, 0);
        assert_int_equal(ret, pid);
        failure = (WEXITSTATUS(status) != 0);
    }

    /* unlock, but keep the FD open */
    assert_int_equal(ExclusiveFileUnlock(&lock, false), 0);

    /* should be able to close */
    assert_int_equal(close(lock.fd), 0);


    /* TEST CASE 2 -- shared lock in parent, try excl. lock in child, get shared
     *                lock in child, get excl. lock in parent */
    fd = open(TEMP_DIR "/" TEST_FILE, O_CREAT | O_RDWR, 0644);
    lock.fd = fd;

    /* SHARED lock trying to wait */
    assert_int_equal(SharedFileLock(&lock, true), 0);

    /* FD should not be changed */
    assert_int_equal(lock.fd, fd);

    pid = fork();
    if (pid == 0)
    {
        /* child */
        failure = false;

        /* FDs are inherited, fcntl() locks are not */

        /* a shared lock is held by the parent process */
        assert_false(ExclusiveFileLockCheck(&lock));

        /* try to lock without waiting */
        assert_int_equal(ExclusiveFileLock(&lock, false), -1);

        /* try to get a shared lock without waiting */
        assert_int_equal(SharedFileLock(&lock, false), 0);

        /* should not affect parent's lock or FD */
        assert_int_equal(SharedFileUnlock(&lock, true), 0);

        _exit(failure ? 1 : 0);
    }
    else
    {
        /* parent */
        int status;
        int ret = waitpid(pid, &status, 0);
        assert_int_equal(ret, pid);
        failure = (WEXITSTATUS(status) != 0);
    }

    /* we are holding a shared lock so WE should be able to get an exclusive
     * lock */
    assert_true(ExclusiveFileLockCheck(&lock));

    /* upgrade the lock to an exclusive one */
    assert_int_equal(ExclusiveFileLock(&lock, true), 0);

    /* unlock, but keep the FD open */
    assert_int_equal(ExclusiveFileUnlock(&lock, false), 0);

    /* should be able to close both FDs */
    assert_int_equal(close(lock.fd), 0);


    if (failure)
    {
        fprintf(stderr, "FAILED\n");
        return 1;
    }
    else
    {
        fprintf(stderr, "SUCCESS\n");
        return 0;
    }
}
