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

#include "server.h"

#include "item_lib.h"
#include "crypto.h"
#include "files_hashes.h"
#include "env_context.h"
#include "lastseen.h"
#include "conversion.h"
#include "string_lib.h"
#include "signals.h"
#include "mutex.h"
#include "net.h"                      /* SendTransaction,ReceiveTransaction */
#include "tls_generic.h"              /* TLSVerifyPeer */
#include "rlist.h"
#include "misc_lib.h"
#include "cf-serverd-enterprise-stubs.h"
#include "audit.h"
#include "server_tls.h"
#include "server_common.h"

#ifdef HAVE_NOVA
# include "cf.nova.h"
#endif

//******************************************************************
// GLOBAL STATE
//******************************************************************

int CLOCK_DRIFT = 3600;  /* 1hr */
int ACTIVE_THREADS;

int CFD_MAXPROCESSES = 0;
bool DENYBADCLOCKS = true;

int MAXTRIES = 5;
bool LOGENCRYPT = false;
int COLLECT_INTERVAL = 0;
int COLLECT_WINDOW = 10;
bool SERVER_LISTEN = true;

ServerAccess SV;

char CFRUNCOMMAND[CF_BUFSIZE] = { 0 };

//******************************************************************/
// LOCAL CONSTANTS
//******************************************************************/


static void SpawnConnection(EvalContext *ctx, int sd_reply, char *ipaddr);
static void *HandleConnection(ServerConnectionState *conn);
static int BusyWithClassicConnection(EvalContext *ctx, ServerConnectionState *conn);
static int VerifyConnection(ServerConnectionState *conn, char buf[CF_BUFSIZE]);
static int CheckStoreKey(ServerConnectionState *conn, RSA *key);
static ServerConnectionState *NewConn(EvalContext *ctx, int sd);
static void DeleteConn(ServerConnectionState *conn);
static int AuthenticationDialogue(ServerConnectionState *conn, char *recvbuffer, int recvlen);

//******************************************************************/
// LOCAL STATE
//******************************************************************/

static int TRIES = 0;

/*******************************************************************/


//*******************************************************************
// COMMANDS
//*******************************************************************

typedef enum
{
    PROTOCOL_COMMAND_EXEC,
    PROTOCOL_COMMAND_AUTH,
    PROTOCOL_COMMAND_GET,
    PROTOCOL_COMMAND_OPENDIR,
    PROTOCOL_COMMAND_SYNC,
    PROTOCOL_COMMAND_CONTEXTS,
    PROTOCOL_COMMAND_MD5,
    PROTOCOL_COMMAND_MD5_SECURE,
    PROTOCOL_COMMAND_AUTH_CLEAR,
    PROTOCOL_COMMAND_AUTH_SECURE,
    PROTOCOL_COMMAND_SYNC_SECURE,
    PROTOCOL_COMMAND_GET_SECURE,
    PROTOCOL_COMMAND_VERSION,
    PROTOCOL_COMMAND_OPENDIR_SECURE,
    PROTOCOL_COMMAND_VAR,
    PROTOCOL_COMMAND_VAR_SECURE,
    PROTOCOL_COMMAND_CONTEXT,
    PROTOCOL_COMMAND_CONTEXT_SECURE,
    PROTOCOL_COMMAND_QUERY_SECURE,
    PROTOCOL_COMMAND_CALL_ME_BACK,
    PROTOCOL_COMMAND_STARTTLS,
    PROTOCOL_COMMAND_BAD
} ProtocolCommandClassic;

static const char *PROTOCOL_CLASSIC[] =
{
    "EXEC",
    "AUTH",                     /* old protocol */
    "GET",
    "OPENDIR",
    "SYNCH",
    "CLASSES",
    "MD5",
    "SMD5",
    "CAUTH",
    "SAUTH",
    "SSYNCH",
    "SGET",
    "VERSION",
    "SOPENDIR",
    "VAR",
    "SVAR",
    "CONTEXT",
    "SCONTEXT",
    "SQUERY",
    "SCALLBACK",
    "STARTTLS",
    NULL
};

ProtocolCommandClassic GetCommandClassic(char *str)
{
    int i;
    for (i = 0; PROTOCOL_CLASSIC[i] != NULL; i++)
    {
        int cmdlen = strlen(PROTOCOL_CLASSIC[i]);
        if ((strncmp(str, PROTOCOL_CLASSIC[i], cmdlen) == 0) &&
            (str[cmdlen] == ' ' || str[cmdlen] == '\0'))
        {
            return i;
        }
    }
    assert (i == PROTOCOL_COMMAND_BAD);
    return i;
}


/****************************************************************************/

void ServerEntryPoint(EvalContext *ctx, int sd_accepted, char *ipaddr)
{
    char intime[64];
    time_t now;

    Log(LOG_LEVEL_VERBOSE,
        "Obtained IP address of '%s' on socket %d from accept",
        ipaddr, sd_accepted);

    if ((SV.nonattackerlist) && (!IsMatchItemIn(SV.nonattackerlist, MapAddress(ipaddr))))
    {
        Log(LOG_LEVEL_ERR, "Not allowing connection from non-authorized IP '%s'", ipaddr);
        cf_closesocket(sd_accepted);
        return;
    }

    if (IsMatchItemIn(SV.attackerlist, MapAddress(ipaddr)))
    {
        Log(LOG_LEVEL_ERR, "Denying connection from non-authorized IP '%s'", ipaddr);
        cf_closesocket(sd_accepted);
        return;
    }

    if ((now = time((time_t *) NULL)) == -1)
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
            cf_closesocket(sd_accepted);
            return;
        }

        ThreadUnlock(cft_count);
    }

    if (SV.logconns)
    {
        Log(LOG_LEVEL_INFO, "Accepting connection from %s", ipaddr);
    }
    else
    {
        Log(LOG_LEVEL_INFO, "Accepting connection from %s", ipaddr);
    }

    snprintf(intime, 63, "%d", (int) now);

    if (!ThreadLock(cft_count))
    {
        return;
    }

    PrependItem(&SV.connectionlist, MapAddress(ipaddr), intime);

    if (!ThreadUnlock(cft_count))
    {
       return;
    }

    SpawnConnection(ctx, sd_accepted, ipaddr);

}

/**********************************************************************/

void PurgeOldConnections(Item **list, time_t now)
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

static void SpawnConnection(EvalContext *ctx, int sd_accepted, char *ipaddr)
{
    ServerConnectionState *conn;
    int ret;
    pthread_t tid;
    pthread_attr_t threadattrs;

    if ((conn = NewConn(ctx, sd_accepted)) == NULL)
    {
        return;
    }

    strncpy(conn->ipaddr, ipaddr, CF_MAX_IP_LEN - 1);

    Log(LOG_LEVEL_VERBOSE, "New connection...(from %s, sd %d)",
        conn->ipaddr, sd_accepted);
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

    ret = pthread_create(&tid, &threadattrs,
                         (void *(*)(void *)) HandleConnection, conn);
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
        Log(LOG_LEVEL_WARNING, "Thread is being handled from main loop!");
        HandleConnection(conn);
    }
}

/*********************************************************************/

void DisableSendDelays(int sockfd)
{
    int yes = 1;

    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *) &yes, sizeof(yes)) == -1)
    {
        Log(LOG_LEVEL_INFO, "Unable to disable Nagle algorithm, expect performance problems. (setsockopt(TCP_NODELAY): %s)", GetErrorStr());
    }
}

/*********************************************************************/

static void *HandleConnection(ServerConnectionState *conn)
{
    char output[CF_BUFSIZE];

# ifdef HAVE_PTHREAD_SIGMASK
    sigset_t sigmask;

    sigemptyset(&sigmask);
    pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
# endif

    if (!ThreadLock(cft_server_children))
    {
        DeleteConn(conn);
        return NULL;
    }

    ACTIVE_THREADS++;

    if (ACTIVE_THREADS >= CFD_MAXPROCESSES)
    {
        ACTIVE_THREADS--;

        if (TRIES++ > MAXTRIES) /* When to say we're hung / apoptosis threshold */
        {
            Log(LOG_LEVEL_ERR, "Server seems to be paralyzed. DOS attack? Committing apoptosis...");
            FatalError(conn->ctx, "Terminating");
        }

        if (!ThreadUnlock(cft_server_children))
        {
        }

        Log(LOG_LEVEL_ERR, "Too many threads (>=%d) -- increase server maxconnections?", CFD_MAXPROCESSES);
        snprintf(output, CF_BUFSIZE, "BAD: Server is currently too busy -- increase maxconnections or splaytime?");
        SendTransaction(&conn->conn_info, output, 0, CF_DONE);
        DeleteConn(conn);
        return NULL;
    }
    else
    {
        ThreadUnlock(cft_server_children);
    }

    TRIES = 0;                  /* As long as there is activity, we're not stuck */

    DisableSendDelays(conn->conn_info.sd);

    struct timeval tv = {
        .tv_sec = CONNTIMEOUT * 20,
    };

    SetReceiveTimeout(conn->conn_info.sd, &tv);

    /* TODO different busy loops for the two protocol versions */
    switch (conn->conn_info.type)
    {
    case CF_PROTOCOL_CLASSIC:
        while (BusyWithClassicConnection(conn->ctx, conn))
        {
        }
        break;
    case CF_PROTOCOL_TLS:
        while (BusyWithNewProtocol(conn->ctx, conn))
        {
        }
        break;
    default:
        UnexpectedError("HandleConnection: ProtocolVersion %d!",
                        conn->conn_info.type);
    }

    Log(LOG_LEVEL_INFO, "Connection from %s is closed, terminating thread",
        conn->ipaddr);

    if (!ThreadLock(cft_server_children))
    {
        DeleteConn(conn);
        return NULL;
    }

    ACTIVE_THREADS--;

    if (!ThreadUnlock(cft_server_children))
    {
    }

    DeleteConn(conn);
    return NULL;
}

/*********************************************************************/

static int BusyWithClassicConnection(EvalContext *ctx, ServerConnectionState *conn)
{
    time_t tloc, trem = 0;
    char recvbuffer[CF_BUFSIZE + CF_BUFEXT], sendbuffer[CF_BUFSIZE], check[CF_BUFSIZE];
    char filename[CF_BUFSIZE], buffer[CF_BUFSIZE], args[CF_BUFSIZE], out[CF_BUFSIZE];
    long time_no_see = 0;
    unsigned int len = 0;
    int drift, plainlen, received, encrypted = 0;
    ServerFileGetState get_args;
    Item *classes;

    memset(recvbuffer, 0, CF_BUFSIZE + CF_BUFEXT);
    memset(&get_args, 0, sizeof(get_args));

    received = ReceiveTransaction(&conn->conn_info, recvbuffer, NULL);
    if (received == -1 || received == 0)
    {
        return false;
    }

    if (strlen(recvbuffer) == 0)
    {
        Log(LOG_LEVEL_WARNING, "Got NULL transmission, skipping!");
        return true;
    }

    /* Don't process request if we're signalled to exit. */
    if (IsPendingTermination())
    {
        return false;
    }

    switch (GetCommandClassic(recvbuffer))
    {
    case PROTOCOL_COMMAND_EXEC:
        memset(args, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "EXEC %255[^\n]", args);

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "Server refusal due to incorrect identity");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (!AllowedUser(conn->username))
        {
            Log(LOG_LEVEL_INFO, "Server refusal due to non-allowed user");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (!conn->rsa_auth)
        {
            Log(LOG_LEVEL_INFO, "Server refusal due to no RSA authentication");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (!AccessControl(ctx, CommandArg0(CFRUNCOMMAND), conn, false))
        {
            Log(LOG_LEVEL_INFO, "Server refusal due to denied access to requested object");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (!MatchClasses(ctx, conn))
        {
            Log(LOG_LEVEL_INFO, "Server refusal due to failed class/context match");
            Terminate(&conn->conn_info);
            return false;
        }

        DoExec(ctx, conn, args);
        Terminate(&conn->conn_info);
        return false;

    case PROTOCOL_COMMAND_VERSION:

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
        }

        snprintf(conn->output, CF_BUFSIZE, "OK: %s", Version());
        SendTransaction(&conn->conn_info, conn->output, 0, CF_DONE);
        return conn->id_verified;

    case PROTOCOL_COMMAND_AUTH_CLEAR:

        conn->id_verified = VerifyConnection(conn, (char *) (recvbuffer + strlen("CAUTH ")));

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
        }

        return conn->id_verified;       /* are we finished yet ? */

    case PROTOCOL_COMMAND_AUTH_SECURE:            /* This is where key agreement takes place */

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (!AuthenticationDialogue(conn, recvbuffer, received))
        {
            Log(LOG_LEVEL_INFO, "Auth dialogue error");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        return true;

    case PROTOCOL_COMMAND_GET:

        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "GET %d %[^\n]", &(get_args.buf_size), filename);

        if ((get_args.buf_size < 0) || (get_args.buf_size > CF_BUFSIZE))
        {
            Log(LOG_LEVEL_INFO, "GET buffer out of bounds");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (!AccessControl(ctx, filename, conn, false))
        {
            Log(LOG_LEVEL_INFO, "Access denied to get object");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        memset(sendbuffer, 0, CF_BUFSIZE);

        if (get_args.buf_size >= CF_BUFSIZE)
        {
            get_args.buf_size = 2048;
        }

        get_args.connect = conn;
        get_args.encrypt = false;
        get_args.replybuff = sendbuffer;
        get_args.replyfile = filename;

        CfGetFile(&get_args);

        return true;

    case PROTOCOL_COMMAND_GET_SECURE:

        memset(buffer, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "SGET %u %d", &len, &(get_args.buf_size));

        if (received != len + CF_PROTO_OFFSET)
        {
            Log(LOG_LEVEL_VERBOSE, "Protocol error SGET");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        plainlen = DecryptString(conn->encryption_type, recvbuffer + CF_PROTO_OFFSET, buffer, conn->session_key, len);

        cfscanf(buffer, strlen("GET"), strlen("dummykey"), check, sendbuffer, filename);

        if (strcmp(check, "GET") != 0)
        {
            Log(LOG_LEVEL_INFO, "SGET/GET problem");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        if ((get_args.buf_size < 0) || (get_args.buf_size > 8192))
        {
            Log(LOG_LEVEL_INFO, "SGET bounding error");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (get_args.buf_size >= CF_BUFSIZE)
        {
            get_args.buf_size = 2048;
        }

        Log(LOG_LEVEL_DEBUG, "Confirm decryption, and thus validity of caller");
        Log(LOG_LEVEL_DEBUG, "SGET '%s' with blocksize %d", filename, get_args.buf_size);

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (!AccessControl(ctx, filename, conn, true))
        {
            Log(LOG_LEVEL_INFO, "Access control error");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        memset(sendbuffer, 0, CF_BUFSIZE);

        get_args.connect = conn;
        get_args.encrypt = true;
        get_args.replybuff = sendbuffer;
        get_args.replyfile = filename;

        CfEncryptGetFile(&get_args);
        return true;

    case PROTOCOL_COMMAND_OPENDIR_SECURE:

        memset(buffer, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "SOPENDIR %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_VERBOSE, "Protocol error OPENDIR: %d", len);
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (conn->session_key == NULL)
        {
            Log(LOG_LEVEL_INFO, "No session key");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);

        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

        if (strncmp(recvbuffer, "OPENDIR", 7) != 0)
        {
            Log(LOG_LEVEL_INFO, "Opendir failed to decrypt");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "OPENDIR %[^\n]", filename);

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (!AccessControl(ctx, filename, conn, true))        /* opendir don't care about privacy */
        {
            Log(LOG_LEVEL_INFO, "Access error");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        CfSecOpenDirectory(conn, sendbuffer, filename);
        return true;

    case PROTOCOL_COMMAND_OPENDIR:

        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "OPENDIR %[^\n]", filename);

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (!AccessControl(ctx, filename, conn, true))        /* opendir don't care about privacy */
        {
            Log(LOG_LEVEL_INFO, "DIR access error");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        CfOpenDirectory(conn, sendbuffer, filename);
        return true;

    case PROTOCOL_COMMAND_SYNC_SECURE:

        memset(buffer, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "SSYNCH %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_VERBOSE, "Protocol error SSYNCH: %d", len);
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (conn->session_key == NULL)
        {
            Log(LOG_LEVEL_INFO, "Bad session key");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);

        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

        if (plainlen < 0)
        {
            DebugBinOut((char *) conn->session_key, 32, "Session key");
            Log(LOG_LEVEL_ERR, "Bad decrypt (%d)", len);
        }

        if (strncmp(recvbuffer, "SYNCH", 5) != 0)
        {
            Log(LOG_LEVEL_INFO, "No synch");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        /* roll through, no break */

    case PROTOCOL_COMMAND_SYNC:

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "SYNCH %ld STAT %[^\n]", &time_no_see, filename);

        trem = (time_t) time_no_see;

        if ((time_no_see == 0) || (filename[0] == '\0'))
        {
            break;
        }

        if ((tloc = time((time_t *) NULL)) == -1)
        {
            sprintf(conn->output, "Couldn't read system clock\n");
            Log(LOG_LEVEL_INFO, "Couldn't read system clock. (time: %s)", GetErrorStr());
            SendTransaction(&conn->conn_info, "BAD: clocks out of synch", 0, CF_DONE);
            return true;
        }

        drift = (int) (tloc - trem);

        if (!AccessControl(ctx, filename, conn, true))
        {
            Log(LOG_LEVEL_INFO, "Access control in sync");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        if (DENYBADCLOCKS && (drift * drift > CLOCK_DRIFT * CLOCK_DRIFT))
        {
            snprintf(conn->output, CF_BUFSIZE - 1, "BAD: Clocks are too far unsynchronized %ld/%ld\n", (long) tloc,
                     (long) trem);
            SendTransaction(&conn->conn_info, conn->output, 0, CF_DONE);
            return true;
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "Clocks were off by %ld", (long) tloc - (long) trem);
            StatFile(conn, sendbuffer, filename);
        }

        return true;

    case PROTOCOL_COMMAND_MD5_SECURE:

        sscanf(recvbuffer, "SMD5 %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Decryption error");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

        if (strncmp(recvbuffer, "MD5", 3) != 0)
        {
            Log(LOG_LEVEL_INFO, "MD5 protocol error");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        /* roll through, no break */

    case PROTOCOL_COMMAND_MD5:

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        CompareLocalHash(conn, sendbuffer, recvbuffer);
        return true;

    case PROTOCOL_COMMAND_VAR_SECURE:

        sscanf(recvbuffer, "SVAR %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Decrypt error SVAR");
            RefuseAccess(conn, 0, "decrypt error SVAR");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);
        encrypted = true;

        if (strncmp(recvbuffer, "VAR", 3) != 0)
        {
            Log(LOG_LEVEL_INFO, "VAR protocol defect");
            RefuseAccess(conn, 0, "decryption failure");
            return false;
        }

        /* roll through, no break */

    case PROTOCOL_COMMAND_VAR:

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        if (!LiteralAccessControl(ctx, recvbuffer, conn, encrypted))
        {
            Log(LOG_LEVEL_INFO, "Literal access failure");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        GetServerLiteral(ctx, conn, sendbuffer, recvbuffer, encrypted);
        return true;

    case PROTOCOL_COMMAND_CONTEXT_SECURE:

        sscanf(recvbuffer, "SCONTEXT %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Decrypt error SCONTEXT, len,received = %d,%d", len, received);
            RefuseAccess(conn, 0, "decrypt error SCONTEXT");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);
        encrypted = true;

        if (strncmp(recvbuffer, "CONTEXT", 7) != 0)
        {
            Log(LOG_LEVEL_INFO, "CONTEXT protocol defect...");
            RefuseAccess(conn, 0, "Decryption failed?");
            return false;
        }

        /* roll through, no break */

    case PROTOCOL_COMMAND_CONTEXT:

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, "Context probe");
            return true;
        }

        if ((classes = ContextAccessControl(ctx, recvbuffer, conn, encrypted)) == NULL)
        {
            Log(LOG_LEVEL_INFO, "Context access failure on %s", recvbuffer);
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        ReplyServerContext(conn, encrypted, classes);
        return true;

    case PROTOCOL_COMMAND_QUERY_SECURE:

        sscanf(recvbuffer, "SQUERY %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Decrypt error SQUERY");
            RefuseAccess(conn, 0, "decrypt error SQUERY");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

        if (strncmp(recvbuffer, "QUERY", 5) != 0)
        {
            Log(LOG_LEVEL_INFO, "QUERY protocol defect");
            RefuseAccess(conn, 0, "decryption failure");
            return false;
        }

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        if (!LiteralAccessControl(ctx, recvbuffer, conn, true))
        {
            Log(LOG_LEVEL_INFO, "Query access failure");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (GetServerQuery(conn, recvbuffer))
        {
            return true;
        }

        break;
    case PROTOCOL_COMMAND_STARTTLS:
        /* TODO move out of ClassicProtocol protocol loop */
        if (ServerStartTLS(&conn->conn_info) < 0)
        {
            Log(LOG_LEVEL_ERR, "Could not start TLS session as requested by client");
            return false;
        }
        else
        {
            int ret;

            /* Send/Receive "CFE_v%d" version string and agree on version. */
            ret = ServerNegotiateProtocol(&conn->conn_info);
            if (ret <= 0)
            {
                return false;
            }

            /* Receive IDENTITY USER=asdf plain string. */
            ret = ServerIdentifyClient(&conn->conn_info, conn->username,
                                       sizeof(conn->username));
            if (ret != 1)
            {
                return false;
            }

            /* We *now* (maybe a bit late) verify the key that the client sent
             * us in the TLS handshake, since we need the username to do
             * so. TODO in the future store keys irrelevant of username, so
             * that we can match them before IDENTIFY. */
            ret = TLSVerifyPeer(&conn->conn_info, conn->ipaddr, conn->username);
            if (ret == -1)                                      /* error */
            {
                return false;
            }

            if (ret == 1)                                    /* trusted key */
            {
                Log(LOG_LEVEL_VERBOSE,
                    "%s: Client is TRUSTED, public key MATCHES stored one.",
                    conn->conn_info.remote_keyhash_str);
            }

            if (ret == 0)                                  /* untrusted key */
            {
                Log(LOG_LEVEL_WARNING,
                    "%s: Client's public key is UNKNOWN!",
                    conn->conn_info.remote_keyhash_str);

                if ((SV.trustkeylist != NULL) &&
                    (IsMatchItemIn(SV.trustkeylist, MapAddress(conn->ipaddr))))
                {
                    Log(LOG_LEVEL_VERBOSE,
                        "Host %s was found in the \"trustkeysfrom\" list",
                        conn->ipaddr);
                    Log(LOG_LEVEL_WARNING,
                        "%s: Explicitly trusting this key from now on.",
                        conn->conn_info.remote_keyhash_str);

                    conn->trust = true;
                    SavePublicKey("root", conn->conn_info.remote_keyhash_str,
                                  conn->conn_info.remote_key);
                }
                else
                {
                    Log(LOG_LEVEL_ERR, "TRUST FAILED, WARNING: possible MAN IN THE MIDDLE attack, dropping connection!");
                    Log(LOG_LEVEL_ERR, "Open server's ACL if you really want to start trusting this new key.");
                    return false;
                }
            }

            /* skipping CAUTH */
            conn->id_verified = 1;
            /* skipping SAUTH, allow access to read-only files */
            conn->rsa_auth = 1;
            LastSaw1(conn->ipaddr, conn->conn_info.remote_keyhash_str,
                     LAST_SEEN_ROLE_ACCEPT);

            ServerSendWelcome(conn);

            return true;
        }
        break;

    case PROTOCOL_COMMAND_CALL_ME_BACK:

        sscanf(recvbuffer, "SCALLBACK %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Decrypt error CALL_ME_BACK");
            RefuseAccess(conn, 0, "decrypt error CALL_ME_BACK");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

        if (strncmp(recvbuffer, "CALL_ME_BACK collect_calls", strlen("CALL_ME_BACK collect_calls")) != 0)
        {
            Log(LOG_LEVEL_INFO, "CALL_ME_BACK protocol defect");
            RefuseAccess(conn, 0, "decryption failure");
            return false;
        }

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        if (!LiteralAccessControl(ctx, recvbuffer, conn, true))
        {
            Log(LOG_LEVEL_INFO, "Query access failure");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (ReceiveCollectCall(conn))
        {
            return true;
        }

    case PROTOCOL_COMMAND_AUTH:
    case PROTOCOL_COMMAND_CONTEXTS:
    case PROTOCOL_COMMAND_BAD:
        ProgrammingError("Unexpected protocol command");
    }

    sprintf(sendbuffer, "BAD: Request denied\n");
    SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
    Log(LOG_LEVEL_INFO, "Closing connection, due to request: '%s'", recvbuffer);
    return false;
}

/**************************************************************/
/* Level 4                                                    */
/**************************************************************/

/******************************************************************/

/**************************************************************/
/*********************************************************************/

static int VerifyConnection(ServerConnectionState *conn, char buf[CF_BUFSIZE])
 /* Try reverse DNS lookup
    and RFC931 username lookup to check the authenticity. */
{
    char ipstring[CF_MAXVARSIZE], fqname[CF_MAXVARSIZE], username[CF_MAXVARSIZE];
    char dns_assert[CF_MAXVARSIZE], ip_assert[CF_MAXVARSIZE];
    int matched = false;

    Log(LOG_LEVEL_DEBUG, "Connecting host identifies itself as '%s'", buf);

    memset(ipstring, 0, CF_MAXVARSIZE);
    memset(fqname, 0, CF_MAXVARSIZE);
    memset(username, 0, CF_MAXVARSIZE);

    sscanf(buf, "%255s %255s %255s", ipstring, fqname, username);

    Log(LOG_LEVEL_DEBUG, "(ipstring=[%s],fqname=[%s],username=[%s],socket=[%s])",
            ipstring, fqname, username, conn->ipaddr);

    strlcpy(dns_assert, fqname, CF_MAXVARSIZE);
    ToLowerStrInplace(dns_assert);

    strncpy(ip_assert, ipstring, CF_MAXVARSIZE - 1);

/* It only makes sense to check DNS by reverse lookup if the key had to be
   accepted on trust. Once we have a positive key ID, the IP address is
   irrelevant fr authentication...
   We can save a lot of time by not looking this up ... */

    if ((conn->trust == false) ||
        (IsMatchItemIn(SV.skipverify, MapAddress(conn->ipaddr))))
    {
        Log(LOG_LEVEL_VERBOSE,
              "Allowing %s to connect without (re)checking ID\n", ip_assert);
        Log(LOG_LEVEL_VERBOSE,
              "Non-verified Host ID is %s (Using skipverify)\n", dns_assert);
        strncpy(conn->hostname, dns_assert, CF_MAXVARSIZE);
        Log(LOG_LEVEL_VERBOSE,
              "Non-verified User ID seems to be %s (Using skipverify)\n",
              username);
        strncpy(conn->username, username, CF_MAXVARSIZE);

#ifdef __MINGW32__            /* NT uses security identifier instead of uid */

        if (!NovaWin_UserNameToSid(username, (SID *) conn->sid,
                                   CF_MAXSIDSIZE, false))
        {
            memset(conn->sid, 0, CF_MAXSIDSIZE); /* is invalid sid - discarded */
        }

#else /* !__MINGW32__ */

        struct passwd *pw;
        if ((pw = getpwnam(username)) == NULL)    /* Keep this inside mutex */
        {
            conn->uid = -2;
        }
        else
        {
            conn->uid = pw->pw_uid;
        }

#endif /* !__MINGW32__ */
        return true;
    }

    if (strcmp(ip_assert, MapAddress(conn->ipaddr)) != 0)
    {
        Log(LOG_LEVEL_VERBOSE,
              "IP address mismatch between client's assertion (%s) "
              "and socket (%s) - untrustworthy connection\n",
              ip_assert, conn->ipaddr);
        return false;
    }

    if (strlen(dns_assert) == 0)
    {
        Log(LOG_LEVEL_VERBOSE,
              "DNS asserted name was empty - untrustworthy connection\n");
        return false;
    }

    if (strcmp(dns_assert, "skipident") == 0)
    {
        Log(LOG_LEVEL_VERBOSE,
              "DNS asserted name was withheld before key exchange"
              " - untrustworthy connection\n");
        return false;
    }

    Log(LOG_LEVEL_VERBOSE,
          "Socket caller address appears honest (%s matches %s)\n",
          ip_assert, MapAddress(conn->ipaddr));

    Log(LOG_LEVEL_VERBOSE, "Socket originates from %s=%s",
          ip_assert, dns_assert);

    Log(LOG_LEVEL_DEBUG, "Attempting to verify honesty by looking up hostname '%s'",
            dns_assert);

/* Do a reverse DNS lookup, like tcp wrappers to see if hostname matches IP */
    struct addrinfo *response, *ap;
    struct addrinfo query = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };
    int err;

    err = getaddrinfo(dns_assert, NULL, &query, &response);
    if (err != 0)
    {
        Log(LOG_LEVEL_ERR,
              "VerifyConnection: Unable to lookup (%s): %s",
              dns_assert, gai_strerror(err));
    }
    else
    {
        for (ap = response; ap != NULL; ap = ap->ai_next)
        {
            /* No lookup, just convert ai_addr to string. */
            char txtaddr[CF_MAX_IP_LEN] = "";
            getnameinfo(ap->ai_addr, ap->ai_addrlen,
                        txtaddr, sizeof(txtaddr),
                        NULL, 0, NI_NUMERICHOST);

            if (strcmp(MapAddress(conn->ipaddr), txtaddr) == 0)
            {
                Log(LOG_LEVEL_DEBUG, "Found match");
                matched = true;
            }
        }
        freeaddrinfo(response);
    }


# ifdef __MINGW32__                   /* NT uses security identifier instead of uid */
    if (!NovaWin_UserNameToSid(username, (SID *) conn->sid, CF_MAXSIDSIZE, false))
    {
        memset(conn->sid, 0, CF_MAXSIDSIZE);    /* is invalid sid - discarded */
    }

# else/* !__MINGW32__ */
    struct passwd *pw;
    if ((pw = getpwnam(username)) == NULL)      /* Keep this inside mutex */
    {
        conn->uid = -2;
    }
    else
    {
        conn->uid = pw->pw_uid;
    }
# endif/* !__MINGW32__ */

    if (!matched)
    {
        Log(LOG_LEVEL_INFO, "Failed on DNS reverse lookup of '%s'. (gethostbyname: %s)",
            dns_assert, GetErrorStr());
        Log(LOG_LEVEL_INFO, "Client sent: %s", buf);
        return false;
    }

    Log(LOG_LEVEL_VERBOSE, "Host ID is %s", dns_assert);
    strncpy(conn->hostname, dns_assert, CF_MAXVARSIZE - 1);

    Log(LOG_LEVEL_VERBOSE, "User ID seems to be %s", username);
    strncpy(conn->username, username, CF_MAXVARSIZE - 1);

    return true;
}

/**************************************************************/

/**************************************************************/

/**************************************************************/

/**************************************************************/


/**************************************************************/


/**************************************************************/

/**************************************************************/

static int AuthenticationDialogue(ServerConnectionState *conn, char *recvbuffer, int recvlen)
{
    char in[CF_BUFSIZE], *out, *decrypted_nonce;
    BIGNUM *counter_challenge = NULL;
    unsigned char digest[EVP_MAX_MD_SIZE + 1] = { 0 };
    unsigned int crypt_len, nonce_len = 0, encrypted_len = 0;
    char sauth[10], iscrypt = 'n', enterprise_field = 'c';
    int len_n = 0, len_e = 0, keylen, session_size;
    unsigned long err;
    RSA *newkey;
    int digestLen = 0;
    HashMethod digestType;

    if ((PRIVKEY == NULL) || (PUBKEY == NULL))
    {
        Log(LOG_LEVEL_ERR, "No public/private key pair exists, create one with cf-key");
        return false;
    }

    if (FIPS_MODE)
    {
        digestType = CF_DEFAULT_DIGEST;
        digestLen = CF_DEFAULT_DIGEST_LEN;
    }
    else
    {
        digestType = HASH_METHOD_MD5;
        digestLen = CF_MD5_LEN;
    }

/* proposition C1 */
/* Opening string is a challenge from the client (some agent) */

    sauth[0] = '\0';

    sscanf(recvbuffer, "%s %c %u %u %c", sauth, &iscrypt, &crypt_len, &nonce_len, &enterprise_field);

    if ((crypt_len == 0) || (nonce_len == 0) || (strlen(sauth) == 0))
    {
        Log(LOG_LEVEL_INFO, "Protocol format error in authentation from IP %s", conn->hostname);
        return false;
    }

    if (nonce_len > CF_NONCELEN * 2)
    {
        Log(LOG_LEVEL_INFO, "Protocol deviant authentication nonce from %s", conn->hostname);
        return false;
    }

    if (crypt_len > 2 * CF_NONCELEN)
    {
        Log(LOG_LEVEL_INFO, "Protocol abuse in unlikely cipher from %s", conn->hostname);
        return false;
    }

/* Check there is no attempt to read past the end of the received input */

    if (recvbuffer + CF_RSA_PROTO_OFFSET + nonce_len > recvbuffer + recvlen)
    {
        Log(LOG_LEVEL_INFO, "Protocol consistency error in authentication from %s", conn->hostname);
        return false;
    }

    if ((strcmp(sauth, "SAUTH") != 0) || (nonce_len == 0) || (crypt_len == 0))
    {
        Log(LOG_LEVEL_INFO, "Protocol error in RSA authentication from IP '%s'", conn->hostname);
        return false;
    }

    Log(LOG_LEVEL_DEBUG, "Challenge encryption = %c, nonce = %d, buf = %d", iscrypt, nonce_len, crypt_len);


    decrypted_nonce = xmalloc(crypt_len);

    if (iscrypt == 'y')
    {
        if (RSA_private_decrypt
            (crypt_len, recvbuffer + CF_RSA_PROTO_OFFSET, decrypted_nonce, PRIVKEY, RSA_PKCS1_PADDING) <= 0)
        {
            err = ERR_get_error();

            Log(LOG_LEVEL_ERR,
                "Private decrypt failed = '%s'. Probably the client has the wrong public key for this server",
                ERR_reason_error_string(err));
            free(decrypted_nonce);
            return false;
        }
    }
    else
    {
        if (nonce_len > crypt_len)
        {
            Log(LOG_LEVEL_ERR, "Illegal challenge");
            free(decrypted_nonce);
            return false;
        }

        memcpy(decrypted_nonce, recvbuffer + CF_RSA_PROTO_OFFSET, nonce_len);
    }

/* Client's ID is now established by key or trusted, reply with digest */

    HashString(decrypted_nonce, nonce_len, digest, digestType);

    free(decrypted_nonce);

/* Get the public key from the client */
    newkey = RSA_new();

/* proposition C2 */
    if ((len_n = ReceiveTransaction(&conn->conn_info, recvbuffer, NULL)) == -1)
    {
        Log(LOG_LEVEL_INFO, "Protocol error 1 in RSA authentation from IP %s", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if (len_n == 0)
    {
        Log(LOG_LEVEL_INFO, "Protocol error 2 in RSA authentation from IP %s", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if ((newkey->n = BN_mpi2bn(recvbuffer, len_n, NULL)) == NULL)
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Private decrypt failed = %s", ERR_reason_error_string(err));
        RSA_free(newkey);
        return false;
    }

/* proposition C3 */

    if ((len_e = ReceiveTransaction(&conn->conn_info, recvbuffer, NULL)) == -1)
    {
        Log(LOG_LEVEL_INFO, "Protocol error 3 in RSA authentation from IP %s", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if (len_e == 0)
    {
        Log(LOG_LEVEL_INFO, "Protocol error 4 in RSA authentation from IP %s", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if ((newkey->e = BN_mpi2bn(recvbuffer, len_e, NULL)) == NULL)
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Private decrypt failed = %s", ERR_reason_error_string(err));
        RSA_free(newkey);
        return false;
    }

    /* Compute and store hash of the client's public key. */
    HashPubKey(newkey, conn->conn_info.remote_keyhash, CF_DEFAULT_DIGEST);
    HashPrintSafe(CF_DEFAULT_DIGEST, conn->conn_info.remote_keyhash,
                  conn->conn_info.remote_keyhash_str);

    Log(LOG_LEVEL_VERBOSE, "Public key identity of host '%s' is '%s'",
        conn->ipaddr, conn->conn_info.remote_keyhash_str);

    LastSaw1(conn->ipaddr, conn->conn_info.remote_keyhash_str,
             LAST_SEEN_ROLE_ACCEPT);

    if (!CheckStoreKey(conn, newkey))   /* conceals proposition S1 */
    {
        if (!conn->trust)
        {
            RSA_free(newkey);
            return false;
        }
    }

/* Reply with digest of original challenge */

/* proposition S2 */

    SendTransaction(&conn->conn_info, digest, digestLen, CF_DONE);

/* Send counter challenge to be sure this is a live session */

    counter_challenge = BN_new();
    if (counter_challenge == NULL)
    {
        Log(LOG_LEVEL_ERR, "Cannot allocate BIGNUM structure for counter challenge");
        RSA_free(newkey);
        return false;
    }

    BN_rand(counter_challenge, CF_NONCELEN, 0, 0);
    nonce_len = BN_bn2mpi(counter_challenge, in);

// hash the challenge from the client

    HashString(in, nonce_len, digest, digestType);

    encrypted_len = RSA_size(newkey);   /* encryption buffer is always the same size as n */

    out = xmalloc(encrypted_len + 1);

    if (RSA_public_encrypt(nonce_len, in, out, newkey, RSA_PKCS1_PADDING) <= 0)
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Public encryption failed = %s", ERR_reason_error_string(err));
        RSA_free(newkey);
        free(out);
        return false;
    }

/* proposition S3 */
    SendTransaction(&conn->conn_info, out, encrypted_len, CF_DONE);

/* if the client doesn't have our public key, send it */

    if (iscrypt != 'y')
    {
        /* proposition S4  - conditional */
        memset(in, 0, CF_BUFSIZE);
        len_n = BN_bn2mpi(PUBKEY->n, in);
        SendTransaction(&conn->conn_info, in, len_n, CF_DONE);

        /* proposition S5  - conditional */
        memset(in, 0, CF_BUFSIZE);
        len_e = BN_bn2mpi(PUBKEY->e, in);
        SendTransaction(&conn->conn_info, in, len_e, CF_DONE);
    }

/* Receive reply to counter_challenge */

/* proposition C4 */
    memset(in, 0, CF_BUFSIZE);

    if (ReceiveTransaction(&conn->conn_info, in, NULL) == -1)
    {
        BN_free(counter_challenge);
        free(out);
        RSA_free(newkey);
        return false;
    }

    if (HashesMatch(digest, in, digestType))    /* replay / piggy in the middle attack ? */
    {
        if (!conn->trust)
        {
            Log(LOG_LEVEL_VERBOSE, "Strong authentication of client %s/%s achieved", conn->hostname, conn->ipaddr);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Weak authentication of trusted client %s/%s (key accepted on trust).",
                  conn->hostname, conn->ipaddr);
        }
    }
    else
    {
        BN_free(counter_challenge);
        free(out);
        RSA_free(newkey);
        Log(LOG_LEVEL_INFO, "Challenge response from client %s was incorrect - ID false?", conn->ipaddr);
        return false;
    }

/* Receive random session key,... */

/* proposition C5 */

    memset(in, 0, CF_BUFSIZE);

    if ((keylen = ReceiveTransaction(&conn->conn_info, in, NULL)) == -1)
    {
        BN_free(counter_challenge);
        free(out);
        RSA_free(newkey);
        return false;
    }

    if (keylen > CF_BUFSIZE / 2)
    {
        BN_free(counter_challenge);
        free(out);
        RSA_free(newkey);
        Log(LOG_LEVEL_INFO, "Session key length received from %s is too long", conn->ipaddr);
        return false;
    }

    session_size = CfSessionKeySize(enterprise_field);
    conn->session_key = xmalloc(session_size);
    conn->encryption_type = enterprise_field;

    Log(LOG_LEVEL_VERBOSE, "Receiving session key from client (size=%d)...", keylen);

    Log(LOG_LEVEL_DEBUG, "keylen = %d, session_size = %d", keylen, session_size);

    if (keylen == CF_BLOWFISHSIZE)      /* Support the old non-ecnrypted for upgrade */
    {
        memcpy(conn->session_key, in, session_size);
    }
    else
    {
        /* New protocol encrypted */

        if (RSA_private_decrypt(keylen, in, out, PRIVKEY, RSA_PKCS1_PADDING) <= 0)
        {
            err = ERR_get_error();
            Log(LOG_LEVEL_ERR, "Private decrypt failed = %s", ERR_reason_error_string(err));
            return false;
        }

        memcpy(conn->session_key, out, session_size);
    }

    BN_free(counter_challenge);
    free(out);
    RSA_free(newkey);
    conn->rsa_auth = true;
    return true;
}

void DeleteAuthList(Auth *ap)
{
    if (ap != NULL)
    {
        DeleteAuthList(ap->next);
        ap->next = NULL;

        DeleteItemList(ap->accesslist);
        DeleteItemList(ap->maproot);
        free(ap->path);
        free((char *) ap);
    }
}

/***************************************************************/
/* Level 5                                                     */
/***************************************************************/

/***************************************************************/

/***************************************************************/

/***************************************************************/

static int CheckStoreKey(ServerConnectionState *conn, RSA *key)
{
    RSA *savedkey;
    char udigest[CF_MAXVARSIZE];

    snprintf(udigest, CF_MAXVARSIZE - 1, "%s",
             conn->conn_info.remote_keyhash_str);

    if ((savedkey = HavePublicKey(conn->username, MapAddress(conn->ipaddr), udigest)))
    {
        Log(LOG_LEVEL_VERBOSE, "A public key was already known from %s/%s - no trust required", conn->hostname,
              conn->ipaddr);

        Log(LOG_LEVEL_VERBOSE, "Adding IP %s to SkipVerify - no need to check this if we have a key", conn->ipaddr);
        IdempPrependItem(&SV.skipverify, MapAddress(conn->ipaddr), NULL);

        if ((BN_cmp(savedkey->e, key->e) == 0) && (BN_cmp(savedkey->n, key->n) == 0))
        {
            Log(LOG_LEVEL_VERBOSE, "The public key identity was confirmed as %s@%s", conn->username, conn->hostname);
            SendTransaction(&conn->conn_info, "OK: key accepted", 0, CF_DONE);
            RSA_free(savedkey);
            return true;
        }
    }

    /* Finally, if we're still here then the key is new (not in ppkeys
     * directory): Allow access only if host is listed in "trustkeysfrom" body
     * server control option. */

    if ((SV.trustkeylist != NULL) && (IsMatchItemIn(SV.trustkeylist, MapAddress(conn->ipaddr))))
    {
        Log(LOG_LEVEL_VERBOSE, "Host %s/%s was found in the list of hosts to trust", conn->hostname, conn->ipaddr);
        conn->trust = true;
        SendTransaction(&conn->conn_info, "OK: unknown key was accepted on trust", 0, CF_DONE);
        SavePublicKey(conn->username, udigest, key);
        return true;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "No previous key found, and unable to accept this one on trust");
        SendTransaction(&conn->conn_info, "BAD: key could not be accepted on trust", 0, CF_DONE);
        return false;
    }
}

/***************************************************************/
/* Toolkit/Class: conn                                         */
/***************************************************************/

static ServerConnectionState *NewConn(EvalContext *ctx, int sd)
{
    ServerConnectionState *conn;
    struct sockaddr addr;
    socklen_t size = sizeof(addr);

    if (getsockname(sd, &addr, &size) == -1)
    {
       return NULL;
    }

    conn = xcalloc(1, sizeof(*conn));

    conn->ctx = ctx;
    conn->conn_info.type = CF_PROTOCOL_CLASSIC;
    conn->conn_info.sd = sd;
    conn->conn_info.ssl = NULL;
    conn->conn_info.remote_key = NULL;
    conn->id_verified = false;
    conn->rsa_auth = false;
    conn->trust = false;
    conn->hostname[0] = '\0';
    conn->ipaddr[0] = '\0';
    conn->username[0] = '\0';
    conn->session_key = NULL;
    conn->encryption_type = 'c';
    conn->maproot = false;      /* Only public files (chmod o+r) accessible */

    Log(LOG_LEVEL_DEBUG, "New socket %d", sd);

    return conn;
}

/***************************************************************/

static void DeleteConn(ServerConnectionState *conn)
{
    /* Sockets should have already been closed by the client, so we are just
     * making sure here in case an error occured. */
    switch(conn->conn_info.type)
    {
    case CF_PROTOCOL_CLASSIC:
        cf_closesocket(conn->conn_info.sd);
        break;
    case CF_PROTOCOL_TLS:
        SSL_shutdown(conn->conn_info.ssl);
        SSL_free(conn->conn_info.ssl);
        cf_closesocket(conn->conn_info.sd);
        break;
    default:
        UnexpectedError("DeleteConn: ProtocolVersion %d!",
                        conn->conn_info.type);
    }

    free(conn->session_key);

    if (conn->conn_info.remote_key != NULL)
    {
        RSA_free(conn->conn_info.remote_key);
    }

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
