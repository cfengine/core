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

#include "cf-serverd-functions.h"

#include "server_transform.h"

int main(int argc, char *argv[])
{
    EvalContext *ctx = EvalContextNew();
    GenericAgentConfig *config = CheckOpts(argc, argv);
    GenericAgentConfigApply(ctx, config);

    GenericAgentDiscoverContext(ctx, config);

    Policy *policy = NULL;
    if (GenericAgentCheckPolicy(ctx, config, false))
    {
        policy = GenericAgentLoadPolicy(ctx, config);
    }
    else if (config->tty_interactive)
    {
        exit(EXIT_FAILURE);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "CFEngine was not able to get confirmation of promises from cf-promises, so going to failsafe");
        EvalContextHeapAddHard(ctx, "failsafe_fallback");
        GenericAgentConfigSetInputFile(config, GetWorkDir(), "failsafe.cf");
        policy = GenericAgentLoadPolicy(ctx, config);
    }

    CheckForPolicyHub(ctx);

    ThisAgentInit();
    KeepPromises(ctx, policy, config);
    Summarize();

    StartServer(ctx, policy, config);

    GenericAgentConfigDestroy(config);
    EvalContextDestroy(ctx);

    return 0;
}
