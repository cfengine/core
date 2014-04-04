#include <test.h>

#include <compiler.h>
#include <process_lib.h>
#include <process_unix_priv.h>

/* This mock implements single fake process with a several tunable parameters:
   - process' start time,
   - reaction time for STOP/CONT/INT/TERM/KILL signal,
   - whether SIGINT/SIGTERM are blocked,
   - does the caller have permission to send signals to fake process

   This mock also records what signals were delivered to the process to check later.
*/

/* Settings */

time_t proc_1_start_time;
time_t proc_1_reaction_time;
bool proc_1_int_blocked;
bool proc_1_term_blocked;
bool proc_1_have_access;

/* State */

time_t current_time;

bool exists;
bool stopped;

time_t signal_time;
bool has_stop;
bool has_cont;
bool has_int;
bool has_term;
bool has_kill;

/* History */

bool was_stopped;
int exit_signal;

void InitTime(void)
{
    current_time = 1;
}

void InitFakeProcess(time_t start_time, time_t reaction_time,
                     bool int_blocked, bool term_blocked, bool have_access)
{
    proc_1_start_time = start_time;
    proc_1_reaction_time = reaction_time;
    proc_1_int_blocked = int_blocked;
    proc_1_term_blocked = term_blocked;
    proc_1_have_access = have_access;

    exists = true;
    stopped = false;

    signal_time = -1;
    has_stop = false;
    has_cont = false;
    has_int = false;
    has_term = false;
    has_kill = false;

    was_stopped = false;
    exit_signal = 0;
}

time_t GetProcessStartTime(pid_t pid)
{
    assert_int_equal(pid, 1);

    if (proc_1_have_access && exists)
    {
        return proc_1_start_time;
    }
    else
    {
        return PROCESS_START_TIME_UNKNOWN;
    }
}

ProcessState GetProcessState(pid_t pid)
{
    assert_int_equal(pid, 1);

    if (!proc_1_have_access || !exists)
    {
        return PROCESS_STATE_DOES_NOT_EXIST;
    }

    if (stopped)
    {
        return PROCESS_STATE_STOPPED;
    }
    else
    {
        return PROCESS_STATE_RUNNING;
    }
}

int kill(pid_t pid, int signal)
{
    assert_int_equal(pid, 1);

    if (!proc_1_have_access)
    {
        errno = EPERM;
        return -1;
    }

    if (!exists)
    {
        errno = ESRCH;
        return -1;
    }

    if (signal == 0)
    {
        return 0;
    }

    if (signal_time == -1)
    {
        signal_time = current_time + proc_1_reaction_time;
    }

    if (signal == SIGSTOP)
    {
        has_stop = true;
    }
    else if (signal == SIGCONT)
    {
        has_cont = true;
    }
    else if (signal == SIGINT)
    {
        has_int = true;
    }
    else if (signal == SIGTERM)
    {
        has_term = true;
    }
    else if (signal == SIGKILL)
    {
        has_kill = true;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

void FakeProcessDoSignals(void)
{
    if (has_stop)
    {
        stopped = true;
        was_stopped = true;
        has_stop = false;
    }

    if (has_cont)
    {
        stopped = false;
        has_cont = false;
    }

    if (has_int)
    {
        if (!proc_1_int_blocked)
        {
            exists = false;
            exit_signal = SIGINT;
            stopped = false;
            signal_time = -1;
        }
        has_int = false;
    }

    if (has_term)
    {
        if (!proc_1_term_blocked)
        {
            exists = false;
            exit_signal = SIGTERM;
            stopped = false;
            signal_time = -1;
        }
        has_term = false;
    }

    if (has_kill)
    {
        exists = false;
        exit_signal = SIGKILL;
        stopped = false;
        signal_time = -1;

        has_kill = false;
    }

    signal_time = -1;
}


int nanosleep(const struct timespec *req, struct timespec *rem)
{
    time_t sleep_time;

    /* Simulate EINTR every second time */

    static bool got_eintr = false;

    if (!got_eintr)
    {
        got_eintr = true;
        sleep_time = 2 * req->tv_nsec / 3;
    }
    else
    {
        got_eintr = false;
        sleep_time = req->tv_nsec;
    }

    time_t next_time = current_time + sleep_time;
    if (signal_time != -1 && next_time >= signal_time)
    {
        FakeProcessDoSignals();
    }

    current_time = next_time;

    if (got_eintr)
    {
        rem->tv_sec = 0;
        rem->tv_nsec = req->tv_nsec - sleep_time;
        errno = EINTR;
        return -1;
    }
    else
    {
        return 0;
    }
}

/* Tests */

void test_kill_simple_process(void)
{
    InitTime();
    InitFakeProcess(12345, 100, false, false, true);

    int res = GracefulTerminate(1, 12345);
    assert_true(res);

    FakeProcessDoSignals();

    assert_false(exists);
    assert_int_equal(exit_signal, SIGINT);
}

void test_kill_wrong_process(void)
{
    InitTime();
    InitFakeProcess(66666, 100, false, false, true);

    int res = GracefulTerminate(1, 12345);
    assert_true(res);

    FakeProcessDoSignals();

    assert_true(exists);
    assert_false(stopped);
    assert_false(was_stopped); /* We should not touch this process at all */
    assert_int_equal(signal_time, (time_t)-1); /* No pending signals either */
}

void test_kill_long_reacting_signal(void)
{
    /* This process is very slow in reaction. It should not be left stopped though */
    InitTime();
    InitFakeProcess(12345, 2000000000, false, false, true);

    int res = GracefulTerminate(1, 12345);
    assert_true(res);

    FakeProcessDoSignals();

    assert_true(exists); /* We should not kill this process */
    assert_false(stopped); /* It should either be running or waiting to process SIGCONT */
}

void test_kill_no_sigint(void)
{
    /* This process blocks SIGINT */
    InitTime();
    InitFakeProcess(12345, 100, true, false, true);

    int res = GracefulTerminate(1, 12345);
    assert_true(res);

    FakeProcessDoSignals();

    assert_false(exists);
    assert_int_equal(exit_signal, SIGTERM);
}

void test_kill_no_sigint_sigterm(void)
{
    /* This process only can be killed by SIGKILL */
    InitTime();
    InitFakeProcess(12345, 100, true, true, true);

    int res = GracefulTerminate(1, 12345);
    assert_true(res);

    FakeProcessDoSignals();

    assert_false(exists);
    assert_int_equal(exit_signal, SIGKILL);
}

void test_kill_anothers_process(void)
{
    /* This process is not owned by killer */
    InitTime();
    InitFakeProcess(12345, 100, true, true, false);

    int res = GracefulTerminate(1, 12345);
    assert_true(res);

    FakeProcessDoSignals();

    assert_true(exists);
    assert_false(was_stopped);
}

int main()
{
    PRINT_TEST_BANNER();

    const UnitTest tests[] =
    {
        unit_test(test_kill_simple_process),
        unit_test(test_kill_wrong_process),
        unit_test(test_kill_long_reacting_signal),
        unit_test(test_kill_no_sigint),
        unit_test(test_kill_no_sigint_sigterm),
        unit_test(test_kill_anothers_process),
    };

    return run_tests(tests);
}
