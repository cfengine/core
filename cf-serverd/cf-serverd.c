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

#include "cf-serverd-functions.h"

#include "server_transform.h"
#include "cfstream.h"
#include "logging.h"

int main(int argc, char *argv[])
{
    GenericAgentConfig *config = CheckOpts(argc, argv);

    ReportContext *report_context = OpenReports(config->agent_type);
    GenericAgentDiscoverContext(config, report_context);

    Policy *policy = NULL;
    if (GenericAgentCheckPolicy(config, false))
    {
        policy = GenericAgentLoadPolicy(config->agent_type, config, report_context);
    }
    else if (config->tty_interactive)
    {
        FatalError("CFEngine was not able to get confirmation of promises from cf-promises, please verify input file\n");
    }
    else
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "CFEngine was not able to get confirmation of promises from cf-promises, so going to failsafe\n");
        HardClass("failsafe_fallback");
        GenericAgentConfigSetInputFile(config, "failsafe.cf");
        policy = GenericAgentLoadPolicy(config->agent_type, config, report_context);
    }

    CheckLicenses();

    ThisAgentInit();
    KeepPromises(policy, config, report_context);
    Summarize();

    StartServer(policy, config, report_context);

    ReportContextDestroy(report_context);
    GenericAgentConfigDestroy(config);
    return 0;
}
