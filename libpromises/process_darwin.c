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
#include <process_lib.h>
#include <process_unix_priv.h>

#include <sys/param.h>
#include <sys/sysctl.h>

typedef struct
{
    time_t starttime;
    char state;
} ProcessStat;

static bool GetProcessStat(pid_t pid, ProcessStat *state)
{
    int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
    struct kinfo_proc psinfo;
    size_t len = sizeof(psinfo);

    /* Zeroed because a lookup that finds nothing succeeds without writing
     * anything to the buffer, see below. */
    memset(&psinfo, 0, sizeof(psinfo));

    if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &psinfo, &len, NULL, 0) != 0)
    {
        return false;
    }

    /* Unlike other BSDs, KERN_PROC_PID on Darwin reports a pid that does not
     * exist by succeeding and setting the returned length to zero, rather than
     * by failing with ESRCH. Without this check we would read a stale or
     * uninitialised kinfo_proc and report a dead process as running. */
    if (len == 0)
    {
        return false;
    }

    state->starttime = psinfo.kp_proc.p_starttime.tv_sec;

    switch (psinfo.kp_proc.p_stat)
    {
        case SRUN:
        case SIDL:
            state->state = 'R';
            break;
        case SSTOP:
            state->state = 'T';
            break;
        case SSLEEP:
            state->state = 'S';
            break;
        case SZOMB:
            state->state = 'Z';
            break;
        default:
            state->state = 'X';
    }

    return true;
}

time_t GetProcessStartTime(pid_t pid)
{
    ProcessStat st;
    if (GetProcessStat(pid, &st))
    {
        return st.starttime;
    }
    else
    {
        return PROCESS_START_TIME_UNKNOWN;
    }
}

ProcessState GetProcessState(pid_t pid)
{
    ProcessStat st;
    if (GetProcessStat(pid, &st))
    {
        switch (st.state)
        {
        case 'T':
            return PROCESS_STATE_STOPPED;
        case 'Z':
            return PROCESS_STATE_ZOMBIE;
        default:
            return PROCESS_STATE_RUNNING;
        }
    }
    else
    {
        return PROCESS_STATE_DOES_NOT_EXIST;
    }
}
