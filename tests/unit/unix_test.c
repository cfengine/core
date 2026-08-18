#include <test.h>

#include <exec_tools.h>

/* ShellCommandReturnsZero() always reaps the child it forks (either via the
 * WNOHANG poll loop, or via the blocking drain on the pending-termination
 * path), so ALARM_PID must not still name that pid on return -- the pid is
 * immediately recyclable and a later stray alarm firing against it would
 * hit an unrelated process. */

static void test_shell_command_resets_alarm_pid_on_success(void)
{
    ALARM_PID = -999; /* sentinel, distinct from both -1 and any real pid */

    bool result = ShellCommandReturnsZero("true", SHELL_TYPE_USE);

    assert_true(result);
    assert_int_equal(ALARM_PID, -1);
}

static void test_shell_command_resets_alarm_pid_on_nonzero_exit(void)
{
    ALARM_PID = -999;

    bool result = ShellCommandReturnsZero("false", SHELL_TYPE_USE);

    assert_false(result);
    assert_int_equal(ALARM_PID, -1);
}

int main()
{
    PRINT_TEST_BANNER();

    const UnitTest tests[] =
    {
        unit_test(test_shell_command_resets_alarm_pid_on_success),
        unit_test(test_shell_command_resets_alarm_pid_on_nonzero_exit),
    };

    return run_tests(tests);
}
