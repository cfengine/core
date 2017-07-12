/*
   Copyright 2017 Northern.tech AS

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

#include <cf3.defs.h>

#include <process_lib.h>
#include <process_unix_priv.h>
#include <files_lib.h>


typedef struct
{
    time_t starttime;
    char state;
} ProcessStat;

static bool GetProcessStat(pid_t pid, ProcessStat *state)
{
    char filename[64];
    xsnprintf(filename, sizeof(filename), "/proc/%jd/stat", (intmax_t) pid);

    int fd;
    for (;;)
    {
        if ((fd = open(filename, O_RDONLY)) != -1)
        {
            break;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (errno == ENOENT || errno == ENOTDIR)
        {
            return false;
        }

        if (errno == EACCES)
        {
            return false;
        }

        assert (fd != -1 && "Unable to open /proc/<pid>/stat");
    }

    char stat[CF_BUFSIZE];
    int res = FullRead(fd, stat, sizeof(stat) - 1); /* -1 for the '\0', below */
    close(fd);

    if (res < 0)
    {
        return false;
    }
    assert(res < CF_BUFSIZE);
    stat[res] = '\0'; /* read() doesn't '\0'-terminate */

    /* stat entry is of form: <pid> (<task name>) <various info...>
     * To avoid choking on weird task names, we search for the closing
     * parenthesis first: */
    char *p = memrchr(stat, ')', res);
    if (p == NULL)
    {
        /* Wrong field format! */
        return false;
    }

    p++; // Skip the parenthesis

    char proc_state[2];
    unsigned long long starttime;

    if (sscanf(p,
               "%1s" /* state */
               "%*s" /* ppid */
               "%*s" /* pgrp */
               "%*s" /* session */
               "%*s" /* tty_nr */
               "%*s" /* tpgid */
               "%*s" /* flags */
               "%*s" /* minflt */
               "%*s" /* cminflt */
               "%*s" /* majflt */
               "%*s" /* cmajflt */
               "%*s" /* utime */
               "%*s" /* stime */
               "%*s" /* cutime */
               "%*s" /* cstime */
               "%*s" /* priority */
               "%*s" /* nice */
               "%*s" /* num_threads */
               "%*s" /* itrealvalue */
               "%llu" /* starttime */,
               proc_state,
               &starttime) != 2)
    {
        return false;
    }

    state->state = proc_state[0];
    state->starttime = (time_t)(starttime / sysconf(_SC_CLK_TCK));

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
