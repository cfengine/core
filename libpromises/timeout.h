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

#ifndef CFENGINE_TIMEOUT_H
#define CFENGINE_TIMEOUT_H

void SetTimeOut(int timeout);

/* Cancel a pending alarm and restore the default handler. Callers used to
 * open-code this; it also has to clear the armed flag, so that a command which
 * completes in time does not leave it set for the next, unrelated, child. It
 * does not clear what TimeOutHasFired() and TimeOutSignalledProcess() report:
 * that record stays readable after the disarm, until the next SetTimeOut(). */
void ClearTimeOut(void);

/* True between SetTimeOut() arming the alarm and the alarm being disarmed.
 * Consulted by code that forks a child which the timeout may have to
 * terminate, to decide whether that child needs a process group of its own. */
bool TimeOutIsArmed(void);

/* True if the alarm armed by the last SetTimeOut() actually fired. Lets a
 * caller report that a command was timed out even when the command's own exit
 * status would otherwise read as success. Cleared by SetTimeOut(). */
bool TimeOutHasFired(void);

/* True if that alarm also had a process to signal. False means the command
 * exceeded its timeout but was never terminated, which callers must not
 * describe as a termination. */
bool TimeOutSignalledProcess(void);
void TimeOut(void);
time_t SetReferenceTime(void);

#endif
