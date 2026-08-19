/*
  Copyright 2026 Northern.tech AS

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
#include <logging.h>
#include <stdio.h>
#include <stdlib.h>
#include <writer.h>
#include <config.h>
#include <generic_agent.h>
#include <man.h>
#include <cleanup.h>
#include <prototypes3.h>

static const Component COMPONENT =
{
    .name = "cf-reactor",
    .website = CF_WEBSITE,
    .copyright = CF_COPYRIGHT
};

static const char *const CF_REACTOR_SHORT_DESCRIPTION = "CFEngine event reaction daemon";

static const char *const CF_REACTOR_MANPAGE_LONG_DESCRIPTION =
        "cf-reactor is a daemon reacting on events, currently only NOTIFY events from PostgreSQL.";

static const struct option OPTIONS[] =
{
    {"debug", no_argument, 0, 'd'},
    {"no-fork", no_argument, 0, 'F'},
    {"log-level", required_argument, 0, 'g'},
    {"help", no_argument, 0, 'h'},
    {"inform", no_argument, 0, 'I'},
    {"timestamp", no_argument, 0, 'l'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, '\0'}
};

static const char *const HINTS[] =
{
    "Enable debugging output and run in foreground",
    "Run as a foreground process (do not fork)",
    "Specify how detailed logs should be. Possible values: 'error', 'warning', 'notice', 'info', 'verbose', 'debug'",
    "Print the help message",
    "Print basic information about actions being taken",
    "Log timestamps on each line of log output",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    NULL
};

int main(int argc, char *argv[])
{
    bool no_fork = false;

    extern char *optarg;
    int longopt_idx;
    int c;
    while ((c = getopt_long(argc, argv, "dFg:hIlMvV",
                            OPTIONS, &longopt_idx))
           != -1)
    {
        switch (c)
        {
        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            no_fork = true;
            break;

        case 'F':
            no_fork = true;
            break;

        case 'g':
            LogSetGlobalLevelArgOrExit(optarg);
            break;

        case 'h':
            {
                Writer *w = FileWriter(stdout);
                WriterWriteHelp(w, &COMPONENT, OPTIONS, HINTS, NULL, false, true);
                FileWriterDetach(w);
            }
            DoCleanupAndExit(EXIT_SUCCESS);

        case 'I':
            LogSetGlobalLevel(LOG_LEVEL_INFO);
            break;

        case 'l':
            LoggingEnableTimestamps(true);
            break;

        case 'M':
            {
                Writer *out = FileWriter(stdout);
                ManPageWrite(out, "cf-reactor", time(NULL),
                             CF_REACTOR_SHORT_DESCRIPTION,
                             CF_REACTOR_MANPAGE_LONG_DESCRIPTION,
                             OPTIONS, HINTS,
                             NULL, false,
                             true);
                FileWriterDetach(out);
                DoCleanupAndExit(EXIT_SUCCESS);
            }

        case 'v':
            LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
            no_fork = true;
            break;

        case 'V':
            {
                Writer *w = FileWriter(stdout);
                GenericAgentWriteVersion(w);
                FileWriterDetach(w);
            }
            DoCleanupAndExit(EXIT_SUCCESS);

        }
    }

    return ReactorEnterpriseMain(no_fork);
}
