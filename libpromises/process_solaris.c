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
    snprintf(filename, CF_BUFSIZE, "/proc/%jd/status", (intmax_t) pid);

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
/* This is the documented way to check for zombie process, on Solaris 9
 * however I couldn't get it to work: Killing a child process and then reading
 * /proc/PID/psinfo *before* reaping the dead child, resulted in
 * psinfo.pr_nlwp == 1 and psinfo.pr_lwp.pr_lwpid == 1.
 */

#if 0
    psinfo_t psinfo;
    if (GetProcessPsinfo(pid, &psinfo))
    {
        if (psinfo.pr_nlwp == 0 &&
            psinfo.pr_lwp.pr_lwpid == 0)
        {
            return PROCESS_STATE_ZOMBIE;
        }
        else
        {
            /* Then, we must read the "status" file to get
             * pstatus.pr_lwp.pr_flags, because the psinfo.pr_lwp.pr_flag is
             * deprecated. */
            pstatus_t pstatus;
            if (GetProcessPstatus(pid &pstatus))
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
        }
    }

    return PROCESS_STATE_DOES_NOT_EXIST;
#endif



    /* HACK WARNING: By experimentation I figured out that on Solaris 9 there
       is no clear way to figure out if a process is zombie, but if the
       "status" file is not there while the "psinfo" file is, then it's most
       probably a zombie. */

    pstatus_t pstatus;
    bool success = GetProcessPstatus(pid, &pstatus);

    if (!success && errno == ENOENT)                 /* file does not exist */
    {
        psinfo_t psinfo;
        if (GetProcessPsinfo(pid, &psinfo))
        {
            /* /proc/PID/psinfo exists, /proc/PID/status does not exist */
            return PROCESS_STATE_ZOMBIE;
        }
        else                   /* Neither status nor psinfo could be opened */
        {
            return PROCESS_STATE_DOES_NOT_EXIST;
        }
    }

    /* Read the flags from status, since reading it from
       psinfo is deprecated. */
    if (success)
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

    /* If we reach this point it's probably because /proc/PID/status couldn't
     * be opened because of EPERM. We can't do anything to that process, so we
     * might as well pretend it does not exist. */

    return PROCESS_STATE_DOES_NOT_EXIST;
}
