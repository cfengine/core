/*
  Copyright 2023 Northern.tech AS

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


#include <platform.h>

#include <cf-serverd-functions.h>
#include <cf-serverd-enterprise-stubs.h> /* CleanReportBookFilterSet() */
#include <server_transform.h>
#include <known_dirs.h>
#include <loading.h>
#include <cleanup.h>
#include <signals.h>                     /* RequestReloadConfig() */

static void ThisAgentInit(void)
{
    umask(077);
}


int main(int argc, char *argv[])
{
    /* Ensure that if fd 0,1,2 are closed, we reserve them to avoid opening
     * the listening socket on them and closing it later when daemonising. */
    int fd = -1;
    do
    {
        fd = open(NULLFILE, O_RDWR, 0);

    } while (fd == STDIN_FILENO  ||
             fd == STDOUT_FILENO ||
             fd == STDERR_FILENO);
    close(fd);

    GenericAgentConfig *config = CheckOpts(argc, argv);
    EvalContext *ctx = EvalContextNew();
    GenericAgentConfigApply(ctx, config);

    const char *program_invocation_name = argv[0];
    const char *last_dir_sep = strrchr(program_invocation_name, FILE_SEPARATOR);
    const char *program_name = (last_dir_sep != NULL ? last_dir_sep + 1 : program_invocation_name);
    GenericAgentDiscoverContext(ctx, config, program_name);

    Policy *policy = SelectAndLoadPolicy(config, ctx, false, false);

    if (!policy)
    {
        Log(LOG_LEVEL_ERR, "Error reading CFEngine policy. Exiting...");
        DoCleanupAndExit(EXIT_FAILURE);
    }

    GenericAgentPostLoadInit(ctx);
    ThisAgentInit();

    bool unresolved_constraints;
    KeepPromises(ctx, policy, config, &unresolved_constraints);
    Summarize();
    if (unresolved_constraints)
    {
        Log(LOG_LEVEL_WARNING,
            "Unresolved variables found in cf-serverd policy, scheduling policy reload");
        RequestReloadConfig();
    }

    int threads_left = StartServer(ctx, &policy, config);

    if (threads_left <= 0)
    {
        PolicyDestroy(policy);
        GenericAgentFinalize(ctx, config);
        CleanReportBookFilterSet();
    }
    CallCleanupFunctions();
    return 0;
}
