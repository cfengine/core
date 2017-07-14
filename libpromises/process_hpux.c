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

#include <process_lib.h>
#include <process_unix_priv.h>

#include <sys/pstat.h>

time_t GetProcessStartTime(pid_t pid)
{
    struct pst_status proc;

    if (pstat_getproc(&proc, sizeof(proc), 0, pid) > 0)
    {
        return proc.pst_start;
    }
    else
    {
        return PROCESS_START_TIME_UNKNOWN;
    }
}

ProcessState GetProcessState(pid_t pid)
{
    struct pst_status proc;

    if (pstat_getproc(&proc, sizeof(proc), 0, pid) > 0)
    {
        switch (proc.pst_stat)
        {
        case PS_STOP:
            return PROCESS_STATE_STOPPED;
        case PS_ZOMBIE:
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
