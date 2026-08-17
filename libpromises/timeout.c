/*
  Copyright 2024 Northern.tech AS

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

#include <cf3.defs.h>
#include <timeout.h>
#include <process_lib.h>

/* Set by TimeOut() when the alarm fires, so that the caller can tell "the
 * command timed out" from "the command finished". Written from a signal
 * handler, hence volatile sig_atomic_t. */
static volatile sig_atomic_t TIMEOUT_FIRED = 0; /* GLOBAL_X */

/* Set only when TimeOut() actually had a process to signal. The alarm can fire
 * with ALARM_PID already cleared -- cf_pclose() clears it before waiting -- in
 * which case the command timed out but was never terminated, and saying
 * otherwise would be a false statement in an error message. */
static volatile sig_atomic_t TIMEOUT_SIGNALLED = 0; /* GLOBAL_X */

/* Set while a timeout alarm is pending. cf_popen()'s child consults it to
 * decide whether to lead a process group of its own; only a child that may
 * have to be killed as a tree needs one. */
static bool TIMEOUT_ARMED = false; /* GLOBAL_X */

void SetTimeOut(int timeout)
{
    ALARM_PID = -1;
    TIMEOUT_FIRED = 0;
    TIMEOUT_SIGNALLED = 0;
    TIMEOUT_ARMED = true;
    signal(SIGALRM, (void *) TimeOut);
    alarm(timeout);
}

void ClearTimeOut(void)
{
    /* Deliberately leaves TIMEOUT_FIRED and TIMEOUT_SIGNALLED alone: they
     * record what happened to the last armed timeout, and remain readable
     * after the disarm. Only the next SetTimeOut() resets them. */
    alarm(0);
    signal(SIGALRM, SIG_DFL);
    TIMEOUT_ARMED = false;
}

bool TimeOutIsArmed(void)
{
    return TIMEOUT_ARMED;
}

bool TimeOutHasFired(void)
{
    return TIMEOUT_FIRED != 0;
}

bool TimeOutSignalledProcess(void)
{
    return TIMEOUT_SIGNALLED != 0;
}

/*************************************************************************/

void TimeOut()
{
    alarm(0);
    TIMEOUT_FIRED = 1;
    TIMEOUT_ARMED = false;

    if (ALARM_PID != -1)
    {
        TIMEOUT_SIGNALLED = 1;
        Log(LOG_LEVEL_VERBOSE, "Time out of process %jd", (intmax_t)ALARM_PID);

        /* Read the process group while the process is still alive to be read:
         * once GracefulTerminate() has killed it, getpgid() fails with ESRCH and
         * we would have no safe way to tell whether it led a group of its own. */
        const pid_t pgid = getpgid(ALARM_PID);
        if (pgid == -1)
        {
            Log(LOG_LEVEL_WARNING,
                "Could not read the process group of timed-out process %jd (getpgid: %s), not signalling its process group",
                (intmax_t)ALARM_PID, GetErrorStr());
        }

        GracefulTerminate(ALARM_PID, PROCESS_START_TIME_UNKNOWN);

        /* GracefulTerminate() only reaches the process we started. Anything that
         * process spawned survives it, and keeps the pipe open, so the caller
         * stays blocked reading a command it has already given up on.
         *
         * Guarded on the timed-out process leading its own group, which
         * cf_popen()'s child arranges with setpgid(). If that did not take
         * effect the process is still in our group, its pgid is not its pid, and
         * a negative kill() here would signal an unrelated group -- possibly our
         * own. */
        if (pgid == ALARM_PID)
        {
            kill(-ALARM_PID, SIGKILL);
        }
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "%s> Time out", VPREFIX);
    }
}

/*************************************************************************/

time_t SetReferenceTime(void)
{
    time_t tloc;

    if ((tloc = time((time_t *) NULL)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't read system clock. (time: %s)", GetErrorStr());
    }

    CFSTARTTIME = tloc;
    Log(LOG_LEVEL_VERBOSE, "Reference time set to '%s'", ctime(&tloc));

    return tloc;
}
