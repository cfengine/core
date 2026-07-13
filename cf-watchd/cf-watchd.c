#include <stdio.h>
#include <unistd.h>

#include <exec_tools.h>
#include <signals.h>
#include <generic_agent.h>
#include <logging.h>
#include <eval_context.h>
#include <cleanup.h>
#include <man.h>
#include <known_dirs.h>


static GenericAgentConfig *CheckOpts(int argc, char **argv);
static void WatchStart();

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

int NO_FORK = false;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/


static const char *const CF_WATCHD_SHORT_DESCRIPTION =
    "event watching daemon for CFEngine";

static const char *const CF_WATCHD_MANPAGE_LONG_DESCRIPTION =
    "cf-watchd is the event watching and reacting daemon for CFEngine. It watches specific events defined in the policy "
    "and runs policy code on reaction to these events.";

static const Component COMPONENT =
{
    .name = "cf-watchd",
    .website = CF_WEBSITE,
    .copyright = CF_COPYRIGHT
};

static const struct option OPTIONS[] =
{
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"no-lock", no_argument, 0, 'K'},
    {"file", required_argument, 0, 'f'},
    {"log-level", required_argument, 0, 'g'},
    {"inform", no_argument, 0, 'I'},
    {"no-fork", no_argument, 0, 'F'},
    {"man", no_argument, 0, 'M'},
    {"color", optional_argument, 0, 'C'},
    {"timestamp", no_argument, 0, 'l'},
    /* Only long option for the rest */
    {"ignore-preferred-augments", no_argument, 0, 0},
    {NULL, 0, 0, '\0'}
};

static const char *const HINTS[] =
{
    "Print the help message",
    "Enable debugging output",
    "Output verbose information about the behaviour of cf-watchd",
    "Output the version of the software",
    "Ignore system lock",
    "Specify an alternative input file than the default. This option is overridden by FILE if supplied as argument.",
    "Specify how detailed logs should be. Possible values: 'error', 'warning', 'notice', 'info', 'verbose', 'debug'",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Run process in foreground, not as a daemon",
    "Outputs the man page of the program",
    "Enable colorized output. Possible values: 'always', 'auto', 'never'. If option is used, the default value is 'auto'",
    "Log timestamps on each line of log output",
    "Ignore def_preferred.json file in favor of def.json",
    NULL
};

/*****************************************************************************/

int main(int argc, char **argv)
{
  GenericAgentConfig *config = CheckOpts(argc, argv);
  EvalContext *ctx = EvalContextNew();
  GenericAgentConfigApply(ctx, config);

#ifndef __MINGW32__
  pid_t existing_pid = ReadPID("cf-watchd.pid");
  if ((existing_pid != -1) && (kill(existing_pid, 0) == 0))
  {
      Log(LOG_LEVEL_ERR, "Another instance of cf-watchd is already running, terminating");
      return 1;
  }
#endif

#ifdef __MINGW32__

  if (!NO_FORK)
  {
    Log(LOG_LEVEL_VERBOSE, "Windows does not support starting processes in the background - starting in foreground");
  }

#else /* !__MINGW32__ */

  if ((!NO_FORK) && (fork() != 0))
  {
    Log(LOG_LEVEL_INFO, "cf-watchd: starting");
    _exit(EXIT_SUCCESS);
  }

  if (!NO_FORK)
  {
      ActAsDaemon();
  }

#endif /* !__MINGW32__ */


  umask(077);
  WritePID("cf-watchd.pid");

  signal(SIGINT, HandleSignalsForDaemon);
  signal(SIGTERM, HandleSignalsForDaemon);
  signal(SIGBUS, HandleSignalsForDaemon);
  signal(SIGHUP, HandleSignalsForDaemon);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGUSR1, HandleSignalsForDaemon);
  signal(SIGUSR2, HandleSignalsForDaemon);

  WatchStart();

  GenericAgentFinalize(ctx, config);
  CallCleanupFunctions();

  return 0;
}

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_WATCH, GetTTYInteractive());

    int longopt_idx;
    while ((c = getopt_long(argc, argv, "dvIf:g:VKMFhC::l",
                            OPTIONS, &longopt_idx)) != -1)
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
            ManPageWrite(out, "cf-watchd", time(NULL),
                         CF_WATCHD_SHORT_DESCRIPTION,
                         CF_WATCHD_MANPAGE_LONG_DESCRIPTION,
                         OPTIONS, HINTS,
                         NULL, false,
                         true);
            FileWriterDetach(out);
            DoCleanupAndExit(EXIT_SUCCESS);
        }

        case 'C':
            if (!GenericAgentConfigParseColor(config, optarg))
            {
                DoCleanupAndExit(EXIT_FAILURE);
            }
            break;

        case 'l':
            LoggingEnableTimestamps(true);
            break;

        /* long options only */
        case 0:
        {
            const char *const option_name = OPTIONS[longopt_idx].name;
            if (StringEqual(option_name, "ignore-preferred-augments"))
            {
                config->ignore_preferred_augments = true;
            }
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

static void WatchStart()
{
  for (int i = 0; !IsPendingTermination(); i++)
  {
    /* Do something */
    Log(LOG_LEVEL_INFO, "cf-watchd: %d", i);
    sleep(1);
  }
}
