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

#include <platform.h>

#include <client_code.h>        // cfnet_init
#include <crypto.h>             // CryptoInitialize

#include <bootstrap.h>          // GetAmPolicyHub
#include <cf-serverd-enterprise-stubs.h>
#include <conversion.h>         // MapAddress
#include <exec_tools.h>         // ActAsDaemon
#include <item_lib.h>           // DeleteItemList
#include <known_dirs.h>         // GetInputDir
#include <loading.h>            // LoadPolicy
#include <locks.h>              // AcquireLock
#include <man.h>                // ManPageWrite
#include <mutex.h>              // ThreadLock
#include <net.h>
#include <openssl/err.h>        // ERR_get_error
#include <policy_server.h>      // PolicyServerReadFile
#include <printsize.h>          // PRINTSIZE
#include <server_access.h>      // acl_Free
#include <server_code.h>        // InitServer
#include <server_common.h>
#include <server_tls.h>         // ServerTLSInitialize
#include <server_transform.h>   // Summarize
#include <signals.h>            // ReloadConfigRequested
#include <string_lib.h>         // StringFormat, etc.
#include <sysinfo.h>            // DetectEnvironment
#include <systype.h>            // CLASSTEXT
#include <time_classes.h>       // UpdateTimeClasses
#include <timeout.h>            // SetReferenceTime
#include <tls_generic.h>        // TLSLogError

#define CFTESTD_QUEUE_SIZE 10

// ============================= CFTestD_Config ==============================
typedef struct
{
    char *report_file;
    char *report;
    int report_len;
    char *key_file;
    RSA *priv_key;
    RSA *pub_key;
} CFTestD_Config;

/*******************************************************************/
/* Command line option parsing                                     */
/*******************************************************************/

static const struct option OPTIONS[] = {
    {"address", required_argument, 0, 'a'},
    {"debug", no_argument, 0, 'd'},
    {"help", no_argument, 0, 'h'},
    {"inform", no_argument, 0, 'I'},
    {"key-file", required_argument, 0, 'k'},
    {"timestamp", no_argument, 0, 'l'},
    {"port", required_argument, 0, 'p'},
    {"report", required_argument, 0, 'r'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, '\0'}};

static const char *const HINTS[] = {
    "Bind to a specific address",
    "Enable debugging output",
    "Print the help message",
    "Print basic information about what cf-testd does",
    "Specify a path to the key (private) to use for communication",
    "Log timestamps on each line of log output",
    "Set the port cf-testd will listen on",
    "Read report from file",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    NULL};

CFTestD_Config *CFTestD_ConfigInit()
{
    CFTestD_Config *r =
        (CFTestD_Config *)(xcalloc(1, sizeof(CFTestD_Config)));
    return r;
}

void CFTestD_ConfigDestroy(CFTestD_Config *config)
{
    free(config->report_file);
    free(config->report);
    free(config->key_file);
    free(config);
}

void CFTestD_Help()
{
    Writer *w = FileWriter(stdout);
    WriterWriteHelp(w, "cf-testd", OPTIONS, HINTS, true, NULL);
    FileWriterDetach(w);
}

CFTestD_Config *CFTestD_CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int c;
    CFTestD_Config *config = CFTestD_ConfigInit();
    assert(config != NULL);

    while ((c = getopt_long(argc, argv, "a:df:hIk:lp:vV", OPTIONS, NULL)) != -1)
    {
        switch (c)
        {
        case 'a':
            SetBindInterface(optarg);
            break;
        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            break;
        case 'h':
            CFTestD_Help();
            exit(EXIT_SUCCESS);
        case 'I':
            LogSetGlobalLevel(LOG_LEVEL_INFO);
            break;
        case 'k':
            config->key_file = xstrdup(optarg);
            break;
        case 'l':
            LoggingEnableTimestamps(true);
            break;
        case 'p':
        {
            bool ret = SetCfenginePort(optarg);
            if (!ret)
            {
                /* the function call above logs an error for us (if any) */
                exit(EXIT_FAILURE);
            }
            break;
        }
        case 'r':
            config->report_file = xstrdup(optarg);
            break;
        case 'v':
            LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
            break;
        case 'V':
        {
            Writer *w = FileWriter(stdout);
            GenericAgentWriteVersion(w);
            FileWriterDetach(w);
        }
            exit(EXIT_SUCCESS);
        default:
            CFTestD_Help();
            exit(EXIT_FAILURE);
        }
    }

    if (optind < argc) // More unparsed arguments left after getopt_long
    {
        // Enumerate and print all invalid arguments:
        Log(LOG_LEVEL_ERR, "Invalid command line arguments:");

        int start = optind;
        int stop  = argc;
        int total = stop - start;
        for (int i = 0; i < total; ++i)
        {
            Log(LOG_LEVEL_ERR,
                "[%d/%d]: %s\n",
                i + 1,
                total,
                argv[start + i]);
        }
    }

    return config;
}

bool CFTestD_TLSSessionEstablish(ServerConnectionState *conn)
{
    if (conn->conn_info->status == CONNECTIONINFO_STATUS_ESTABLISHED)
    {
        return true;
    }

    bool established = BasicServerTLSSessionEstablish(conn);
    if (!established)
    {
        return false;
    }

    Log(LOG_LEVEL_VERBOSE, "TLS session established, checking trust...");

    /* Send/Receive "CFE_v%d" version string, agree on version, receive
       identity (username) of peer. */
    char username[sizeof(conn->username)] = "";
    bool id_success = ServerIdentificationDialog(
        conn->conn_info,
        username,
        sizeof(username)
    );
    if (!id_success)
    {
        return false;
    }

    /* No CAUTH, SAUTH in non-classic protocol. */
    conn->user_data_set = 1;
    conn->rsa_auth      = 1;

    ServerSendWelcome(conn);
    return true;
}

bool CFTestD_GetServerQuery(
    ServerConnectionState *conn, char *recvbuffer, CFTestD_Config *config)
{
    char query[CF_BUFSIZE];
    const int report_len            = config->report_len;
    const char *const report        = config->report;
    ConnectionInfo *const conn_info = conn->conn_info;

    query[0] = '\0';
    sscanf(recvbuffer, "QUERY %255[^\n]", query);

    if (strlen(query) == 0)
    {
        return false;
    }

    if (report_len == 0)
    {
        Log(LOG_LEVEL_INFO,
            "No report file argument so returning canned data from enterprise plugin server\n");
        return CFTestD_ReturnQueryData(conn, query);
    }

    Log(LOG_LEVEL_INFO,
        "Report file argument specified. Returning report of length %d",
        report_len);

    // TODO for variables reports we will eventually need to split on \m
    // instead of \n since variable values can contain '\n's.

    size_t num_items = StringCountTokens(report, report_len, "\n");

    if (num_items < 3)
    {
        Log(LOG_LEVEL_ERR,
            "report file must have a CFR timestamp header line and at least two lines of items\n");
        return false;
    }

    StringRef ts_ref = StringGetToken(report, report_len, 0, "\n");
    char *ts         = xstrndup(ts_ref.data, ts_ref.len);
    char *header     = StringFormat("CFR: 0 %s %d\n", ts, report_len);
    SendTransaction(conn_info, header, SafeStringLength(header), CF_MORE);

    for (ssize_t i = 1; i < num_items; i++)
    {
        StringRef item_ref =
            StringGetToken(report, report_len, i, "\n");
        char *item = xstrndup(item_ref.data, item_ref.len);

        SendTransaction(conn_info, item, SafeStringLength(item), CF_MORE);
    }

    const char end_reply[] = "QUERY complete";
    SendTransaction(conn_info, end_reply, SafeStringLength(end_reply), CF_DONE);

    return true;
}

static bool CFTestD_ProtocolError(
    ServerConnectionState *conn, const char *recvbuffer, char *sendbuffer)
{
    strcpy(sendbuffer, "BAD: Request denied");
    SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
    Log(LOG_LEVEL_INFO,
        "Closing connection due to illegal request: %s",
        recvbuffer);
    return false;
}

static bool CFTestD_BusyLoop(
    ServerConnectionState *conn, CFTestD_Config *config)
{
    char recvbuffer[CF_BUFSIZE + CF_BUFEXT]        = "";
    char sendbuffer[CF_BUFSIZE - CF_INBAND_OFFSET] = "";

    const int received =
        ReceiveTransaction(conn->conn_info, recvbuffer, NULL);

    if (received == -1)
    {
        /* Already Log()ged in case of error. */
        return false;
    }
    if (received > CF_BUFSIZE - 1)
    {
        UnexpectedError("Received transaction of size %d", received);
        return false;
    }

    if (strlen(recvbuffer) == 0)
    {
        Log(LOG_LEVEL_WARNING,
            "Got NULL transmission (of size %d)",
            received);
        return true;
    }
    /* Don't process request if we're signalled to exit. */
    if (IsPendingTermination())
    {
        Log(LOG_LEVEL_VERBOSE, "Server must exit, closing connection");
        return false;
    }

    // See BusyWithNewProtocol() in server_tls.c for how to add other commands
    switch (GetCommandNew(recvbuffer))
    {
    case PROTOCOL_COMMAND_QUERY:
    {
        char query[256], name[128];
        int ret1 = sscanf(recvbuffer, "QUERY %255[^\n]", query);
        int ret2 = sscanf(recvbuffer, "QUERY %127s", name);
        if (ret1 != 1 || ret2 != 1)
        {
            return CFTestD_ProtocolError(conn, recvbuffer, sendbuffer);
        }

        if (CFTestD_GetServerQuery(conn, recvbuffer, config))
        {
            return true;
        }

        break;
    }
    case PROTOCOL_COMMAND_BAD:
    default:
        Log(LOG_LEVEL_WARNING, "Unexpected protocol command: %s", recvbuffer);
    }

    return CFTestD_ProtocolError(conn, recvbuffer, sendbuffer);
}

static ServerConnectionState *CFTestD_NewConn(ConnectionInfo *info)
{
#if 1
    /* TODO: why do we do this ?  We fail if getsockname() fails, but
     * don't use the information it returned.  Was the intent to use
     * it to fill in conn->ipaddr ? */
    struct sockaddr_storage addr;
    socklen_t size = sizeof(addr);
    int sockfd     = ConnectionInfoSocket(info);
    int sockname   = getsockname(sockfd, (struct sockaddr *)&addr, &size);

    if (sockname == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Could not obtain socket address. (getsockname: '%s')",
            GetErrorStr());
        return NULL;
    }
#endif

    ServerConnectionState *conn = xcalloc(1, sizeof(*conn));
    conn->ctx                   = NULL;
    conn->conn_info             = info;
    conn->encryption_type       = 'c';
    /* Only public files (chmod o+r) accessible to non-root */
    conn->uid = CF_UNKNOWN_OWNER; /* Careful, 0 is root! */
    /* conn->maproot is false: only public files (chmod o+r) are accessible */

    Log(LOG_LEVEL_DEBUG,
        "New connection (socket %d).",
        ConnectionInfoSocket(info));
    return conn;
}

static void CFTestD_DeleteConn(ServerConnectionState *conn)
{
    int sd = ConnectionInfoSocket(conn->conn_info);
    if (sd != SOCKET_INVALID)
    {
        cf_closesocket(sd);
    }
    ConnectionInfoDestroy(&conn->conn_info);

    free(conn->session_key);
    free(conn);
}

static void *CFTestD_HandleConnection(void *c, CFTestD_Config *config)
{
    ServerConnectionState *conn = c;

    Log(LOG_LEVEL_INFO, "Accepting connection");

    bool established = CFTestD_TLSSessionEstablish(conn);
    if (!established)
    {
        Log(LOG_LEVEL_ERR, "Could not establish TLS Session");
        return NULL;
    }
    int ret = getnameinfo(
        (const struct sockaddr *)&conn->conn_info->ss,
        conn->conn_info->ss_len,
        conn->revdns,
        sizeof(conn->revdns),
        NULL,
        0,
        NI_NAMEREQD);
    if (ret != 0)
    {
        Log(LOG_LEVEL_INFO,
            "Reverse lookup failed (getnameinfo: %s)!",
            gai_strerror(ret));
    }
    else
    {
        Log(LOG_LEVEL_INFO, "Hostname (reverse looked up): %s", conn->revdns);
    }

    while (CFTestD_BusyLoop(conn, config))
    {
    }

    CFTestD_DeleteConn(conn);
    return NULL;
}

static void CFTestD_SpawnConnection(
    const char *ipaddr, ConnectionInfo *info, CFTestD_Config *config)
{
    ServerConnectionState *conn = CFTestD_NewConn(info);
    ConnectionInfoSocket(info);
    strlcpy(conn->ipaddr, ipaddr, CF_MAX_IP_LEN);

    Log(LOG_LEVEL_WARNING, "Connection is being handled from main loop!");
    CFTestD_HandleConnection(conn, config);
}

/* Try to accept a connection; handle if we get one. */
static void CFTestD_AcceptAndHandle(int sd, CFTestD_Config *config)
{
    /* TODO embed ConnectionInfo into ServerConnectionState. */
    ConnectionInfo *info = ConnectionInfoNew(); /* Uses xcalloc() */

    info->ss_len = sizeof(info->ss);
    info->sd     = accept(sd, (struct sockaddr *)&info->ss, &info->ss_len);
    if (info->sd == -1)
    {
        Log(LOG_LEVEL_INFO, "Error accepting connection (%s)", GetErrorStr());
        ConnectionInfoDestroy(&info);
        return;
    }

    Log(LOG_LEVEL_DEBUG,
        "Socket descriptor returned from accept(): %d",
        info->sd);

    /* Just convert IP address to string, no DNS lookup. */
    char ipaddr[CF_MAX_IP_LEN] = "";
    getnameinfo(
        (const struct sockaddr *)&info->ss,
        info->ss_len,
        ipaddr,
        sizeof(ipaddr),
        NULL,
        0,
        NI_NUMERICHOST);

    /* IPv4 mapped addresses (e.g. "::ffff:192.168.1.2") are
     * hereby represented with their IPv4 counterpart. */
    CFTestD_SpawnConnection(MapAddress(ipaddr), info, config);
}

int CFTestD_StartServer(CFTestD_Config *config)
{
    bool tls_init_ok = ServerTLSInitialize(config->priv_key, config->pub_key, NULL);
    if (!tls_init_ok)
    {
        return -1;
    }

    int sd = InitServer(CFTESTD_QUEUE_SIZE);

    MakeSignalPipe();

    int selected = 0;
    while (selected != -1)
    {
        selected = WaitForIncoming(sd);

        if (selected > 0)
        {
            Log(LOG_LEVEL_DEBUG, "select(): %d", selected);
            CFTestD_AcceptAndHandle(sd, config);
        }
    }
    Log(LOG_LEVEL_ERR,
        "Error while waiting for connections. (select: %s)",
        GetErrorStr());

    Log(LOG_LEVEL_NOTICE, "Cleaning up and exiting...");
    if (sd != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Closing listening socket");
        cf_closesocket(sd);
    }

    return 0;
}

static void HandleSignal(int signum)
{
    switch (signum)
    {
    case SIGTERM:
    case SIGINT:
        // flush all logging before process ends.
        fflush(stdout);
        exit(EXIT_FAILURE);
        break;
    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

    Log(LOG_LEVEL_VERBOSE, "Starting cf-testd");
    CryptoInitialize();

    CFTestD_Config *config = CFTestD_CheckOpts(argc, argv);

    char *priv_key_path = NULL;
    char *pub_key_path = NULL;
    if (config->key_file != NULL)
    {
        priv_key_path = config->key_file;
        pub_key_path = xstrdup(priv_key_path);
        StringReplace(pub_key_path, strlen(pub_key_path) + 1,
                      "priv", "pub");
    }
    LoadSecretKeys(priv_key_path, pub_key_path, &(config->priv_key), &(config->pub_key));
    free(pub_key_path);

    cfnet_init(NULL, NULL);
    char *report_file      = config->report_file;

    if (report_file != NULL)
    {
        Log(LOG_LEVEL_NOTICE, "Got file argument: '%s'", report_file);
        if (!FileCanOpen(report_file, "r"))
        {
            Log(LOG_LEVEL_ERR,
                "Can't open file '%s' for reading",
                report_file);
            exit(EXIT_FAILURE);
        }

        Writer *contents = FileRead(report_file, SIZE_MAX, NULL);
        if (!contents)
        {
            Log(LOG_LEVEL_ERR, "Error reading report file '%s'", report_file);
            exit(EXIT_FAILURE);
        }
        config->report     = StringWriterClose(contents);
        config->report_len = SafeStringLength(config->report);

        Log(LOG_LEVEL_NOTICE,
            "Read %d bytes for report contents",
            config->report_len);

        if (config->report_len <= 0)
        {
            Log(LOG_LEVEL_ERR, "Report file contained no bytes");
            exit(EXIT_FAILURE);
        }

        Log(LOG_LEVEL_DEBUG, "Got report file contents: %s", config->report);
    }

    Log(LOG_LEVEL_INFO, "Starting server...");
    fflush(stdout); // for debugging startup

    int r = CFTestD_StartServer(config);
    CFTestD_ConfigDestroy(config);

    return r;
}
