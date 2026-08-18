#include <test.h>

#include <cf3.defs.h>
#include <cf3.extern.h>
#include <pipes.h>
#include <timeout.h>

/* The alarm handler interrupts the sleep, so this normally returns as soon as
 * it fires. Loop anyway: the alarm may already have fired before we get here,
 * and a sleep interrupted by anything else must not end the wait early. */
static void WaitForAlarm(void)
{
    for (int i = 0; (i < 30) && !TimeOutHasFired(); i++)
    {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 100 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
}

static void test_set_arms_and_resets_the_record(void)
{
    SetTimeOut(3600);
    assert_true(TimeOutIsArmed());
    assert_false(TimeOutHasFired());
    assert_false(TimeOutSignalledProcess());
    ClearTimeOut();
}

/* A command that finished in time must not leave the flag set for the next,
 * unrelated, child: it is what decides whether cf_popen()'s child puts itself
 * in a process group of its own. */
static void test_clear_disarms(void)
{
    SetTimeOut(3600);
    ClearTimeOut();
    assert_false(TimeOutIsArmed());
}

static void test_fired_alarm_without_a_process(void)
{
    SetTimeOut(1);
    /* Production starts the clock only once a pid is registered; start it by
     * hand to reach the handler's no-process branch. */
    StartTimeOutClock();
    WaitForAlarm();

    assert_true(TimeOutHasFired());
    /* Nothing else disarms on this path -- the handler does it itself. */
    assert_false(TimeOutIsArmed());
    /* SetTimeOut() cleared ALARM_PID, so there was no process to signal and
     * the caller must not describe the command as terminated. */
    assert_false(TimeOutSignalledProcess());
}

/* ClearTimeOut() runs after the command is reaped, before the caller reports
 * on it. Clearing the record there would make a timed-out command whose exit
 * status reads as success indistinguishable from one that finished. */
static void test_clear_preserves_the_record(void)
{
    SetTimeOut(1);
    StartTimeOutClock();
    WaitForAlarm();
    assert_true(TimeOutHasFired());

    ClearTimeOut();
    assert_false(TimeOutIsArmed());
    assert_true(TimeOutHasFired());
    assert_false(TimeOutSignalledProcess());
}

/* The other tests never give TimeOut() a process to signal, so they cannot
 * tell a ClearTimeOut() that wipes a TRUE TimeOutSignalledProcess() from one
 * that leaves it alone -- both look identical when the flag started false.
 * Fork a real child and let the alarm reach it. */
static void test_clear_preserves_a_true_signalled_flag(void)
{
    pid_t child = fork();
    if (child == 0)
    {
        /* Long enough to still be alive when TimeOut() runs GracefulTerminate()
         * on it; short enough that a leaked child does not linger. */
        sleep(5);
        _exit(0);
    }
    assert_true(child > 0);

    SetTimeOut(1);
    ALARM_PID = child;
    /* Same order as cf_popen(): publish the pid, then start the clock. */
    StartTimeOutClock();
    WaitForAlarm();

    assert_true(TimeOutHasFired());
    assert_true(TimeOutSignalledProcess());

    ClearTimeOut();
    assert_false(TimeOutIsArmed());
    assert_true(TimeOutSignalledProcess());

    int status;
    waitpid(child, &status, 0);
}

static void test_next_set_resets_the_record(void)
{
    SetTimeOut(1);
    StartTimeOutClock();
    WaitForAlarm();
    assert_true(TimeOutHasFired());

    SetTimeOut(3600);
    assert_true(TimeOutIsArmed());
    assert_false(TimeOutHasFired());
    assert_false(TimeOutSignalledProcess());
    ClearTimeOut();
}

/* The arming-order half of the timeout guarantee: the clock must not run
 * before cf_popen() has a pid to kill. Arming immediately, as SetTimeOut()
 * used to, lets the alarm fire during the caller's own setup -- umask(),
 * logging, cf_popen dispatch -- and burn the whole timeout on nothing, after
 * which the command runs unbounded.
 *
 * Deliberately not a fixed-wait liveness check: it waits twice the timeout
 * before forking at all, so the old behaviour fires with certainty rather than
 * by timing luck, and the command it then runs outlives its timeout by 4s. */
static void test_clock_does_not_run_before_the_fork(void)
{
    SetTimeOut(1);

    for (int i = 0; (i < 20) && !TimeOutHasFired(); i++)
    {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 100 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
    /* Two seconds into a one-second timeout, with no child yet. */
    assert_false(TimeOutHasFired());

    FILE *pp = cf_popen("/bin/sleep 5", "r", true);
    assert_true(pp != NULL);

    /* Ends at EOF, which arrives when the alarm terminates the child. */
    char buf[64];
    while (fread(buf, 1, sizeof(buf), pp) > 0)
    {
    }
    cf_pclose(pp);

    assert_true(TimeOutHasFired());
    assert_true(TimeOutSignalledProcess());
    ClearTimeOut();
}

static void test_set_leaves_the_clock_stopped(void)
{
    SetTimeOut(3600);
    assert_true(TimeOutIsArmed());
    /* alarm(0) returns the seconds left on a running clock, and 0 if none is
     * running. Armed, but not yet ticking. */
    assert_int_equal(alarm(0), 0);
    ClearTimeOut();
}

/* One-shot: a second fork under the same timeout runs on the time already
 * ticking, so the second start must not restart the command's budget. The
 * second StartTimeOutClock() is deliberately issued against a *running* clock
 * -- reading it with alarm(0) first would cancel it, and a starter that resets
 * a live timer would then look identical to one that leaves it alone. */
static void test_start_runs_the_clock_once(void)
{
    SetTimeOut(5);
    StartTimeOutClock();

    struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
    nanosleep(&ts, NULL);

    StartTimeOutClock();

    const int left = alarm(0);
    assert_true(left > 0);
    /* The remainder of the original 5s, not a fresh 5s. */
    assert_true(left <= 4);
    ClearTimeOut();
}

/* A timeout armed but never followed by a fork -- cf_popen() failing, or a
 * caller returning early -- must be fully retired, not left able to start. */
static void test_clear_retires_an_unstarted_clock(void)
{
    SetTimeOut(3600);
    ClearTimeOut();

    StartTimeOutClock();
    assert_int_equal(alarm(0), 0);
    assert_false(TimeOutIsArmed());
}

int main()
{
    const UnitTest tests[] =
    {
        unit_test(test_set_arms_and_resets_the_record),
        unit_test(test_clear_disarms),
        unit_test(test_fired_alarm_without_a_process),
        unit_test(test_clear_preserves_the_record),
        unit_test(test_clear_preserves_a_true_signalled_flag),
        unit_test(test_next_set_resets_the_record),
        unit_test(test_clock_does_not_run_before_the_fork),
        unit_test(test_set_leaves_the_clock_stopped),
        unit_test(test_start_runs_the_clock_once),
        unit_test(test_clear_retires_an_unstarted_clock)
    };

    PRINT_TEST_BANNER();
    return run_tests(tests);
}
