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
#include <logging.h>            // thread-specific log prefix
#include <cleanup.h>

#ifndef __MINGW32__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <inaddr.h>
#endif

#define CFTESTD_QUEUE_SIZE 10
#define WAIT_CHECK_TIMEOUT 5

/* Strictly speaking/programming, this should use a lock, but it's not needed
 * for this testing tool. The worst that can happen is that some threads would
 * do one more WaitForIncoming iteration (WAIT_CHECK_TIMEOUT seconds). */
static bool TERMINATE = false;

// ============================= CFTestD_Config ==============================
typedef struct
{
    char *report_file;
    char *report_data;
    Seq *report;
    int report_len;
    char *key_file;
    RSA *priv_key;
    RSA *pub_key;
    SSL_CTX *ssl_ctx;
    char *address;
    pthread_t t_id;
    int ret;
} CFTestD_Config;

/*******************************************************************/
/* Command line option parsing                                     */
/*******************************************************************/

static const struct option OPTIONS[] = {
    {"address", required_argument, 0, 'a'},
    {"debug", no_argument, 0, 'd'},
    {"log-level", required_argument, 0, 'g'},
    {"help", no_argument, 0, 'h'},
    {"inform", no_argument, 0, 'I'},
    {"jobs", required_argument, 0, 'j'},
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
    "Specify how detailed logs should be. Possible values: 'error', 'warning', 'notice', 'info', 'verbose', 'debug'",
    "Print the help message",
    "Print basic information about what cf-testd does",
    "Number of jobs (threads) to run in parallel. Use '%d' in the report path"
    " and key file path (will be replaced by the thread number, starting with"
    " 0). Threads will bind to different addresses incrementally.",
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
    SeqDestroy(config->report);
    free(config->key_file);
    free(config->address);
    free(config);
}

void CFTestD_Help()
{
    Writer *w = FileWriter(stdout);
    WriterWriteHelp(w, "cf-testd", OPTIONS, HINTS, true, NULL);
    FileWriterDetach(w);
}

CFTestD_Config *CFTestD_CheckOpts(int argc, char **argv, long *n_threads)
{
    extern char *optarg;
    int c;
    CFTestD_Config *config = CFTestD_ConfigInit();
    assert(config != NULL);

    while ((c = getopt_long(argc, argv, "a:df:g:hIj:k:lp:vV", OPTIONS, NULL)) != -1)
    {
        switch (c)
        {
        case 'a':
            config->address = xstrdup(optarg);
            break;
        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            break;
        case 'h':
            CFTestD_Help();
            DoCleanupAndExit(EXIT_SUCCESS);
        case 'I':
            LogSetGlobalLevel(LOG_LEVEL_INFO);
            break;
        case 'g':
            LogSetGlobalLevelArgOrExit(optarg);
            break;
        case 'j':
        {
            int ret = StringToLong(optarg, n_threads);
            if (ret != 0)
            {
                Log(LOG_LEVEL_ERR, "Failed to parse number of threads/jobs from '%s'\n", optarg);
                *n_threads = 1;
            }
            break;
        }
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
                DoCleanupAndExit(EXIT_FAILURE);
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
            DoCleanupAndExit(EXIT_SUCCESS);
        default:
            CFTestD_Help();
            DoCleanupAndExit(EXIT_FAILURE);
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

bool CFTestD_TLSSessionEstablish(ServerConnectionState *conn, CFTestD_Config *config)
{
    if (conn->conn_info->status == CONNECTIONINFO_STATUS_ESTABLISHED)
    {
        return true;
    }

    bool established = BasicServerTLSSessionEstablish(conn, config->ssl_ctx);
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
    Seq *report          = config->report;
    const int report_len = config->report_len;
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

    const size_t n_report_lines = SeqLength(report);

    assert(n_report_lines > 1);
    char *ts = SeqAt(report, 0);
    char *header = StringFormat("CFR: 0 %s %d\n", ts, report_len);
    SendTransaction(conn_info, header, SafeStringLength(header), CF_MORE);
    free(header);

    for (size_t i = 1; i < n_report_lines; i++)
    {
        const char *report_line = SeqAt(report, i);
        SendTransaction(conn_info, report_line, SafeStringLength(report_line), CF_MORE);
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

static void *CFTestD_HandleConnection(ServerConnectionState *conn, CFTestD_Config *config)
{
    Log(LOG_LEVEL_INFO, "Accepting connection");

    bool established = CFTestD_TLSSessionEstablish(conn, config);
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
    int ret = -1;

    bool tls_init_ok = ServerTLSInitialize(config->priv_key, config->pub_key, &(config->ssl_ctx));
    if (!tls_init_ok)
    {
        return -1;
    }

    int sd = InitServer(CFTESTD_QUEUE_SIZE, config->address);

    int selected = 0;
    while (!TERMINATE && (selected != -1))
    {
        selected = WaitForIncoming(sd, WAIT_CHECK_TIMEOUT);

        if (selected > 0)
        {
            Log(LOG_LEVEL_DEBUG, "select(): %d", selected);
            CFTestD_AcceptAndHandle(sd, config);
        }
    }
    if (!TERMINATE)
    {
        Log(LOG_LEVEL_ERR,
            "Error while waiting for connections. (select: %s)",
            GetErrorStr());
    }
    else
    {
        ret = 0;
    }

    Log(LOG_LEVEL_NOTICE, "Cleaning up and exiting...");
    if (sd != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Closing listening socket");
        cf_closesocket(sd);
    }

    return ret;
}

static char *LogAddPrefix(LoggingPrivContext *log_ctx,
                          ARG_UNUSED LogLevel level,
                          const char *raw)
{
    const char *ip_addr = log_ctx->param;
    return ip_addr ? StringConcatenate(4, "[", ip_addr, "] ", raw) : xstrdup(raw);
}

static bool CFTestD_GetReportLine(char *data, char **report_line_start, size_t *report_line_length)
{
    int ret = sscanf(data, "%10zd", report_line_length);
    if (ret != 1)
    {
        /* incorrect number of items scanned (could be EOF) */
        return false;
    }

    *report_line_start = data + 10;
    return true;
}

static void *CFTestD_ServeReport(void *config_arg)
{
    CFTestD_Config *config = (CFTestD_Config *) config_arg;

    /* Set prefix for all Log()ging: */
    LoggingPrivContext *prior = LoggingPrivGetContext();
    LoggingPrivContext log_ctx = {
        .log_hook = LogAddPrefix,
        .param = config->address
    };
    LoggingPrivSetContext(&log_ctx);

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

    char *report_file = config->report_file;

    if (report_file != NULL)
    {
        Log(LOG_LEVEL_NOTICE, "Got file argument: '%s'", report_file);
        if (!FileCanOpen(report_file, "r"))
        {
            Log(LOG_LEVEL_ERR,
                "Can't open file '%s' for reading",
                report_file);
            DoCleanupAndExit(EXIT_FAILURE);
        }

        Writer *contents = FileRead(report_file, SIZE_MAX, NULL);
        if (!contents)
        {
            Log(LOG_LEVEL_ERR, "Error reading report file '%s'", report_file);
            DoCleanupAndExit(EXIT_FAILURE);
        }

        size_t report_data_len = StringWriterLength(contents);
        config->report_data = StringWriterClose(contents);

        Seq *report = SeqNew(64, NULL);
        size_t report_len = 0;

        StringRef ts_ref = StringGetToken(config->report_data, report_data_len, 0, "\n");
        char *ts = (char *) ts_ref.data;
        *(ts + ts_ref.len) = '\0';
        SeqAppend(report, ts);

        /* start right after the newline after the timestamp header */
        char *position = ts + ts_ref.len + 1;
        char *report_line;
        size_t report_line_len;
        while (CFTestD_GetReportLine(position, &report_line, &report_line_len))
        {
            *(report_line + report_line_len) = '\0';
            SeqAppend(report, report_line);
            report_len += report_line_len;
            position = report_line + report_line_len + 1; /* there's an extra newline after each report_line */
        }

        config->report = report;
        config->report_len = report_len;

        Log(LOG_LEVEL_NOTICE,
            "Read %d bytes for report contents",
            config->report_len);

        if (config->report_len <= 0)
        {
            Log(LOG_LEVEL_ERR, "Report file contained no bytes");
            DoCleanupAndExit(EXIT_FAILURE);
        }
    }

    Log(LOG_LEVEL_INFO, "Starting server at %s...", config->address);
    fflush(stdout); // for debugging startup

    config->ret = CFTestD_StartServer(config);

    free(config->report_data);

    /* we don't really need to do this here because the process is about the
     * terminate, but it's a good way the cleanup actually works and doesn't
     * cause a segfault or something */
    ServerTLSDeInitialize(&(config->priv_key), &(config->pub_key), &(config->ssl_ctx));

    LoggingPrivSetContext(prior);

    return NULL;
}

static void HandleSignal(int signum)
{
    switch (signum)
    {
    case SIGTERM:
    case SIGINT:
        // flush all logging before process ends.
        fflush(stdout);
        fprintf(stderr, "Terminating...\n");
        TERMINATE = true;
        break;
    default:
        break;
    }
}

/**
 * @param ip_str string representation of an IPv4 address (the usual one, with
 *               4 octets separated by dots)
 * @return a new string representing the incremented IP address (HAS TO BE FREED)
 */
static char *IncrementIPaddress(const char *ip_str)
{
    uint32_t ip = (uint32_t) inet_addr(ip_str);
    if (ip == INADDR_NONE)
    {
        Log(LOG_LEVEL_ERR, "Failed to parse address: '%s'", ip_str);
        return NULL;
    }

    int step = 1;
    char *last_dot = strrchr(ip_str, '.');
    assert(last_dot != NULL);   /* the doc comment says there must be dots! */
    if (StringSafeEqual(last_dot + 1, "255"))
    {
        /* avoid the network address (ending with 0) */
        step = 2;
    }
    else if (StringSafeEqual(last_dot + 1, "254"))
    {
        /* avoid the broadcast address and the network address */
        step = 3;
    }

    uint32_t ip_num = ntohl(ip);
    ip_num += step;
    ip = htonl(ip_num);

    struct in_addr ip_struct;
    ip_struct.s_addr = ip;

    return xstrdup(inet_ntoa(ip_struct));
}

int main(int argc, char *argv[])
{
    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

    Log(LOG_LEVEL_VERBOSE, "Starting cf-testd");
    cfnet_init(NULL, NULL);
    MakeSignalPipe();

    long n_threads = 1;
    CFTestD_Config *config = CFTestD_CheckOpts(argc, argv, &n_threads);
    if (config->address == NULL)
    {
        /* default to localhost */
        config->address = xstrdup("127.0.0.1");
    }

    CFTestD_Config **thread_configs = (CFTestD_Config**) xcalloc(n_threads, sizeof(CFTestD_Config*));
    for (int i = 0; i < n_threads; i++)
    {
        thread_configs[i] = (CFTestD_Config*) xmalloc(sizeof(CFTestD_Config));

        if (config->report_file != NULL && strstr(config->report_file, "%d") != NULL)
        {
            /* replace the '%d' with the thread number */
            asprintf(&(thread_configs[i]->report_file), config->report_file, i);
        }
        else
        {
            thread_configs[i]->report_file = SafeStringDuplicate(config->report_file);
        }

        if (config->key_file != NULL && strstr(config->key_file, "%d") != NULL)
        {
            /* replace the '%d' with the thread number */
            asprintf(&(thread_configs[i]->key_file), config->key_file, i);
        }
        else
        {
            thread_configs[i]->key_file = SafeStringDuplicate(config->key_file);
        }

        if (i == 0)
        {
            thread_configs[i]->address = xstrdup(config->address);
        }
        else
        {
            thread_configs[i]->address = IncrementIPaddress(thread_configs[i-1]->address);
        }
    }

    CFTestD_ConfigDestroy(config);

    bool failure = false;
    for (int i = 0; !failure && (i < n_threads); i++)
    {
        int ret = pthread_create(&(thread_configs[i]->t_id), NULL, CFTestD_ServeReport, thread_configs[i]);
        if (ret != 0)
        {
            Log(LOG_LEVEL_ERR, "Failed to create a new thread nr. %d: %s\n", i, strerror(ret));
            failure = true;
        }
    }

    if (failure)
    {
        return EXIT_FAILURE;
    }

    for (int i = 0; i < n_threads; i++)
    {
        int ret = pthread_join(thread_configs[i]->t_id, NULL);
        if (ret != 0)
        {
            Log(LOG_LEVEL_ERR, "Failed to join the thread nr. %d: %s\n", i, strerror(ret));
        }
        else
        {
            failure = failure && (thread_configs[i]->ret != 0);
        }
        CFTestD_ConfigDestroy(thread_configs[i]);
    }

    return failure ? EXIT_FAILURE : EXIT_SUCCESS;
}
