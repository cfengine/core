/*
   Copyright 2018 Northern.tech AS

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
#include <files_lib.h>

#include <procinfo.h>

/*
 * AIX 5.3 is missing this declaration
 */
#ifndef HAVE_GETPROCS64
int getprocs64(void *procsinfo, int sizproc, void *fdsinfo, int sizfd, pid_t *index, int count);
#endif
static bool FillProcEntry(struct procentry64* pe, pid_t pid)
{
    pid_t nextpid = pid;
    int ret = getprocs64(pe, sizeof(*pe), NULL, 0, &nextpid, 1);

    /*
     * getprocs64 may
     *  - return -1 => we can't access this process (EPERM)
     *  - return 0 => end of process table, no such process
     *  - return another process' info => no such process
     */
    return ret == 1 && pe->pi_pid == pid;
}

time_t GetProcessStartTime(pid_t pid)
{
    struct procentry64 pe;

    if (FillProcEntry(&pe, pid))
    {
        return pe.pi_start;
    }
    else
    {
        return PROCESS_START_TIME_UNKNOWN;
    }
}

ProcessState GetProcessState(pid_t pid)
{
    struct procentry64 pe;

    if (FillProcEntry(&pe, pid))
    {
        switch (pe.pi_state)
        {
        case SSTOP:
            return PROCESS_STATE_STOPPED;
        case SZOMB:
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
