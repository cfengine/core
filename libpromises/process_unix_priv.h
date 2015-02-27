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

#ifndef CFENGINE_PROCESS_PRIV_H
#define CFENGINE_PROCESS_PRIV_H

/*
 * Unix-like OS should provide implementations of the following functions.
 */

/*
 * GetProcessStartTime (see process_lib.h)
 */

typedef enum
{
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_STOPPED,
    PROCESS_STATE_ZOMBIE,
    PROCESS_STATE_DOES_NOT_EXIST
} ProcessState;

/*
 * Obtain process state.
 *
 * @return PROCESS_STATE_RUNNING if process exists and is running,
 * @return PROCESS_STATE_STOPPED if process exists and has been stopped by SIGSTOP signal,
 * @return PROCESS_STATE_ZOMBIE  if process exists and is zombie,
 * @return PROCESS_STATE_DOES_NOT_EXIST if process cannot be found.
 */
ProcessState GetProcessState(pid_t pid);

#endif
