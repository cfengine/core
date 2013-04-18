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

const AgentDiagnosticCheck *AgentDiagosticsAllChecks(void);

#endif
