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
#include <cf-serverd-enterprise-stubs.h>

#include <client_code.h>
#include <server_transform.h>
#include <bootstrap.h>
#include <scope.h>
#include <signals.h>
#include <mutex.h>
#include <locks.h>
#include <exec_tools.h>
#include <unix.h>
#include <man.h>
#include <tls_server.h>                              /* ServerTLSInitialize */
#include <timeout.h>
#include <unix_iface.h>
#include <known_dirs.h>
#include <sysinfo.h>
#include <time_classes.h>
#include <connection_info.h>
#include <file_lib.h>
#include <loading.h>
#include <printsize.h>

#include "server_access.h"

static const size_t QUEUESIZE = 50;
int NO_FORK = false; /* GLOBAL_A */

/*******************************************************************/
/* Command line options                                            */
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
    {"legacy-output", no_argument, 0, 'l'},
    {"color", optional_argument, 0, 'C'},
    {NULL, 0, 0, '\0'}
};

static const char *const HINTS[] =
{
    "Print the help message",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    "Specify an alternative input file than the default",
    "Define a list of comma separated classes to be defined at the start of execution",
    "Define a list of comma separated classes to be undefined at the start of execution",
    "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Activate internal diagnostics (developers only)",
    "Run as a foreground processes (do not fork)",
    "Set the internal value of LD_LIBRARY_PATH for child processes",
    "Generates avahi configuration file to enable policy server to be discovered in the network",
    "Use legacy output format",
    "Enable colorized output. Possible values: 'always', 'auto', 'never'. If option is used, the default value is 'auto'",
    NULL
};

/*******************************************************************/

static void KeepHardClasses(EvalContext *ctx)
{
    char name[CF_BUFSIZE];
    if (name != NULL)
    {
        char *existing_policy_server = ReadPolicyServerFile(CFWORKDIR);
        if (existing_policy_server)
        {
            if (GetAmPolicyHub(CFWORKDIR))
            {
                EvalContextClassPutHard(ctx, "am_policy_hub", "source=bootstrap");
            }
            free(existing_policy_server);
        }
    }

    /* FIXME: why is it not in generic_agent?! */
    GenericAgentAddEditionClasses(ctx);
}

#ifdef HAVE_AVAHI_CLIENT_CLIENT_H
#ifdef HAVE_AVAHI_COMMON_ADDRESS_H
static int GenerateAvahiConfig(const char *path);
#define SUPPORT_AVAHI_CONFIG
#endif
#endif

GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_SERVER);

    while ((c = getopt_long(argc, argv, "dvIKf:D:N:VSxLFMhAlC::", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'l':
            LEGACY_OUTPUT = true;
            break;

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
            NO_FORK = true;
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
            exit(EXIT_SUCCESS);

        case 'h':
            {
                Writer *w = FileWriter(stdout);
                GenericAgentWriteHelp(w, "cf-serverd", OPTIONS, HINTS, true);
                FileWriterDetach(w);
            }
            exit(EXIT_SUCCESS);

        case 'M':
            {
                Writer *out = FileWriter(stdout);
                ManPageWrite(out, "cf-serverd", time(NULL),
                             CF_SERVERD_SHORT_DESCRIPTION,
                             CF_SERVERD_MANPAGE_LONG_DESCRIPTION,
                             OPTIONS, HINTS,
                             true);
                FileWriterDetach(out);
                exit(EXIT_SUCCESS);
            }

        case 'x':
            Log(LOG_LEVEL_ERR, "Self-diagnostic functionality is retired.");
            exit(EXIT_SUCCESS);

        case 'A':
#ifdef SUPPORT_AVAHI_CONFIG
            Log(LOG_LEVEL_NOTICE, "Generating Avahi configuration file.");
            if (GenerateAvahiConfig("/etc/avahi/services/cfengine-hub.service") != 0)
            {
                exit(EXIT_FAILURE);
            }
            cf_popen("/etc/init.d/avahi-daemon restart", "r", true);
            Log(LOG_LEVEL_NOTICE, "Avahi configuration file generated successfuly.");
#else
            Log(LOG_LEVEL_ERR, "Generating avahi configuration can only be done when avahi-daemon and libavahi are installed on the machine.");
#endif
            exit(EXIT_SUCCESS);

        case 'C':
            if (!GenericAgentConfigParseColor(config, optarg))
            {
                exit(EXIT_FAILURE);
            }
            break;

        default:
            {
                Writer *w = FileWriter(stdout);
                GenericAgentWriteHelp(w, "cf-serverd", OPTIONS, HINTS, true);
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

/*******************************************************************/

void ThisAgentInit(void)
{
    umask(077);
}

/*******************************************************************/

void StartServer(EvalContext *ctx, Policy **policy, GenericAgentConfig *config)
{
    int sd = -1;
    fd_set rset;
    int ret_val;
    CfLock thislock;
    extern int COLLECT_WINDOW;

    struct sockaddr_storage cin;
    socklen_t addrlen = sizeof(cin);

    MakeSignalPipe();

    signal(SIGINT, HandleSignalsForDaemon);
    signal(SIGTERM, HandleSignalsForDaemon);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignalsForDaemon);
    signal(SIGUSR2, HandleSignalsForDaemon);

    ServerTLSInitialize();

    sd = SetServerListenState(ctx, QUEUESIZE, SERVER_LISTEN, &InitServer);

    TransactionContext tc = {
        .ifelapsed = 0,
        .expireafter = 1,
    };

    Policy *server_cfengine_policy = PolicyNew();
    Promise *pp = NULL;
    {
        Bundle *bp = PolicyAppendBundle(server_cfengine_policy, NamespaceDefault(), "server_cfengine_bundle", "agent", NULL, NULL);
        PromiseType *tp = BundleAppendPromiseType(bp, "server_cfengine");

        pp = PromiseTypeAppendPromise(tp, config->input_file, (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, NULL);
    }
    assert(pp);

    thislock = AcquireLock(ctx, pp->promiser, VUQNAME, CFSTARTTIME, tc, pp, false);

    if (thislock.lock == NULL)
    {
        PolicyDestroy(server_cfengine_policy);
        return;
    }

    if (sd != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Listening for connections ...");
    }

    if (!NO_FORK)
    {
#ifdef __MINGW32__

        Log(LOG_LEVEL_VERBOSE,
            "Windows does not support starting processes in the background - running in foreground");

#else /* !__MINGW32__ */

        if (fork() != 0)
        {
            _exit(EXIT_SUCCESS);
        }

        ActAsDaemon();
#endif /* !__MINGW32__ */
    }

#ifndef __MINGW32__
    /* Close sd on exec, needed for not passing the socket to cf-runagent
     * spawned commands. */
    fcntl(sd, F_SETFD, FD_CLOEXEC);
#endif /* !__MINGW32__ */

    WritePID("cf-serverd.pid");
    CollectCallStart(COLLECT_INTERVAL);

    while (!IsPendingTermination())
    {
        /* Note that this loop is executed from main thread only, but
           ACTIVE_THREADS might still change from connection threads. */
        if (ThreadLock(cft_server_children))
        {
            if (ACTIVE_THREADS == 0)
            {
                CheckFileChanges(ctx, policy, config);
            }
            ThreadUnlock(cft_server_children);
        }
        if (LSD_MAXREADERS < CFD_MAXPROCESSES)
        {
            int rc = UpdateLastSeenMaxReaders(CFD_MAXPROCESSES);
            if (rc == 0)
            {
                LSD_MAXREADERS = CFD_MAXPROCESSES;
            }
        }
        /* Check whether we have established peering with a hub */
        if (CollectCallHasPending())
        {
            int waiting_queue = 0;
            int new_client = CollectCallGetPending(&waiting_queue);
            assert(new_client >= 0);
            if (waiting_queue > COLLECT_WINDOW)
            {
                Log(LOG_LEVEL_INFO,
                    "Closing collect call with queue longer than the allocated window [%d > %d]",
                    waiting_queue, COLLECT_WINDOW);
                cf_closesocket(new_client);
            }
            else
            {
                ConnectionInfo *info = ConnectionInfoNew();
                assert(info);

                ConnectionInfoSetSocket(info, new_client);
                ServerEntryPoint(ctx, POLICY_SERVER, info);
                CollectCallMarkProcessed();
            }
        }
        else
        {
            /* check if listening is working */
            if (sd != -1)
            {
                // Look for normal incoming service requests
                int signal_pipe = GetSignalPipe();
                FD_ZERO(&rset);
                FD_SET(sd, &rset);
                FD_SET(signal_pipe, &rset);

                Log(LOG_LEVEL_DEBUG, "Waiting at incoming select...");
                struct timeval timeout = {
                    .tv_sec = 60,
                    .tv_usec = 0
                };
                int max_fd = (sd > signal_pipe) ? (sd + 1) : (signal_pipe + 1);
                ret_val = select(max_fd, &rset, NULL, NULL, &timeout);

                // Empty the signal pipe. We don't need the values.
                unsigned char buf;
                while (recv(signal_pipe, &buf, 1, 0) > 0) {}

                if (ret_val == -1)      /* Error received from call to select */
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        Log(LOG_LEVEL_ERR, "select failed. (select: %s)", GetErrorStr());
                        exit(1);
                    }
                }
                else if (!ret_val) /* No data waiting, we must have timed out! */
                {
                    continue;
                }

                if (FD_ISSET(sd, &rset))
                {
                    int new_client = accept(sd, (struct sockaddr *)&cin, &addrlen);
                    if (new_client == -1)
                    {
                        continue;
                    }
                    /* Just convert IP address to string, no DNS lookup. */
                    char ipaddr[CF_MAX_IP_LEN] = "";
                    getnameinfo((struct sockaddr *) &cin, addrlen,
                                ipaddr, sizeof(ipaddr),
                                NULL, 0, NI_NUMERICHOST);

                    ConnectionInfo *info = ConnectionInfoNew();
                    if (info)
                    {
                        ConnectionInfoSetSocket(info, new_client);
                        ServerEntryPoint(ctx, ipaddr, info);
                    }
                }
            }
        }
    }
    CollectCallStop();
    PolicyDestroy(server_cfengine_policy);
}

/*********************************************************************/
/* Level 2                                                           */
/*********************************************************************/

int InitServer(size_t queue_size)
{
    int sd = -1;

    if ((sd = OpenReceiverChannel()) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to start server");
        exit(EXIT_FAILURE);
    }

    if (listen(sd, queue_size) == -1)
    {
        Log(LOG_LEVEL_ERR, "listen failed. (listen: %s)", GetErrorStr());
        exit(EXIT_FAILURE);
    }

    return sd;
}

int OpenReceiverChannel(void)
{
    struct addrinfo *response, *ap;
    struct addrinfo query = {
        .ai_flags = AI_PASSIVE,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    /* Listen to INADDR(6)_ANY if BINDINTERFACE unset. */
    char *ptr = NULL;
    if (BINDINTERFACE[0] != '\0')
    {
        ptr = BINDINTERFACE;
    }

    char servname[PRINTSIZE(CFENGINE_PORT)];
    sprintf(servname, "%d", CFENGINE_PORT);

    /* Resolve listening interface. */
    if (getaddrinfo(ptr, servname, &query, &response) != 0)
    {
        Log(LOG_LEVEL_ERR, "DNS/service lookup failure. (getaddrinfo: %s)", GetErrorStr());
        return -1;
    }

    int sd = -1;
    for (ap = response; ap != NULL; ap = ap->ai_next)
    {
        if ((sd = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol)) == -1)
        {
            continue;
        }

       #ifdef IPV6_V6ONLY
        /* Some platforms don't listen to both address families (windows) for
           the IPv6 loopback address and need this flag. Some other platforms
           won't even honour this flag (openbsd). */
        if (BINDINTERFACE[0] == '\0')
        {
            int no = 0;
            if (setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY,
                           &no, sizeof(no)) == -1)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Failed to clear IPv6-only flag on listening socket"
                    " (setsockopt: %s)",
                    GetErrorStr());
            }
        }
        #endif

        int yes = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
                       &yes, sizeof(yes)) == -1)
        {
            Log(LOG_LEVEL_WARNING,
                "Socket option SO_REUSEADDR was not accepted. (setsockopt: %s)",
                GetErrorStr());
        }

        struct linger cflinger = {
            .l_onoff = 1,
            .l_linger = 60
        };
        if (setsockopt(sd, SOL_SOCKET, SO_LINGER,
                       &cflinger, sizeof(cflinger)) == -1)
        {
            Log(LOG_LEVEL_ERR, "Socket option SO_LINGER was not accepted. (setsockopt: %s)", GetErrorStr());
            exit(EXIT_FAILURE);
        }

        if (bind(sd, ap->ai_addr, ap->ai_addrlen) != -1)
        {
            if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
            {
                /* Convert IP address to string, no DNS lookup performed. */
                char txtaddr[CF_MAX_IP_LEN] = "";
                getnameinfo(ap->ai_addr, ap->ai_addrlen,
                            txtaddr, sizeof(txtaddr),
                            NULL, 0, NI_NUMERICHOST);
                Log(LOG_LEVEL_DEBUG, "Bound to address '%s' on '%s' = %d", txtaddr,
                    CLASSTEXT[VSYSTEMHARDCLASS], VSYSTEMHARDCLASS);
            }
            break;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Could not bind server address. (bind: %s)", GetErrorStr());
            cf_closesocket(sd);
        }
    }

    if (sd < 0)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open/bind a socket");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(response);
    return sd;
}

/*********************************************************************/
/* Level 3                                                           */
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

void CheckFileChanges(EvalContext *ctx, Policy **policy, GenericAgentConfig *config)
{
    Log(LOG_LEVEL_DEBUG, "Checking file updates for input file '%s'",
        config->input_file);

    time_t validated_at = ReadTimestampFromPolicyValidatedFile(config, NULL);

    if (config->agent_specific.daemon.last_validated_at < validated_at)
    {
        /* Rereading policies now, so update timestamp. */
        config->agent_specific.daemon.last_validated_at = validated_at;

        Log(LOG_LEVEL_VERBOSE, "New promises detected...");

        if (GenericAgentArePromisesValid(config))
        {
            Log(LOG_LEVEL_INFO, "Rereading policy file '%s'", config->input_file);

            /* STEP 1: Free everything */

            EvalContextClear(ctx);

            strcpy(VDOMAIN, "undefined.domain");

            /* Old ACLs */
            DeleteAuthList(&SV.admit, &SV.admittail);
            DeleteAuthList(&SV.deny, &SV.denytail);
            DeleteAuthList(&SV.varadmit, &SV.varadmittail);
            DeleteAuthList(&SV.vardeny, &SV.vardenytail);
            DeleteAuthList(&SV.roles, &SV.rolestail);

            /* body server control ACLs */
            DeleteItemList(SV.trustkeylist);    SV.trustkeylist = NULL;
            DeleteItemList(SV.attackerlist);    SV.attackerlist = NULL;
            DeleteItemList(SV.nonattackerlist); SV.nonattackerlist = NULL;
            DeleteItemList(SV.multiconnlist);   SV.multiconnlist = NULL;
            DeleteItemList(SV.allowlegacyconnects);
            SV.allowlegacyconnects = NULL;

            /* New ACLs */
            acl_Free(paths_acl);    paths_acl = NULL;
            acl_Free(classes_acl);  classes_acl = NULL;
            acl_Free(vars_acl);     vars_acl = NULL;
            acl_Free(literals_acl); literals_acl = NULL;
            acl_Free(query_acl);    query_acl = NULL;

            StringMapDestroy(SV.path_shortcuts);  SV.path_shortcuts = NULL;
            free(SV.allowciphers);                SV.allowciphers = NULL;
            PolicyDestroy(*policy);               *policy = NULL;

            /* STEP 2: Set Environment, Parse and Evaluate policy */

            /*
             * TODO why is this done separately here? What's the difference to
             * calling the same steps as in cf-serverd.c:main()? Those are:
             *   GenericAgentConfigApply();     // not here!
             *   GenericAgentDiscoverContext(); // not here!
             *   if (GenericAgentCheckPolicy()) // not here!
             *     policy=GenericAgentLoadPolicy();
             *   KeepPromises();
             *   Summarize();
             */

            char *existing_policy_server = ReadPolicyServerFile(GetWorkDir());
            SetPolicyServer(ctx, existing_policy_server);
            free(existing_policy_server);

            UpdateLastPolicyUpdateTime(ctx);

            DetectEnvironment(ctx);
            KeepHardClasses(ctx);

            EvalContextClassPutHard(ctx, CF_AGENTTYPES[AGENT_TYPE_SERVER], "cfe_internal,source=agent");

            time_t t = SetReferenceTime();
            UpdateTimeClasses(ctx, t);
            *policy = LoadPolicy(ctx, config);
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

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, FprintAvahiCfengineTag, FILE *, fp)
{
    fprintf(fp,"<name replace-wildcards=\"yes\" >CFEngine Community %s Policy Server on %s </name>\n", Version(), "%h");
}

#ifdef SUPPORT_AVAHI_CONFIG
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
#endif /* SUPPORT_AVAHI_CONFIG */
