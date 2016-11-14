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

#include <generic_agent.h>
#include <mon.h>

#include <eval_context.h>
#include <env_monitor.h>
#include <conversion.h>
#include <vars.h>
#include <signals.h>
#include <scope.h>
#include <known_dirs.h>
#include <man.h>
#include <bootstrap.h>
#include <timeout.h>
#include <time_classes.h>
#include <loading.h>

typedef enum
{
    MONITOR_CONTROL_FORGET_RATE,
    MONITOR_CONTROL_MONITOR_FACILITY,
    MONITOR_CONTROL_HISTOGRAMS,
    MONITOR_CONTROL_TCP_DUMP,
    MONITOR_CONTROL_NONE
} MonitorControl;

static void ThisAgentInit(EvalContext *ctx);
static GenericAgentConfig *CheckOpts(int argc, char **argv);
static void KeepPromises(EvalContext *ctx, const Policy *policy);

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

extern int NO_FORK;

extern const ConstraintSyntax CFM_CONTROLBODY[];

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *const CF_MONITORD_SHORT_DESCRIPTION =
    "monitoring daemon for CFEngine";

static const char *const CF_MONITORD_MANPAGE_LONG_DESCRIPTION =
    "cf-monitord is the monitoring daemon for CFEngine. It samples probes defined in policy code and attempts to learn the "
    "normal system state based on current and past observations. Current estimates are made available as "
    "special variables (e.g. $(mon.av_cpu)) to cf-agent, which may use them to inform policy decisions.";

static const struct option OPTIONS[] =
{
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"dry-run", no_argument, 0, 'n'},
    {"version", no_argument, 0, 'V'},
    {"no-lock", no_argument, 0, 'K'},
    {"file", required_argument, 0, 'f'},
    {"inform", no_argument, 0, 'I'},
    {"diagnostic", no_argument, 0, 'x'},
    {"no-fork", no_argument, 0, 'F'},
    {"histograms", no_argument, 0, 'H'},
    {"tcpdump", no_argument, 0, 'T'},
    {"color", optional_argument, 0, 'C'},
    {"timestamp", no_argument, 0, 'l'},
    {NULL, 0, 0, '\0'}
};

static const char *const HINTS[] =
{
    "Print the help message",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "All talk and no action mode - make no changes, only inform of promises not kept",
    "Output the version of the software",
    "Ignore system lock",
    "Specify an alternative input file than the default",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Activate internal diagnostics (developers only)",
    "Run process in foreground, not as a daemon",
    "Ignored for backward compatibility",
    "Interface with tcpdump if available to collect data about network",
    "Enable colorized output. Possible values: 'always', 'auto', 'never'. If option is used, the default value is 'auto'",
    "Log timestamps on each line of log output",
    NULL
};

/*****************************************************************************/

int main(int argc, char *argv[])
{
    GenericAgentConfig *config = CheckOpts(argc, argv);
    EvalContext *ctx = EvalContextNew();
    GenericAgentConfigApply(ctx, config);

    GenericAgentDiscoverContext(ctx, config);
    Policy *policy = LoadPolicy(ctx, config);

    GenericAgentPostLoadInit(ctx);
    ThisAgentInit(ctx);

    KeepPromises(ctx, policy);

    MonitorStartServer(ctx, policy);

    PolicyDestroy(policy);
    GenericAgentFinalize(ctx, config);
    return 0;
}

/*******************************************************************/

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_MONITOR, GetTTYInteractive());

    while ((c = getopt_long(argc, argv, "dvnIf:VSxHTKMFhC::l",
                            OPTIONS, NULL)) != -1)
    {
        switch (c)
        {
        case 'f':
            GenericAgentConfigSetInputFile(config, GetInputDir(), optarg);
            MINUSF = true;
            break;

        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            NO_FORK = true;
            break;

        case 'K':
            config->ignore_locks = true;
            break;

        case 'I':
            LogSetGlobalLevel(LOG_LEVEL_INFO);
            break;

        case 'v':
            LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
            NO_FORK = true;
            break;

        case 'F':
            NO_FORK = true;
            break;

        case 'H':              /* Keep accepting this option for compatibility -- no longer used */
            break;

        case 'T':
            MonNetworkSnifferEnable(true);
            break;

        case 'V':
        {
            Writer *w = FileWriter(stdout);
            GenericAgentWriteVersion(w);
            FileWriterDetach(w);
        }
        exit(EXIT_SUCCESS);

        case 'h':
        {
            Writer *w = FileWriter(stdout);
            WriterWriteHelp(w, "cf-monitord", OPTIONS, HINTS, true);
            FileWriterDetach(w);
        }
        exit(EXIT_SUCCESS);

        case 'M':
        {
            Writer *out = FileWriter(stdout);
            ManPageWrite(out, "cf-monitord", time(NULL),
                         CF_MONITORD_SHORT_DESCRIPTION,
                         CF_MONITORD_MANPAGE_LONG_DESCRIPTION,
                         OPTIONS, HINTS,
                         true);
            FileWriterDetach(out);
            exit(EXIT_SUCCESS);
        }

        case 'x':
            Log(LOG_LEVEL_ERR, "Self-diagnostic functionality is retired.");
            exit(EXIT_SUCCESS);

        case 'C':
            if (!GenericAgentConfigParseColor(config, optarg))
            {
                exit(EXIT_FAILURE);
            }
            break;

        case 'l':
            LoggingEnableTimestamps(true);
            break;

        default:
        {
            Writer *w = FileWriter(stdout);
            WriterWriteHelp(w, "cf-monitord", OPTIONS, HINTS, true);
            FileWriterDetach(w);
        }
        exit(EXIT_FAILURE);
        }
    }

    if (!GenericAgentConfigParseArguments(config, argc - optind, argv + optind))
    {
        Log(LOG_LEVEL_ERR, "Too many arguments");
        exit(EXIT_FAILURE);
    }

    return config;
}

/*****************************************************************************/

static void KeepPromises(EvalContext *ctx, const Policy *policy)
{
    Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_MONITOR);
    if (constraints)
    {
        for (size_t i = 0; i < SeqLength(constraints); i++)
        {
            Constraint *cp = SeqAt(constraints, i);

            if (!IsDefinedClass(ctx, cp->classes))
            {
                continue;
            }

            VarRef *ref = VarRefParseFromScope(cp->lval, "control_monitor");
            const void *value = EvalContextVariableGet(ctx, ref, NULL);
            VarRefDestroy(ref);
            if (!value)
            {
                Log(LOG_LEVEL_ERR, "Unknown lval '%s' in monitor control body", cp->lval);
                continue;
            }

            if (strcmp(cp->lval, CFM_CONTROLBODY[MONITOR_CONTROL_HISTOGRAMS].lval) == 0)
            {
                /* Keep accepting this option for backward compatibility. */
            }

            if (strcmp(cp->lval, CFM_CONTROLBODY[MONITOR_CONTROL_TCP_DUMP].lval) == 0)
            {
                MonNetworkSnifferEnable(BooleanFromString(value));
            }

            if (strcmp(cp->lval, CFM_CONTROLBODY[MONITOR_CONTROL_FORGET_RATE].lval) == 0)
            {
                sscanf(value, "%lf", &FORGETRATE);
                Log(LOG_LEVEL_DEBUG, "forget rate %f", FORGETRATE);
            }
        }
    }
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

static void ThisAgentInit(EvalContext *ctx)
{
    umask(077);
    strcpy(VPREFIX, "cf-monitord");

    time_t t = SetReferenceTime();
    UpdateTimeClasses(ctx, t);

    signal(SIGINT, HandleSignalsForDaemon);
    signal(SIGTERM, HandleSignalsForDaemon);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignalsForDaemon);
    signal(SIGUSR2, HandleSignalsForDaemon);

    FORGETRATE = 0.6;

    MonitorInitialize();
}
