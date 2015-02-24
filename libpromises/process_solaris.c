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

#include <cf3.defs.h>
#include <process_lib.h>
#include <process_unix_priv.h>
#include <file_lib.h>

/*
 * procfs.h is not 64-bit off_t clean, but the only affected structure is
 * priovec, which we don't use. Hence we may work around #error in sys/procfs.h
 * by lying that we are not compiling with large file support (while we do).
 */
#define _FILE_OFFSET_BITS 32

#include <procfs.h>

static bool GetProcessPsinfo(pid_t pid, psinfo_t *psinfo)
{
    char filename[CF_BUFSIZE];
    snprintf(filename, CF_BUFSIZE, "/proc/%d/psinfo", (int)pid);

    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        return false;
    }

    int res = FullRead(fd, psinfo, sizeof(*psinfo));
    close(fd);
    return res == sizeof(*psinfo);
}

time_t GetProcessStartTime(pid_t pid)
{
    psinfo_t psinfo;
    if (GetProcessPsinfo(pid, &psinfo))
    {
        return psinfo.pr_start.tv_sec;
    }
    else
    {
        return PROCESS_START_TIME_UNKNOWN;
    }
}

static bool GetProcessPstatus(pid_t pid, pstatus_t *pstatus)
{
    char filename[CF_BUFSIZE];
    snprintf(filename, CF_BUFSIZE, "/proc/%d/status", (int)pid);

    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        return false;
    }

    int res = FullRead(fd, pstatus, sizeof(*pstatus));
    close(fd);
    return res == sizeof(*pstatus);
}

ProcessState GetProcessState(pid_t pid)
{
    pstatus_t pstatus;
    if (GetProcessPstatus(pid, &pstatus))
    {
        if (pstatus.pr_lwp.pr_flags & PR_STOPPED)
        {
            return PROCESS_STATE_STOPPED;
        }
        else
        {
            return PROCESS_STATE_RUNNING;
        }
    }
    else
    {
        return PROCESS_STATE_DOES_NOT_EXIST;
    }
}
