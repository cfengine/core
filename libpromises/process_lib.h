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

#ifndef CFENGINE_PROCESS_H
#define CFENGINE_PROCESS_H

#include <platform.h>


/* TODO move to libutils, once windows implementation is merged. */


#define PROCESS_START_TIME_UNKNOWN ((time_t) 0)


/**
 * Obtain start time of specified process.
 *
 * @return start time (Unix timestamp) of specified process
 * @return PROCESS_START_TIME_UNKNOWN if start time cannot be determined
 */
time_t GetProcessStartTime(pid_t pid);

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
