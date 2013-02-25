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

#include "cf3.defs.h"

bool AcquireLockByID(char *lock_id, int acquire_after_minutes);
time_t FindLockTime(char *name);
bool InvalidateLockTime(char *lock_id);
bool EnforcePromise(enum cfopaction action);

void SetSyslogHost(const char *host);
void SetSyslogPort(uint16_t port);
void SetSyslogFacility(int facility);
void RemoteSysLog(int log_priority, const char *log_string);

void SummarizeTransaction(EvalContext *ctx, Attributes attr, const Promise *pp, const char *logname);
CfLock AcquireLock(char *operand, char *host, time_t now, Attributes attr, Promise *pp, int ignoreProcesses);
void YieldCurrentLock(CfLock this);
void GetLockName(char *lockname, char *locktype, char *base, Rlist *params);

int ThreadLock(pthread_mutex_t *name);
int ThreadUnlock(pthread_mutex_t *name);

void PurgeLocks(void);
int ShiftChange(EvalContext *ctx);

int WriteLock(char *lock);
CF_DB *OpenLock(void);
void CloseLock(CF_DB *dbp);

#endif
