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

#ifndef CFENGINE_AGENT_DIAGNOSTICS_H
#define CFENGINE_AGENT_DIAGNOSTICS_H

#include "writer.h"

typedef struct
{
    bool success;
    char *message;
} AgentDiagnosticsResult;

typedef AgentDiagnosticsResult AgentDiagnosticCheckFn(const char *workdir);

typedef struct
{
    const char *description;
    AgentDiagnosticCheckFn *check;
} AgentDiagnosticCheck;

void AgentDiagnosticsRun(const char *workdir, const AgentDiagnosticCheck checks[], Writer *output);

// Checks
AgentDiagnosticsResult AgentDiagnosticsCheckHavePrivateKey(const char *workdir);
AgentDiagnosticsResult AgentDiagnosticsCheckHavePublicKey(const char *workdir);
AgentDiagnosticsResult AgentDiagnosticsCheckIsBootstrapped(const char *workdir);
AgentDiagnosticsResult AgentDiagnosticsCheckAmPolicyServer(const char *workdir);


AgentDiagnosticsResult AgentDiagnosticsResultNew(bool success, char *message);

const AgentDiagnosticCheck *AgentDiagnosticsAllChecks(void);

#endif
