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

#ifndef CFENGINE_SYSINFO_H
#define CFENGINE_SYSINFO_H

#include "cf3.defs.h"

void DiscoverVersion(EvalContext *ctx);

void GetNameInfo3(EvalContext *ctx, AgentType agent_type);
void Get3Environment(EvalContext *ctx, AgentType agent_type);
void BuiltinClasses(EvalContext *ctx);
void OSClasses(EvalContext *ctx);
bool IsInterfaceAddress(const char *adr);
void DetectDomainName(EvalContext *ctx, const char *orig_nodename);
const char *GetWorkDir(void);

void CreateHardClassesFromCanonification(EvalContext *ctx, const char *canonified);

// FIX: win_proc.c?
int GetCurrentUserName(char *userName, int userNameLen);

int GetUptimeMinutes(time_t now);

#endif
