#include <test.h>

#include <compiler.h>
#include <process_lib.h>
#include <process_unix_priv.h>

/* Stubs for /proc/<pid>/stat. */

static const char *filecontents[2] = {
    "1 (i()))))))-:!#!#@)) S 1 3927 3927 1025 3927 4202752 359 0 2 0 0 0 0 0 20 0 1 0 65535 19234816 226 18446744073709551615 1 1 0 0 0 0 0 6 0 18446744073709551615 0 0 17 2 0 0 40 0 0",
    "3929 (getty) T 1 3929 3929 1027 3929 4202752 359 0 1 0 0 0 0 0 20 0 1 0 100000 19234816 225 18446744073709551615 1 1 0 0 0 0 0 6 0 18446744073709551615 0 0 17 0 0 0 42 0 0",
};

static int filepos[2];

int open(const char *filename, ARG_UNUSED int flags, ...)
{
    if (!strcmp(filename, "/proc/1/stat"))
    {
        filepos[0] = 0;
        return 0;
    }
    else if (!strcmp(filename, "/proc/2/stat"))
    {
        filepos[1] = 0;
        static int got_intr = false;
        if (!got_intr)
        {
            got_intr = true;
            errno = EINTR;
            return -1;
        }

        return 1;
    }
    else if (!strcmp(filename, "/proc/666/stat"))
    {
        errno = EACCES;
        return -1;
    }
    else
    {
        errno = ENOENT;
        return -1;
    }
}

ssize_t read(int fd, void *buffer, ARG_UNUSED size_t buf_size)
{
    if (fd == 0)
    {
        if (filepos[0] < strlen(filecontents[0]))
        {
            memcpy(buffer, filecontents[0], strlen(filecontents[0]));
            filepos[0] = strlen(filecontents[0]);
            return strlen(filecontents[0]);
        }
        else
        {
            return 0;
        }
    }

    if (fd == 1)
    {
        static bool got_eintr = false;

        if (!got_eintr)
        {
            got_eintr = true;
            errno = EINTR;
            return -1;
        }
        else
        {
            got_eintr = false;
        }

        if (filepos[1] < strlen(filecontents[1]))
        {
            memcpy(buffer, filecontents[1] + filepos[1], 1);
            filepos[1]++;
            return 1;
        }
        else
        {
            return 0;
        }
    }

    errno = EIO;
    return -1;
}

int close(ARG_UNUSED int fd)
{
    return 0;
}

static void test_get_start_time_process1(void)
{
    time_t t = GetProcessStartTime(1);
    assert_int_equal(t, 65535 / sysconf(_SC_CLK_TCK));
}


static void test_get_start_time_process2(void)
{
    time_t t2 = GetProcessStartTime(2);
    assert_int_equal(t2, 100000 / sysconf(_SC_CLK_TCK));
}

static void test_get_start_time_process3(void)
{
    time_t t3 = GetProcessStartTime(3);
    assert_int_equal(t3, PROCESS_START_TIME_UNKNOWN);
}

static void test_get_start_time_process666(void)
{
    time_t t4 = GetProcessStartTime(666);
    assert_int_equal(t4, PROCESS_START_TIME_UNKNOWN);
}


static void test_get_state_process1(void)
{
    ProcessState s = GetProcessState(1);
    assert_int_equal(s, PROCESS_STATE_RUNNING);
}

static void test_get_state_process2(void)
{
    ProcessState s = GetProcessState(2);
    assert_int_equal(s, PROCESS_STATE_STOPPED);
}

static void test_get_state_process3(void)
{
    ProcessState s = GetProcessState(3);
    assert_int_equal(s, PROCESS_STATE_DOES_NOT_EXIST);
}

static void test_get_state_process666(void)
{
    ProcessState s = GetProcessState(666);
    assert_int_equal(s, PROCESS_STATE_DOES_NOT_EXIST);
}


int main()
{
    PRINT_TEST_BANNER();

    const UnitTest tests[] =
    {
        unit_test(test_get_start_time_process1),
        unit_test(test_get_start_time_process2),
        unit_test(test_get_start_time_process3),
        unit_test(test_get_start_time_process666),
        unit_test(test_get_state_process1),
        unit_test(test_get_state_process2),
        unit_test(test_get_state_process3),
        unit_test(test_get_state_process666),
    };

    return run_tests(tests);
}
