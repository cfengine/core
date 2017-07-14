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

#ifndef CFENGINE_VERIFY_FILES_UTILS_H
#define CFENGINE_VERIFY_FILES_UTILS_H

#include <cf3.defs.h>
#include <cfnet.h>                                       /* AgentConnection */
#include <comparray.h>

extern const Rlist *SINGLE_COPY_LIST;

void SetFileAutoDefineList(const Rlist *auto_define_list);

void VerifyFileLeaf(EvalContext *ctx, char *path, struct stat *sb, Attributes attr, const Promise *pp, PromiseResult *result);
int DepthSearch(EvalContext *ctx, char *name, struct stat *sb, int rlevel, Attributes attr, const Promise *pp, dev_t rootdevice, PromiseResult *result);
bool CfCreateFile(EvalContext *ctx, char *file, const Promise *pp, Attributes attr, PromiseResult *result_out);
void SetSearchDevice(struct stat *sb, const Promise *pp);

PromiseResult ScheduleCopyOperation(EvalContext *ctx, char *destination, Attributes attr, const Promise *pp);
PromiseResult ScheduleLinkChildrenOperation(EvalContext *ctx, char *destination, char *source, int rec, Attributes attr, const Promise *pp);
PromiseResult ScheduleLinkOperation(EvalContext *ctx, char *destination, char *source, Attributes attr, const Promise *pp);

bool CopyRegularFile(EvalContext *ctx, const char *source, const char *dest, struct stat sstat, struct stat dstat,
                     Attributes attr, const Promise *pp, CompressedArray **inode_cache, AgentConnection *conn, PromiseResult *result);

/* To be implemented in Nova for Win32 */
bool VerifyOwner(EvalContext *ctx, const char *file, const Promise *pp, Attributes attr, struct stat *sb, PromiseResult *result);

#endif
