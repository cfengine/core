#include "agent-diagnostics.h"

#include "alloc.h"
#include "crypto.h"
#include "files_interfaces.h"
#include "string_lib.h"

#include <assert.h>

AgentDiagnosticsResult AgentDiagnosticsResultNew(bool success, char *message)
{
    return (AgentDiagnosticsResult) { success, message };
}

static void AgentDiagnosticsResultDestroy(AgentDiagnosticsResult result)
{
    free(result.message);
}

void AgentDiagnosticsRun(const char *workdir, const AgentDiagnosticCheck checks[], Writer *output)
{
    for (int i = 0; checks[i].description; i++)
    {
        AgentDiagnosticsResult result = checks[i].check(workdir);
        WriterWriteF(output, "[ %s ] %s: %s\n",
                     result.success ? " OK " : "FAIL",
                     checks[i].description,
                     result.message);
        AgentDiagnosticsResultDestroy(result);
    }
}

AgentDiagnosticsResult AgentDiagosticsCheckHavePrivateKey(ARG_UNUSED const char *workdir)
{
    const char *path = PrivateKeyFile(workdir);
    assert(path);
    struct stat sb;
    return AgentDiagnosticsResultNew((cfstat(path, &sb) == 0), xstrdup(path));
}

AgentDiagnosticsResult AgentDiagosticsCheckHavePublicKey(ARG_UNUSED const char *workdir)
{
    const char *path = PublicKeyFile(workdir);
    assert(path);
    struct stat sb;
    return AgentDiagnosticsResultNew((cfstat(path, &sb) == 0), xstrdup(path));
}

const AgentDiagnosticCheck *AgentDiagosticsAllChecks(void)
{
    static const AgentDiagnosticCheck checks[] =
    {
        { "Check that private key exists", &AgentDiagosticsCheckHavePrivateKey },
        { "Check that public key exists", &AgentDiagosticsCheckHavePublicKey },

        { NULL, NULL }
    };

    return checks;
}
