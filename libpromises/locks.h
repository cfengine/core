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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_LOCKS_H
#define CFENGINE_LOCKS_H

#include "cf3.defs.h"

bool AcquireLockByID(const char *lock_id, int acquire_after_minutes);
time_t FindLockTime(char *name);
bool InvalidateLockTime(const char *lock_id);


CfLock AcquireLock(EvalContext *ctx, char *operand, char *host, time_t now, TransactionContext tc, const Promise *pp, int ignoreProcesses);
void YieldCurrentLock(CfLock this);
void GetLockName(char *lockname, char *locktype, char *base, Rlist *params);

void PurgeLocks(void);

int WriteLock(char *lock);
CF_DB *OpenLock(void);
void CloseLock(CF_DB *dbp);

#endif
