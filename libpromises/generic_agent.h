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

#ifndef CFENGINE_GENERIC_AGENT_H
#define CFENGINE_GENERIC_AGENT_H

#include "cf3.defs.h"

#include "policy.h"

typedef struct
{
    AgentType agent_type;

    Rlist *bundlesequence;
    char *input_file;
    bool check_not_writable_by_others;
    bool check_runnable;
    bool tty_interactive; // agent is running interactively, via tty/terminal interface

    // change to evaluation behavior from the policy itself
    bool ignore_missing_bundles;
    bool ignore_missing_inputs;

    union
    {
        struct
        {
            enum
            {
                GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_NONE,
                GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_CF,
                GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_JSON
            } policy_output_format;
        } common;
    } agent_specific;

} GenericAgentConfig;

const char *GenericAgentResolveInputPath(const char *filename, const char *base_input_file);
void GenericAgentDiscoverContext(GenericAgentConfig *config, ReportContext *report_context);
bool GenericAgentCheckPolicy(GenericAgentConfig *config, bool force_validation);
Policy *GenericAgentLoadPolicy(AgentType agent_type, GenericAgentConfig *config, const ReportContext *report_context);

void InitializeGA(GenericAgentConfig *config);
void Syntax(const char *comp, const struct option options[], const char *hints[], const char *id);
void ManPage(const char *component, const struct option options[], const char *hints[], const char *id);
void PrintVersionBanner(const char *component);
int CheckPromises(const char *input_file);
Policy *ReadPromises(AgentType agent_type, GenericAgentConfig *config, const ReportContext *report_context);
int NewPromiseProposals(const char *input_file, const Rlist *input_files);
void CompilationReport(Policy *policy, char *fname);
void HashVariables(Policy *policy, const char *name, const ReportContext *report_context);
void HashControls(const Policy *policy, GenericAgentConfig *config);
void CloseLog(void);
Seq *ControlBodyConstraints(const Policy *policy, AgentType agent);

/**
 * @brief Conventience function for getting the effective list of input_files from common body control.
 * @param policy Policy where inputs are specified
 * @return Pointer to the Rlist in the DOM
 */
const Rlist *InputFiles(Policy *policy);


void SetFacility(const char *retval);
void CheckBundleParameters(char *scope, Rlist *args);
void PromiseBanner(Promise *pp);
void BannerBundle(Bundle *bp, Rlist *args);
void BannerSubBundle(Bundle *bp, Rlist *args);
void WritePID(char *filename);
ReportContext *OpenCompilationReportFiles(const char *fname);
void CheckLicenses(void);
void ReloadPromises(AgentType ag);

ReportContext *OpenReports(AgentType agent_type);
void CloseReports(const char *agents, ReportContext *report_context);

GenericAgentConfig *GenericAgentConfigNewDefault(AgentType agent_type);
void GenericAgentConfigDestroy(GenericAgentConfig *config);

const char *AgentTypeToString(AgentType agent_type);

void GenericAgentConfigSetInputFile(GenericAgentConfig *config, const char *input_file);
void GenericAgentConfigSetBundleSequence(GenericAgentConfig *config, const Rlist *bundlesequence);

#endif
