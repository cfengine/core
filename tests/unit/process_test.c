/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#include <process_lib.h>
#include <process_unix_priv.h>

#include <unistd.h>

static void test_process_start_time(void)
{
    int this_pid, new_pid;
    time_t this_starttime;

    this_pid = getpid();
    this_starttime = GetProcessStartTime(this_pid);

    sleep(1);

    new_pid = fork();

    assert_true(new_pid >= 0);
    if (new_pid == 0)
    {
        execl("/bin/sleep", "/bin/sleep", "5", NULL);
        assert_true(false);
    }

    time_t newproc_starttime = GetProcessStartTime(new_pid);
    // We might have slipped by a few seconds, but shouldn't be much.
    assert_true(newproc_starttime >= this_starttime + 1 && newproc_starttime <= this_starttime + 5);

    kill(new_pid, SIGKILL);
}

static void test_process_state(void)
{
    int new_pid;

    new_pid = fork();

    assert_true(new_pid >= 0);
    if (new_pid == 0)
    {
        execl("/bin/sleep", "/bin/sleep", "5", NULL);
        assert_true(false);
    }

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
    assert_int_equal(state, PROCESS_STATE_RUNNING);

    kill(new_pid, SIGSTOP);
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
    assert_int_equal(state, PROCESS_STATE_STOPPED);

    kill(new_pid, SIGCONT);
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
    assert_int_equal(state, PROCESS_STATE_RUNNING);

    kill(new_pid, SIGKILL);
}

int main()
{
    PRINT_TEST_BANNER();

    const UnitTest tests[] =
    {
        unit_test(test_process_start_time),
        unit_test(test_process_state),
    };

    return run_tests(tests);
}
