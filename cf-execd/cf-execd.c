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

#include "generic_agent.h"
#include "cf-execd-runner.h"

#include "bootstrap.h"
#include "sysinfo.h"
#include "env_context.h"
#include "promises.h"
#include "vars.h"
#include "item_lib.h"
#include "conversion.h"
#include "ornaments.h"
#include "scope.h"
#include "hashes.h"
#include "unix.h"
#include "string_lib.h"
#include "signals.h"
#include "locks.h"
#include "exec_tools.h"
#include "rlist.h"
#include "processes_select.h"
#include "man.h"

#include <assert.h>

#ifdef HAVE_NOVA
# include "cf.nova.h"
#endif

#define CF_EXEC_IFELAPSED 0
#define CF_EXEC_EXPIREAFTER 1

static int NO_FORK;
static int ONCE;
static int WINSERVICE = true;

static pthread_attr_t threads_attrs;

/*******************************************************************/

static GenericAgentConfig *CheckOpts(EvalContext *ctx, int argc, char **argv);
void ThisAgentInit(void);
static bool ScheduleRun(EvalContext *ctx, Policy **policy, GenericAgentConfig *config, ExecConfig *exec_config);
#ifndef __MINGW32__
static void Apoptosis(void);
#endif

static bool LocalExecInThread(const ExecConfig *config);

void StartServer(EvalContext *ctx, Policy *policy, GenericAgentConfig *config, ExecConfig *exec_config);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *CF_EXECD_SHORT_DESCRIPTION = "scheduling daemon for cf-agent";

static const char *CF_EXECD_MANPAGE_LONG_DESCRIPTION =
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
    {"legacy-output", no_argument, 0, 'l'},
    {NULL, 0, 0, '\0'}
};

static const char *HINTS[sizeof(OPTIONS)/sizeof(OPTIONS[0])] =
{
    "Print the help message",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "All talk and no action mode - make no changes, only inform of promises not kept",
    "Output the version of the software",
    "Specify an alternative input file than the default",
    "Define a list of comma separated classes to be defined at the start of execution",
    "Define a list of comma separated classes to be undefined at the start of execution",
    "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Activate internal diagnostics (developers only)",
    "Run as a foreground processes (do not fork)",
    "Run once and then exit (implies no-fork)",
    "Do not run as a service on windows - use this when running from a command shell (CFEngine Nova only)",
    "Set the internal value of LD_LIBRARY_PATH for child processes",
    "Use legacy output format",
    NULL
};

/*****************************************************************************/

int main(int argc, char *argv[])
{
    EvalContext *ctx = EvalContextNew();

    GenericAgentConfig *config = CheckOpts(ctx, argc, argv);
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

    ExecConfig *exec_config = ExecConfigNewDefault(!ONCE, VFQNAME, VIPADDRESS);
    ExecConfigUpdate(ctx, policy, exec_config);
    SetFacility(exec_config->log_facility);

#ifdef __MINGW32__
    if (WINSERVICE)
    {
        NovaWin_StartExecService();
    }
    else
#endif /* __MINGW32__ */
    {
        StartServer(ctx, policy, config, exec_config);
    }

    ExecConfigDestroy(exec_config);
    GenericAgentConfigDestroy(config);

    return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

static GenericAgentConfig *CheckOpts(EvalContext *ctx, int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    int c;
    char ld_library_path[CF_BUFSIZE];
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_EXECUTOR);

    while ((c = getopt_long(argc, argv, "dvnKIf:D:N:VxL:hFOV1gMWl", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'l':
            LEGACY_OUTPUT = true;
            break;

        case 'f':

            if (optarg && strlen(optarg) < 5)
            {
                Log(LOG_LEVEL_ERR, " -f used but argument '%s' incorrect", optarg);
                exit(EXIT_FAILURE);
            }

            GenericAgentConfigSetInputFile(config, GetWorkDir(), optarg);
            MINUSF = true;
            break;

        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            break;

        case 'K':
            IGNORELOCK = true;
            break;

        case 'D':
            config->heap_soft = StringSetFromString(optarg, ',');
            break;

        case 'N':
            config->heap_negated = StringSetFromString(optarg, ',');
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
            IGNORELOCK = true;
            EvalContextHeapAddHard(ctx, "opt_dry_run");
            break;

        case 'L':
            snprintf(ld_library_path, CF_BUFSIZE - 1, "LD_LIBRARY_PATH=%s", optarg);
            if (putenv(xstrdup(ld_library_path)) != 0)
            {
            }
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
            PrintVersion();
            exit(0);

        case 'h':
            PrintHelp("cf-execd", OPTIONS, HINTS, true);
            exit(0);

        case 'M':
            {
                Writer *out = FileWriter(stdout);
                ManPageWrite(out, "cf-execd", time(NULL),
                             CF_EXECD_SHORT_DESCRIPTION,
                             CF_EXECD_MANPAGE_LONG_DESCRIPTION,
                             OPTIONS, HINTS,
                             true);
                FileWriterDetach(out);
                exit(EXIT_SUCCESS);
            }

        case 'x':
            Log(LOG_LEVEL_ERR, "Self-diagnostic functionality is retired.");
            exit(0);

        default:
            PrintHelp("cf-execd", OPTIONS, HINTS, true);
            exit(1);

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

void ThisAgentInit(void)
{
    umask(077);
}

/*****************************************************************************/

/* Might be called back from NovaWin_StartExecService */
void StartServer(EvalContext *ctx, Policy *policy, GenericAgentConfig *config, ExecConfig *exec_config)
{
#if !defined(__MINGW32__)
    time_t now = time(NULL);
#endif

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
#endif

#ifdef __MINGW32__

    if (!NO_FORK)
    {
        Log(LOG_LEVEL_VERBOSE, "Windows does not support starting processes in the background - starting in foreground");
    }

#else /* !__MINGW32__ */

    if ((!NO_FORK) && (fork() != 0))
    {
        Log(LOG_LEVEL_INFO, "cf-execd starting %.24s", ctime(&now));
        _exit(0);
    }

    if (!NO_FORK)
    {
        ActAsDaemon(0);
    }

#endif /* !__MINGW32__ */

    WritePID("cf-execd.pid");
    signal(SIGINT, HandleSignalsForDaemon);
    signal(SIGTERM, HandleSignalsForDaemon);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignalsForDaemon);
    signal(SIGUSR2, HandleSignalsForDaemon);

    umask(077);

    if (ONCE)
    {
        LocalExec(exec_config);
        CloseLog();
    }
    else
    {
        while (!IsPendingTermination())
        {
            if (ScheduleRun(ctx, &policy, config, exec_config))
            {
                Log(LOG_LEVEL_VERBOSE, "Sleeping for splaytime %d seconds", exec_config->splay_time);
                sleep(exec_config->splay_time);

                if (!LocalExecInThread(exec_config))
                {
                    Log(LOG_LEVEL_INFO, "Unable to run agent in thread, falling back to blocking execution");
                    LocalExec(exec_config);
                }
            }
        }
    }
}

/*****************************************************************************/

static void *LocalExecThread(void *param)
{
#if !defined(__MINGW32__)
    sigset_t sigmask;
    sigemptyset(&sigmask);
    pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
#endif

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
    static char promiser_buf[CF_SMALLBUF];
    snprintf(promiser_buf, sizeof(promiser_buf), "%s/bin/cf-execd", CFWORKDIR);

    if (LoadProcessTable(&PROCESSTABLE))
    {
        char myuid[32];
        snprintf(myuid, 32, "%d", (int)getuid());

        Rlist *owners = NULL;
        RlistPrependScalar(&owners, myuid);

        ProcessSelect process_select = {
            .owner = owners,
            .process_result = "process_owner",
        };

        Item *killlist = SelectProcesses(PROCESSTABLE, promiser_buf, process_select, true);

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

static Reload CheckNewPromises(EvalContext *ctx, const GenericAgentConfig *config, const Rlist *input_files)
{
    if (NewPromiseProposals(ctx, config, input_files))
    {
        Log(LOG_LEVEL_VERBOSE, "New promises detected...");

        if (CheckPromises(config))
        {
            return RELOAD_FULL;
        }
        else
        {
            Log(LOG_LEVEL_INFO, "New promises file contains syntax errors -- ignoring");
            PROMISETIME = time(NULL);
        }
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "No new promises found");
    }

    return RELOAD_ENVIRONMENT;
}

static bool ScheduleRun(EvalContext *ctx, Policy **policy, GenericAgentConfig *config, ExecConfig *exec_config)
{
    Log(LOG_LEVEL_VERBOSE, "Sleeping for pulse time %d seconds...", CFPULSETIME);
    sleep(CFPULSETIME);         /* 1 Minute resolution is enough */

    /*
     * FIXME: this logic duplicates the one from cf-serverd.c. Unify ASAP.
     */

    if (CheckNewPromises(ctx, config, InputFiles(ctx, *policy)) == RELOAD_FULL)
    {
        /* Full reload */

        Log(LOG_LEVEL_INFO, "Re-reading promise file '%s'", config->input_file);

        EvalContextHeapClear(ctx);

        DeleteItemList(IPADDRESSES);
        IPADDRESSES = NULL;

        ScopeDeleteAll();

        strcpy(VDOMAIN, "undefined.domain");

        PolicyDestroy(*policy);
        *policy = NULL;

        {
            char *existing_policy_server = ReadPolicyServerFile(GetWorkDir());
            SetPolicyServer(ctx, existing_policy_server);
            free(existing_policy_server);
        }

        ScopeNewSpecial(ctx, "sys", "policy_hub", POLICY_SERVER, DATA_TYPE_STRING);

        GetNameInfo3(ctx, AGENT_TYPE_EXECUTOR);
        GetInterfacesInfo(ctx, AGENT_TYPE_EXECUTOR);
        Get3Environment(ctx, AGENT_TYPE_EXECUTOR);
        BuiltinClasses(ctx);
        OSClasses(ctx);

        EvalContextHeapAddHard(ctx, CF_AGENTTYPES[AGENT_TYPE_EXECUTOR]);

        SetReferenceTime(ctx, true);

        GenericAgentConfigSetBundleSequence(config, NULL);

        *policy = GenericAgentLoadPolicy(ctx, config);
        ExecConfigUpdate(ctx, *policy, exec_config);

        SetFacility(exec_config->log_facility);
    }
    else
    {
        /* Environment reload */

        EvalContextHeapClear(ctx);

        DeleteItemList(IPADDRESSES);
        IPADDRESSES = NULL;

        ScopeClear("this");
        ScopeClear("mon");
        ScopeClear("sys");

        GetInterfacesInfo(ctx, AGENT_TYPE_EXECUTOR);
        Get3Environment(ctx, AGENT_TYPE_EXECUTOR);
        BuiltinClasses(ctx);
        OSClasses(ctx);
        SetReferenceTime(ctx, true);
    }

    {
        StringSetIterator it = StringSetIteratorInit(exec_config->schedule);
        const char *time_context = NULL;
        while ((time_context = StringSetIteratorNext(&it)))
        {
            if (IsDefinedClass(ctx, time_context, NULL))
            {
                Log(LOG_LEVEL_VERBOSE, "Waking up the agent at %s ~ %s", ctime(&CFSTARTTIME), time_context);
                return true;
            }
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Nothing to do at %s", ctime(&CFSTARTTIME));
    return false;
}

