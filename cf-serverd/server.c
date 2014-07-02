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
#include <printsize.h>

#include "server_classic.h"                    /* BusyWithClassicConnection */


/*
  The only exported function in this file is the following, used only in
  cf-serverd-functions.c.

  void ServerEntryPoint(EvalContext *ctx, const char *ipaddr, ConnectionInfo *info);

  TODO move this file to cf-serverd-functions.c or most probably server_common.c.
*/


//******************************************************************
// GLOBAL STATE
//******************************************************************

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

/******************************************************************/

static void SpawnConnection(EvalContext *ctx, const char *ipaddr, ConnectionInfo *info);
static void PurgeOldConnections(Item **list, time_t now);
static void *HandleConnection(void *conn);
static ServerConnectionState *NewConn(EvalContext *ctx, ConnectionInfo *info);
static void DeleteConn(ServerConnectionState *conn);

/****************************************************************************/

void ServerEntryPoint(EvalContext *ctx, const char *ipaddr, ConnectionInfo *info)
{
    time_t now;

    Log(LOG_LEVEL_VERBOSE,
        "Obtained IP address of '%s' on socket %d from accept",
        ipaddr, ConnectionInfoSocket(info));

    /* TODO change nonattackerlist, attackerlist and especially connectionlist
     *      to binary searched lists, or remove them from the main thread! */
    if ((SV.nonattackerlist) && (!IsMatchItemIn(SV.nonattackerlist, ipaddr)))
    {
        Log(LOG_LEVEL_ERR,
            "Remote host '%s' not in allowconnects, denying connection",
            ipaddr);
        cf_closesocket(ConnectionInfoSocket(info));
        ConnectionInfoDestroy(&info);
        return;
    }

    if (IsMatchItemIn(SV.attackerlist, ipaddr))
    {
        Log(LOG_LEVEL_ERR,
            "Remote host '%s' is in denyconnects, denying connection",
            ipaddr);
        cf_closesocket(ConnectionInfoSocket(info));
        ConnectionInfoDestroy(&info);
        return;
    }

    if ((now = time(NULL)) == -1)
    {
       now = 0;
    }

    PurgeOldConnections(&SV.connectionlist, now);

    if (!IsMatchItemIn(SV.multiconnlist, ipaddr))
    {
        if (!ThreadLock(cft_count))
        {
            cf_closesocket(ConnectionInfoSocket(info));
            ConnectionInfoDestroy(&info);
            return;
        }

        if (IsItemIn(SV.connectionlist, ipaddr))
        {
            ThreadUnlock(cft_count);
            Log(LOG_LEVEL_ERR,
                "Remote host '%s' is not in allowallconnects, denying second simultaneous connection",
                ipaddr);
            cf_closesocket(ConnectionInfoSocket(info));
            ConnectionInfoDestroy(&info);
            return;
        }

        ThreadUnlock(cft_count);
    }

    char intime[PRINTSIZE(now)];
    xsnprintf(intime, sizeof(intime), "%jd", (intmax_t) now);

    if (!ThreadLock(cft_count))
    {
        cf_closesocket(ConnectionInfoSocket(info));
        ConnectionInfoDestroy(&info);
        return;
    }

    PrependItem(&SV.connectionlist, ipaddr, intime);
    ThreadUnlock(cft_count);

    SpawnConnection(ctx, ipaddr, info);
}

/**********************************************************************/

static void PurgeOldConnections(Item **list, time_t now)
   /* Some connections might not terminate properly. These should be cleaned
      every couple of hours. That should be enough to prevent spamming. */
{
    assert(list != NULL);

    Log(LOG_LEVEL_DEBUG, "Purging Old Connections...");

    if (ThreadLock(cft_count))
    {
        Item *ip, *next;
        for (ip = *list; ip != NULL; ip = next)
        {
            int then = 0;
            sscanf(ip->classes, "%d", &then);

            next = ip->next;

            if (now > then + 7200)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "IP address '%s' has been more than two hours in connection list, purging",
                    ip->name);
                DeleteItem(list, ip);
            }
        }
        ThreadUnlock(cft_count);
    }

    Log(LOG_LEVEL_DEBUG, "Done purging old connections");
}

/*********************************************************************/

static void SpawnConnection(EvalContext *ctx, const char *ipaddr, ConnectionInfo *info)
{
    ServerConnectionState *conn = NULL;
    int ret;
    pthread_t tid;
    pthread_attr_t threadattrs;

    conn = NewConn(ctx, info);                 /* freed in HandleConnection */
    int sd_accepted = ConnectionInfoSocket(info);
    strlcpy(conn->ipaddr, ipaddr, CF_MAX_IP_LEN );

    Log(LOG_LEVEL_VERBOSE,
        "New connection (from %s, sd %d), spawning new thread...",
        conn->ipaddr, sd_accepted);

    ret = pthread_attr_init(&threadattrs);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "SpawnConnection: Unable to initialize thread attributes (%s)",
            GetErrorStr());
        goto err;
    }
    ret = pthread_attr_setdetachstate(&threadattrs, PTHREAD_CREATE_DETACHED);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "SpawnConnection: Unable to set thread to detached state (%s).",
            GetErrorStr());
        goto cleanup;
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
        goto cleanup;
    }

  cleanup:
    pthread_attr_destroy(&threadattrs);
  err:
    if (ret != 0)
    {
        Log(LOG_LEVEL_WARNING, "Thread is being handled from main loop!");
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

/* TRIES: counts the number of consecutive connections dropped. */
static int TRIES = 0;

static void *HandleConnection(void *c)
{
    ServerConnectionState *conn = c;
    int ret;

    /* Set logging prefix to be the IP address for all of thread's lifetime. */

    /* This stack-allocated struct should be valid for all the lifetime of the
     * thread. Just make sure that after calling DeleteConn() (which frees
     * ipaddr), you exit the thread right away. */
    LoggingPrivContext log_ctx = {
        .log_hook = LogHook,
        .param = conn->ipaddr
    };
    LoggingPrivSetContext(&log_ctx);

    Log(LOG_LEVEL_INFO, "Accepting connection");

    /* We test if number of active threads is greater than max, if so we deny
       connection, if it happened too many times within a short timeframe then we
       kill ourself.TODO this test should be done *before* spawning the thread. */
    ret = ThreadLock(cft_server_children);
    if (!ret)
    {
        Log(LOG_LEVEL_ERR, "Unable to thread-lock, closing connection!");
        goto ret2;
    }
    else if (ACTIVE_THREADS > CFD_MAXPROCESSES)
    {
        if (TRIES > MAXTRIES)
        {
            /* This happens when no thread was freed while we had to drop 5
             * (or maxconnections/3) consecutive connections, because none of
             * the existing threads finished. */
            Log(LOG_LEVEL_CRIT,
                "Server seems to be paralyzed. DOS attack? "
                "Committing apoptosis...");
            ThreadUnlock(cft_server_children);
            FatalError(conn->ctx, "Terminating");
        }

        TRIES++;
        Log(LOG_LEVEL_ERR,
            "Too many threads (%d > %d), dropping connection! "
            "Increase server maxconnections?",
            ACTIVE_THREADS, CFD_MAXPROCESSES);

        ThreadUnlock(cft_server_children);
        goto ret2;
    }

    ACTIVE_THREADS++;
    TRIES = 0;
    ThreadUnlock(cft_server_children);

    DisableSendDelays(ConnectionInfoSocket(conn->conn_info));

    /* 20 times the connect() timeout should be enough to avoid MD5
     * computation timeouts on big files on old slow Solaris 8 machines. */
    SetReceiveTimeout(ConnectionInfoSocket(conn->conn_info),
                      CONNTIMEOUT * 20 * 1000);

    if (ConnectionInfoConnectionStatus(conn->conn_info) != CF_CONNECTION_ESTABLISHED)
    {
        /* Decide the protocol used. */
        ret = ServerTLSPeek(conn->conn_info);
        if (ret == -1)
        {
            goto ret1;
        }
    }

    ProtocolVersion protocol_version = ConnectionInfoProtocolVersion(conn->conn_info);
    if (protocol_version == CF_PROTOCOL_LATEST)
    {
        ret = ServerTLSSessionEstablish(conn);
        if (ret == -1)
        {
            goto ret1;
        }
    }
    else if (protocol_version < CF_PROTOCOL_LATEST &&
             protocol_version > CF_PROTOCOL_UNDEFINED)
    {
        /* This connection is legacy protocol. Do we allow it? */
        if (SV.allowlegacyconnects != NULL &&           /* By default we do */
            !IsMatchItemIn(SV.allowlegacyconnects, conn->ipaddr))
        {
            Log(LOG_LEVEL_INFO,
                "Connection is not using latest protocol, denying");
            goto ret1;
        }
    }
    else
    {
        UnexpectedError("HandleConnection: ProtocolVersion %d!",
                        ConnectionInfoProtocolVersion(conn->conn_info));
        goto ret1;
    }


    /* =========================  MAIN LOOPS  ========================= */
    if (protocol_version >= CF_PROTOCOL_TLS)
    {
        /* New protocol does DNS reverse look up of the connected
         * IP address, to check hostname access_rules. */
        if (NEED_REVERSE_LOOKUP)
        {
            ret = getnameinfo((const struct sockaddr *) &conn->conn_info->ss,
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
        }

        while (BusyWithNewProtocol(conn->ctx, conn))
        {
        }
    }
    else if (protocol_version == CF_PROTOCOL_CLASSIC)
    {
        while (BusyWithClassicConnection(conn->ctx, conn))
        {
        }
    }
    /* ============================================================ */


    Log(LOG_LEVEL_INFO, "Closed connection, terminating thread");

  ret1:
    ThreadLock(cft_server_children);
    ACTIVE_THREADS--;
    ThreadUnlock(cft_server_children);

  ret2:
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
    conn->user_data_set = false;
    conn->rsa_auth = false;
    conn->hostname[0] = '\0';
    conn->ipaddr[0] = '\0';
    conn->username[0] = '\0';
    conn->session_key = NULL;
    conn->encryption_type = 'c';
    conn->maproot = false;      /* Only public files (chmod o+r) accessible */
    conn->revdns[0] = '\0';

    Log(LOG_LEVEL_DEBUG, "New socket %d", ConnectionInfoSocket(info));

    return conn;
}

/***************************************************************/

/**
 * @note This function is thread-safe. Do NOT wrap it with mutex!
 */
static void DeleteConn(ServerConnectionState *conn)
{
    cf_closesocket(ConnectionInfoSocket(conn->conn_info));
    ConnectionInfoDestroy(&conn->conn_info);
    free(conn->session_key);

    if (conn->ipaddr != NULL)
    {
        if (ThreadLock(cft_count))
        {
            DeleteItemMatching(&SV.connectionlist, conn->ipaddr);
            ThreadUnlock(cft_count);
        }
    }

    *conn = (ServerConnectionState) {0};
    free(conn);
}
