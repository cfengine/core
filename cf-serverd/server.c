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

#include <server.h>

#include <item_lib.h>
#include <crypto.h>
#include <files_hashes.h>
#include <eval_context.h>
#include <lastseen.h>
#include <conversion.h>
#include <string_lib.h>
#include <signals.h>
#include <mutex.h>
#include <net.h>                      /* SendTransaction,ReceiveTransaction */
#include <tls_generic.h>              /* TLSVerifyPeer */
#include <rlist.h>
#include <misc_lib.h>
#include <cf-serverd-enterprise-stubs.h>
#include <audit.h>
#include <cfnet.h>
#include <tls_server.h>
#include <server_common.h>
#include <connection_info.h>
#include <cf-windows-functions.h>
#include <logging_priv.h>                          /* LoggingPrivSetContext */

#include "server_classic.h"                    /* BusyWithClassicConnection */


/*
  The only exported function in this file is the following, used only in
  cf-serverd-functions.c:

  void ServerEntryPoint(EvalContext *ctx, char *ipaddr, ConnectionInfo *info);

  TODO move this file to cf-serverd-functions.c or most probably server_common.c.
*/


//******************************************************************
// GLOBAL STATE
//******************************************************************

/* TODO: rather than counting threads (within locks inside threads,
 * with ugly "try, wait, umm, what now?" code in the main thread) it
 * may make more sense to have a pool of persistent threads, managed
 * along with a queue of work for them by an object that creates new
 * threads on demand, up to its limit, when its queue is long enough
 * to justify doing so.  We can then queue ServerConnectionState*s to
 * be handled by the pool; we'll need to lock adding to the queue in
 * main thread and popping tasks off it in the workers.  The object
 * can then be responsible for sending a shutdown message to the
 * threads at the end of our run and pthread_join()ing them.
 */
int ACTIVE_THREADS = 0; /* GLOBAL_X */

int CFD_MAXPROCESSES = 0; /* GLOBAL_P */
bool DENYBADCLOCKS = true; /* GLOBAL_P */

int MAXTRIES = 5; /* GLOBAL_P */
bool LOGENCRYPT = false; /* GLOBAL_P */
int COLLECT_INTERVAL = 0; /* GLOBAL_P */
int COLLECT_WINDOW = 10; /* GLOBAL_P */
bool SERVER_LISTEN = true; /* GLOBAL_P */

ServerAccess SV = { 0 }; /* GLOBAL_P */

char CFRUNCOMMAND[CF_MAXVARSIZE] = { 0 };                       /* GLOBAL_P */

//******************************************************************/
// LOCAL CONSTANTS
//******************************************************************/


static void SpawnConnection(EvalContext *ctx, char *ipaddr, ConnectionInfo *info);
static void PurgeOldConnections(Item **list, time_t now);
static void *HandleConnection(void *conn);
static ServerConnectionState *NewConn(EvalContext *ctx, ConnectionInfo *info);
static void DeleteConn(ServerConnectionState *conn);

//******************************************************************/
// LOCAL STATE
//******************************************************************/

static int TRIES = 0; /* GLOBAL_X */


/****************************************************************************/

void ServerEntryPoint(EvalContext *ctx, char *ipaddr, ConnectionInfo *info)
{
    char intime[64];

    Log(LOG_LEVEL_VERBOSE,
        "Obtained IP address of '%s' on socket %d from accept",
        ipaddr, ConnectionInfoSocket(info));

    if ((SV.nonattackerlist) && (!IsMatchItemIn(SV.nonattackerlist, MapAddress(ipaddr))))
    {
        Log(LOG_LEVEL_ERR, "Not allowing connection from non-authorized IP '%s'", ipaddr);
        cf_closesocket(ConnectionInfoSocket(info));
        ConnectionInfoDestroy(&info);
        return;
    }

    if (IsMatchItemIn(SV.attackerlist, MapAddress(ipaddr)))
    {
        Log(LOG_LEVEL_ERR, "Denying connection from non-authorized IP '%s'", ipaddr);
        cf_closesocket(ConnectionInfoSocket(info));
        ConnectionInfoDestroy(&info);
        return;
    }

    time_t now = time(NULL);
    if (now == -1)
    {
       now = 0;
    }

    PurgeOldConnections(&SV.connectionlist, now);

    if (!IsMatchItemIn(SV.multiconnlist, MapAddress(ipaddr)))
    {
        if (!ThreadLock(cft_count))
        {
            return;
        }

        if (IsItemIn(SV.connectionlist, MapAddress(ipaddr)))
        {
            ThreadUnlock(cft_count);
            Log(LOG_LEVEL_ERR, "Denying repeated connection from '%s'", ipaddr);
            cf_closesocket(ConnectionInfoSocket(info));
            ConnectionInfoDestroy(&info);
            return;
        }

        ThreadUnlock(cft_count);
    }

    Log(SV.logconns ? LOG_LEVEL_INFO : LOG_LEVEL_VERBOSE,
        "Accepting connection from %s", ipaddr);

    snprintf(intime, 63, "%d", (int) now);

    if (!ThreadLock(cft_count))
    {
        cf_closesocket(ConnectionInfoSocket(info));
        ConnectionInfoDestroy(&info);
        return;
    }

    PrependItem(&SV.connectionlist, MapAddress(ipaddr), intime);

    if (!ThreadUnlock(cft_count))
    {
        cf_closesocket(ConnectionInfoSocket(info));
        ConnectionInfoDestroy(&info);
        return;
    }

    SpawnConnection(ctx, ipaddr, info);
}

/**********************************************************************/

static void PurgeOldConnections(Item **list, time_t now)
   /* Some connections might not terminate properly. These should be cleaned
      every couple of hours. That should be enough to prevent spamming. */
{
    Item *ip;
    int then = 0;

    Log(LOG_LEVEL_DEBUG, "Purging Old Connections...");

    if (!ThreadLock(cft_count))
    {
        return;
    }

    if (list == NULL)
    {
        ThreadUnlock(cft_count);
        return;
    }

    Item *next;

    for (ip = *list; ip != NULL; ip = next)
    {
        sscanf(ip->classes, "%d", &then);

        next = ip->next;

        if (now > then + 7200)
        {
            Log(LOG_LEVEL_VERBOSE, "Purging IP address %s from connection list", ip->name);
            DeleteItem(list, ip);
        }
    }

    if (!ThreadUnlock(cft_count))
    {
        return;
    }

    Log(LOG_LEVEL_DEBUG, "Done purging old connections");
}

/*********************************************************************/

static void SpawnConnection(EvalContext *ctx, char *ipaddr, ConnectionInfo *info)
{
    ServerConnectionState *conn = NULL;
    int ret;
    pthread_t tid;
    pthread_attr_t threadattrs;

    conn = NewConn(ctx, info);
    strlcpy(conn->ipaddr, ipaddr, CF_MAX_IP_LEN );

    Log(LOG_LEVEL_VERBOSE, "New connection...(from %s, sd %d)",
        conn->ipaddr, ConnectionInfoSocket(info));
    Log(LOG_LEVEL_VERBOSE, "Spawning new thread...");

    ret = pthread_attr_init(&threadattrs);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "SpawnConnection: Unable to initialize thread attributes (%s)",
            GetErrorStr());
        goto err2;
    }
    ret = pthread_attr_setdetachstate(&threadattrs, PTHREAD_CREATE_DETACHED);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "SpawnConnection: Unable to set thread to detached state (%s).",
            GetErrorStr());
        goto err1;
    }
    ret = pthread_attr_setstacksize(&threadattrs, 1024 * 1024);
    if (ret != 0)
    {
        Log(LOG_LEVEL_WARNING,
            "SpawnConnection: Unable to set thread stack size (%s).",
            GetErrorStr());
        /* Continue with default thread stack size. */
    }

    ret = pthread_create(&tid, &threadattrs, HandleConnection, conn);
    if (ret != 0)
    {
        errno = ret;
        Log(LOG_LEVEL_ERR,
            "Unable to spawn worker thread. (pthread_create: %s)",
            GetErrorStr());
        goto err1;
    }

  err1:
    pthread_attr_destroy(&threadattrs);
  err2:
    if (ret != 0)
    {
        Log(LOG_LEVEL_WARNING, "Handling thread's work from main loop!");
        HandleConnection(conn);
    }
}

/*********************************************************************/

static void DisableSendDelays(int sockfd)
{
    int yes = 1;

    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *) &yes, sizeof(yes)) == -1)
    {
        Log(LOG_LEVEL_INFO, "Unable to disable Nagle algorithm, expect performance problems. (setsockopt(TCP_NODELAY): %s)", GetErrorStr());
    }
}

/*********************************************************************/

static char *LogHook(LoggingPrivContext *log_ctx, ARG_UNUSED LogLevel level, const char *message)
{
    const char *ipaddr = log_ctx->param;
    return StringConcatenate(3, ipaddr, "> ", message);
}

static void *HandleConnection(void *c)
{
    ServerConnectionState *conn = c;
    int ret;
    char output[CF_BUFSIZE];

    /* Set logging prefix to be the IP address for all of thread's lifetime. */

    /* This stack-allocated struct should be valid for all the lifetime of the
     * thread. Just make sure that after calling DeleteConn() (which frees
     * ipaddr), you exit right away. */
    LoggingPrivContext log_ctx = {
        .log_hook = LogHook,
        .param = conn->ipaddr
    };

    LoggingPrivSetContext(&log_ctx);

    if (!ThreadLock(cft_server_children))
    {
        DeleteConn(conn);
        return NULL;
    }

    ACTIVE_THREADS++;

    if (ACTIVE_THREADS < CFD_MAXPROCESSES)
    {
        ThreadUnlock(cft_server_children);
    }
    else
    {
        ACTIVE_THREADS--;

        if (TRIES++ > MAXTRIES) /* When to say we're hung / apoptosis threshold */
        {
            Log(LOG_LEVEL_ERR, "Server seems to be paralyzed. DOS attack? Committing apoptosis...");
            FatalError(conn->ctx, "Terminating");
        }

        if (!ThreadUnlock(cft_server_children))
        {
            /* TODO: what ? */
        }

        Log(LOG_LEVEL_ERR, "Too many threads (>=%d) -- increase server maxconnections?", CFD_MAXPROCESSES);
        snprintf(output, CF_BUFSIZE, "BAD: Server is currently too busy -- increase maxconnections or splaytime?");
        SendTransaction(conn->conn_info, output, 0, CF_DONE);
        DeleteConn(conn);
        return NULL;
    }

    TRIES = 0;                  /* As long as there is activity, we're not stuck */

    DisableSendDelays(ConnectionInfoSocket(conn->conn_info));

    struct timeval tv = {
        .tv_sec = CONNTIMEOUT * 20,
    };
    SetReceiveTimeout(ConnectionInfoSocket(conn->conn_info), &tv);

    if (ConnectionInfoConnectionStatus(conn->conn_info) != CF_CONNECTION_ESTABLISHED)
    {
        /* Decide the protocol used. */
        ret = ServerTLSPeek(conn->conn_info);
        if (ret == -1)
        {
            DeleteConn(conn);
            return NULL;
        }
    }

    switch (ConnectionInfoProtocolVersion(conn->conn_info))
    {

    case CF_PROTOCOL_CLASSIC:
    {
        while (BusyWithClassicConnection(conn->ctx, conn))
        {
        }
        break;
    }

    case CF_PROTOCOL_TLS:
    {
        ret = ServerTLSSessionEstablish(conn);
        if (ret == -1)
        {
            DeleteConn(conn);
            return NULL;
        }

        while (BusyWithNewProtocol(conn->ctx, conn))
        {
        }
        break;
    }

    default:
        UnexpectedError("HandleConnection: ProtocolVersion %d!",
                        ConnectionInfoProtocolVersion(conn->conn_info));
    }

    Log(LOG_LEVEL_INFO, "Connection closed, terminating thread");

    if (!ThreadLock(cft_server_children))
    {
        DeleteConn(conn);
        return NULL;
    }

    ACTIVE_THREADS--;

    if (!ThreadUnlock(cft_server_children))
    {
        /* TODO: what ? */
    }

    DeleteConn(conn);
    return NULL;
}


/***************************************************************/
/* Toolkit/Class: conn                                         */
/***************************************************************/

static ServerConnectionState *NewConn(EvalContext *ctx, ConnectionInfo *info)
{
    ServerConnectionState *conn = NULL;
    struct sockaddr_storage addr;
    socklen_t size = sizeof(addr);

    if (getsockname(ConnectionInfoSocket(info), (struct sockaddr *)&addr, &size) == -1)
    {
        Log(LOG_LEVEL_ERR, "Could not obtain socket address. (getsockname: '%s')", GetErrorStr());
        return NULL;
    }

    conn = xcalloc(1, sizeof(*conn));
    conn->ctx = ctx;
    conn->conn_info = info;
    conn->id_verified = false;
    conn->rsa_auth = false;
    conn->hostname[0] = '\0';
    conn->ipaddr[0] = '\0';
    conn->username[0] = '\0';
    conn->session_key = NULL;
    conn->encryption_type = 'c';
    conn->maproot = false;      /* Only public files (chmod o+r) accessible */

    Log(LOG_LEVEL_DEBUG, "New socket %d", ConnectionInfoSocket(info));

    return conn;
}

/***************************************************************/

static void DeleteConn(ServerConnectionState *conn)
{
    cf_closesocket(ConnectionInfoSocket(conn->conn_info));
    ConnectionInfoDestroy(&conn->conn_info);
    free(conn->session_key);
    if (conn->ipaddr != NULL)
    {
        if (!ThreadLock(cft_count))
        {
            return;
        }

        DeleteItemMatching(&SV.connectionlist, MapAddress(conn->ipaddr));

        if (!ThreadUnlock(cft_count))
        {
            return;
        }
    }

    *conn = (ServerConnectionState) {0};
    free(conn);
}
