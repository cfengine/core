/*
   Copyright 2017 Northern.tech AS

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

#ifndef CFENGINE_BOOTSTRAP_H
#define CFENGINE_BOOTSTRAP_H

#include <cf3.defs.h>


void EvalContextSetPolicyServer(EvalContext *ctx, const char *new_policy_server);

// POLICY SERVER FILE FUNCTIONS:
char* ReadPolicyServerFile(const char *workdir);
void SetPolicyServerFromFile(EvalContext *ctx, const char *workdir);
bool ParsePolicyServerFile(const char *workdir, char **host, char **port);
bool LookUpPolicyServerFile(const char *workdir, char **ipaddr, char **port);
bool WritePolicyServerFile(const char *workdir, const char *new_policy_server);
bool RemovePolicyServerFile(const char *workdir);

// POLICY HUB FUNCTIONS:
void UpdateLastPolicyUpdateTime(EvalContext *ctx);
bool GetAmPolicyHub(void);
bool WriteAmPolicyHubFile(bool am_policy_hub);

// FAILSAFE FUNCTIONS:
bool WriteBuiltinFailsafePolicy(const char *workdir);
bool WriteBuiltinFailsafePolicyToPath(const char *filename);

// POLICY FILE FUNCTIONS:
bool RemoveAllExistingPolicyInInputs(const char *inputdir);
bool MasterfileExists(const char *masterdir);

#endif
