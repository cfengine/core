/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_TRANSACTION_H
#define CFENGINE_TRANSACTION_H

bool AcquireLockByID(char *lock_id, int acquire_after_minutes);
time_t FindLockTime(char *name);
bool InvalidateLockTime(char *lock_id);
bool EnforcePromise(enum cfopaction action);

void SetSyslogHost(const char *host);
void SetSyslogPort(uint16_t port);
void SetSyslogFacility(int facility);
void RemoteSysLog(int log_priority, const char *log_string);

#endif  /* NOT CFENGINE_TRANSACTION_H */
