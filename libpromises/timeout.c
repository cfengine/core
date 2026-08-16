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

void SetTimeOut(int timeout)
{
    ALARM_PID = -1;
    signal(SIGALRM, (void *) TimeOut);
    alarm(timeout);
}

/*************************************************************************/

void TimeOut()
{
    alarm(0);

    if (ALARM_PID != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Time out of process %jd", (intmax_t)ALARM_PID);

        /* Read the process group while the process is still alive to be read:
         * once GracefulTerminate() has killed it, getpgid() fails with ESRCH and
         * we would have no safe way to tell whether it led a group of its own. */
        const pid_t pgid = getpgid(ALARM_PID);

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
