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

/* All three are written from the signal handler, hence sig_atomic_t. */

/* The alarm fired. */
static volatile sig_atomic_t TIMEOUT_FIRED = 0; /* GLOBAL_X */

/* ...and had a process to signal. The alarm can fire with ALARM_PID already
 * cleared, i.e. timed out but never terminated. */
static volatile sig_atomic_t TIMEOUT_SIGNALLED = 0; /* GLOBAL_X */

/* An alarm is pending. */
static volatile sig_atomic_t TIMEOUT_ARMED = 0; /* GLOBAL_X */

void SetTimeOut(int timeout)
{
    ALARM_PID = -1;
    TIMEOUT_FIRED = 0;
    TIMEOUT_SIGNALLED = 0;
    TIMEOUT_ARMED = 1;
    signal(SIGALRM, (void *) TimeOut);
    alarm(timeout);
}

void ClearTimeOut(void)
{
    /* Leaves TIMEOUT_FIRED/TIMEOUT_SIGNALLED readable after the disarm; only
     * SetTimeOut() resets them. */
    alarm(0);
    signal(SIGALRM, SIG_DFL);
    TIMEOUT_ARMED = 0;
}

bool TimeOutIsArmed(void)
{
    return TIMEOUT_ARMED != 0;
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
    TIMEOUT_ARMED = 0;

    if (ALARM_PID != -1)
    {
        TIMEOUT_SIGNALLED = 1;
        Log(LOG_LEVEL_VERBOSE, "Time out of process %jd", (intmax_t)ALARM_PID);

#ifndef __MINGW32__
        /* Must be read before GracefulTerminate(): afterwards getpgid() fails
         * with ESRCH. */
        const pid_t pgid = getpgid(ALARM_PID);
        if (pgid == -1)
        {
            Log(LOG_LEVEL_WARNING,
                "Could not read the process group of timed-out process %jd (getpgid: %s), not signalling its process group",
                (intmax_t)ALARM_PID, GetErrorStr());
        }
#endif

        GracefulTerminate(ALARM_PID, PROCESS_START_TIME_UNKNOWN);

#ifndef __MINGW32__
        /* GracefulTerminate() reaches only the process we started; its
         * descendants keep the pipe open. The pgid check matters: if setpgid()
         * in cf_popen()'s child did not take effect, the process is still in
         * our group and a negative kill() would signal us. */
        if (pgid == ALARM_PID)
        {
            kill(-ALARM_PID, SIGKILL);
        }
#endif
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
