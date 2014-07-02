#include <test.h>

#include <compiler.h>
#include <process_lib.h>
#include <process_unix_priv.h>

/*
 * procfs.h is not 64-bit off_t clean, but the only affected structure is
 * priovec, which we don't use. Hence we may work around #error in sys/procfs.h
 * by lying that we are not compiling with large file support (while we do).
 */
#define _FILE_OFFSET_BITS 32

#include <procfs.h>

int open(const char *filename, int flags, ...)
{
    if (!strcmp(filename, "/proc/1/psinfo"))
    {
        return 1;
    }

    if (!strcmp(filename, "/proc/1/status"))
    {
        return 101;
    }

    if (!strcmp(filename, "/proc/2/status"))
    {
        return 102;
    }

    errno = ENOENT;
    return -1;
}

int fdpos[3];

ssize_t read(int fd, void *buf, size_t bufsize)
{
    if (fd == 1)
    {
        if (fdpos[0] == 0)
        {
            psinfo_t psinfo;
            psinfo.pr_start.tv_sec = 100;
            memcpy(buf, &psinfo, sizeof(psinfo));
            fdpos[0] = sizeof(psinfo);
            return sizeof(psinfo);
        }
        else
        {
            return 0;
        }
    }

    if (fd == 101)
    {
        if (fdpos[1] == 0)
        {
            pstatus_t pstatus;
            pstatus.pr_lwp.pr_flags = PR_STOPPED;
            pstatus.pr_lwp.pr_why = PR_SIGNALLED;
            memcpy(buf, &pstatus, sizeof(pstatus));
            fdpos[1] = sizeof(pstatus);
            return sizeof(pstatus);
        }
        else
        {
            return 0;
        }
    }

    if (fd == 102)
    {
        if (fdpos[2] == 0)
        {
            pstatus_t pstatus;
            pstatus.pr_lwp.pr_flags = 0;
            pstatus.pr_lwp.pr_why = 0;
            memcpy(buf, &pstatus, sizeof(pstatus));
            fdpos[2] = sizeof(pstatus);
            return sizeof(pstatus);
        }
        else
        {
            return 0;
        }
    }

    errno = EIO;
    return -1;
}

int close(int fd)
{
    return 0;
}

static void test_get_start_time_process1(void)
{
    time_t t = GetProcessStartTime(1);
    assert_int_equal(t, 100);
}

static void test_get_start_time_process2(void)
{
    time_t t = GetProcessStartTime(2);
    assert_int_equal(t, PROCESS_START_TIME_UNKNOWN);
}

static void test_get_state_process1(void)
{
    ProcessState s = GetProcessState(1);
    assert_int_equal(s, PROCESS_STATE_STOPPED);
}

static void test_get_state_process2(void)
{
    ProcessState s = GetProcessState(2);
    assert_int_equal(s, PROCESS_STATE_RUNNING);
}

static void test_get_state_process3(void)
{
    ProcessState s = GetProcessState(3);
    assert_int_equal(s, PROCESS_STATE_DOES_NOT_EXIST);
}

int main()
{
    PRINT_TEST_BANNER();

    const UnitTest tests[] =
    {
        unit_test(test_get_start_time_process1),
        unit_test(test_get_start_time_process2),
        unit_test(test_get_state_process1),
        unit_test(test_get_state_process2),
        unit_test(test_get_state_process3),
    };

    return run_tests(tests);
}
