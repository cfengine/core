/*
  Copyright 2020 Northern.tech AS

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

#ifndef CFENGINE_PROCESS_H
#define CFENGINE_PROCESS_H

#include <platform.h>
#include <item_lib.h>

/* TODO move to libutils, once windows implementation is merged. */


#define PROCESS_START_TIME_UNKNOWN ((time_t) 0)

#define PROCDIR "/proc"

// keys for Json structure
#define JPROC_KEY_UID             "uid"      /* uid */
#define JPROC_KEY_EUID            "euid"     /* effective uid */
#define JPROC_KEY_CMD             "cmd"      /* cmd (argv[0]) only */
#define JPROC_KEY_CMDLINE         "cmdline"  /* cmd line and args */
#define JPROC_KEY_PSTATE          "pstate"   /* process state */
#define JPROC_KEY_PPID            "ppid"     /* parent pid */
#define JPROC_KEY_PGID            "pgid"     /* process group */
#define JPROC_KEY_TTY             "tty"      /* terminal dev_t */
#define JPROC_KEY_TTYNAME         "ttyname"  /* terminal name (e.g. "pts/nnn" */
#define JPROC_KEY_PRIORITY        "priority" /* priority */
#define JPROC_KEY_THREADS         "threads"  /* threads */
#define JPROC_KEY_CPUTIME         "ttime"    /* elapsed time (seconds) */
#define JPROC_KEY_STARTTIME_BOOT  "stime_b"  /* starttime (seconds since boot) */
#define JPROC_KEY_STARTTIME_EPOCH "stime_e"  /* starttime (seconds since epoch) */
#define JPROC_KEY_RES_KB          "rkb"      /* resident kb */
#define JPROC_KEY_VIRT_KB         "vkb"      /* virtual kb */

/*
 * JSON "proc" info.
 *
 * A list of objects keyed by 'pid', reflecting "/proc/<pid>".
 *
 * {
 *   "i" : { "ppid" : i_parent, "cmd" : i_cmd, ... },
 *   "j" : { "ppid" : j_parent, "cmd" : j_cmd, ... },
 *   ...
 *   },
 * }
 */


/**
 * Obtain ps-like information about specified process.
 *
 * @return JSON data structure of information about specified process
 */
JsonElement *LoadProcStat(pid_t pid);

/**
 * Obtain start time of specified process.
 *
 * @return start time (Unix timestamp) of specified process
 * @return PROCESS_START_TIME_UNKNOWN if start time cannot be determined
 */
time_t GetProcessStartTime(pid_t pid);

/**
 * Obtain system boot time
 *
 * @return boot time (second since epoch) of the system
 * @return -1 if start time cannot be determined
 */
time_t LoadBootTime(void);

/**
 * Gracefully kill the process with pid #pid and start time #process_start_time.
 *
 * Under Unix this will send SIGINT, then SIGTERM and then SIGKILL if process
 * does not exit.
 *
 * #process_start_time may be PROCESS_START_TIME_UNKNOWN, which will disable
 * safety check for killing right process.
 *
 * @return true if process was killed successfully, false otherwise.
 * @return false if killing failed for any reason, or if the PID wasn't
 *               present in the first place.
 */
bool GracefulTerminate(pid_t pid, time_t process_start_time);


#endif
