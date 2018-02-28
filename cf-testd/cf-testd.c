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

#include <client_code.h>                // cfnet_init
#include <crypto.h>                     // CryptoInitialize

#include <cf-serverd-enterprise-stubs.h>
#include <net.h>
#include <server_common.h>
#include <server_access.h>              // acl_Free
#include <item_lib.h>                   // DeleteItemList
#include <server_transform.h>           // Summarize
#include <bootstrap.h>                  // GetAmPolicyHub
#include <policy_server.h>              // PolicyServerReadFile
#include <systype.h>                    // CLASSTEXT
#include <mutex.h>                      // ThreadLock
#include <locks.h>                      // AcquireLock
#include <exec_tools.h>                 // ActAsDaemon
#include <man.h>                        // ManPageWrite
#include <server_tls.h>                 // ServerTLSInitialize
#include <tls_generic.h>                // TLSLogError
#include <timeout.h>                    // SetReferenceTime
#include <known_dirs.h>                 // GetInputDir
#include <sysinfo.h>                    // DetectEnvironment
#include <time_classes.h>               // UpdateTimeClasses
#include <loading.h>                    // LoadPolicy
#include <printsize.h>                  // PRINTSIZE
#include <conversion.h>                 // MapAddress
#include <server_code.h>                // InitServer
#include <signals.h>                    // ReloadConfigRequested
#include <openssl/err.h>                // ERR_get_error

// ============================= CFTestD_Config ==============================
typedef struct {
    char *file;
}CFTestD_Config;

/*******************************************************************/
/* Command line option parsing                                     */
/*******************************************************************/

static const struct option OPTIONS[] =
{
    {"address",   required_argument, 0, 'a'},
    {"debug",     no_argument,       0, 'd'},
    {"file",      required_argument, 0, 'f'},
    {"help",      no_argument,       0, 'h'},
    {"inform",    no_argument,       0, 'I'},
    {"port",      required_argument, 0, 'p'},
    {"timestamp", no_argument,       0, 'l'},
    {"verbose",   no_argument,       0, 'v'},
    {"version",   no_argument,       0, 'V'},
    {NULL,        0,                 0, '\0'}
};

static const char *const HINTS[] =
{
    "Bind to a specific address",
    "Enable debugging output",
    "Read report from file",
    "Print the help message",
    "Print basic information about what cf-testd does",
    "Set the port cf-testd will listen on",
    "Log timestamps on each line of log output",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    NULL
};

CFTestD_Config *CFTestD_ConfigInit()
{
    CFTestD_Config* r = (CFTestD_Config *)(xcalloc(1, sizeof(CFTestD_Config)));
    return r;
}

void CFTestD_ConfigDestroy(CFTestD_Config *config)
{
    free(config->file);
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

    while ((c = getopt_long(argc, argv, "a:df:hIlp:vV", OPTIONS, NULL))
           != -1)
    {
        switch (c)
        {
        case 'a':
            SetBindInterface(optarg);
            break;
        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            break;
        case 'f':
            config->file = xstrdup(optarg);
            break;
        case 'h':
            CFTestD_Help();
            exit(EXIT_SUCCESS);
        case 'I':
            LogSetGlobalLevel(LOG_LEVEL_INFO);
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
        case 'l':
            LoggingEnableTimestamps(true);
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
        int stop = argc;
        int total = stop - start;
        for (int i = 0; i<total; ++i)
        {
            Log(LOG_LEVEL_ERR, "[%d/%d]: %s\n", i+1, total, argv[start + i]);
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
    bool id_success = ServerIdentificationDialog(conn->conn_info,
                                                 username, sizeof(username));
    if (!id_success)
    {
        return false;
    }

    /* No CAUTH, SAUTH in non-classic protocol. */
    conn->user_data_set = 1;
    conn->rsa_auth = 1;

    ServerSendWelcome(conn);
    return true;
}

bool CFTestD_GetServerQuery(
    ServerConnectionState *conn, char *recvbuffer)
{
    char query[CF_BUFSIZE];

    query[0] = '\0';
    sscanf(recvbuffer, "QUERY %255[^\n]", query);

    if (strlen(query) == 0)
    {
        return false;
    }

    return CFTestD_ReturnQueryData(conn, query);
}

static bool CFTestD_ProtocolError(
    ServerConnectionState *conn, const char *recvbuffer, char *sendbuffer)
{
    strcpy(sendbuffer, "BAD: Request denied");
    SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
    Log(LOG_LEVEL_INFO,
        "Closing connection due to illegal request: %s", recvbuffer);
    return false;
}

static bool CFTestD_BusyLoop(ServerConnectionState *conn)
{
    char recvbuffer[CF_BUFSIZE + CF_BUFEXT] = "";
    char sendbuffer[CF_BUFSIZE - CF_INBAND_OFFSET] = "";

    const int received = ReceiveTransaction(conn->conn_info,
                                            recvbuffer, NULL);

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
            "Got NULL transmission (of size %d)", received);
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

        if (CFTestD_GetServerQuery(conn, recvbuffer))
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

    if (getsockname(ConnectionInfoSocket(info), (struct sockaddr *)&addr, &size) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Could not obtain socket address. (getsockname: '%s')",
            GetErrorStr());
        return NULL;
    }
#endif

    ServerConnectionState *conn = xcalloc(1, sizeof(*conn));
    conn->ctx = NULL;
    conn->conn_info = info;
    conn->encryption_type = 'c';
    /* Only public files (chmod o+r) accessible to non-root */
    conn->uid = CF_UNKNOWN_OWNER;                    /* Careful, 0 is root! */
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

static void *CFTestD_HandleConnection(void *c)
{
    ServerConnectionState *conn = c;

    Log(LOG_LEVEL_INFO, "Accepting connection");

    bool established = CFTestD_TLSSessionEstablish(conn);
    if (!established)
    {
        Log(LOG_LEVEL_ERR, "Could not establish TLS Session");
        return NULL;
    }
    int ret = getnameinfo((const struct sockaddr *) &conn->conn_info->ss,
                          conn->conn_info->ss_len,
                          conn->revdns, sizeof(conn->revdns),
                          NULL, 0, NI_NAMEREQD);
    if (ret != 0)
    {
        Log(LOG_LEVEL_INFO,
            "Reverse lookup failed (getnameinfo: %s)!",
            gai_strerror(ret));
    }
    else
    {
        Log(LOG_LEVEL_INFO,
            "Hostname (reverse looked up): %s",
            conn->revdns);
    }

    while (CFTestD_BusyLoop(conn))
    {
    }

    CFTestD_DeleteConn(conn);
    return NULL;
}

static void CFTestD_SpawnConnection(const char *ipaddr, ConnectionInfo *info)
{
    ServerConnectionState *conn = CFTestD_NewConn(info);
    ConnectionInfoSocket(info);
    strlcpy(conn->ipaddr, ipaddr, CF_MAX_IP_LEN);

    Log(LOG_LEVEL_WARNING, "Connection is being handled from main loop!");
    CFTestD_HandleConnection(conn);
}

/* Try to accept a connection; handle if we get one. */
static void CFTestD_AcceptAndHandle(int sd)
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
    CFTestD_SpawnConnection(MapAddress(ipaddr), info);
}

int CFTestD_StartServer()
{
    bool tls_init_ok = ServerTLSInitialize();
    if (!tls_init_ok)
    {
        return -1;
    }

    int sd = InitServer(10);

    MakeSignalPipe();

    int selected = 0;
    while (selected != -1)
    {
        selected = WaitForIncoming(sd);

        if (selected > 0)
        {
            Log(LOG_LEVEL_DEBUG, "select(): %d", selected);
            CFTestD_AcceptAndHandle(sd);
        }
    }
    Log(LOG_LEVEL_ERR, "Error while waiting for connections. (select: %s)",
        GetErrorStr());

    Log(LOG_LEVEL_NOTICE, "Cleaning up and exiting...");
    if (sd != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Closing listening socket");
        cf_closesocket(sd);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    Log(LOG_LEVEL_VERBOSE, "Starting cf-testd");
    CryptoInitialize();
    LoadSecretKeys();
    cfnet_init(NULL, NULL);
    CFTestD_Config *config = CFTestD_CheckOpts(argc, argv);

    if (config->file != NULL)
    {
        Log(LOG_LEVEL_NOTICE, "Got file argument: '%s'", config->file);
        Log(LOG_LEVEL_ERR, "File input not yet supported");
        // TODO: implement file input
        exit(EXIT_FAILURE);
    }

    int r = CFTestD_StartServer();
    CFTestD_ConfigDestroy(config);

    return r;
}
