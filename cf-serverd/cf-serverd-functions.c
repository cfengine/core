/*
   Copyright 2018 Northern.tech AS

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

#include <cf-serverd-functions.h>
#include <cf-serverd-enterprise-stubs.h>

#include <server_access.h>
#include <client_code.h>
#include <server_code.h>
#include <server_transform.h>
#include <bootstrap.h>
#include <policy_server.h>
#include <scope.h>
#include <signals.h>
#include <systype.h>
#include <mutex.h>
#include <locks.h>
#include <exec_tools.h>
#include <unix.h>
#include <man.h>
#include <server_tls.h>                              /* ServerTLSInitialize */
#include <timeout.h>
#include <known_dirs.h>
#include <sysinfo.h>
#include <time_classes.h>
#include <connection_info.h>
#include <string_lib.h>
#include <file_lib.h>
#include <loading.h>
#include <printsize.h>
#include <cleanup.h>

#define WAIT_INCOMING_TIMEOUT 10

/* see man:listen(3) */
#define DEFAULT_LISTEN_QUEUE_SIZE 128
#define MAX_LISTEN_QUEUE_SIZE 2048

int NO_FORK = false; /* GLOBAL_A */

/*******************************************************************/
/* Command line option parsing                                     */
/*******************************************************************/

static const char *const CF_SERVERD_SHORT_DESCRIPTION = "CFEngine file server daemon";

static const char *const CF_SERVERD_MANPAGE_LONG_DESCRIPTION =
        "cf-serverd is a socket listening daemon providing two services: it acts as a file server for remote file copying "
        "and it allows an authorized cf-runagent to start a cf-agent run. cf-agent typically connects to a "
        "cf-serverd instance to request updated policy code, but may also request additional files for download. "
        "cf-serverd employs role based access control (defined in policy code) to authorize requests.";

static const struct option OPTIONS[] =
{
    {"help", no_argument, 0, 'h'},
    {"log-level", required_argument, 0, 'g'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"file", required_argument, 0, 'f'},
    {"define", required_argument, 0, 'D'},
    {"negate", required_argument, 0, 'N'},
    {"no-lock", no_argument, 0, 'K'},
    {"inform", no_argument, 0, 'I'},
    {"diagnostic", no_argument, 0, 'x'},
    {"no-fork", no_argument, 0, 'F'},
    {"ld-library-path", required_argument, 0, 'L'},
    {"generate-avahi-conf", no_argument, 0, 'A'},
    {"color", optional_argument, 0, 'C'},
    {"timestamp", no_argument, 0, 'l'},
    {NULL, 0, 0, '\0'}
};

static const char *const HINTS[] =
{
    "Print the help message",
    "Specify how detailed logs should be. Possible values: 'error', 'warning', 'notice', 'info', 'verbose', 'debug'",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    "Specify an alternative input file than the default. This option is overridden by FILE if supplied as argument.",
    "Define a list of comma separated classes to be defined at the start of execution",
    "Define a list of comma separated classes to be undefined at the start of execution",
    "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Activate internal diagnostics (developers only)",
    "Run as a foreground processes (do not fork)",
    "Set the internal value of LD_LIBRARY_PATH for child processes",
    "Generates avahi configuration file to enable policy server to be discovered in the network",
    "Enable colorized output. Possible values: 'always', 'auto', 'never'. If option is used, the default value is 'auto'",
    "Log timestamps on each line of log output",
    NULL
};

#ifdef HAVE_AVAHI_CLIENT_CLIENT_H
#ifdef HAVE_AVAHI_COMMON_ADDRESS_H
static int GenerateAvahiConfig(const char *path)
{
    FILE *fout;
    Writer *writer = NULL;
    fout = safe_fopen(path, "w+");
    if (fout == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to open '%s'", path);
        return -1;
    }
    writer = FileWriter(fout);
    fprintf(fout, "<?xml version=\"1.0\" standalone='no'?>\n");
    fprintf(fout, "<!DOCTYPE service-group SYSTEM \"avahi-service.dtd\">\n");
    XmlComment(writer, "This file has been automatically generated by cf-serverd.");
    XmlStartTag(writer, "service-group", 0);
    FprintAvahiCfengineTag(fout);
    XmlStartTag(writer, "service", 0);
    XmlTag(writer, "type", "_cfenginehub._tcp",0);
    DetermineCfenginePort();
    XmlStartTag(writer, "port", 0);
    WriterWriteF(writer, "%d", CFENGINE_PORT);
    XmlEndTag(writer, "port");
    XmlEndTag(writer, "service");
    XmlEndTag(writer, "service-group");
    fclose(fout);

    return 0;
}
#define SUPPORT_AVAHI_CONFIG
#endif
#endif

GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_SERVER, GetTTYInteractive());

    while ((c = getopt_long(argc, argv, "dvIKf:g:D:N:VSxLFMhAC::l",
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
            NO_FORK = true;
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
            NO_FORK = true;
            break;

        case 'g':
            LogSetGlobalLevelArgOrExit(optarg);
            break;

        case 'F':
            NO_FORK = true;
            break;

        case 'L':
        {
            static char ld_library_path[CF_BUFSIZE]; /* GLOBAL_A */
            Log(LOG_LEVEL_VERBOSE, "Setting LD_LIBRARY_PATH to '%s'", optarg);
            snprintf(ld_library_path, CF_BUFSIZE - 1, "LD_LIBRARY_PATH=%s", optarg);
            putenv(ld_library_path);
            break;
        }

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
                WriterWriteHelp(w, "cf-serverd", OPTIONS, HINTS, true, NULL);
                FileWriterDetach(w);
            }
            DoCleanupAndExit(EXIT_SUCCESS);

        case 'M':
            {
                Writer *out = FileWriter(stdout);
                ManPageWrite(out, "cf-serverd", time(NULL),
                             CF_SERVERD_SHORT_DESCRIPTION,
                             CF_SERVERD_MANPAGE_LONG_DESCRIPTION,
                             OPTIONS, HINTS,
                             true);
                FileWriterDetach(out);
                DoCleanupAndExit(EXIT_SUCCESS);
            }

        case 'x':
            Log(LOG_LEVEL_ERR, "Self-diagnostic functionality is retired.");
            DoCleanupAndExit(EXIT_SUCCESS);

        case 'A':
#ifdef SUPPORT_AVAHI_CONFIG
            Log(LOG_LEVEL_NOTICE, "Generating Avahi configuration file.");
            if (GenerateAvahiConfig("/etc/avahi/services/cfengine-hub.service") != 0)
            {
                DoCleanupAndExit(EXIT_FAILURE);
            }
            cf_popen("/etc/init.d/avahi-daemon restart", "r", true);
            Log(LOG_LEVEL_NOTICE, "Avahi configuration file generated successfully.");
#else
            Log(LOG_LEVEL_ERR, "Generating avahi configuration can only be done when avahi-daemon and libavahi are installed on the machine.");
#endif
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
                WriterWriteHelp(w, "cf-serverd", OPTIONS, HINTS, true, NULL);
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

/*********************************************************************/
/* Policy Reloading                                                  */
/*********************************************************************/

static void DeleteAuthList(Auth **list, Auth **list_tail)
{
    Auth *ap = *list;

    while (ap != NULL)
    {
        Auth *ap_next = ap->next;

        DeleteItemList(ap->accesslist);
        DeleteItemList(ap->maproot);
        free(ap->path);
        free(ap);

        /* Just make sure the tail was consistent. */
        if (ap_next == NULL)
            assert(ap == *list_tail);

        ap = ap_next;
    }

    *list = NULL;
    *list_tail = NULL;
}

static void KeepHardClasses(EvalContext *ctx)
{
    char *existing_policy_server = PolicyServerReadFile(GetWorkDir());
    if (existing_policy_server)
    {
        free(existing_policy_server);
        if (GetAmPolicyHub())
        {
            MarkAsPolicyServer(ctx);
        }
    }

    /* FIXME: why is it not in generic_agent?! */
    GenericAgentAddEditionClasses(ctx);
}

/* Must not be called unless ACTIVE_THREADS is zero: */
static void ClearAuthAndACLs(void)
{
    /* Must have no currently open connections to free the ACLs. */
    assert(SERVER_ACCESS.connectionlist == NULL);

    /* Bundle server access_rules legacy ACLs */
    DeleteAuthList(&SERVER_ACCESS.admit, &SERVER_ACCESS.admittail);
    DeleteAuthList(&SERVER_ACCESS.deny, &SERVER_ACCESS.denytail);
    DeleteAuthList(&SERVER_ACCESS.varadmit, &SERVER_ACCESS.varadmittail);
    DeleteAuthList(&SERVER_ACCESS.vardeny, &SERVER_ACCESS.vardenytail);

    /* body server control ACLs */
    DeleteItemList(SERVER_ACCESS.trustkeylist);        SERVER_ACCESS.trustkeylist = NULL;
    DeleteItemList(SERVER_ACCESS.attackerlist);        SERVER_ACCESS.attackerlist = NULL;
    DeleteItemList(SERVER_ACCESS.nonattackerlist);     SERVER_ACCESS.nonattackerlist = NULL;
    DeleteItemList(SERVER_ACCESS.allowuserlist);       SERVER_ACCESS.allowuserlist = NULL;
    DeleteItemList(SERVER_ACCESS.multiconnlist);       SERVER_ACCESS.multiconnlist = NULL;
    DeleteItemList(SERVER_ACCESS.allowuserlist);       SERVER_ACCESS.allowuserlist = NULL;
    DeleteItemList(SERVER_ACCESS.allowlegacyconnects); SERVER_ACCESS.allowlegacyconnects = NULL;

    StringMapDestroy(SERVER_ACCESS.path_shortcuts);    SERVER_ACCESS.path_shortcuts  = NULL;
    free(SERVER_ACCESS.allowciphers);                  SERVER_ACCESS.allowciphers    = NULL;
    free(SERVER_ACCESS.allowtlsversion);               SERVER_ACCESS.allowtlsversion = NULL;

    /* body server control new ACLs */
    NEED_REVERSE_LOOKUP = false;
    acl_Free(paths_acl);    paths_acl    = NULL;
    acl_Free(classes_acl);  classes_acl  = NULL;
    acl_Free(vars_acl);     vars_acl     = NULL;
    acl_Free(literals_acl); literals_acl = NULL;
    acl_Free(query_acl);    query_acl    = NULL;
    acl_Free(bundles_acl);  bundles_acl  = NULL;
    acl_Free(roles_acl);    roles_acl    = NULL;
}

static void CheckFileChanges(EvalContext *ctx, Policy **policy, GenericAgentConfig *config)
{
    Log(LOG_LEVEL_DEBUG, "Checking file updates for input file '%s'",
        config->input_file);

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
            Log(LOG_LEVEL_NOTICE, "Rereading policy file '%s'",
                config->input_file);

            /* STEP 1: Free everything */

            EvalContextClear(ctx);

            strcpy(VDOMAIN, "undefined.domain");

            ClearAuthAndACLs();
            PolicyDestroy(*policy);               *policy = NULL;

            /* STEP 2: Set Environment, Parse and Evaluate policy */

            /*
             * TODO why is this done separately here? What's the difference to
             * calling the same steps as in cf-serverd.c:main()? Those are:
             *   GenericAgentConfigApply();     // not here!
             *   GenericAgentDiscoverContext(); // not here!
             *   EvalContextClassPutHard("server");             // only here!
             *   if (GenericAgentCheckPolicy()) // not here!
             *     policy = LoadPolicy();
             *   ThisAgentInit();               // not here, only calls umask()
             *   ReloadHAConfig();                              // only here!
             *   KeepPromises();
             *   Summarize();
             * Plus the following from within StartServer() which is only
             * called during startup:
             *   InitSignals();                  // not here
             *   ServerTLSInitialize();          // not here
             *   SetServerListenState();         // not here
             *   InitServer()                    // not here
             *   PolicyNew()+AcquireServerLock() // not here
             *   PrepareServer(sd);              // not here
             *   CollectCallStart();  // both
             */

            EvalContextSetPolicyServerFromFile(ctx, GetWorkDir());

            UpdateLastPolicyUpdateTime(ctx);

            DetectEnvironment(ctx);
            GenericAgentDiscoverContext(ctx, config);
            KeepHardClasses(ctx);

            /* During startup this is done in GenericAgentDiscoverContext(). */
            EvalContextClassPutHard(ctx, CF_AGENTTYPES[AGENT_TYPE_SERVER], "cfe_internal,source=agent");

            time_t t = SetReferenceTime();
            UpdateTimeClasses(ctx, t);

            /* TODO BUG: this modifies config, but previous config has not
             * been reset/free'd. Ideally we would want LoadPolicy to not
             * modify config at all, but only modify ctx. */
            *policy = LoadPolicy(ctx, config);

            /* Reload HA related configuration */
            ReloadHAConfig();

            KeepPromises(ctx, *policy, config);
            Summarize();
        }
        else
        {
            Log(LOG_LEVEL_INFO, "File changes contain errors -- ignoring");
        }
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "No new promises found");
    }
}


/* Set up standard signal-handling. */
static void InitSignals()
{
    MakeSignalPipe();

    signal(SIGINT, HandleSignalsForDaemon);
    signal(SIGTERM, HandleSignalsForDaemon);
    signal(SIGHUP, HandleSignalsForDaemon);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignalsForDaemon);
    signal(SIGUSR2, HandleSignalsForDaemon);
}

/* Prepare synthetic agent promise and lock it. */
static CfLock AcquireServerLock(EvalContext *ctx,
                                GenericAgentConfig *config,
                                Policy *server_policy)
{
    Promise *pp = NULL;
    {
        Bundle *bp = PolicyAppendBundle(server_policy, NamespaceDefault(),
                                        "server_cfengine_bundle", "agent",
                                        NULL, NULL);
        PromiseType *tp = BundleAppendPromiseType(bp, "server_cfengine");

        pp = PromiseTypeAppendPromise(tp, config->input_file,
                                      (Rval) { NULL, RVAL_TYPE_NOPROMISEE },
                                      NULL, NULL);
    }
    assert(pp);

    TransactionContext tc = {
        .ifelapsed = 0,
        .expireafter = 1,
    };
    return AcquireLock(ctx, pp->promiser, VUQNAME, CFSTARTTIME, tc, pp, false);
}

/* Final preparations for running as server */
static void PrepareServer(int sd)
{
    if (sd != -1)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Listening for connections on socket descriptor %d ...", sd);
    }

    if (!NO_FORK)
#ifdef __MINGW32__
    {
        Log(LOG_LEVEL_VERBOSE,
            "Windows does not support starting processes in the background - running in foreground");
    }
#else
    {
        if (fork() != 0)                                        /* parent */
        {
            _exit(EXIT_SUCCESS);
        }

        ActAsDaemon();
    }
#endif

    /* Close sd on exec, needed for not passing the socket to cf-runagent
     * spawned commands. */
    SetCloseOnExec(sd, true);

    Log(LOG_LEVEL_NOTICE, "Server is starting...");
    WritePID("cf-serverd.pid"); /* Arranges for cleanup() to tidy it away */
}

/* Wait for connection-handler threads to finish their work.
 *
 * @return Number of live threads remaining after waiting.
 */
static int WaitOnThreads()
{
    int result = 1;
    for (int i = 2; i > 0; i--)
    {
        if (ThreadLock(cft_server_children))
        {
            result = ACTIVE_THREADS;
            ThreadUnlock(cft_server_children);
        }

        if (result == 0)
        {
            break;
        }

        Log(LOG_LEVEL_VERBOSE,
            "Waiting %ds for %d connection threads to finish",
            i, result);

        sleep(1);
    }

    if (result > 0)
    {
        Log(LOG_LEVEL_VERBOSE,
            "There are %d connection threads left, exiting anyway",
            result);
    }
    else
    {
        assert(result == 0);
        Log(LOG_LEVEL_VERBOSE,
            "All threads are done, cleaning up allocations");
        ClearAuthAndACLs();
        ServerTLSDeInitialize(NULL, NULL, NULL);
    }

    return result;
}

static void CollectCallIfDue(EvalContext *ctx)
{
    /* Check whether we have established peering with a hub */
    if (CollectCallHasPending())
    {
        extern int COLLECT_WINDOW;
        int waiting_queue = 0;
        int new_client = CollectCallGetPending(&waiting_queue);
        assert(new_client >= 0);
        if (waiting_queue > COLLECT_WINDOW)
        {
            Log(LOG_LEVEL_INFO,
                "Abandoning collect call attempt with queue longer than collect_window [%d > %d]",
                waiting_queue, COLLECT_WINDOW);
            cf_closesocket(new_client);
            CollectCallMarkProcessed();
        }
        else
        {
            ConnectionInfo *info = ConnectionInfoNew();
            assert(info);

            ConnectionInfoSetSocket(info, new_client);
            info->is_call_collect = true; /* Mark processed when done. */
            ServerEntryPoint(ctx, PolicyServerGetIP(), info);
        }
    }
}

/* Check for new policy just before spawning a thread.
 *
 * Server reconfiguration can only happen when no threads are active,
 * so this is a good time to do it; but we do still have to check for
 * running threads. */
static void PolicyUpdateIfSafe(EvalContext *ctx, Policy **policy,
                               GenericAgentConfig *config)
{
    if (ThreadLock(cft_server_children))
    {
        int prior = COLLECT_INTERVAL;
        if (ACTIVE_THREADS == 0)
        {
            CheckFileChanges(ctx, policy, config);
        }
        ThreadUnlock(cft_server_children);

        /* Check for change in call-collect interval: */
        if (prior != COLLECT_INTERVAL)
        {
            /* Start, stop or change schedule, as appropriate. */
            CollectCallStart(COLLECT_INTERVAL);
        }
    }
}

/* Try to accept a connection; handle if we get one. */
static void AcceptAndHandle(EvalContext *ctx, int sd)
{
    /* TODO embed ConnectionInfo into ServerConnectionState. */
    ConnectionInfo *info = ConnectionInfoNew(); /* Uses xcalloc() */

    info->ss_len = sizeof(info->ss);
    info->sd = accept(sd, (struct sockaddr *) &info->ss, &info->ss_len);
    if (info->sd == -1)
    {
        Log(LOG_LEVEL_INFO, "Error accepting connection (%s)", GetErrorStr());
        ConnectionInfoDestroy(&info);
        return;
    }

    Log(LOG_LEVEL_DEBUG, "Socket descriptor returned from accept(): %d",
        info->sd);

    /* Just convert IP address to string, no DNS lookup. */
    char ipaddr[CF_MAX_IP_LEN] = "";
    getnameinfo((const struct sockaddr *) &info->ss, info->ss_len,
                ipaddr, sizeof(ipaddr),
                NULL, 0, NI_NUMERICHOST);

    /* IPv4 mapped addresses (e.g. "::ffff:192.168.1.2") are
     * hereby represented with their IPv4 counterpart. */
    ServerEntryPoint(ctx, MapAddress(ipaddr), info);
}

static size_t GetListenQueueSize(void)
{
    const char *const queue_size_var = getenv("CF_SERVERD_LISTEN_QUEUE_SIZE");
    if (queue_size_var != NULL)
    {
        long queue_size;
        int ret = StringToLong(queue_size_var, &queue_size);
        if ((ret == 0) && (queue_size > 0) && (queue_size <= MAX_LISTEN_QUEUE_SIZE))
        {
            return (size_t) queue_size;
        }
        Log(LOG_LEVEL_WARNING,
            "$CF_SERVERD_LISTEN_QUEUE_SIZE = '%s' doesn't specify a valid number for listen queue size, "
            "falling back to default (%d).",
            queue_size_var, DEFAULT_LISTEN_QUEUE_SIZE);
    }

    return DEFAULT_LISTEN_QUEUE_SIZE;
}

/**
 *  @retval >0 Number of threads still working
 *  @retval 0  All threads are done
 *  @retval -1 Server didn't run
 */
int StartServer(EvalContext *ctx, Policy **policy, GenericAgentConfig *config)
{
    InitSignals();

    bool tls_init_ok = ServerTLSInitialize(NULL, NULL, NULL);
    if (!tls_init_ok)
    {
        return -1;
    }

    size_t queue_size = GetListenQueueSize();
    int sd = SetServerListenState(ctx, queue_size, NULL, SERVER_LISTEN, &InitServer);

    /* Necessary for our use of select() to work in WaitForIncoming(): */
    assert(sd < sizeof(fd_set) * CHAR_BIT &&
           GetSignalPipe() < sizeof(fd_set) * CHAR_BIT);

    Policy *server_cfengine_policy = PolicyNew();
    CfLock thislock = AcquireServerLock(ctx, config, server_cfengine_policy);
    if (thislock.lock == NULL)
    {
        PolicyDestroy(server_cfengine_policy);
        if (sd >= 0)
        {
            cf_closesocket(sd);
        }
        return -1;
    }

    PrepareServer(sd);
    CollectCallStart(COLLECT_INTERVAL);

    while (!IsPendingTermination())
    {
        CollectCallIfDue(ctx);

        int selected = WaitForIncoming(sd, WAIT_INCOMING_TIMEOUT);

        Log(LOG_LEVEL_DEBUG, "select(): %d", selected);
        if (selected == -1)
        {
            Log(LOG_LEVEL_ERR,
                "Error while waiting for connections. (select: %s)",
                GetErrorStr());
            break;
        }
        else if (selected >= 0) /* timeout or success */
        {
            PolicyUpdateIfSafe(ctx, policy, config);

            /* Is there a new connection pending at our listening socket? */
            if (selected > 0)
            {
                AcceptAndHandle(ctx, sd);
            }
        } /* else: interrupted, maybe pending termination. */
    }
    Log(LOG_LEVEL_NOTICE, "Cleaning up and exiting...");

    CollectCallStop();
    if (sd != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Closing listening socket");
        cf_closesocket(sd);                       /* Close listening socket */
    }

    /* This is a graceful exit, give 2 seconds chance to threads. */
    int threads_left = WaitOnThreads();
    YieldCurrentLock(thislock);
    PolicyDestroy(server_cfengine_policy);

    return threads_left;
}
