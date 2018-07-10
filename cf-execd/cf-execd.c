/*
   Copyright 2017 Northern.tech AS

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

#include <cf-execd.h>

#include <cf-execd-runner.h>
#include <item_lib.h>
#include <known_dirs.h>
#include <man.h>
#include <ornaments.h>
#include <exec_tools.h>
#include <signals.h>
#include <processes_select.h>
#include <bootstrap.h>
#include <sysinfo.h>
#include <timeout.h>
#include <time_classes.h>
#include <loading.h>
#include <printsize.h>
#include <cleanup.h>

#include <cf-windows-functions.h>

#define CF_EXEC_IFELAPSED 0
#define CF_EXEC_EXPIREAFTER 1

static int NO_FORK = false; /* GLOBAL_A */
static int ONCE = false; /* GLOBAL_A */
static int WINSERVICE = true; /* GLOBAL_A */

static pthread_attr_t threads_attrs; /* GLOBAL_T, initialized by pthread_attr_init */

/*******************************************************************/

static GenericAgentConfig *CheckOpts(int argc, char **argv);

void ThisAgentInit(void);
static bool ScheduleRun(EvalContext *ctx, Policy **policy, GenericAgentConfig *config,
                        ExecdConfig **execd_config, ExecConfig **exec_config);
#ifndef __MINGW32__
static void Apoptosis(void);
#endif

static bool LocalExecInThread(const ExecConfig *config);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *const CF_EXECD_SHORT_DESCRIPTION =
    "scheduling daemon for cf-agent";

static const char *const CF_EXECD_MANPAGE_LONG_DESCRIPTION =
    "cf-execd is the scheduling daemon for cf-agent. It runs cf-agent locally according to a schedule specified in "
    "policy code (executor control body). After a cf-agent run is completed, cf-execd gathers output from cf-agent, "
    "and may be configured to email the output to a specified address. It may also be configured to splay (randomize) the "
    "execution schedule to prevent synchronized cf-agent runs across a network.";

static const struct option OPTIONS[] =
{
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"dry-run", no_argument, 0, 'n'},
    {"version", no_argument, 0, 'V'},
    {"file", required_argument, 0, 'f'},
    {"define", required_argument, 0, 'D'},
    {"negate", required_argument, 0, 'N'},
    {"no-lock", no_argument, 0, 'K'},
    {"inform", no_argument, 0, 'I'},
    {"diagnostic", no_argument, 0, 'x'},
    {"no-fork", no_argument, 0, 'F'},
    {"once", no_argument, 0, 'O'},
    {"no-winsrv", no_argument, 0, 'W'},
    {"ld-library-path", required_argument, 0, 'L'},
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
    "Specify an alternative input file than the default. This option is overridden by FILE if supplied as argument.",
    "Define a list of comma separated classes to be defined at the start of execution",
    "Define a list of comma separated classes to be undefined at the start of execution",
    "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Activate internal diagnostics (developers only)",
    "Run as a foreground processes (do not fork)",
    "Run once and then exit (implies no-fork)",
    "Do not run as a service on windows - use this when running from a command shell (CFEngine Nova only)",
    "Set the internal value of LD_LIBRARY_PATH for child processes",
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

    Policy *policy = SelectAndLoadPolicy(config, ctx, false, false);
    
    if (!policy)
    {
        Log(LOG_LEVEL_ERR, "Error reading CFEngine policy. Exiting...");
        DoCleanupAndExit(EXIT_FAILURE);
    }

    GenericAgentPostLoadInit(ctx);
    ThisAgentInit();

    ExecConfig *exec_config = ExecConfigNew(!ONCE, ctx, policy);
    ExecdConfig *execd_config = ExecdConfigNew(ctx, policy);
    SetFacility(execd_config->log_facility);

#ifdef __MINGW32__
    if (WINSERVICE)
    {
        NovaWin_StartExecService();
    }
    else
#endif /* __MINGW32__ */
    {
        StartServer(ctx, policy, config, &execd_config, &exec_config);
    }

    GenericAgentFinalize(ctx, config);
    ExecConfigDestroy(exec_config);
    ExecdConfigDestroy(execd_config);

    return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int c;
    char ld_library_path[CF_BUFSIZE];
    
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_EXECUTOR, GetTTYInteractive());


    while ((c = getopt_long(argc, argv, "dvnKIf:D:N:VxL:hFOV1gMWC::l",
                            OPTIONS, NULL))
           != -1)
    {
        switch (c)
        {
        case 'f':
            GenericAgentConfigSetInputFile(config, GetInputDir(), optarg);
            MINUSF = true;
            break;

        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            break;

        case 'K':
            config->ignore_locks = true;
            break;

        case 'D':
            {
                StringSet *defined_classes = StringSetFromString(optarg, ',');
                if (! config->heap_soft)
                {
                    config->heap_soft = defined_classes;
                }
                else
                {
                    StringSetJoin(config->heap_soft, defined_classes);
                    free(defined_classes);
                }
            }
            break;

        case 'N':
            {
                StringSet *negated_classes = StringSetFromString(optarg, ',');
                if (! config->heap_negated)
                {
                    config->heap_negated = negated_classes;
                }
                else
                {
                    StringSetJoin(config->heap_negated, negated_classes);
                    free(negated_classes);
                }
            }
            break;

        case 'I':
            LogSetGlobalLevel(LOG_LEVEL_INFO);
            break;

        case 'v':
            LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
            NO_FORK = true; // TODO: really?
            break;

        case 'n':
            DONTDO = true;
            config->ignore_locks = true;
            break;

        case 'L':
            snprintf(ld_library_path, CF_BUFSIZE - 1, "LD_LIBRARY_PATH=%s", optarg);
            putenv(xstrdup(ld_library_path));
            break;

        case 'W':
            WINSERVICE = false;
            break;

        case 'F':
            NO_FORK = true;
            break;

        case 'O':
            ONCE = true;
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
            GenericAgentWriteHelp(w, "cf-execd", OPTIONS, HINTS, true);
            FileWriterDetach(w);
        }
        DoCleanupAndExit(EXIT_SUCCESS);

        case 'M':
        {
            Writer *out = FileWriter(stdout);
            ManPageWrite(out, "cf-execd", time(NULL),
                         CF_EXECD_SHORT_DESCRIPTION,
                         CF_EXECD_MANPAGE_LONG_DESCRIPTION,
                         OPTIONS, HINTS,
                         true);
            FileWriterDetach(out);
            DoCleanupAndExit(EXIT_SUCCESS);
        }

        case 'x':
            Log(LOG_LEVEL_ERR, "Self-diagnostic functionality is retired.");
            DoCleanupAndExit(EXIT_SUCCESS);

        case 'C':
            if (!GenericAgentConfigParseColor(config, optarg))
            {
                DoCleanupAndExit(EXIT_FAILURE);
            }
            break;

        case 'l':
            LoggingEnableTimestamps(true);
            break;

        default:
        {
            Writer *w = FileWriter(stdout);
            GenericAgentWriteHelp(w, "cf-execd", OPTIONS, HINTS, true);
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

void ThisAgentInit(void)
{
    umask(077);
}

/*****************************************************************************/

// msg should include exactly one reference to unsigned int.
static unsigned int MaybeSleepLog(LogLevel level, const char *msg, unsigned int seconds)
{
    if (IsPendingTermination())
    {
        return seconds;
    }

    Log(level, msg, seconds);

    return sleep(seconds);
}

/*****************************************************************************/

/* Might be called back from NovaWin_StartExecService */
void StartServer(EvalContext *ctx, Policy *policy, GenericAgentConfig *config, ExecdConfig **execd_config, ExecConfig **exec_config)
{
    pthread_attr_init(&threads_attrs);
    pthread_attr_setdetachstate(&threads_attrs, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&threads_attrs, (size_t)2048*1024);

    Banner("Starting executor");

#ifndef __MINGW32__
    if (!ONCE)
    {
        /* Kill previous instances of cf-execd if those are still running */
        Apoptosis();
    }

    time_t now = time(NULL);
    if ((!NO_FORK) && (fork() != 0))
    {
        Log(LOG_LEVEL_INFO, "cf-execd starting %.24s", ctime(&now));
        _exit(EXIT_SUCCESS);
    }

    if (!NO_FORK)
    {
        ActAsDaemon();
    }

#else  /* __MINGW32__ */

    if (!NO_FORK)
    {
        Log(LOG_LEVEL_VERBOSE, "Windows does not support starting processes in the background - starting in foreground");
    }

#endif

    WritePID("cf-execd.pid");
    signal(SIGINT, HandleSignalsForDaemon);
    signal(SIGTERM, HandleSignalsForDaemon);
    signal(SIGHUP, HandleSignalsForDaemon);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignalsForDaemon);
    signal(SIGUSR2, HandleSignalsForDaemon);

    umask(077);

    if (ONCE)
    {
        LocalExec(*exec_config);
        CloseLog();
    }
    else
    {
        while (!IsPendingTermination())
        {
            if (ScheduleRun(ctx, &policy, config, execd_config, exec_config))
            {
                MaybeSleepLog(LOG_LEVEL_VERBOSE, "Sleeping for splaytime %u seconds", (*execd_config)->splay_time);

                // We are sleeping both above and inside ScheduleRun(), so make
                // sure a terminating signal did not arrive during that time.
                if (IsPendingTermination())
                {
                    break;
                }

                if (!LocalExecInThread(*exec_config))
                {
                    Log(LOG_LEVEL_INFO, "Unable to run agent in thread, falling back to blocking execution");
                    LocalExec(*exec_config);
                }
            }
        }
    }
    PolicyDestroy(policy);
}

/*****************************************************************************/

static void *LocalExecThread(void *param)
{
    ExecConfig *config = (ExecConfig *)param;
    LocalExec(config);
    ExecConfigDestroy(config);

    return NULL;
}

static bool LocalExecInThread(const ExecConfig *config)
{
    ExecConfig *thread_config = ExecConfigCopy(config);

    pthread_t tid;

    if (pthread_create(&tid, &threads_attrs, LocalExecThread, thread_config) == 0)
    {
        return true;
    }
    else
    {
        ExecConfigDestroy(thread_config);
        Log(LOG_LEVEL_INFO, "Can't create thread. (pthread_create: %s)", GetErrorStr());
        return false;
    }
}

#ifndef __MINGW32__

static void Apoptosis(void)
{
    char promiser_buf[CF_SMALLBUF];
    snprintf(promiser_buf, sizeof(promiser_buf), "%s%cbin%ccf-execd",
             GetWorkDir(), FILE_SEPARATOR, FILE_SEPARATOR);

    if (LoadProcessTable(&PROCESSTABLE))
    {
        char myuid[PRINTSIZE(unsigned)];
        xsnprintf(myuid, sizeof(myuid), "%u", (unsigned) getuid());

        Rlist *owners = NULL;
        RlistPrepend(&owners, myuid, RVAL_TYPE_SCALAR);

        ProcessSelect process_select = {
            .owner = owners,
            .process_result = "process_owner",
        };

        Item *killlist = SelectProcesses(PROCESSTABLE, promiser_buf, process_select, true);
        RlistDestroy(owners);

        for (Item *ip = killlist; ip != NULL; ip = ip->next)
        {
            pid_t pid = ip->counter;

            if (pid != getpid() && kill(pid, SIGTERM) < 0)
            {
                if (errno == ESRCH)
                {
                    /* That's ok, process exited voluntarily */
                }
                else
                {
                    Log(LOG_LEVEL_ERR, "Unable to kill stale cf-execd process pid=%d. (kill: %s)",
                        (int)pid, GetErrorStr());
                }
            }
        }
    }

    DeleteItemList(PROCESSTABLE);

    Log(LOG_LEVEL_VERBOSE, "Pruning complete");
}

#endif

typedef enum
{
    RELOAD_ENVIRONMENT,
    RELOAD_FULL
} Reload;

static Reload CheckNewPromises(GenericAgentConfig *config)
{
    Log(LOG_LEVEL_DEBUG, "Checking file updates for input file '%s'", config->input_file);

    time_t validated_at = ReadTimestampFromPolicyValidatedFile(config, NULL);

    bool reload_config = false;

    if (config->agent_specific.daemon.last_validated_at < validated_at)
    {
        Log(LOG_LEVEL_VERBOSE, "New promises detected...");
        reload_config = true;
    }
    if (ReloadConfigRequested())
    {
        Log(LOG_LEVEL_VERBOSE, "Force reload of inputs files...");
        reload_config = true;
    }

    if (reload_config)
    {
        ClearRequestReloadConfig();

        /* Rereading policies now, so update timestamp. */
        config->agent_specific.daemon.last_validated_at = validated_at;

        if (GenericAgentArePromisesValid(config))
        {
            return RELOAD_FULL;
        }
        else
        {
            Log(LOG_LEVEL_INFO, "New promises file contains syntax errors -- ignoring");
        }
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "No new promises found");
    }

    return RELOAD_ENVIRONMENT;
}

static bool ScheduleRun(EvalContext *ctx, Policy **policy, GenericAgentConfig *config,
                        ExecdConfig **execd_config, ExecConfig **exec_config)
{
    /* 1 Minute resolution is enough */
    MaybeSleepLog(LOG_LEVEL_VERBOSE, "Sleeping for pulse time %u seconds...", CFPULSETIME);

    /*
     * FIXME: this logic duplicates the one from cf-serverd.c. Unify ASAP.
     */

    if (CheckNewPromises(config) == RELOAD_FULL)
    {
        /* Full reload */

        Log(LOG_LEVEL_INFO, "Re-reading promise file '%s'", config->input_file);

        EvalContextClear(ctx);

        strcpy(VDOMAIN, "undefined.domain");

        PolicyDestroy(*policy);
        *policy = NULL;

        {
            char *existing_policy_server = ReadPolicyServerFile(GetWorkDir());
            SetPolicyServer(ctx, existing_policy_server);
            free(existing_policy_server);
        }
        UpdateLastPolicyUpdateTime(ctx);

        DetectEnvironment(ctx);

        Log(LOG_LEVEL_INFO, "Re-evaluating augments" );
        LoadAugments(ctx, config);

        EvalContextClassPutHard(ctx, CF_AGENTTYPES[AGENT_TYPE_EXECUTOR], "cfe_internal,source=agent");

        time_t t = SetReferenceTime();
        UpdateTimeClasses(ctx, t);

        GenericAgentConfigSetBundleSequence(config, NULL);

        *policy = LoadPolicy(ctx, config);
        ExecConfigDestroy(*exec_config);
        ExecdConfigDestroy(*execd_config);

        *exec_config = ExecConfigNew(!ONCE, ctx, *policy);
        *execd_config = ExecdConfigNew(ctx, *policy);

        SetFacility((*execd_config)->log_facility);
    }
    else
    {
        /* Environment reload */

        EvalContextClear(ctx);

        DetectEnvironment(ctx);

        time_t t = SetReferenceTime();
        UpdateTimeClasses(ctx, t);
    }

    {
        StringSetIterator it = StringSetIteratorInit((*execd_config)->schedule);
        const char *time_context = NULL;
        while ((time_context = StringSetIteratorNext(&it)))
        {
            if (IsDefinedClass(ctx, time_context))
            {
                Log(LOG_LEVEL_VERBOSE, "Waking up the agent at %s ~ %s", ctime(&CFSTARTTIME), time_context);
                return true;
            }
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Nothing to do at %s", ctime(&CFSTARTTIME));
    return false;
}

