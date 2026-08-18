#include <test.h>

#include <cf3.defs.h>
#include <cf3.extern.h>
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
    WaitForAlarm();
    assert_true(TimeOutHasFired());

    SetTimeOut(3600);
    assert_true(TimeOutIsArmed());
    assert_false(TimeOutHasFired());
    assert_false(TimeOutSignalledProcess());
    ClearTimeOut();
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
        unit_test(test_next_set_resets_the_record)
    };

    PRINT_TEST_BANNER();
    return run_tests(tests);
}
