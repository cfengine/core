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
#include <stdlib.h>
#include <eval_context.h>
#include <writer.h>
#include <config.h>
#include <generic_agent.h>
#include <man.h>
#include <cleanup.h>
#include <prototypes3.h>
#include <signal.h>             /* signal, kill */
#include <signals.h>            /* GetSignalPipe, MakeSignalPipe, IsPendingTermination, HandleSignalsForDaemon */
#include <exec_tools.h>
#include <reactor_context.h>

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

int NO_FORK = false;

/*****************************************************************************/
/* Constants                                                                 */
/*****************************************************************************/

#define DEFAULT_POLL_INTERVAL_SECS 30

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const Component COMPONENT =
{
    .name = "cf-reactor",
    .website = CF_WEBSITE,
    .copyright = CF_COPYRIGHT
};

static const char *const CF_REACTOR_SHORT_DESCRIPTION = "CFEngine event reaction daemon";

static const char *const CF_REACTOR_MANPAGE_LONG_DESCRIPTION =
    "cf-reactor is a daemon reacting on events. It watches specific events defined in the policy "
    "and runs policy code on reaction to these events.";

static const struct option OPTIONS[] =
{
    {"debug", no_argument, 0, 'd'},
    {"no-fork", no_argument, 0, 'F'},
    {"log-level", required_argument, 0, 'g'},
    {"help", no_argument, 0, 'h'},
    {"inform", no_argument, 0, 'I'},
    {"timestamp", no_argument, 0, 'l'},
    {"man", no_argument, 0, 'M'},
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

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_REACTOR, GetTTYInteractive());

    int longopt_idx;
    while ((c = getopt_long(argc, argv, "dFg:hIlMvV",
                            OPTIONS, &longopt_idx)) != -1)
    {
        switch (c)
        {
        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            NO_FORK = true;
            break;

        case 'I':
            LogSetGlobalLevel(LOG_LEVEL_INFO);
            break;

        case 'v':
            LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
            NO_FORK = true;
            break;

        case 'g':
            LogSetGlobalLevelArgOrExit(optarg);
            break;

        case 'F':
            NO_FORK = true;
            break;

        case 'V':
        {
            Writer *w = FileWriter(stdout);
            GenericAgentWriteVersion(w);
            FileWriterDetach(w);
        }
        DoCleanupAndExit(EXIT_SUCCESS);

        case 'h':
        {
            Writer *w = FileWriter(stdout);
            WriterWriteHelp(w, &COMPONENT, OPTIONS, HINTS, NULL, false, true);
            FileWriterDetach(w);
        }
        DoCleanupAndExit(EXIT_SUCCESS);

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

        case 'l':
            LoggingEnableTimestamps(true);
            break;

        /* long options only */
        case 0:
        {
            // TODO: handle long options if they are added in the future
            break;
        }

        default:
        {
            Writer *w = FileWriter(stdout);
            WriterWriteHelp(w, &COMPONENT, OPTIONS, HINTS, NULL, false, true);
            FileWriterDetach(w);
        }
        DoCleanupAndExit(EXIT_FAILURE);
        }
    }

    if (!GenericAgentConfigParseArguments(config, argc - optind, argv + optind))
    {
        Log(LOG_LEVEL_ERR, "Too many arguments");
        DoCleanupAndExit(EXIT_FAILURE);
    }

    return config;
}

/*****************************************************************************/


int main(int argc, char *argv[])
{
    GenericAgentConfig *config = CheckOpts(argc, argv);
    EvalContext *ctx = EvalContextNew();
    GenericAgentConfigApply(ctx, config);

#ifdef __MINGW32__

    if (!NO_FORK)
    {
        Log(LOG_LEVEL_VERBOSE, "Windows does not support starting processes in the background - starting in foreground");
    }

#else /* !__MINGW32__ */
    pid_t existing_pid = ReadPID("cf-reactor.pid");
    if ((existing_pid != -1) && (kill(existing_pid, 0) == 0))
    {
        Log(LOG_LEVEL_ERR, "Another instance of cf-reactor is already running (pid %jd), terminating",
            (intmax_t) existing_pid);
        return 1;
    }

    if ((!NO_FORK) && (fork() != 0))
    {
        Log(LOG_LEVEL_INFO, "cf-reactor: starting");
        _exit(EXIT_SUCCESS);
    }

    if (!NO_FORK)
    {
        ActAsDaemon();
    }

#endif /* !__MINGW32__ */

    umask(077);
    WritePID("cf-reactor.pid");
    MakeSignalPipe();

    signal(SIGINT, HandleSignalsForDaemon);
    signal(SIGTERM, HandleSignalsForDaemon);
    signal(SIGBUS, HandleSignalsForDaemon);
    signal(SIGHUP, HandleSignalsForDaemon);
    signal(SIGUSR1, HandleSignalsForDaemon);
    signal(SIGUSR2, HandleSignalsForDaemon);

    ReactorContext reactor_ctx;
    if (!ReactorContextInitialize(&reactor_ctx))
    {
        GenericAgentFinalize(ctx, config);
        DoCleanupAndExit(EXIT_FAILURE);
    }

    /* Writing to a pipe whose spawned process already exited (e.g. cfbs
     * rejecting its arguments before reading its stdin) must fail with EPIPE
     * rather than terminate the whole daemon. Set after ReactorContextInitialize(),
     * so that the spawner and the processes it execs keep the default handling. */
    signal(SIGPIPE, SIG_IGN);

    /* We need an initial value here for the first iteration of the cycle
     * below. */
    time_t next_tick = time(NULL) + DEFAULT_POLL_INTERVAL_SECS;
    while (!IsPendingTermination())
    {
        int max_fd = ReactorContextSetupFileDescriptors(&reactor_ctx);

        /* Determine how much time is remaining until the next tick. */
        time_t last_tick = time(NULL);
        time_t remaining = next_tick > last_tick ? next_tick - last_tick : 0;

        struct timeval timeout = { .tv_sec = remaining };
        int ret = select(max_fd, &reactor_ctx.readfds, NULL, NULL, &timeout);

        /* Reschedule the backstop tick against the current time (not
         * `last_tick`, which was captured before select() potentially
         * blocked for the whole `remaining` duration), so that both call
         * sites of ReactorNovaHandleTimeout() agree on what "the next tick"
         * means, instead of one of them silently doubling the interval. */
        next_tick = time(NULL) + DEFAULT_POLL_INTERVAL_SECS;

        if (ret < 0)
        {
            if (errno == EINTR)
            {
                /* Not an error, just a signal delivered while blocked in
                 * select(). Loop around: the top of the loop re-checks
                 * whether termination is pending and rebuilds the fd set
                 * from scratch. */
                continue;
            }

            /*** error ***/
            Log(LOG_LEVEL_ERR, "Failed to poll events: %s", GetErrorStr());
            break;
        }
        else if (ret == 0)
        {
            /*** timeout ***/
            ReactorNovaHandleTimeout(&next_tick);
            continue;
        }
        /* else */

        ReactorContextHandleEvents(&reactor_ctx, &next_tick);
    }
    ReactorContextFinalize(&reactor_ctx);

    GenericAgentFinalize(ctx, config);
    CallCleanupFunctions();

    return 0;
}
