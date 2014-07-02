#include <test.h>

#include <compiler.h>
#include <process_lib.h>
#include <process_unix_priv.h>

#include <procinfo.h>

/*
 * AIX 5.3 is missing this declaration
 */
#ifndef GETPROCS64
int getprocs64(struct procentry64 *, int, struct fdsinfo64 *, int, pid_t *, int);

int getprocs64(struct procentry64* pe, int process_size, struct fdsinfo64 *fi, int files_size, pid_t* pid, int count)
{
    assert_int_equal(count, 1);
    assert_true(fi == NULL);

    switch (*pid)
    {
    /* Normal process, running, started 1 jan 2000 00:00:00 */
    case 1:
        memset(pe, 0, sizeof(struct procentry64));
        pe->pi_pid = 1;
        pe->pi_start = 946681200;
        pe->pi_state = SACTIVE;
        *pid = 2;
        return 1;

    /* Normal process, stopped, started 31 dec 1980 23:59:59 */
    case 2:
        memset(pe, 0, sizeof(struct procentry64));
        pe->pi_pid = 2;
        pe->pi_start = 347151599;
        pe->pi_state = SSTOP;
        *pid = 3;
        return 1;

    /* Permission denied, getprocs64 returns EINVAL */
    case 666:
        errno = EINVAL;
        return -1;

    /* Non-existing process, getprocs64 returns another process' info */
    case 1000:
        memset(pe, 0, sizeof(struct procentry64));
        pe->pi_pid = 1001;
        pe->pi_start = 312312313;
        pe->pi_state = SACTIVE;
        *pid = 1002;
        return 1;

    /* Non-existing process, table sentinel. getprocs64 return 0 */
    case 1000000:
        return 0;
    }
}
#endif
static void test_get_start_time_process1(void)
{
    time_t t = GetProcessStartTime(1);
    assert_int_equal(t, 946681200);
}


static void test_get_start_time_process2(void)
{
    time_t t2 = GetProcessStartTime(2);
    assert_int_equal(t2, 347151599);
}

static void test_get_start_time_process666(void)
{
    time_t t = GetProcessStartTime(666);
    assert_int_equal(t, PROCESS_START_TIME_UNKNOWN);
}

static void test_get_start_time_process1000(void)
{
    time_t t = GetProcessStartTime(1000);
    assert_int_equal(t, PROCESS_START_TIME_UNKNOWN);
}

static void test_get_start_time_process1000000(void)
{
    time_t t = GetProcessStartTime(1000000);
    assert_int_equal(t, PROCESS_START_TIME_UNKNOWN);
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

static void test_get_state_process666(void)
{
    ProcessState s = GetProcessState(666);
    assert_int_equal(s, PROCESS_STATE_DOES_NOT_EXIST);
}

static void test_get_state_process1000(void)
{
    ProcessState s = GetProcessState(1000);
    assert_int_equal(s, PROCESS_STATE_DOES_NOT_EXIST);
}

static void test_get_state_process1000000(void)
{
    ProcessState s = GetProcessState(1000000);
    assert_int_equal(s, PROCESS_STATE_DOES_NOT_EXIST);
}


int main()
{
    PRINT_TEST_BANNER();

    const UnitTest tests[] =
    {
        unit_test(test_get_start_time_process1),
        unit_test(test_get_start_time_process2),
        unit_test(test_get_start_time_process666),
        unit_test(test_get_start_time_process1000),
        unit_test(test_get_start_time_process1000000),
        unit_test(test_get_state_process1),
        unit_test(test_get_state_process2),
        unit_test(test_get_state_process666),
        unit_test(test_get_state_process1000),
        unit_test(test_get_state_process1000000),
    };

    return run_tests(tests);
}
