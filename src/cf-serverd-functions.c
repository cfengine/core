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

#include "cf-serverd-functions.h"

static const size_t QUEUESIZE = 50;
int NO_FORK = false;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *ID = "The server daemon provides two services: it acts as a\n"
    "file server for remote file copying and it allows an\n"
    "authorized cf-runagent to start a cf-agent process and\n"
    "set certain additional classes with role-based access control.\n";

static const struct option OPTIONS[15] =
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
    {NULL, 0, 0, '\0'}
};

static const char *HINTS[15] =
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
    NULL
};

/*******************************************************************/

GenericAgentConfig CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    char ld_library_path[CF_BUFSIZE];
    int optindex = 0;
    int c;
    GenericAgentConfig config = GenericAgentDefaultConfig(cf_server);

    while ((c = getopt_long(argc, argv, "dvIKf:D:N:VSxLFMh", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'f':

            if (optarg && strlen(optarg) < 5)
            {
                FatalError(" -f used but argument \"%s\" incorrect", optarg);
            }

            SetInputFile(optarg);
            MINUSF = true;
            break;

        case 'd':
            DEBUG = true;
            NO_FORK = true;

        case 'K':
            IGNORELOCK = true;
            break;

        case 'D':
            NewClassesFromString(optarg);
            break;

        case 'N':
            NegateClassesFromString(optarg);
            break;

        case 'I':
            INFORM = true;
            break;

        case 'v':
            VERBOSE = true;
            NO_FORK = true;
            break;

        case 'F':
            NO_FORK = true;
            break;

        case 'L':
            CfOut(cf_verbose, "", "Setting LD_LIBRARY_PATH=%s\n", optarg);
            snprintf(ld_library_path, CF_BUFSIZE - 1, "LD_LIBRARY_PATH=%s", optarg);
            putenv(ld_library_path);
            break;

        case 'V':
            PrintVersionBanner("cf-serverd");
            exit(0);

        case 'h':
            Syntax("cf-serverd - cfengine's server agent", OPTIONS, HINTS, ID);
            exit(0);

        case 'M':
            ManPage("cf-serverd - cfengine's server agent", OPTIONS, HINTS, ID);
            exit(0);

        case 'x':
            SelfDiagnostic();
            exit(0);

        default:
            Syntax("cf-serverd - cfengine's server agent", OPTIONS, HINTS, ID);
            exit(1);

        }
    }

    if (argv[optind] != NULL)
    {
        CfOut(cf_error, "", "Unexpected argument with no preceding option: %s\n", argv[optind]);
        FatalError("Aborted");
    }

    CfDebug("Set debugging\n");

    return config;
}

/*******************************************************************/

void ThisAgentInit(void)
{
    NewScope("remote_access");
    umask(077);
}

/*******************************************************************/

void StartServer(Policy *policy, GenericAgentConfig config, const ReportContext *report_context)
{
    int sd, sd_reply;
    fd_set rset;
    struct timeval timeout;
    int ret_val;
    Promise *pp = NewPromise("server_cfengine", "the server daemon");
    Attributes dummyattr = { {0} };
    CfLock thislock;
    time_t starttime = time(NULL), last_collect = 0;

#if defined(HAVE_GETADDRINFO)
    socklen_t addrlen = sizeof(struct sockaddr_in6);
    struct sockaddr_in6 cin;
#else
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in cin;
#endif

    memset(&dummyattr, 0, sizeof(dummyattr));

    if ((sd = OpenReceiverChannel()) == -1)
    {
        CfOut(cf_error, "", "Unable to start server");
        exit(1);
    }

    signal(SIGINT, HandleSignals);
    signal(SIGTERM, HandleSignals);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignals);
    signal(SIGUSR2, HandleSignals);

    if (listen(sd, QUEUESIZE) == -1)
    {
        CfOut(cf_error, "listen", "listen failed");
        exit(1);
    }

    dummyattr.transaction.ifelapsed = 0;
    dummyattr.transaction.expireafter = 1;

    thislock = AcquireLock(pp->promiser, VUQNAME, CFSTARTTIME, dummyattr, pp, false);

    if (thislock.lock == NULL)
    {
        return;
    }

    CfOut(cf_inform, "", "cf-serverd starting %.24s\n", cf_ctime(&starttime));
    CfOut(cf_verbose, "", "Listening for connections ...\n");

#ifdef MINGW

    if (!NO_FORK)
    {
        CfOut(cf_verbose, "", "Windows does not support starting processes in the background - starting in foreground");
    }

#else /* NOT MINGW */

    if ((!NO_FORK) && (fork() != 0))
    {
        GenericDeInitialize();
        exit(0);
    }

    if (!NO_FORK)
    {
        ActAsDaemon(sd);
    }

#endif /* NOT MINGW */

    WritePID("cf-serverd.pid");

/* Andrew Stribblehill <ads@debian.org> -- close sd on exec */
#ifndef MINGW
    fcntl(sd, F_SETFD, FD_CLOEXEC);
#endif

    while (true)
    {
        time_t now = time(NULL);

        /* Note that this loop logic is single threaded, but ACTIVE_THREADS
           might still change in threads pertaining to service handling */

        if (ThreadLock(cft_server_children))
        {
            if (ACTIVE_THREADS == 0)
            {
                CheckFileChanges(&policy, config, report_context);
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

        // Look for normal incoming service requests

        FD_ZERO(&rset);
        FD_SET(sd, &rset);

        timeout.tv_sec = 10;    /* Set a 10 second timeout for select */
        timeout.tv_usec = 0;

        CfDebug(" -> Waiting at incoming select...\n");

        ret_val = select((sd + 1), &rset, NULL, NULL, &timeout);

        if (ret_val == -1)      /* Error received from call to select */
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                CfOut(cf_error, "select", "select failed");
                exit(1);
            }
        }
        else if (!ret_val)      /* No data waiting, we must have timed out! */
        {
            continue;
        }

        CfOut(cf_verbose, "", " -> Accepting a connection\n");

        if ((sd_reply = accept(sd, (struct sockaddr *) &cin, &addrlen)) != -1)
        {
            char ipaddr[CF_MAXVARSIZE];

            memset(ipaddr, 0, CF_MAXVARSIZE);
            ThreadLock(cft_getaddr);
            snprintf(ipaddr, CF_MAXVARSIZE - 1, "%s", sockaddr_ntop((struct sockaddr *) &cin));
            ThreadUnlock(cft_getaddr);

            ServerEntryPoint(sd_reply, ipaddr, SV);
        }
    }

    YieldCurrentLock(thislock); /* We never get here - this is done by a signal handler */
}

/*********************************************************************/
/* Level 2                                                           */
/*********************************************************************/

int OpenReceiverChannel(void)
{
    int sd;
    int yes = 1;

    struct linger cflinger;

#if defined(HAVE_GETADDRINFO)
    struct addrinfo query, *response, *ap;
#else
    struct sockaddr_in sin;
#endif

    cflinger.l_onoff = 1;
    cflinger.l_linger = 60;

#if defined(HAVE_GETADDRINFO)
    char *ptr = NULL;

    memset(&query, 0, sizeof(struct addrinfo));

    query.ai_flags = AI_PASSIVE;
    query.ai_family = AF_UNSPEC;
    query.ai_socktype = SOCK_STREAM;

/*
 * HvB : Bas van der Vlies
*/
    if (BINDINTERFACE[0] != '\0')
    {
        ptr = BINDINTERFACE;
    }

    if (getaddrinfo(ptr, STR_CFENGINEPORT, &query, &response) != 0)
    {
        CfOut(cf_error, "getaddrinfo", "DNS/service lookup failure");
        return -1;
    }

    sd = -1;

    for (ap = response; ap != NULL; ap = ap->ai_next)
    {
        if ((sd = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol)) == -1)
        {
            continue;
        }

        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(int)) == -1)
        {
            CfOut(cf_error, "setsockopt", "Socket options were not accepted");
            exit(1);
        }

        if (setsockopt(sd, SOL_SOCKET, SO_LINGER, (char *) &cflinger, sizeof(struct linger)) == -1)
        {
            CfOut(cf_error, "setsockopt", "Socket options were not accepted");
            exit(1);
        }

        if (bind(sd, ap->ai_addr, ap->ai_addrlen) == 0)
        {
            if (DEBUG)
            {
                ThreadLock(cft_getaddr);
                printf("Bound to address %s on %s=%d\n", sockaddr_ntop(ap->ai_addr), CLASSTEXT[VSYSTEMHARDCLASS],
                       VSYSTEMHARDCLASS);
                ThreadUnlock(cft_getaddr);
            }

            if (VSYSTEMHARDCLASS == mingw || VSYSTEMHARDCLASS == openbsd || VSYSTEMHARDCLASS == freebsd
                || VSYSTEMHARDCLASS == netbsd || VSYSTEMHARDCLASS == dragonfly)
            {
                continue;       /* *bsd doesn't map ipv6 addresses */
            }
            else
            {
                break;
            }
        }

        CfOut(cf_error, "bind", "Could not bind server address");
        cf_closesocket(sd);
        sd = -1;
    }

    if (sd < 0)
    {
        CfOut(cf_error, "", "Couldn't open bind an open socket\n");
        exit(1);
    }

    if (response != NULL)
    {
        freeaddrinfo(response);
    }
#else

    memset(&sin, 0, sizeof(sin));

    if (BINDINTERFACE[0] != '\0')
    {
        sin.sin_addr.s_addr = GetInetAddr(BINDINTERFACE);
    }
    else
    {
        sin.sin_addr.s_addr = INADDR_ANY;
    }

    sin.sin_port = (unsigned short) SHORT_CFENGINEPORT;
    sin.sin_family = AF_INET;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        CfOut(cf_error, "socket", "Couldn't open socket");
        exit(1);
    }

    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(int)) == -1)
    {
        CfOut(cf_error, "sockopt", "Couldn't set socket options");
        exit(1);
    }

    if (setsockopt(sd, SOL_SOCKET, SO_LINGER, (char *) &cflinger, sizeof(struct linger)) == -1)
    {
        CfOut(cf_error, "sockopt", "Couldn't set socket options");
        exit(1);
    }

    if (bind(sd, (struct sockaddr *) &sin, sizeof(sin)) == -1)
    {
        CfOut(cf_error, "bind", "Couldn't bind to socket");
        exit(1);
    }

#endif

    return sd;
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

void CheckFileChanges(Policy **policy, GenericAgentConfig config, const ReportContext *report_context)
{
    if (EnterpriseExpiry())
    {
        CfOut(cf_error, "", "!! This enterprise license is invalid.");
    }

    CfDebug("Checking file updates on %s\n", VINPUTFILE);

    if (NewPromiseProposals())
    {
        CfOut(cf_verbose, "", " -> New promises detected...\n");

        if (CheckPromises(cf_server, report_context))
        {
            CfOut(cf_inform, "", "Rereading config files %s..\n", VINPUTFILE);

            /* Free & reload -- lock this to avoid access errors during reload */

            DeleteAlphaList(&VHEAP);
            DeleteItemList(VNEGHEAP);
            InitAlphaList(&VHEAP);
            DeleteAlphaList(&VADDCLASSES);
            InitAlphaList(&VADDCLASSES);

            DeleteItemList(IPADDRESSES);
            IPADDRESSES = NULL;

            DeleteItemList(SV.trustkeylist);
            DeleteItemList(SV.skipverify);
            DeleteItemList(SV.attackerlist);
            DeleteItemList(SV.nonattackerlist);
            DeleteItemList(SV.multiconnlist);

            DeleteAuthList(VADMIT);
            DeleteAuthList(VDENY);
            //DeleteRlist(VINPUTLIST); This is just a pointer, cannot free it

            DeleteAllScope();

            strcpy(VDOMAIN, "undefined.domain");
            POLICY_SERVER[0] = '\0';

            VADMIT = VADMITTOP = NULL;
            VDENY = VDENYTOP = NULL;

            VNEGHEAP = NULL;
            SV.trustkeylist = NULL;
            SV.skipverify = NULL;
            SV.attackerlist = NULL;
            SV.nonattackerlist = NULL;
            SV.multiconnlist = NULL;

            VINPUTLIST = NULL;

            PolicyDestroy(*policy);
            *policy = NULL;

            ERRORCOUNT = 0;

            NewScope("sys");

            SetPolicyServer(POLICY_SERVER);
            NewScalar("sys", "policy_hub", POLICY_SERVER, cf_str);

            if (EnterpriseExpiry())
            {
                CfOut(cf_error, "",
                      "Cfengine - autonomous configuration engine. This enterprise license is invalid.\n");
            }

            NewScope("const");
            NewScope("this");
            NewScope("control_server");
            NewScope("control_common");
            NewScope("mon");
            NewScope("remote_access");
            GetNameInfo3();
            GetInterfacesInfo(cf_server);
            Get3Environment();
            BuiltinClasses();
            OSClasses();

            NewClass(CF_AGENTTYPES[THIS_AGENT_TYPE]);

            SetReferenceTime(true);
            *policy = ReadPromises(cf_server, CF_SERVERC, config, report_context);
            KeepPromises(*policy, report_context);
            Summarize();

        }
        else
        {
            CfOut(cf_inform, "", " !! File changes contain errors -- ignoring");
            PROMISETIME = time(NULL);
        }
    }
    else
    {
        CfDebug(" -> No new promises found\n");
    }
}

#if !defined(HAVE_GETADDRINFO)
in_addr_t GetInetAddr(char *host)
{
    struct in_addr addr;
    struct hostent *hp;

    addr.s_addr = inet_addr(host);

    if ((addr.s_addr == INADDR_NONE) || (addr.s_addr == 0))
    {
        if ((hp = gethostbyname(host)) == 0)
        {
            FatalError("host not found: %s", host);
        }

        if (hp->h_addrtype != AF_INET)
        {
            FatalError("unexpected address family: %d\n", hp->h_addrtype);
        }

        if (hp->h_length != sizeof(addr))
        {
            FatalError("unexpected address length %d\n", hp->h_length);
        }

        memcpy((char *) &addr, hp->h_addr, hp->h_length);
    }

    return (addr.s_addr);
}
#endif
