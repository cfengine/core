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

#ifndef CFENGINE_GENERIC_AGENT_H
#define CFENGINE_GENERIC_AGENT_H

#include <cf3.defs.h>

#include <policy.h>
#include <set.h>

#define GENERIC_AGENT_CHECKSUM_SIZE ((2*CF_SHA1_LEN) + 1)
#define GENERIC_AGENT_CHECKSUM_METHOD HASH_METHOD_SHA1

typedef struct
{
    AgentType agent_type;

    Rlist *bundlesequence;

    char *original_input_file;
    char *input_file;
    char *input_dir;
    char *tag_release_dir;

    bool check_not_writable_by_others;
    bool check_runnable;

    StringSet *heap_soft;
    StringSet *heap_negated;
    bool ignore_locks;

    bool tty_interactive; // agent is running interactively, via tty/terminal interface
    bool color;

    ProtocolVersion protocol_version;

    // agent state
    bool ignore_missing_bundles;
    bool ignore_missing_inputs;

    struct
    {
        struct
        {
            enum
            {
                GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_NONE,
                GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_CF,
                GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_JSON
            } policy_output_format;
            unsigned int parser_warnings;
            unsigned int parser_warnings_error;
            bool eval_functions;
            bool show_classes;
            bool show_variables;
        } common;
        struct
        {
            char *bootstrap_policy_server;
            bool bootstrap_trust_server;
        } agent;
        struct
        {
            /* Time of the last validated_at timestamp seen. */
            time_t last_validated_at;
        } daemon;                                     /* execd, serverd etc */
    } agent_specific;

} GenericAgentConfig;

ENTERPRISE_VOID_FUNC_2ARG_DECLARE(void, GenericAgentSetDefaultDigest, HashMethod *, digest, int *, digest_len);
const char *GenericAgentResolveInputPath(const GenericAgentConfig *config, const char *input_file);
void MarkAsPolicyServer(EvalContext *ctx);
void GenericAgentDiscoverContext(EvalContext *ctx, GenericAgentConfig *config);
bool GenericAgentCheckPolicy(GenericAgentConfig *config, bool force_validation, bool write_validated_file);

ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, GenericAgentAddEditionClasses, EvalContext *, ctx);
void GenericAgentInitialize(EvalContext *ctx, GenericAgentConfig *config);
void GenericAgentFinalize(EvalContext *ctx, GenericAgentConfig *config);
ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, GenericAgentWriteVersion, Writer *, w);
void GenericAgentWriteHelp(Writer *w, const char *comp, const struct option options[], const char *const hints[], bool accepts_file_argument);
bool GenericAgentArePromisesValid(const GenericAgentConfig *config);
time_t ReadTimestampFromPolicyValidatedFile(const GenericAgentConfig *config, const char *maybe_dirname);

bool GenericAgentIsPolicyReloadNeeded(const GenericAgentConfig *config);

void CloseLog(void);
Seq *ControlBodyConstraints(const Policy *policy, AgentType agent);

void SetFacility(const char *retval);
void CheckBundleParameters(char *scope, Rlist *args);
void WritePID(char *filename);

bool GenericAgentConfigParseArguments(GenericAgentConfig *config, int argc, char **argv);
bool GenericAgentConfigParseWarningOptions(GenericAgentConfig *config, const char *warning_options);
bool GenericAgentConfigParseColor(GenericAgentConfig *config, const char *mode);

Policy *SelectAndLoadPolicy(GenericAgentConfig *config, EvalContext *ctx, bool validate_policy, bool write_validated_file);
GenericAgentConfig *GenericAgentConfigNewDefault(AgentType agent_type, bool tty_interactive);
bool GetTTYInteractive(void);
void GenericAgentConfigDestroy(GenericAgentConfig *config);
void GenericAgentConfigApply(EvalContext *ctx, const GenericAgentConfig *config);

bool CheckAndGenerateFailsafe(const char *inputdir, const char *input_file);
void GenericAgentConfigSetInputFile(GenericAgentConfig *config, const char *inputdir, const char *input_file);
void GenericAgentConfigSetBundleSequence(GenericAgentConfig *config, const Rlist *bundlesequence);
bool GenericAgentTagReleaseDirectory(const GenericAgentConfig *config, const char *dirname, bool write_validated, bool write_release);

void GetReleaseIdFile(const char *base_path, char *filename, size_t max_size);

bool GenericAgentPostLoadInit(const EvalContext *ctx);


void LoadAugments(EvalContext *ctx, GenericAgentConfig *config);

#endif
