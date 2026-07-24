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

#ifndef CFENGINE_NFS_H
#define CFENGINE_NFS_H

#include <cf3.defs.h>
#include <sequence.h> // Seq

extern bool LoadMountInfo(Seq *list);

/* Option subset matching for CFE-90 mount option verification.
 * Checks whether all options in 'promised_opts' are present in
 * 'actual_opts' (kernel-resolved options).  Kernel-added NFS
 * auto-negotiated options are ignored.  Returns true if all
 * user-specified options are satisfied. */
extern bool OptionsSubsetMatches(const char *promised_opts, const char *actual_opts);

/* CFE-90: Reconcile an already-mounted filesystem whose source or options
 * diverge from the promise, using the mechanisms in a->mount.remount_methods
 * (in order, verifying after each).  Only invoked when remount is enabled. */
PromiseResult ReconcileMountOptions(EvalContext *ctx, char *name, const Attributes *a, const Promise *pp);

void DeleteMountInfo(Seq *list);
int VerifyNotInFstab(EvalContext *ctx, char *name, const Attributes *a, const Promise *pp, PromiseResult *result);
int VerifyInFstab(EvalContext *ctx, char *name, const Attributes *a, const Promise *pp, PromiseResult *result);
PromiseResult VerifyMount(EvalContext *ctx, char *name, const Attributes *a, const Promise *pp);
PromiseResult VerifyUnmount(EvalContext *ctx, char *name, const Attributes *a, const Promise *pp);
void CleanupNFS(void);
void MountAll(void);

#endif
