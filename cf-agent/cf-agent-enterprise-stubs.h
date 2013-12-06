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

#ifndef CFENGINE_CF_AGENT_ENTERPRISE_STUBS_H
#define CFENGINE_CF_AGENT_ENTERPRISE_STUBS_H

#include <cf3.defs.h>
#include <cfnet.h>

#if defined(__MINGW32__)
PromiseResult VerifyRegistryPromise(EvalContext *ctx, Attributes a, Promise *pp);
#endif
ENTERPRISE_FUNC_3ARG_DECLARE(PromiseResult, VerifyWindowsService, EvalContext *, ctx, Attributes, a, Promise *, pp);

ENTERPRISE_VOID_FUNC_2ARG_DECLARE(void, LastSawBundle, const Bundle *, bundle, double, compliance);

typedef bool (*CopyRegularFileFunction)(EvalContext *ctx,
                                       const char *source,
                                       const char *dest,
                                       struct stat sstat,
                                       struct stat dstat,
                                       Attributes attr,
                                       Promise *pp,
                                       CompressedArray **inode_cache,
                                       AgentConnection *conn,
                                       PromiseResult *result);
typedef void (*DeleteCompressedArrayFunction)(CompressedArray *start);
ENTERPRISE_FUNC_8ARG_DECLARE(PromiseResult, LogFileChange,
                             EvalContext *, ctx,
                             const char *, file,
                             int, change, Attributes, a, Promise *, pp, CopyRegularFileFunction, CopyRegularFilePtr,
                             const char *, destination, DeleteCompressedArrayFunction, DeleteCompressedArrayPtr);

ENTERPRISE_FUNC_5ARG_DECLARE(PromiseResult, Nova_CheckNtACL,
                             EvalContext *, ctx, const char *, file_path, Acl, acl, Attributes, a, Promise *, pp);

ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, ReportPatches, PackageManager *, list);

ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, Nova_NoteVarUsageDB, EvalContext *, ctx);
ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, Nova_TrackExecution, const char *, input_file);
ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, GenerateDiffReports, const char *, input_file);

#endif

