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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <cf-serverd-functions.h>
#include <cf-serverd-enterprise-stubs.h> /* CleanReportBookFilterSet() */

#include <server_transform.h>
#include <known_dirs.h>
#include <loading.h>


static void ThisAgentInit(void)
{
    umask(077);
}


int main(int argc, char *argv[])
{
    GenericAgentConfig *config = CheckOpts(argc, argv);
    EvalContext *ctx = EvalContextNew();
    GenericAgentConfigApply(ctx, config);

    GenericAgentDiscoverContext(ctx, config);

    Policy *policy = NULL;
    if (GenericAgentCheckPolicy(config, false, false))
    {
        policy = LoadPolicy(ctx, config);
    }
    else if (config->tty_interactive)
    {
        exit(EXIT_FAILURE);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "CFEngine was not able to get confirmation of promises from cf-promises, so going to failsafe");
        EvalContextClassPutHard(ctx, "failsafe_fallback", "attribute_name=Errors,source=agent");
        GenericAgentConfigSetInputFile(config, GetInputDir(), "failsafe.cf");
        policy = LoadPolicy(ctx, config);
    }

    ThisAgentInit();
    KeepPromises(ctx, policy, config);
    Summarize();

    int threads_left = StartServer(ctx, &policy, config);

    if (threads_left <= 0)
    {
        PolicyDestroy(policy);
        GenericAgentFinalize(ctx, config);
        CleanReportBookFilterSet();
    }

    return 0;
}
