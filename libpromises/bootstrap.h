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

/**
 * @brief Sets sys.policy_hub and the POLICY_SERVER global.
 * @param ctx EvalContext is used to set related variables
 * @param new_policy_server IP of new policy server
 */
void SetPolicyServer(EvalContext *ctx, const char *new_policy_server);

/**
 * @brief Updates sys.last_policy_update variable from $(sys.masterdir)/cf_promises_validated
 * @param ctx EvalContext to put variable into
 */
void UpdateLastPolicyUpdateTime(EvalContext *ctx);

/**
 * @return The contents of policy_server.dat, or NULL if file is not found. Return value must be freed.
 */
char *ReadPolicyServerFile(const char *workdir);

/**
 * @brief Write new_policy_server to the policy_server.dat file.
 * @return True if successful
 */
bool WritePolicyServerFile(const char *workdir, const char *new_policy_server);

/**
 * @brief Remove the policy_server.dat file
 * @return True if successful
 */
bool RemovePolicyServerFile(const char *workdir);

/**
 * @return True if the file STATEDIR/am_policy_hub exists
 */
bool GetAmPolicyHub(void);

/**
 * @brief Set the STATEDIR/am_policy_hub marker file.
 * @param am_policy_hub If true, create marker file. If false, delete it.
 * @return True if successful
 */
bool WriteAmPolicyHubFile(bool am_policy_hub);

/**
 * @brief Write the builtin failsafe policy to the default location
 * @return True if successful
 */
bool WriteBuiltinFailsafePolicy(const char *workdir);

/**
 * @brief Exposed for testing. Use WriteBuiltinFailsafePolicy.
 */
bool WriteBuiltinFailsafePolicyToPath(const char *filename);

/**
 * @brief Removes all files in $(sys.inputdir)
 * @param inputdir
 * @return True if successful
 */
bool RemoveAllExistingPolicyInInputs(const char *inputdir);

/**
 * @return True if the file $(sys.masterdir)/promises.cf exists
 */
bool MasterfileExists(const char *masterdir);

#endif
