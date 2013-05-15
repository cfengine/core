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

#include "cf-serverd-functions.h"

#include "client_code.h"
#include "server_transform.h"
#include "bootstrap.h"
#include "scope.h"
#include "signals.h"
#include "mutex.h"
#include "locks.h"
#include "exec_tools.h"
#include "unix.h"
#include "man.h"

#include <assert.h>

static const size_t QUEUESIZE = 50;
int NO_FORK = false;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *CF_SERVERD_SHORT_DESCRIPTION = "CFEngine file server daemon";

static const char *CF_SERVERD_MANPAGE_LONG_DESCRIPTION =
        "cf-serverd is a socket listening daemon providing two services: it acts as a file server for remote file copying "
        "and it allows an authorized cf-runagent to start a cf-agent run. cf-agent typically connects to a "
        "cf-serverd instance to request updated policy code, but may also request additional files for download. "
        "cf-serverd employs role based access control (defined in policy code) to authorize requests.";

static const struct option OPTIONS[16] =
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
    {NULL, 0, 0, '\0'}
};

static const char *HINTS[16] =
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
                EvalContextHeapAddHard(ctx, "am_policy_hub");
            }
            free(existing_policy_server);
        }
    }

    /* FIXME: why is it not in generic_agent?! */
#if defined HAVE_NOVA
    EvalContextHeapAddHard(ctx, "nova_edition");
    EvalContextHeapAddHard(ctx, "enterprise_edition");
#else
    EvalContextHeapAddHard(ctx, "community_edition");
#endif
}

#ifdef HAVE_AVAHI_CLIENT_CLIENT_H
#ifdef HAVE_AVAHI_COMMON_ADDRESS_H
static int GenerateAvahiConfig(const char *path);
#endif
#endif

GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    char ld_library_path[CF_BUFSIZE];
    int optindex = 0;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_SERVER);

    while ((c = getopt_long(argc, argv, "dvIKf:D:N:VSxLFMhAl", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'l':
            LEGACY_OUTPUT = true;
            break;

        case 'f':

            if (optarg && (strlen(optarg) < 5))
            {
                Log(LOG_LEVEL_ERR, " -f used but argument \"%s\" incorrect", optarg);
                exit(EXIT_FAILURE);
            }

            GenericAgentConfigSetInputFile(config, GetWorkDir(), optarg);
            MINUSF = true;
            break;

        case 'd':
            config->debug_mode = true;
            NO_FORK = true;

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
            NO_FORK = true;
            break;

        case 'F':
            NO_FORK = true;
            break;

        case 'L':
            Log(LOG_LEVEL_VERBOSE, "Setting LD_LIBRARY_PATH=%s", optarg);
            snprintf(ld_library_path, CF_BUFSIZE - 1, "LD_LIBRARY_PATH=%s", optarg);
            putenv(ld_library_path);
            break;

        case 'V':
            PrintVersion();
            exit(0);

        case 'h':
            PrintHelp("cf-serverd", OPTIONS, HINTS, true);
            exit(0);

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
            exit(0);
        case 'A':
#ifdef HAVE_AVAHI_CLIENT_CLIENT_H
#ifdef HAVE_AVAHI_COMMON_ADDRESS_H
            printf("Generating Avahi configuration file.\n");
            if (GenerateAvahiConfig("/etc/avahi/services/cfengine-hub.service") != 0)
            {
                exit(1);
            }
            cf_popen("/etc/init.d/avahi-daemon restart", "r", true);
            printf("Avahi configuration file generated successfuly.\n");
            exit(0);
#else
            printf("This option can only be used when avahi-daemon and libavahi are installed on the machine.\n");
#endif
#endif

        default:
            PrintHelp("cf-serverd", OPTIONS, HINTS, true);
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

/*******************************************************************/

void ThisAgentInit(void)
{
    umask(077);
}

/*******************************************************************/

void StartServer(EvalContext *ctx, Policy *policy, GenericAgentConfig *config)
{
    int sd = -1, sd_reply;
    fd_set rset;
    struct timeval timeout;
    int ret_val;
    CfLock thislock;
    time_t starttime = time(NULL), last_collect = 0;

    struct sockaddr_storage cin;
    socklen_t addrlen = sizeof(cin);

    signal(SIGINT, HandleSignalsForDaemon);
    signal(SIGTERM, HandleSignalsForDaemon);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignalsForDaemon);
    signal(SIGUSR2, HandleSignalsForDaemon);

    sd = SetServerListenState(ctx, QUEUESIZE);

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

    Log(LOG_LEVEL_INFO, "cf-serverd starting %.24s", ctime(&starttime));

    if (sd != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Listening for connections ...");
    }

#ifdef __MINGW32__

    if (!NO_FORK)
    {
        Log(LOG_LEVEL_VERBOSE, "Windows does not support starting processes in the background - starting in foreground");
    }

#else /* !__MINGW32__ */

    if ((!NO_FORK) && (fork() != 0))
    {
        _exit(0);
    }

    if (!NO_FORK)
    {
        ActAsDaemon(sd);
    }

#endif /* !__MINGW32__ */

    WritePID("cf-serverd.pid");

/* Andrew Stribblehill <ads@debian.org> -- close sd on exec */
#ifndef __MINGW32__
    fcntl(sd, F_SETFD, FD_CLOEXEC);
#endif

    while (!IsPendingTermination())
    {
        time_t now = time(NULL);

        /* Note that this loop logic is single threaded, but ACTIVE_THREADS
           might still change in threads pertaining to service handling */

        if (ThreadLock(cft_server_children))
        {
            if (ACTIVE_THREADS == 0)
            {
                CheckFileChanges(ctx, &policy, config);
            }
            ThreadUnlock(cft_server_children);
        }

        // Check whether we should try to establish peering with a hub

        if ((COLLECT_INTERVAL > 0) && ((now - last_collect) > COLLECT_INTERVAL))
        {
            TryCollectCall();
            last_collect = now;
            continue;
        }

        /* check if listening is working */
        if (sd != -1)
        {
            // Look for normal incoming service requests

            FD_ZERO(&rset);
            FD_SET(sd, &rset);

            timeout.tv_sec = 10;    /* Set a 10 second timeout for select */
            timeout.tv_usec = 0;

            Log(LOG_LEVEL_DEBUG, "Waiting at incoming select...");

            ret_val = select((sd + 1), &rset, NULL, NULL, &timeout);

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

            Log(LOG_LEVEL_VERBOSE, "Accepting a connection");

            if ((sd_reply = accept(sd, (struct sockaddr *) &cin, &addrlen)) != -1)
            {
                /* Just convert IP address to string, no DNS lookup. */
                char ipaddr[CF_MAX_IP_LEN] = "";
                getnameinfo((struct sockaddr *) &cin, addrlen,
                            ipaddr, sizeof(ipaddr),
                            NULL, 0, NI_NUMERICHOST);

                ServerEntryPoint(ctx, sd_reply, ipaddr);
            }
        }
    }

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
        exit(1);
    }

    if (listen(sd, queue_size) == -1)
    {
        Log(LOG_LEVEL_ERR, "listen failed. (listen: %s)", GetErrorStr());
        exit(1);
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

    /* Resolve listening interface. */
    if (getaddrinfo(ptr, STR_CFENGINEPORT, &query, &response) != 0)
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

        int yes = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
                       &yes, sizeof(yes)) == -1)
        {
            Log(LOG_LEVEL_ERR, "Socket option SO_REUSEADDR was not accepted. (setsockopt: %s)", GetErrorStr());
            exit(1);
        }

        struct linger cflinger = {
            .l_onoff = 1,
            .l_linger = 60
        };
        if (setsockopt(sd, SOL_SOCKET, SO_LINGER,
                       &cflinger, sizeof(cflinger)) == -1)
        {
            Log(LOG_LEVEL_ERR, "Socket option SO_LINGER was not accepted. (setsockopt: %s)", GetErrorStr());
            exit(1);
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
        exit(1);
    }

    freeaddrinfo(response);
    return sd;
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

void CheckFileChanges(EvalContext *ctx, Policy **policy, GenericAgentConfig *config)
{
    Log(LOG_LEVEL_DEBUG, "Checking file updates for input file '%s'", config->input_file);

    if (NewPromiseProposals(ctx, config, InputFiles(ctx, *policy)))
    {
        Log(LOG_LEVEL_VERBOSE, "New promises detected...");

        if (CheckPromises(config))
        {
            Log(LOG_LEVEL_INFO, "Rereading config files %s..", config->input_file);

            /* Free & reload -- lock this to avoid access errors during reload */
            
            EvalContextHeapClear(ctx);

            DeleteItemList(IPADDRESSES);
            IPADDRESSES = NULL;

            DeleteItemList(SV.trustkeylist);
            DeleteItemList(SV.skipverify);
            DeleteItemList(SV.attackerlist);
            DeleteItemList(SV.nonattackerlist);
            DeleteItemList(SV.multiconnlist);

            DeleteAuthList(SV.admit);
            DeleteAuthList(SV.deny);

            DeleteAuthList(SV.varadmit);
            DeleteAuthList(SV.vardeny);

            DeleteAuthList(SV.roles);

            //DeleteRlist(VINPUTLIST); This is just a pointer, cannot free it

            ScopeDeleteAll();

            strcpy(VDOMAIN, "undefined.domain");
            POLICY_SERVER[0] = '\0';

            SV.admit = NULL;
            SV.admittop = NULL;

            SV.varadmit = NULL;
            SV.varadmittop = NULL;

            SV.deny = NULL;
            SV.denytop = NULL;

            SV.vardeny = NULL;
            SV.vardenytop = NULL;

            SV.roles = NULL;
            SV.rolestop = NULL;

            SV.trustkeylist = NULL;
            SV.skipverify = NULL;
            SV.attackerlist = NULL;
            SV.nonattackerlist = NULL;
            SV.multiconnlist = NULL;

            PolicyDestroy(*policy);
            *policy = NULL;

            {
                char *existing_policy_server = ReadPolicyServerFile(GetWorkDir());
                SetPolicyServer(ctx, existing_policy_server);
                free(existing_policy_server);
            }

            GetNameInfo3(ctx, AGENT_TYPE_SERVER);
            GetInterfacesInfo(ctx, AGENT_TYPE_SERVER);
            Get3Environment(ctx, AGENT_TYPE_SERVER);
            BuiltinClasses(ctx);
            OSClasses(ctx);
            KeepHardClasses(ctx);

            EvalContextHeapAddHard(ctx, CF_AGENTTYPES[config->agent_type]);

            SetReferenceTime(ctx, true);
            *policy = GenericAgentLoadPolicy(ctx, config);
            KeepPromises(ctx, *policy, config);
            Summarize();

        }
        else
        {
            Log(LOG_LEVEL_INFO, "File changes contain errors -- ignoring");
            PROMISETIME = time(NULL);
        }
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "No new promises found");
    }
}

#ifdef HAVE_AVAHI_CLIENT_CLIENT_H
#ifdef HAVE_AVAHI_COMMON_ADDRESS_H
static int GenerateAvahiConfig(const char *path)
{
    FILE *fout;
    Writer *writer = NULL;
    fout = fopen(path, "w+");
    if (fout == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to open %s", path);
        return -1;
    }
    writer = FileWriter(fout);
    fprintf(fout, "<?xml version=\"1.0\" standalone='no'?>\n");
    fprintf(fout, "<!DOCTYPE service-group SYSTEM \"avahi-service.dtd\">\n");
    XmlComment(writer, "This file has been automatically generated by cf-serverd.");
    XmlStartTag(writer, "service-group", 0);
    /* FIXME: make it function */
#ifdef HAVE_NOVA
    fprintf(fout,"<name replace-wildcards=\"yes\" >CFEngine Enterprise %s Policy Hub on %s </name>\n", Version(), "%h");
#else
    fprintf(fout,"<name replace-wildcards=\"yes\" >CFEngine Community %s Policy Server on %s </name>\n", Version(), "%h");
#endif
    XmlStartTag(writer, "service", 0);
    XmlTag(writer, "type", "_cfenginehub._tcp",0);
    DetermineCfenginePort();
    XmlTag(writer, "port", STR_CFENGINEPORT, 0);
    XmlEndTag(writer, "service");
    XmlEndTag(writer, "service-group");
    fclose(fout);

    return 0;
}
#endif
#endif
