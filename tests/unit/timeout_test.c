#include <test.h>

#include <cf3.defs.h>
#include <cf3.extern.h>
#include <timeout.h>
#include <pipes.h>

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

/* CFE-4727: cf_pclose() used to clear ALARM_PID before waiting for the child,
 * so a command that closed its output but kept running was unreachable by the
 * time the alarm fired -- TimeOut() found no process to signal, and the full
 * sleep ran out inside an unbounded waitpid(). The pid must stay registered
 * until the reap, so an alarm firing during it still terminates the child.
 *
 * The sleep is 30 seconds and the elapsed bound 25 because the termination
 * can take a while: each of the ladder's two waits is nominally 1 second but
 * iteration-counted, which overshoots several-fold on Darwin (CFE-4728), and
 * with no process_darwin.c the ladder cannot see the child die and burns its
 * full waits regardless (CFE-4718). Terminated is ~2+2 seconds where those
 * are fixed and up to ~20 here; only running to completion reaches 30.
 *
 * cf_popen_sh() runs before SetTimeOut(), not after: arming first (as
 * verify_exec.c does) leaves a window between the alarm being armed and
 * cf_popen()'s fork actually publishing the child's pid into ALARM_PID, and
 * under load the 2 second alarm can fire inside that window -- observed
 * directly under a parallel `make check`, not theoretical. That race is
 * real but is not what this test pins; forking first and registering the
 * already-known pid, the same way test_clear_preserves_a_true_signalled_flag
 * does above, removes it from this test so a flake there cannot masquerade
 * as a regression here. The payload chains through exec so the timed
 * process is the pid cf_popen_sh() returned, with no intermediate shell
 * left to hold the pipe open once it is killed. */
static void test_pclose_leaves_the_alarm_its_process(void)
{
    FILE *pfp = cf_popen_sh("exec 1>&- 2>&-; exec sleep 30", "r");
    assert_true(pfp != NULL);
    pid_t child;
    assert_true(PipeToPid(&child, pfp));

    SetTimeOut(2);
    ALARM_PID = child;
    time_t start = time(NULL);

    char buf[64];
    while (fgets(buf, sizeof(buf), pfp) != NULL)
    {
    }

    int ret = cf_pclose(pfp);
    time_t elapsed = time(NULL) - start;
    ClearTimeOut();

    assert_true(TimeOutHasFired());
    /* The blocking wait in cf_pclose() is where the alarm found the child. */
    assert_true(TimeOutSignalledProcess());
    /* A terminated child cannot have run its 30 second sleep out. */
    assert_true(elapsed < 25);
    /* And it did not run to completion's exit 0. */
    assert_int_not_equal(0, ret);
    /* The reap still deregisters the pid afterwards. */
    assert_true(ALARM_PID == -1);
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
        unit_test(test_pclose_leaves_the_alarm_its_process),
        unit_test(test_next_set_resets_the_record)
    };

    PRINT_TEST_BANNER();
    return run_tests(tests);
}
