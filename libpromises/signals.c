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

#include "signals.h"

#include "logging_old.h"

static const char *SIGNALS[] =
{
    [SIGHUP] = "SIGHUP",
    [SIGINT] = "SIGINT",
    [SIGTRAP] = "SIGTRAP",
    [SIGKILL] = "SIGKILL",
    [SIGPIPE] = "SIGPIPE",
    [SIGCONT] = "SIGCONT",
    [SIGABRT] = "SIGABRT",
    [SIGSTOP] = "SIGSTOP",
    [SIGQUIT] = "SIGQUIT",
    [SIGTERM] = "SIGTERM",
    [SIGCHLD] = "SIGCHLD",
    [SIGUSR1] = "SIGUSR1",
    [SIGUSR2] = "SIGUSR2",
    [SIGBUS] = "SIGBUS",
    [SIGSEGV] = "SIGSEGV",
};

static bool PENDING_TERMINATION = false;

/********************************************************************/

bool IsPendingTermination(void)
{
    return PENDING_TERMINATION;
}

/********************************************************************/

void HandleSignalsForAgent(int signum)
{
    CfOut(OUTPUT_LEVEL_ERROR, "", "Received signal %d (%s) while doing [%s]", signum, SIGNALS[signum] ? SIGNALS[signum] : "NOSIG",
          CFLOCK);
    CfOut(OUTPUT_LEVEL_ERROR, "", "Logical start time %s ", ctime(&CFSTARTTIME));
    CfOut(OUTPUT_LEVEL_ERROR, "", "This sub-task started really at %s\n", ctime(&CFINITSTARTTIME));
    fflush(stdout);

    if ((signum == SIGTERM) || (signum == SIGINT))
    {
        exit(0);
    }
    else if (signum == SIGUSR1)
    {
        DEBUG = true;
    }
    else if (signum == SIGUSR2)
    {
        DEBUG = false;
    }

/* Reset the signal handler */
    signal(signum, HandleSignalsForAgent);
}

/********************************************************************/

void HandleSignalsForDaemon(int signum)
{
    CfOut(OUTPUT_LEVEL_ERROR, "", "Received signal %d (%s) while doing [%s]", signum, SIGNALS[signum] ? SIGNALS[signum] : "NOSIG",
          CFLOCK);
    CfOut(OUTPUT_LEVEL_ERROR, "", "Logical start time %s ", ctime(&CFSTARTTIME));
    CfOut(OUTPUT_LEVEL_ERROR, "", "This sub-task started really at %s\n", ctime(&CFINITSTARTTIME));
    fflush(stdout);

    if ((signum == SIGTERM) || (signum == SIGINT) || (signum == SIGHUP) || (signum == SIGSEGV) || (signum == SIGKILL)
        || (signum == SIGPIPE))
    {
        PENDING_TERMINATION = true;
    }
    else if (signum == SIGUSR1)
    {
        DEBUG = true;
    }
    else if (signum == SIGUSR2)
    {
        DEBUG = false;
    }

/* Reset the signal handler */
    signal(signum, HandleSignalsForDaemon);
}
