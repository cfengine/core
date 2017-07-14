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

#ifndef CFENGINE_LOCKS_H
#define CFENGINE_LOCKS_H

#include <cf3.defs.h>

time_t FindLockTime(const char *name);

CfLock AcquireLock(EvalContext *ctx, const char *operand, const char *host, time_t now, TransactionContext tc, const Promise *pp, bool ignoreProcesses);
void YieldCurrentLock(CfLock lock);
void YieldCurrentLockAndRemoveFromCache(EvalContext *ctx, CfLock lock,
                                        const char *operand, const Promise *pp);
void GetLockName(char *lockname, const char *locktype, const char *base, const Rlist *params);

void PurgeLocks(void);
void BackupLockDatabase(void);
void PromiseRuntimeHash(const Promise *pp, const char *salt, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type);

int WriteLock(const char *lock);
CF_DB *OpenLock(void);
void CloseLock(CF_DB *dbp);

void WaitForCriticalSection(const char *section_id);
void ReleaseCriticalSection(const char *section_id);
#endif
