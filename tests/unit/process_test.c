/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <test.h>

#include <logging.h>
#include <process_lib.h>
#include <process_unix_priv.h>


pid_t SPAWNED_PID;
pid_t THIS_PID;
time_t THIS_STARTTIME;


static void test_process_start_time(void)
{
    /* Wait a couple of seconds so that process start time differs. */
    printf("Sleeping 2 seconds...\n");
    sleep(2);

    pid_t new_pid = fork();
    assert_true(new_pid >= 0);

    if (new_pid == 0)                                           /* child */
    {
        execl("/bin/sleep", "/bin/sleep", "30", NULL);
        assert_true(false);                                  /* unreachable */
    }

    SPAWNED_PID = new_pid;
    time_t newproc_starttime = GetProcessStartTime(new_pid);

    printf("Spawned a \"sleep\" child with PID %jd and start_time %jd\n",
           (intmax_t) new_pid, (intmax_t) newproc_starttime);

    // We might have slipped by a few seconds, but shouldn't be much.
    assert_int_not_equal(newproc_starttime, PROCESS_START_TIME_UNKNOWN);
    assert_true(newproc_starttime >= THIS_STARTTIME + 1);
    assert_true(newproc_starttime <= THIS_STARTTIME + 15);

    kill(new_pid, SIGKILL);
    wait(NULL);
    SPAWNED_PID = 0;
}

static void test_process_state(void)
{
    int ret;

    pid_t new_pid = fork();
    assert_true(new_pid >= 0);

    if (new_pid == 0)                                           /* child */
    {
        execl("/bin/sleep", "/bin/sleep", "30", NULL);
        assert_true(false);                                  /* unreachable */
    }

    SPAWNED_PID = new_pid;
    printf("Spawned a \"sleep\" child with PID %d\n", new_pid);

    int state = -1000;

    for (int c = 0; c < 10; c++)
    {
        state = GetProcessState(new_pid);
        if (state == PROCESS_STATE_RUNNING)
        {
            break;
        }
        else
        {
            usleep(200000);
        }
    }
    printf("Started, state: %d\n", state);
    assert_int_equal(state, PROCESS_STATE_RUNNING);

    ret = kill(new_pid, SIGSTOP);
    assert_int_equal(ret, 0);

    for (int c = 0; c < 10; c++)
    {
        state = GetProcessState(new_pid);
        if (state == PROCESS_STATE_STOPPED)
        {
            break;
        }
        else
        {
            usleep(200000);
        }
    }
    printf("Stopped, state: %d\n", state);
    assert_int_equal(state, PROCESS_STATE_STOPPED);

    ret = kill(new_pid, SIGCONT);
    assert_int_equal(ret, 0);

    for (int c = 0; c < 10; c++)
    {
        state = GetProcessState(new_pid);
        if (state == PROCESS_STATE_RUNNING)
        {
            break;
        }
        else
        {
            usleep(200000);
        }
    }
    printf("Resumed, state: %d\n", state);
    assert_int_equal(state, PROCESS_STATE_RUNNING);

    /* Terminate the child process and reap the zombie. */
    kill(new_pid, SIGKILL);
    wait(NULL);

    state = GetProcessState(new_pid);
    printf("Killed,  state: %d\n", state);
    assert_int_equal(state, PROCESS_STATE_DOES_NOT_EXIST);

    SPAWNED_PID = 0;
}

static void test_graceful_terminate(void)
{
    int ret, state;

    pid_t new_pid = fork();
    assert_true(new_pid >= 0);

    if (new_pid == 0)                                           /* child */
    {
        execl("/bin/sleep", "/bin/sleep", "30", NULL);
        assert_true(false);                                  /* unreachable */
    }

    time_t start_time = GetProcessStartTime(new_pid);
    SPAWNED_PID = new_pid;

    printf("Spawned a \"sleep\" child with PID %jd and start_time %jd\n",
           (intmax_t) new_pid, (intmax_t) start_time);

    state = GetProcessState(new_pid);
    assert_int_equal(state, PROCESS_STATE_RUNNING);

    printf("Killing child with wrong start_time, child should not die...\n");

    ret = GracefulTerminate(new_pid, 12345);             /* fake start time */
    assert_false(ret);

    state = GetProcessState(new_pid);
    assert_int_equal(state, PROCESS_STATE_RUNNING);

    printf("Killing child with correct start_time, child should die...\n");

    ret = GracefulTerminate(new_pid, start_time);
    assert_true(ret);

    state = GetProcessState(new_pid);
    assert_int_equal(state, PROCESS_STATE_ZOMBIE);

    wait(NULL);                                               /* reap child */

    state = GetProcessState(new_pid);
    assert_int_equal(state, PROCESS_STATE_DOES_NOT_EXIST);

    printf("Child Dead!\n");
    SPAWNED_PID = 0;

    printf("Killing ourself, should fail...\n");
    ret = GracefulTerminate(THIS_PID, THIS_STARTTIME);
    assert_false(ret);

    printf("Killing ourself without specifying starttime, should fail...\n");
    ret = GracefulTerminate(THIS_PID, PROCESS_START_TIME_UNKNOWN);
    assert_false(ret);
}


int main()
{
    PRINT_TEST_BANNER();

    /* Don't miss the messages about GetProcessStartTime not implemented. */
    LogSetGlobalLevel(LOG_LEVEL_DEBUG);

    THIS_PID       = getpid();
    THIS_STARTTIME = GetProcessStartTime(THIS_PID);

    printf("This parent process has PID %jd and start_time %jd\n",
           (intmax_t) THIS_PID, (intmax_t) THIS_STARTTIME);

    const UnitTest tests[] =
    {
        unit_test(test_process_start_time),
        unit_test(test_process_state),
        unit_test(test_graceful_terminate)
    };

    int ret = run_tests(tests);

    /* Make sure no child is alive. */
    if (SPAWNED_PID > 0)
    {
        kill(SPAWNED_PID, SIGKILL);
    }

    return ret;
}
