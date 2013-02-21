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

#include "server.h"

#include "item_lib.h"
#include "crypto.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "env_context.h"
#include "lastseen.h"
#include "dir.h"
#include "conversion.h"
#include "matching.h"
#include "cfstream.h"
#include "string_lib.h"
#include "pipes.h"
#include "signals.h"
#include "transaction.h"
#include "logging.h"
#include "net.h"
#include "rlist.h"
#include "misc_lib.h"

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
    PROTOCOL_COMMAND_BAD
} ProtocolCommand;

//******************************************************************
// GLOBAL STATE
//******************************************************************

int CLOCK_DRIFT = 3600;  /* 1hr */
int ACTIVE_THREADS;

int CFD_MAXPROCESSES = 0;
int CFD_INTERVAL = 0;
int DENYBADCLOCKS = true;

int MAXTRIES = 5;
int LOGCONNS = false;
int LOGENCRYPT = false;
int COLLECT_INTERVAL = 0;
int COLLECT_WINDOW = 10;
bool SERVER_LISTEN = true;

Auth *ROLES = NULL;
Auth *ROLESTOP = NULL;

ServerAccess SV;

Auth *VADMIT = NULL;
Auth *VADMITTOP = NULL;
Auth *VDENY = NULL;
Auth *VDENYTOP = NULL;

Auth *VARADMIT = NULL;
Auth *VARADMITTOP = NULL;
Auth *VARDENY = NULL;
Auth *VARDENYTOP = NULL;

char CFRUNCOMMAND[CF_BUFSIZE] = { 0 };

//******************************************************************/
// LOCAL CONSTANTS
//******************************************************************/

static const size_t CF_BUFEXT = 128;
static const int CF_NOSIZE = -1;

static void SpawnConnection(int sd_reply, char *ipaddr);
static void *HandleConnection(ServerConnectionState *conn);
static int BusyWithConnection(ServerConnectionState *conn);
static int MatchClasses(ServerConnectionState *conn);
static void DoExec(ServerConnectionState *conn, char *sendbuffer, char *args);
static ProtocolCommand GetCommand(char *str);
static int VerifyConnection(ServerConnectionState *conn, char buf[CF_BUFSIZE]);
static void RefuseAccess(ServerConnectionState *conn, char *sendbuffer, int size, char *errmesg);
static int AccessControl(const char *req_path, ServerConnectionState *conn, int encrypt, Auth *vadmit, Auth *vdeny);
static int LiteralAccessControl(char *in, ServerConnectionState *conn, int encrypt, Auth *vadmit, Auth *vdeny);
static Item *ContextAccessControl(char *in, ServerConnectionState *conn, int encrypt, Auth *vadmit, Auth *vdeny);
static void ReplyServerContext(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer, int encrypted, Item *classes);
static int CheckStoreKey(ServerConnectionState *conn, RSA *key);
static int StatFile(ServerConnectionState *conn, char *sendbuffer, char *ofilename);
static void CfGetFile(ServerFileGetState *args);
static void CfEncryptGetFile(ServerFileGetState *args);
static void CompareLocalHash(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer);
static void GetServerLiteral(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer, int encrypted);
static int GetServerQuery(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer);
static int CfOpenDirectory(ServerConnectionState *conn, char *sendbuffer, char *oldDirname);
static int CfSecOpenDirectory(ServerConnectionState *conn, char *sendbuffer, char *dirname);
static void Terminate(int sd);
static int AllowedUser(char *user);
static int AuthorizeRoles(ServerConnectionState *conn, char *args);
static int TransferRights(char *filename, int sd, ServerFileGetState *args, char *sendbuffer, struct stat *sb);
static void AbortTransfer(int sd, char *sendbuffer, char *filename);
static void FailedTransfer(int sd, char *sendbuffer, char *filename);
static void ReplyNothing(ServerConnectionState *conn);
static ServerConnectionState *NewConn(int sd);
static void DeleteConn(ServerConnectionState *conn);
static int cfscanf(char *in, int len1, int len2, char *out1, char *out2, char *out3);
static int AuthenticationDialogue(ServerConnectionState *conn, char *recvbuffer, int recvlen);
static int SafeOpen(char *filename);
static int OptionFound(char *args, char *pos, char *word);

//******************************************************************/
// LOCAL STATE
//******************************************************************/

static const char *PROTOCOL[] =
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
    NULL
};

static int TRIES = 0;

/*******************************************************************/

void ServerEntryPoint(int sd_reply, char *ipaddr, ServerAccess sv)

{
    char intime[64];
    time_t now;
    
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Obtained IP address of %s on socket %d from accept\n", ipaddr, sd_reply);
    
    if ((sv.nonattackerlist) && (!IsMatchItemIn(sv.nonattackerlist, MapAddress(ipaddr))))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Not allowing connection from non-authorized IP %s\n", ipaddr);
        cf_closesocket(sd_reply);
        return;
    }
    
    if (IsMatchItemIn(sv.attackerlist, MapAddress(ipaddr)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Denying connection from non-authorized IP %s\n", ipaddr);
        cf_closesocket(sd_reply);
        return;
    }
    
    if ((now = time((time_t *) NULL)) == -1)
       {
       now = 0;
       }
    
    PurgeOldConnections(&sv.connectionlist, now);
    
    if (!IsMatchItemIn(sv.multiconnlist, MapAddress(ipaddr)))
    {
        if (!ThreadLock(cft_count))
        {
            return;
        }

        if (IsItemIn(sv.connectionlist, MapAddress(ipaddr)))
        {
            ThreadUnlock(cft_count);
            CfOut(OUTPUT_LEVEL_ERROR, "", "Denying repeated connection from \"%s\"\n", ipaddr);
            cf_closesocket(sd_reply);
            return;
        }

        ThreadUnlock(cft_count);
    }
    
    if (sv.logconns)
    {
        CfOut(OUTPUT_LEVEL_LOG, "", "Accepting connection from \"%s\"\n", ipaddr);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Accepting connection from \"%s\"\n", ipaddr);
    }
    
    snprintf(intime, 63, "%d", (int) now);
    
    if (!ThreadLock(cft_count))
    {
        return;
    }
    
    PrependItem(&sv.connectionlist, MapAddress(ipaddr), intime);
    
    if (!ThreadUnlock(cft_count))
    {
       return;
    }
    
    SpawnConnection(sd_reply, ipaddr);
    
}

/**********************************************************************/

void PurgeOldConnections(Item **list, time_t now)
   /* Some connections might not terminate properly. These should be cleaned
      every couple of hours. That should be enough to prevent spamming. */
{
    Item *ip;
    int then = 0;

    CfDebug("Purging Old Connections...\n");

    if (!ThreadLock(cft_count))
    {
        return;
    }

    if (list == NULL)
    {
        return;
    }

    Item *next;

    for (ip = *list; ip != NULL; ip = next)
    {
        sscanf(ip->classes, "%d", &then);

        next = ip->next;

        if (now > then + 7200)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Purging IP address %s from connection list\n", ip->name);
            DeleteItem(list, ip);
        }
    }

    if (!ThreadUnlock(cft_count))
    {
        return;
    }

    CfDebug("Done purging\n");
}

/*********************************************************************/

static void SpawnConnection(int sd_reply, char *ipaddr)
{
    ServerConnectionState *conn;

    pthread_t tid;
    pthread_attr_t threadattrs;

    if ((conn = NewConn(sd_reply)) == NULL)
    {
        return;
    }

    strncpy(conn->ipaddr, ipaddr, CF_MAX_IP_LEN - 1);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "New connection...(from %s:sd %d)\n", conn->ipaddr, sd_reply);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Spawning new thread...\n");

    pthread_attr_init(&threadattrs);
    pthread_attr_setdetachstate(&threadattrs, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&threadattrs, (size_t) 1024 * 1024);

    int ret = pthread_create(&tid, &threadattrs, (void *) HandleConnection, (void *) conn);
    if (ret != 0)
    {
        errno = ret;
        CfOut(OUTPUT_LEVEL_ERROR, "pthread_create", "Unable to spawn worker thread");
        HandleConnection(conn);
    }

    pthread_attr_destroy(&threadattrs);
}

/*********************************************************************/

void DisableSendDelays(int sockfd)
{
    int yes = 1;

    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *) &yes, sizeof(yes)) == -1)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "setsockopt(TCP_NODELAY)", "Unable to disable Nagle algorithm, expect performance problems");
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
            CfOut(OUTPUT_LEVEL_ERROR, "", "Server seems to be paralyzed. DOS attack? Committing apoptosis...");
            FatalError("Terminating");
        }

        if (!ThreadUnlock(cft_server_children))
        {
        }

        CfOut(OUTPUT_LEVEL_ERROR, "", "Too many threads (>=%d) -- increase server maxconnections?", CFD_MAXPROCESSES);
        snprintf(output, CF_BUFSIZE, "BAD: Server is currently too busy -- increase maxconnections or splaytime?");
        SendTransaction(conn->sd_reply, output, 0, CF_DONE);
        DeleteConn(conn);
        return NULL;
    }
    else
    {
        ThreadUnlock(cft_server_children);
    }

    TRIES = 0;                  /* As long as there is activity, we're not stuck */

    DisableSendDelays(conn->sd_reply);

    struct timeval tv = {
        .tv_sec = CONNTIMEOUT,
    };

    SetReceiveTimeout(conn->sd_reply, &tv);

    while (BusyWithConnection(conn))
    {
    }

    CfOut(OUTPUT_LEVEL_VERBOSE,"", "Terminating thread...\n");

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

static int BusyWithConnection(ServerConnectionState *conn)
  /* This is the protocol section. Here we must   */
  /* check that the incoming data are sensible    */
  /* and extract the information from the message */
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

    if ((received = ReceiveTransaction(conn->sd_reply, recvbuffer, NULL)) == -1)
    {
        return false;
    }

    if (strlen(recvbuffer) == 0)
    {
        CfDebug("cf-serverd terminating NULL transmission!\n");
        return false;
    }

    CfDebug(" * Received: [%s] on socket %d\n", recvbuffer, conn->sd_reply);

    switch (GetCommand(recvbuffer))
    {
    case PROTOCOL_COMMAND_EXEC:
        memset(args, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "EXEC %255[^\n]", args);

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Server refusal due to incorrect identity\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (!AllowedUser(conn->username))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Server refusal due to non-allowed user\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (!conn->rsa_auth)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Server refusal due to no RSA authentication\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (!AccessControl(CommandArg0(CFRUNCOMMAND), conn, false, VADMIT, VDENY))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Server refusal due to denied access to requested object\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (!MatchClasses(conn))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Server refusal due to failed class/context match\n");
            Terminate(conn->sd_reply);
            return false;
        }

        DoExec(conn, sendbuffer, args);
        Terminate(conn->sd_reply);
        return false;

    case PROTOCOL_COMMAND_VERSION:

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
        }

        snprintf(conn->output, CF_BUFSIZE, "OK: %s", Version());
        SendTransaction(conn->sd_reply, conn->output, 0, CF_DONE);
        return conn->id_verified;

    case PROTOCOL_COMMAND_AUTH_CLEAR:

        conn->id_verified = VerifyConnection(conn, (char *) (recvbuffer + strlen("CAUTH ")));

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
        }

        return conn->id_verified;       /* are we finished yet ? */

    case PROTOCOL_COMMAND_AUTH_SECURE:            /* This is where key agreement takes place */

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (!AuthenticationDialogue(conn, recvbuffer, received))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Auth dialogue error\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        return true;

    case PROTOCOL_COMMAND_GET:

        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "GET %d %[^\n]", &(get_args.buf_size), filename);

        if ((get_args.buf_size < 0) || (get_args.buf_size > CF_BUFSIZE))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "GET buffer out of bounds\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (!AccessControl(filename, conn, false, VADMIT, VDENY))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Access denied to get object\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
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
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Protocol error SGET\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        plainlen = DecryptString(conn->encryption_type, recvbuffer + CF_PROTO_OFFSET, buffer, conn->session_key, len);

        cfscanf(buffer, strlen("GET"), strlen("dummykey"), check, sendbuffer, filename);

        if (strcmp(check, "GET") != 0)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "SGET/GET problem\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return true;
        }

        if ((get_args.buf_size < 0) || (get_args.buf_size > 8192))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "SGET bounding error\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (get_args.buf_size >= CF_BUFSIZE)
        {
            get_args.buf_size = 2048;
        }

        CfDebug("Confirm decryption, and thus validity of caller\n");
        CfDebug("SGET %s with blocksize %d\n", filename, get_args.buf_size);

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (!AccessControl(filename, conn, true, VADMIT, VDENY))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Access control error\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
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
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Protocol error OPENDIR: %d\n", len);
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (conn->session_key == NULL)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "No session key\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);

        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

        if (strncmp(recvbuffer, "OPENDIR", 7) != 0)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Opendir failed to decrypt\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return true;
        }

        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "OPENDIR %[^\n]", filename);

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (!AccessControl(filename, conn, true, VADMIT, VDENY))        /* opendir don't care about privacy */
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Access error\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        CfSecOpenDirectory(conn, sendbuffer, filename);
        return true;

    case PROTOCOL_COMMAND_OPENDIR:

        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "OPENDIR %[^\n]", filename);

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (!AccessControl(filename, conn, true, VADMIT, VDENY))        /* opendir don't care about privacy */
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "DIR access error\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        CfOpenDirectory(conn, sendbuffer, filename);
        return true;

    case PROTOCOL_COMMAND_SYNC_SECURE:

        memset(buffer, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "SSYNCH %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Protocol error SSYNCH: %d\n", len);
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (conn->session_key == NULL)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Bad session key\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);

        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

        if (plainlen < 0)
        {
            DebugBinOut((char *) conn->session_key, 32, "Session key");
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Bad decrypt (%d)", len);
        }

        if (strncmp(recvbuffer, "SYNCH", 5) != 0)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "No synch\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return true;
        }

        /* roll through, no break */

    case PROTOCOL_COMMAND_SYNC:

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
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
            CfOut(OUTPUT_LEVEL_INFORM, "time", "%s", conn->output);
            SendTransaction(conn->sd_reply, "BAD: clocks out of synch", 0, CF_DONE);
            return true;
        }

        drift = (int) (tloc - trem);

        if (!AccessControl(filename, conn, true, VADMIT, VDENY))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Access control in sync\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return true;
        }

        if (DENYBADCLOCKS && (drift * drift > CLOCK_DRIFT * CLOCK_DRIFT))
        {
            snprintf(conn->output, CF_BUFSIZE - 1, "BAD: Clocks are too far unsynchronized %ld/%ld\n", (long) tloc,
                     (long) trem);
            SendTransaction(conn->sd_reply, conn->output, 0, CF_DONE);
            return true;
        }
        else
        {
            CfDebug("Clocks were off by %ld\n", (long) tloc - (long) trem);
            StatFile(conn, sendbuffer, filename);
        }

        return true;

    case PROTOCOL_COMMAND_MD5_SECURE:

        sscanf(recvbuffer, "SMD5 %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Decryption error\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

        if (strncmp(recvbuffer, "MD5", 3) != 0)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "MD5 protocol error\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        /* roll through, no break */

    case PROTOCOL_COMMAND_MD5:

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return true;
        }

        CompareLocalHash(conn, sendbuffer, recvbuffer);
        return true;

    case PROTOCOL_COMMAND_VAR_SECURE:

        sscanf(recvbuffer, "SVAR %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Decrypt error SVAR\n");
            RefuseAccess(conn, sendbuffer, 0, "decrypt error SVAR");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);
        encrypted = true;

        if (strncmp(recvbuffer, "VAR", 3) != 0)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "VAR protocol defect\n");
            RefuseAccess(conn, sendbuffer, 0, "decryption failure");
            return false;
        }

        /* roll through, no break */

    case PROTOCOL_COMMAND_VAR:

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return true;
        }

        if (!LiteralAccessControl(recvbuffer, conn, encrypted, VARADMIT, VARDENY))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Literal access failure\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        GetServerLiteral(conn, sendbuffer, recvbuffer, encrypted);
        return true;

    case PROTOCOL_COMMAND_CONTEXT_SECURE:

        sscanf(recvbuffer, "SCONTEXT %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Decrypt error SCONTEXT, len,received = %d,%d\n", len, received);
            RefuseAccess(conn, sendbuffer, 0, "decrypt error SCONTEXT");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);
        encrypted = true;

        if (strncmp(recvbuffer, "CONTEXT", 7) != 0)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "CONTEXT protocol defect...\n");
            RefuseAccess(conn, sendbuffer, 0, "Decryption failed?");
            return false;
        }

        /* roll through, no break */

    case PROTOCOL_COMMAND_CONTEXT:

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, "Context probe");
            return true;
        }

        if ((classes = ContextAccessControl(recvbuffer, conn, encrypted, VARADMIT, VARDENY)) == NULL)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Context access failure on %s\n", recvbuffer);
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        ReplyServerContext(conn, sendbuffer, recvbuffer, encrypted, classes);
        return true;

    case PROTOCOL_COMMAND_QUERY_SECURE:

        sscanf(recvbuffer, "SQUERY %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Decrypt error SQUERY\n");
            RefuseAccess(conn, sendbuffer, 0, "decrypt error SQUERY");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

        if (strncmp(recvbuffer, "QUERY", 5) != 0)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "QUERY protocol defect\n");
            RefuseAccess(conn, sendbuffer, 0, "decryption failure");
            return false;
        }

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return true;
        }

        if (!LiteralAccessControl(recvbuffer, conn, true, VARADMIT, VARDENY))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Query access failure\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }

        if (GetServerQuery(conn, sendbuffer, recvbuffer))
        {
            return true;
        }

        break;

    case PROTOCOL_COMMAND_CALL_ME_BACK:

        sscanf(recvbuffer, "SCALLBACK %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Decrypt error CALL_ME_BACK\n");
            RefuseAccess(conn, sendbuffer, 0, "decrypt error CALL_ME_BACK");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);
        
        if (strncmp(recvbuffer, "CALL_ME_BACK collect_calls", strlen("CALL_ME_BACK collect_calls")) != 0)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "CALL_ME_BACK protocol defect\n");
            RefuseAccess(conn, sendbuffer, 0, "decryption failure");
            return false;
        }

        if (!conn->id_verified)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "ID not verified\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return true;
        }

        if (!LiteralAccessControl(recvbuffer, conn, true, VARADMIT, VARDENY))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Query access failure\n");
            RefuseAccess(conn, sendbuffer, 0, recvbuffer);
            return false;
        }
        
        if (ReceiveCollectCall(conn, sendbuffer))
        {
            return true;
        }

    case PROTOCOL_COMMAND_AUTH:
    case PROTOCOL_COMMAND_CONTEXTS:
    case PROTOCOL_COMMAND_BAD:
        ProgrammingError("Unexpected protocol command");
    }

    sprintf(sendbuffer, "BAD: Request denied\n");
    SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
    CfOut(OUTPUT_LEVEL_INFORM, "", "Closing connection, due to request: \"%s\"\n", recvbuffer);
    return false;
}

/**************************************************************/
/* Level 4                                                    */
/**************************************************************/

static int MatchClasses(ServerConnectionState *conn)
{
    char recvbuffer[CF_BUFSIZE];
    Item *classlist = NULL, *ip;
    int count = 0;

    CfDebug("Match classes\n");

    while (true && (count < 10))        /* arbitrary check to avoid infinite loop, DoS attack */
    {
        count++;

        if (ReceiveTransaction(conn->sd_reply, recvbuffer, NULL) == -1)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "ReceiveTransaction", "Unable to read data from network");
            return false;
        }

        CfDebug("Got class buffer %s\n", recvbuffer);

        if (strncmp(recvbuffer, CFD_TERMINATOR, strlen(CFD_TERMINATOR)) == 0)
        {
            if (count == 1)
            {
                CfDebug("No classes were sent, assuming no restrictions...\n");
                return true;
            }

            break;
        }

        classlist = SplitStringAsItemList(recvbuffer, ' ');

        for (ip = classlist; ip != NULL; ip = ip->next)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking whether class %s can be identified as me...\n", ip->name);

            if (IsDefinedClass(ip->name, NULL))
            {
                CfDebug("Class %s matched, accepting...\n", ip->name);
                DeleteItemList(classlist);
                return true;
            }

            if (MatchInAlphaList(&VHEAP, ip->name))
            {
                CfDebug("Class matched regular expression %s, accepting...\n", ip->name);
                DeleteItemList(classlist);
                return true;
            }

            if (MatchInAlphaList(&VHARDHEAP, ip->name))
            {
                CfDebug("Class matched regular expression %s, accepting...\n", ip->name);
                DeleteItemList(classlist);
                return true;
            }

            if (strncmp(ip->name, CFD_TERMINATOR, strlen(CFD_TERMINATOR)) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "No classes matched, rejecting....\n");
                ReplyNothing(conn);
                DeleteItemList(classlist);
                return false;
            }
        }
    }

    ReplyNothing(conn);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "No classes matched, rejecting....\n");
    DeleteItemList(classlist);
    return false;
}

/******************************************************************/

static void DoExec(ServerConnectionState *conn, char *sendbuffer, char *args)
{
    char ebuff[CF_EXPANDSIZE], line[CF_BUFSIZE], *sp;
    int print = false, i;
    FILE *pp;

    if ((CFSTARTTIME = time((time_t *) NULL)) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "time", "Couldn't read system clock\n");
    }

    if (strlen(CFRUNCOMMAND) == 0)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "cf-serverd exec request: no cfruncommand defined\n");
        sprintf(sendbuffer, "Exec request: no cfruncommand defined\n");
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
        return;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Examining command string: %s\n", args);

    for (sp = args; *sp != '\0'; sp++)  /* Blank out -K -f */
    {
        if ((*sp == ';') || (*sp == '&') || (*sp == '|'))
        {
            sprintf(sendbuffer, "You are not authorized to activate these classes/roles on host %s\n", VFQNAME);
            SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
            return;
        }

        if ((OptionFound(args, sp, "-K")) || (OptionFound(args, sp, "-f")))
        {
            *sp = ' ';
            *(sp + 1) = ' ';
        }
        else if (OptionFound(args, sp, "--no-lock"))
        {
            for (i = 0; i < strlen("--no-lock"); i++)
            {
                *(sp + i) = ' ';
            }
        }
        else if (OptionFound(args, sp, "--file"))
        {
            for (i = 0; i < strlen("--file"); i++)
            {
                *(sp + i) = ' ';
            }
        }
        else if ((OptionFound(args, sp, "--define")) || (OptionFound(args, sp, "-D")))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Attempt to activate a predefined role..\n");

            if (!AuthorizeRoles(conn, sp))
            {
                sprintf(sendbuffer, "You are not authorized to activate these classes/roles on host %s\n", VFQNAME);
                SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
                return;
            }
        }
    }

    snprintf(ebuff, CF_BUFSIZE, "%s --inform", CFRUNCOMMAND);

    if (strlen(ebuff) + strlen(args) + 6 > CF_BUFSIZE)
    {
        snprintf(sendbuffer, CF_BUFSIZE, "Command line too long with args: %s\n", ebuff);
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
        return;
    }
    else
    {
        if ((args != NULL) && (strlen(args) > 0))
        {
            strcat(ebuff, " ");
            strncat(ebuff, args, CF_BUFSIZE - strlen(ebuff));
            snprintf(sendbuffer, CF_BUFSIZE, "cf-serverd Executing %s\n", ebuff);
            SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
        }
    }

    CfOut(OUTPUT_LEVEL_INFORM, "", "Executing command %s\n", ebuff);

    if ((pp = cf_popen_sh(ebuff, "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "pipe", "Couldn't open pipe to command %s\n", ebuff);
        snprintf(sendbuffer, CF_BUFSIZE, "Unable to run %s\n", ebuff);
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
        return;
    }

    while (!feof(pp))
    {
        if (ferror(pp))
        {
            fflush(pp);
            break;
        }

        if (CfReadLine(line, CF_BUFSIZE, pp) == -1)
        {
            FatalError("Error in CfReadLine");
        }

        if (ferror(pp))
        {
            fflush(pp);
            break;
        }

        print = false;

        for (sp = line; *sp != '\0'; sp++)
        {
            if (!isspace((int) *sp))
            {
                print = true;
                break;
            }
        }

        if (print)
        {
            snprintf(sendbuffer, CF_BUFSIZE, "%s\n", line);
            if (SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "send", "Sending failed, aborting");
                break;
            }
        }
    }

    cf_pclose(pp);
}

/**************************************************************/

static ProtocolCommand GetCommand(char *str)
{
    int i;
    char op[CF_BUFSIZE];

    sscanf(str, "%4095s", op);

    for (i = 0; PROTOCOL[i] != NULL; i++)
    {
        if (strcmp(op, PROTOCOL[i]) == 0)
        {
            return i;
        }
    }

    return -1;
}

/*********************************************************************/

static int VerifyConnection(ServerConnectionState *conn, char buf[CF_BUFSIZE])
 /* Try reverse DNS lookup
    and RFC931 username lookup to check the authenticity. */
{
    char ipstring[CF_MAXVARSIZE], fqname[CF_MAXVARSIZE], username[CF_MAXVARSIZE];
    char dns_assert[CF_MAXVARSIZE], ip_assert[CF_MAXVARSIZE];
    int matched = false;

#if defined(HAVE_GETADDRINFO)
    struct addrinfo query, *response = NULL, *ap;
    int err;
#else
    struct sockaddr_in raddr;
    int i, j;
    socklen_t len = sizeof(struct sockaddr_in);
    struct hostent *hp = NULL;
#endif

    CfDebug("Connecting host identifies itself as %s\n", buf);

    memset(ipstring, 0, CF_MAXVARSIZE);
    memset(fqname, 0, CF_MAXVARSIZE);
    memset(username, 0, CF_MAXVARSIZE);

    sscanf(buf, "%255s %255s %255s", ipstring, fqname, username);

    CfDebug("(ipstring=[%s],fqname=[%s],username=[%s],socket=[%s])\n", ipstring, fqname, username, conn->ipaddr);

    ThreadLock(cft_system);

    strlcpy(dns_assert, fqname, CF_MAXVARSIZE);
    ToLowerStrInplace(dns_assert);

    strncpy(ip_assert, ipstring, CF_MAXVARSIZE - 1);

    ThreadUnlock(cft_system);

/* It only makes sense to check DNS by reverse lookup if the key had to be accepted
   on trust. Once we have a positive key ID, the IP address is irrelevant fr authentication...
   We can save a lot of time by not looking this up ... */

    if ((conn->trust == false) || (IsMatchItemIn(SV.skipverify, MapAddress(conn->ipaddr))))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Allowing %s to connect without (re)checking ID\n", ip_assert);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Non-verified Host ID is %s (Using skipverify)\n", dns_assert);
        strncpy(conn->hostname, dns_assert, CF_MAXVARSIZE);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Non-verified User ID seems to be %s (Using skipverify)\n", username);
        strncpy(conn->username, username, CF_MAXVARSIZE);

#ifdef __MINGW32__                    /* NT uses security identifier instead of uid */

        if (!NovaWin_UserNameToSid(username, (SID *) conn->sid, CF_MAXSIDSIZE, false))
        {
            memset(conn->sid, 0, CF_MAXSIDSIZE);        /* is invalid sid - discarded */
        }

#else /* !__MINGW32__ */

        struct passwd *pw;
        if ((pw = getpwnam(username)) == NULL)  /* Keep this inside mutex */
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "",
              "IP address mismatch between client's assertion (%s) and socket (%s) - untrustworthy connection\n",
              ip_assert, conn->ipaddr);
        return false;
    }

    if (strlen(dns_assert) == 0)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "DNS asserted name was empty - untrustworthy connection\n");
        return false;
    }

    if (strcmp(dns_assert, "skipident") == 0)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "DNS asserted name was withheld before key exchange - untrustworthy connection\n");
        return false;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Socket caller address appears honest (%s matches %s)\n", ip_assert,
          MapAddress(conn->ipaddr));

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Socket originates from %s=%s\n", ip_assert, dns_assert);

    CfDebug("Attempting to verify honesty by looking up hostname (%s)\n", dns_assert);

/* Do a reverse DNS lookup, like tcp wrappers to see if hostname matches IP */

#if defined(HAVE_GETADDRINFO)

    CfDebug("Using v6 compatible lookup...\n");

    memset(&query, 0, sizeof(struct addrinfo));

    query.ai_family = AF_UNSPEC;
    query.ai_socktype = SOCK_STREAM;
    query.ai_flags = AI_PASSIVE;

    if ((err = getaddrinfo(dns_assert, NULL, &query, &response)) != 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to lookup %s (%s)", dns_assert, gai_strerror(err));
    }

    for (ap = response; ap != NULL; ap = ap->ai_next)
    {
        ThreadLock(cft_getaddr);

        if (strcmp(MapAddress(conn->ipaddr), sockaddr_ntop(ap->ai_addr)) == 0)
        {
            CfDebug("Found match\n");
            matched = true;
        }

        ThreadUnlock(cft_getaddr);
    }

    if (response != NULL)
    {
        freeaddrinfo(response);
    }

#else

    CfDebug("IPV4 hostnname lookup on %s\n", dns_assert);

    ThreadLock(cft_getaddr);

    if ((hp = gethostbyname(dns_assert)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "cf-serverd Couldn't look up name %s\n", fqname);
        CfOut(OUTPUT_LEVEL_LOG, "gethostbyname", "DNS lookup of %s failed", dns_assert);
        matched = false;
    }
    else
    {
        matched = true;

        CfDebug("Looking for the peername of our socket...\n");

        if (getpeername(conn->sd_reply, (struct sockaddr *) &raddr, &len) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "getpeername", "Couldn't get socket address\n");
            matched = false;
        }

        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Looking through hostnames on socket with IPv4 %s\n",
              sockaddr_ntop((struct sockaddr *) &raddr));

        for (i = 0; hp->h_addr_list[i]; i++)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Reverse lookup address found: %d\n", i);
            if (memcmp(hp->h_addr_list[i], (char *) &(raddr.sin_addr), sizeof(raddr.sin_addr)) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Canonical name matched host's assertion - id confirmed as %s\n", dns_assert);
                break;
            }
        }

        if (hp->h_addr_list[0] != NULL)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking address number %d for non-canonical names (aliases)\n", i);

            for (j = 0; hp->h_aliases[j] != NULL; j++)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Comparing [%s][%s]\n", hp->h_aliases[j], ip_assert);

                if (strcmp(hp->h_aliases[j], ip_assert) == 0)
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Non-canonical name (alias) matched host's assertion - id confirmed as %s\n",
                          dns_assert);
                    break;
                }
            }

            if ((hp->h_addr_list[i] != NULL) && (hp->h_aliases[j] != NULL))
            {
                CfOut(OUTPUT_LEVEL_LOG, "", "Reverse hostname lookup failed, host claiming to be %s was %s\n", buf,
                      sockaddr_ntop((struct sockaddr *) &raddr));
                matched = false;
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Reverse lookup succeeded\n");
            }
        }
        else
        {
            CfOut(OUTPUT_LEVEL_LOG, "", "No name was registered in DNS for %s - reverse lookup failed\n", dns_assert);
            matched = false;
        }
    }

    ThreadUnlock(cft_getaddr);

# ifdef __MINGW32__                   /* NT uses security identifier instead of uid */
    if (!NovaWin_UserNameToSid(username, (SID *) conn->sid, CF_MAXSIDSIZE, false))
    {
        memset(conn->sid, 0, CF_MAXSIDSIZE);    /* is invalid sid - discarded */
    }

# else/* !__MINGW32__ */
    if ((pw = getpwnam(username)) == NULL)      /* Keep this inside mutex */
    {
        conn->uid = -2;
    }
    else
    {
        conn->uid = pw->pw_uid;
    }
# endif/* !__MINGW32__ */

#endif

    if (!matched)
    {
        CfOut(OUTPUT_LEVEL_LOG, "gethostbyname", "Failed on DNS reverse lookup of %s\n", dns_assert);
        CfOut(OUTPUT_LEVEL_LOG, "", "Client sent: %s", buf);
        return false;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host ID is %s\n", dns_assert);
    strncpy(conn->hostname, dns_assert, CF_MAXVARSIZE - 1);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "User ID seems to be %s\n", username);
    strncpy(conn->username, username, CF_MAXVARSIZE - 1);

    return true;
}

/**************************************************************/

static int AllowedUser(char *user)
{
    if (IsItemIn(SV.allowuserlist, user))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "User %s granted connection privileges\n", user);
        return true;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "User %s is not allowed on this server\n", user);
    return false;
}

/**************************************************************/

/* 'resolved' argument needs to be at least CF_BUFSIZE long */

bool ResolveFilename(const char *req_path, char *res_path)
{
    char req_dir[CF_BUFSIZE];
    char req_filename[CF_BUFSIZE];

/*
 * Eliminate symlinks from path, but do not resolve the file itself if it is a
 * symlink.
 */

    strlcpy(req_dir, req_path, CF_BUFSIZE);
    ChopLastNode(req_dir);

    strlcpy(req_filename, ReadLastNode(req_path), CF_BUFSIZE);

#if defined HAVE_REALPATH && !defined _WIN32
    if (realpath(req_dir, res_path) == NULL)
    {
        return false;
    }
#else
    memset(res_path, 0, CF_BUFSIZE);
    CompressPath(res_path, req_dir);
#endif

    AddSlash(res_path);
    strlcat(res_path, req_filename, CF_BUFSIZE);

/* Adjust for forward slashes */

    MapName(res_path);

/* NT has case-insensitive path names */

#ifdef __MINGW32__
    int i;

    for (i = 0; i < strlen(res_path); i++)
    {
        res_path[i] = ToLower(res_path[i]);
    }
#endif /* __MINGW32__ */

    return true;
}

/**************************************************************/

static int AccessControl(const char *req_path, ServerConnectionState *conn, int encrypt, Auth *vadmit, Auth *vdeny)
{
    Auth *ap;
    int access = false;
    char transrequest[CF_BUFSIZE];
    struct stat statbuf;
    char translated_req_path[CF_BUFSIZE];
    char transpath[CF_BUFSIZE];

    CfDebug("AccessControl(%s)\n", req_path);

/*
 * /var/cfengine -> $workdir translation.
 */
    TranslatePath(translated_req_path, req_path);

    if (ResolveFilename(translated_req_path, transrequest))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Filename %s is resolved to %s", translated_req_path, transrequest);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "lstat", "Couldn't resolve filename %s from host %s\n", translated_req_path, conn->hostname);
    }

    if (lstat(transrequest, &statbuf) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "lstat", "Couldn't stat filename %s requested by host %s\n", transrequest, conn->hostname);
        return false;
    }

    CfDebug("AccessControl, match(%s,%s) encrypt request=%d\n", transrequest, conn->hostname, encrypt);

    if (vadmit == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "cf-serverd access list is empty, no files are visible\n");
        return false;
    }

    conn->maproot = false;

    for (ap = vadmit; ap != NULL; ap = ap->next)
    {
        int res = false;

        CfDebug("Examining rule in access list (%s,%s)?\n", transrequest, ap->path);

        strncpy(transpath, ap->path, CF_BUFSIZE - 1);
        MapName(transpath);

        if ((strlen(transrequest) > strlen(transpath)) && (strncmp(transpath, transrequest, strlen(transpath)) == 0)
            && (transrequest[strlen(transpath)] == FILE_SEPARATOR))
        {
            res = true;         /* Substring means must be a / to link, else just a substring og filename */
        }

        /* Exact match means single file to admit */

        if (strcmp(transpath, transrequest) == 0)
        {
            res = true;
        }

        if (strcmp(transpath, "/") == 0)
        {
            res = true;
        }

        if (res)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Found a matching rule in access list (%s in %s)\n", transrequest, transpath);

            if (cfstat(transpath, &statbuf) == -1)
            {
                CfOut(OUTPUT_LEVEL_LOG, "",
                      "Warning cannot stat file object %s in admit/grant, or access list refers to dangling link\n",
                      transpath);
                continue;
            }

            if ((!encrypt) && (ap->encrypt == true))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "File %s requires encrypt connection...will not serve\n", transpath);
                access = false;
            }
            else
            {
                CfDebug("Checking whether to map root privileges..\n");

                if ((IsMatchItemIn(ap->maproot, MapAddress(conn->ipaddr))) || (IsRegexItemIn(ap->maproot, conn->hostname)))
                {
                    conn->maproot = true;
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Mapping root privileges to access non-root files\n");
                }

                if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr)))
                    || (IsRegexItemIn(ap->accesslist, conn->hostname)))
                {
                    access = true;
                    CfDebug("Access privileges - match found\n");
                }
            }
            break;
        }
    }

    if (strncmp(transpath, transrequest, strlen(transpath)) == 0)
    {
        for (ap = vdeny; ap != NULL; ap = ap->next)
        {
            if (IsRegexItemIn(ap->accesslist, conn->hostname))
            {
                access = false;
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s explicitly denied access to %s\n", conn->hostname, transrequest);
                break;
            }
        }
    }

    if (access)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s granted access to %s\n", conn->hostname, req_path);

        if (encrypt && LOGENCRYPT)
        {
            /* Log files that were marked as requiring encryption */
            CfOut(OUTPUT_LEVEL_LOG, "", "Host %s granted access to %s\n", conn->hostname, req_path);
        }
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s denied access to %s\n", conn->hostname, req_path);
    }

    if (!conn->rsa_auth)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Cannot map root access without RSA authentication");
        conn->maproot = false;  /* only public files accessible */
    }

    return access;
}

/**************************************************************/

static int LiteralAccessControl(char *in, ServerConnectionState *conn, int encrypt, Auth *vadmit, Auth *vdeny)
{
    Auth *ap;
    int access = false;
    char name[CF_BUFSIZE];

    name[0] = '\0';

    if (strncmp(in, "VAR", 3) == 0)
    {
        sscanf(in, "VAR %255[^\n]", name);
    }
    else if (strncmp(in, "CALL_ME_BACK", strlen("CALL_ME_BACK")) == 0)
    {
        sscanf(in, "CALL_ME_BACK %255[^\n]", name);
    }
    else
    {
        sscanf(in, "QUERY %128s", name);
    }

    CfDebug("\n\nLiteralAccessControl(%s)\n", name);

    conn->maproot = false;

    for (ap = vadmit; ap != NULL; ap = ap->next)
    {
        int res = false;

        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Examining rule in access list (%s,%s)?\n", name, ap->path);

        if (strcmp(ap->path, name) == 0)
        {
            res = true;         /* Exact match means single file to admit */
        }

        if (res)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Found a matching rule in access list (%s in %s)\n", name, ap->path);

            if ((!ap->literal) && (!ap->variable))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "",
                      "Variable/query \"%s\" requires a literal server item...cannot set variable directly by path\n",
                      ap->path);
                access = false;
                break;
            }

            if ((!encrypt) && (ap->encrypt == true))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Variable %s requires encrypt connection...will not serve\n", name);
                access = false;
                break;
            }
            else
            {
                CfDebug("Checking whether to map root privileges..\n");

                if ((IsMatchItemIn(ap->maproot, MapAddress(conn->ipaddr))) || (IsRegexItemIn(ap->maproot, conn->hostname)))
                {
                    conn->maproot = true;
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Mapping root privileges\n");
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "No root privileges granted\n");
                }

                if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr)))
                    || (IsRegexItemIn(ap->accesslist, conn->hostname)))
                {
                    access = true;
                    CfDebug("Access privileges - match found\n");
                }
            }
        }
    }

    for (ap = vdeny; ap != NULL; ap = ap->next)
    {
        if (strcmp(ap->path, name) == 0)
        {
            if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr)))
                || (IsRegexItemIn(ap->accesslist, conn->hostname)))
            {
                access = false;
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s explicitly denied access to %s\n", conn->hostname, name);
                break;
            }
        }
    }

    if (access)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s granted access to literal \"%s\"\n", conn->hostname, name);

        if (encrypt && LOGENCRYPT)
        {
            /* Log files that were marked as requiring encryption */
            CfOut(OUTPUT_LEVEL_LOG, "", "Host %s granted access to literal \"%s\"\n", conn->hostname, name);
        }
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s denied access to literal \"%s\"\n", conn->hostname, name);
    }

    if (!conn->rsa_auth)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Cannot map root access without RSA authentication");
        conn->maproot = false;  /* only public files accessible */
    }

    return access;
}

/**************************************************************/

static Item *ContextAccessControl(char *in, ServerConnectionState *conn, int encrypt, Auth *vadmit, Auth *vdeny)
{
    Auth *ap;
    int access = false;
    char client_regex[CF_BUFSIZE];
    CF_DB *dbp;
    CF_DBC *dbcp;
    int ksize, vsize;
    char *key;
    void *value;
    time_t now = time(NULL);
    CfState q;
    Item *ip, *matches = NULL, *candidates = NULL;

    sscanf(in, "CONTEXT %255[^\n]", client_regex);

    CfDebug("\n\nContextAccessControl(%s)\n", client_regex);


    if (!OpenDB(&dbp, dbid_state))
    {
        return NULL;
    }

    if (!NewDBCursor(dbp, &dbcp))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", " !! Unable to scan persistence cache");
        CloseDB(dbp);
        return NULL;
    }

    while (NextDB(dbp, dbcp, &key, &ksize, &value, &vsize))
    {
        memcpy((void *) &q, value, sizeof(CfState));

        if (now > q.expires)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " Persistent class %s expired\n", key);
            DBCursorDeleteEntry(dbcp);
        }
        else
        {
            if (FullTextMatch(client_regex, key))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " - Found key %s...\n", key);
                AppendItem(&candidates, key, NULL);
            }
        }
    }

    DeleteDBCursor(dbp, dbcp);
    CloseDB(dbp);

    for (ip = candidates; ip != NULL; ip = ip->next)
    {
        for (ap = vadmit; ap != NULL; ap = ap->next)
        {
            int res = false;

            if (FullTextMatch(ap->path, ip->name))
            {
                res = true;
            }

            if (res)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Found a matching rule in access list (%s in %s)\n", ip->name, ap->path);

                if (ap->classpattern == false)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "",
                          "Context %s requires a literal server item...cannot set variable directly by path\n",
                          ap->path);
                    access = false;
                    continue;
                }

                if ((!encrypt) && (ap->encrypt == true))
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "Context %s requires encrypt connection...will not serve\n", ip->name);
                    access = false;
                    break;
                }
                else
                {
                    CfDebug("Checking whether to map root privileges..\n");

                    if ((IsMatchItemIn(ap->maproot, MapAddress(conn->ipaddr)))
                        || (IsRegexItemIn(ap->maproot, conn->hostname)))
                    {
                        conn->maproot = true;
                        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Mapping root privileges\n");
                    }
                    else
                    {
                        CfOut(OUTPUT_LEVEL_VERBOSE, "", "No root privileges granted\n");
                    }

                    if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr)))
                        || (IsRegexItemIn(ap->accesslist, conn->hostname)))
                    {
                        access = true;
                        CfDebug("Access privileges - match found\n");
                    }
                }
            }
        }

        for (ap = vdeny; ap != NULL; ap = ap->next)
        {
            if (strcmp(ap->path, ip->name) == 0)
            {
                if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr)))
                    || (IsRegexItemIn(ap->accesslist, conn->hostname)))
                {
                    access = false;
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s explicitly denied access to context %s\n", conn->hostname, ip->name);
                    break;
                }
            }
        }

        if (access)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s granted access to context \"%s\"\n", conn->hostname, ip->name);
            AppendItem(&matches, ip->name, NULL);

            if (encrypt && LOGENCRYPT)
            {
                /* Log files that were marked as requiring encryption */
                CfOut(OUTPUT_LEVEL_LOG, "", "Host %s granted access to context \"%s\"\n", conn->hostname, ip->name);
            }
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s denied access to context \"%s\"\n", conn->hostname, ip->name);
        }
    }

    DeleteItemList(candidates);
    return matches;
}

/**************************************************************/

static int AuthorizeRoles(ServerConnectionState *conn, char *args)
{
    char *sp;
    Auth *ap;
    char userid1[CF_MAXVARSIZE], userid2[CF_MAXVARSIZE];
    Rlist *rp, *defines = NULL;
    int permitted = false;

    snprintf(userid1, CF_MAXVARSIZE, "%s@%s", conn->username, conn->hostname);
    snprintf(userid2, CF_MAXVARSIZE, "%s@%s", conn->username, conn->ipaddr);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking authorized roles in %s\n", args);

    if (strncmp(args, "--define", strlen("--define")) == 0)
    {
        sp = args + strlen("--define");
    }
    else
    {
        sp = args + strlen("-D");
    }

    while (*sp == ' ')
    {
        sp++;
    }

    defines = RlistFromSplitRegex(sp, "[,:;]", 99, false);

/* For each user-defined class attempt, check RBAC */

    for (rp = defines; rp != NULL; rp = rp->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Verifying %s\n", RlistScalarValue(rp));

        for (ap = ROLES; ap != NULL; ap = ap->next)
        {
            if (FullTextMatch(ap->path, rp->item))
            {
                /* We have a pattern covering this class - so are we allowed to activate it? */
                if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr))) ||
                    (IsRegexItemIn(ap->accesslist, conn->hostname)) ||
                    (IsRegexItemIn(ap->accesslist, userid1)) ||
                    (IsRegexItemIn(ap->accesslist, userid2)) ||
                    (IsRegexItemIn(ap->accesslist, conn->username)))
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Attempt to define role/class %s is permitted", RlistScalarValue(rp));
                    permitted = true;
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Attempt to define role/class %s is denied", RlistScalarValue(rp));
                    RlistDestroy(defines);
                    return false;
                }
            }
        }

    }

    if (permitted)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Role activation allowed\n");
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Role activation disallowed - abort execution\n");
    }

    RlistDestroy(defines);
    return permitted;
}

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
        CfOut(OUTPUT_LEVEL_ERROR, "", "No public/private key pair exists, create one with cf-key\n");
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
        CfOut(OUTPUT_LEVEL_INFORM, "", "Protocol format error in authentation from IP %s\n", conn->hostname);
        return false;
    }

    if (nonce_len > CF_NONCELEN * 2)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Protocol deviant authentication nonce from %s\n", conn->hostname);
        return false;
    }

    if (crypt_len > 2 * CF_NONCELEN)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Protocol abuse in unlikely cipher from %s\n", conn->hostname);
        return false;
    }

/* Check there is no attempt to read past the end of the received input */

    if (recvbuffer + CF_RSA_PROTO_OFFSET + nonce_len > recvbuffer + recvlen)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Protocol consistency error in authentication from %s\n", conn->hostname);
        return false;
    }

    if ((strcmp(sauth, "SAUTH") != 0) || (nonce_len == 0) || (crypt_len == 0))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Protocol error in RSA authentication from IP %s\n", conn->hostname);
        return false;
    }

    CfDebug("Challenge encryption = %c, nonce = %d, buf = %d\n", iscrypt, nonce_len, crypt_len);

    ThreadLock(cft_system);

    decrypted_nonce = xmalloc(crypt_len);

    if (iscrypt == 'y')
    {
        if (RSA_private_decrypt
            (crypt_len, recvbuffer + CF_RSA_PROTO_OFFSET, decrypted_nonce, PRIVKEY, RSA_PKCS1_PADDING) <= 0)
        {
            err = ERR_get_error();

            ThreadUnlock(cft_system);
            CfOut(OUTPUT_LEVEL_ERROR, "", "Private decrypt failed = %s\n", ERR_reason_error_string(err));
            free(decrypted_nonce);
            return false;
        }
    }
    else
    {
        if (nonce_len > crypt_len)
        {
            ThreadUnlock(cft_system);
            CfOut(OUTPUT_LEVEL_ERROR, "", "Illegal challenge\n");
            free(decrypted_nonce);
            return false;
        }

        memcpy(decrypted_nonce, recvbuffer + CF_RSA_PROTO_OFFSET, nonce_len);
    }

    ThreadUnlock(cft_system);

/* Client's ID is now established by key or trusted, reply with digest */

    HashString(decrypted_nonce, nonce_len, digest, digestType);

    free(decrypted_nonce);

/* Get the public key from the client */

    ThreadLock(cft_system);
    newkey = RSA_new();
    ThreadUnlock(cft_system);

/* proposition C2 */
    if ((len_n = ReceiveTransaction(conn->sd_reply, recvbuffer, NULL)) == -1)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Protocol error 1 in RSA authentation from IP %s\n", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if (len_n == 0)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Protocol error 2 in RSA authentation from IP %s\n", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if ((newkey->n = BN_mpi2bn(recvbuffer, len_n, NULL)) == NULL)
    {
        err = ERR_get_error();
        CfOut(OUTPUT_LEVEL_ERROR, "", "Private decrypt failed = %s\n", ERR_reason_error_string(err));
        RSA_free(newkey);
        return false;
    }

/* proposition C3 */

    if ((len_e = ReceiveTransaction(conn->sd_reply, recvbuffer, NULL)) == -1)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Protocol error 3 in RSA authentation from IP %s\n", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if (len_e == 0)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Protocol error 4 in RSA authentation from IP %s\n", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if ((newkey->e = BN_mpi2bn(recvbuffer, len_e, NULL)) == NULL)
    {
        err = ERR_get_error();
        CfOut(OUTPUT_LEVEL_ERROR, "", "Private decrypt failed = %s\n", ERR_reason_error_string(err));
        RSA_free(newkey);
        return false;
    }

    if (DEBUG)
    {
        RSA_print_fp(stdout, newkey, 0);
    }

    HashPubKey(newkey, conn->digest, CF_DEFAULT_DIGEST);

    if (VERBOSE)
    {
        ThreadLock(cft_output);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Public key identity of host \"%s\" is \"%s\"", conn->ipaddr,
              HashPrint(CF_DEFAULT_DIGEST, conn->digest));
        ThreadUnlock(cft_output);
    }

    LastSaw(conn->ipaddr, conn->digest, LAST_SEEN_ROLE_ACCEPT);

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

    SendTransaction(conn->sd_reply, digest, digestLen, CF_DONE);

/* Send counter challenge to be sure this is a live session */

    ThreadLock(cft_system);

    counter_challenge = BN_new();
    if (counter_challenge == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Cannot allocate BIGNUM structure for counter challenge\n");
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
        CfOut(OUTPUT_LEVEL_ERROR, "", "Public encryption failed = %s\n", ERR_reason_error_string(err));
        RSA_free(newkey);
        free(out);
        return false;
    }

    ThreadUnlock(cft_system);

/* proposition S3 */
    SendTransaction(conn->sd_reply, out, encrypted_len, CF_DONE);

/* if the client doesn't have our public key, send it */

    if (iscrypt != 'y')
    {
        /* proposition S4  - conditional */
        memset(in, 0, CF_BUFSIZE);
        len_n = BN_bn2mpi(PUBKEY->n, in);
        SendTransaction(conn->sd_reply, in, len_n, CF_DONE);

        /* proposition S5  - conditional */
        memset(in, 0, CF_BUFSIZE);
        len_e = BN_bn2mpi(PUBKEY->e, in);
        SendTransaction(conn->sd_reply, in, len_e, CF_DONE);
    }

/* Receive reply to counter_challenge */

/* proposition C4 */
    memset(in, 0, CF_BUFSIZE);

    if (ReceiveTransaction(conn->sd_reply, in, NULL) == -1)
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
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Strong authentication of client %s/%s achieved", conn->hostname, conn->ipaddr);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Weak authentication of trusted client %s/%s (key accepted on trust).\n",
                  conn->hostname, conn->ipaddr);
        }
    }
    else
    {
        BN_free(counter_challenge);
        free(out);
        RSA_free(newkey);
        CfOut(OUTPUT_LEVEL_INFORM, "", "Challenge response from client %s was incorrect - ID false?", conn->ipaddr);
        return false;
    }

/* Receive random session key,... */

/* proposition C5 */

    memset(in, 0, CF_BUFSIZE);

    if ((keylen = ReceiveTransaction(conn->sd_reply, in, NULL)) == -1)
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
        CfOut(OUTPUT_LEVEL_INFORM, "", "Session key length received from %s is too long", conn->ipaddr);
        return false;
    }

    ThreadLock(cft_system);

    session_size = CfSessionKeySize(enterprise_field);
    conn->session_key = xmalloc(session_size);
    conn->encryption_type = enterprise_field;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Receiving session key from client (size=%d)...", keylen);

    CfDebug("keylen=%d, session_size=%d\n", keylen, session_size);

    if (keylen == CF_BLOWFISHSIZE)      /* Support the old non-ecnrypted for upgrade */
    {
        memcpy(conn->session_key, in, session_size);
    }
    else
    {
        /* New protocol encrypted */

        if (RSA_private_decrypt(keylen, in, out, PRIVKEY, RSA_PKCS1_PADDING) <= 0)
        {
            ThreadUnlock(cft_system);
            err = ERR_get_error();
            CfOut(OUTPUT_LEVEL_ERROR, "", "Private decrypt failed = %s\n", ERR_reason_error_string(err));
            return false;
        }

        memcpy(conn->session_key, out, session_size);
    }

    ThreadUnlock(cft_system);


    BN_free(counter_challenge);
    free(out);
    RSA_free(newkey);
    conn->rsa_auth = true;
    return true;
}

/**************************************************************/

static int StatFile(ServerConnectionState *conn, char *sendbuffer, char *ofilename)
/* Because we do not know the size or structure of remote datatypes,*/
/* the simplest way to transfer the data is to convert them into */
/* plain text and interpret them on the other side. */
{
    Stat cfst;
    struct stat statbuf, statlinkbuf;
    char linkbuf[CF_BUFSIZE], filename[CF_BUFSIZE];
    int islink = false;

    CfDebug("\nStatFile(%s)\n", filename);

    TranslatePath(filename, ofilename);

    memset(&cfst, 0, sizeof(Stat));

    if (strlen(ReadLastNode(filename)) > CF_MAXLINKSIZE)
    {
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: Filename suspiciously long [%s]\n", filename);
        CfOut(OUTPUT_LEVEL_ERROR, "", "%s", sendbuffer);
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
        return -1;
    }

    if (lstat(filename, &statbuf) == -1)
    {
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: unable to stat file %s", filename);
        CfOut(OUTPUT_LEVEL_VERBOSE, "lstat", "%s", sendbuffer);
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
        return -1;
    }

    cfst.cf_readlink = NULL;
    cfst.cf_lmode = 0;
    cfst.cf_nlink = CF_NOSIZE;

    memset(linkbuf, 0, CF_BUFSIZE);

#ifndef __MINGW32__                   // windows doesn't support symbolic links
    if (S_ISLNK(statbuf.st_mode))
    {
        islink = true;
        cfst.cf_type = FILE_TYPE_LINK; /* pointless - overwritten */
        cfst.cf_lmode = statbuf.st_mode & 07777;
        cfst.cf_nlink = statbuf.st_nlink;

        if (readlink(filename, linkbuf, CF_BUFSIZE - 1) == -1)
        {
            sprintf(sendbuffer, "BAD: unable to read link\n");
            CfOut(OUTPUT_LEVEL_ERROR, "readlink", "%s", sendbuffer);
            SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
            return -1;
        }

        CfDebug("readlink: %s\n", linkbuf);

        cfst.cf_readlink = linkbuf;
    }
#endif /* !__MINGW32__ */

    if ((!islink) && (cfstat(filename, &statbuf) == -1))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "stat", "BAD: unable to stat file %s\n", filename);
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
        return -1;
    }

    CfDebug("Getting size of link deref %s\n", linkbuf);

    if (islink && (cfstat(filename, &statlinkbuf) != -1))       /* linktype=copy used by agent */
    {
        statbuf.st_size = statlinkbuf.st_size;
        statbuf.st_mode = statlinkbuf.st_mode;
        statbuf.st_uid = statlinkbuf.st_uid;
        statbuf.st_gid = statlinkbuf.st_gid;
        statbuf.st_mtime = statlinkbuf.st_mtime;
        statbuf.st_ctime = statlinkbuf.st_ctime;
    }

    if (S_ISDIR(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_DIR;
    }

    if (S_ISREG(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_REGULAR;
    }

    if (S_ISSOCK(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_SOCK;
    }

    if (S_ISCHR(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_CHAR;
    }

    if (S_ISBLK(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_BLOCK;
    }

    if (S_ISFIFO(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_FIFO;
    }

    cfst.cf_mode = statbuf.st_mode & 07777;
    cfst.cf_uid = statbuf.st_uid & 0xFFFFFFFF;
    cfst.cf_gid = statbuf.st_gid & 0xFFFFFFFF;
    cfst.cf_size = statbuf.st_size;
    cfst.cf_atime = statbuf.st_atime;
    cfst.cf_mtime = statbuf.st_mtime;
    cfst.cf_ctime = statbuf.st_ctime;
    cfst.cf_ino = statbuf.st_ino;
    cfst.cf_dev = statbuf.st_dev;
    cfst.cf_readlink = linkbuf;

    if (cfst.cf_nlink == CF_NOSIZE)
    {
        cfst.cf_nlink = statbuf.st_nlink;
    }

#if !defined(__MINGW32__)
    if (statbuf.st_size > statbuf.st_blocks * DEV_BSIZE)
#else
# ifdef HAVE_ST_BLOCKS
    if (statbuf.st_size > statbuf.st_blocks * DEV_BSIZE)
# else
    if (statbuf.st_size > ST_NBLOCKS(statbuf) * DEV_BSIZE)
# endif
#endif
    {
        cfst.cf_makeholes = 1;  /* must have a hole to get checksum right */
    }
    else
    {
        cfst.cf_makeholes = 0;
    }

    memset(sendbuffer, 0, CF_BUFSIZE);

    /* send as plain text */

    CfDebug("OK: type=%d\n mode=%" PRIoMAX "\n lmode=%" PRIoMAX "\n uid=%" PRIuMAX "\n gid=%" PRIuMAX "\n size=%" PRIdMAX "\n atime=%" PRIdMAX "\n mtime=%" PRIdMAX "\n",
            cfst.cf_type, (uintmax_t)cfst.cf_mode, (uintmax_t)cfst.cf_lmode, (intmax_t)cfst.cf_uid, (intmax_t)cfst.cf_gid, (intmax_t) cfst.cf_size,
            (intmax_t) cfst.cf_atime, (intmax_t) cfst.cf_mtime);

    snprintf(sendbuffer, CF_BUFSIZE, "OK: %d %ju %ju %ju %ju %jd %jd %jd %jd %d %d %d %jd",
             cfst.cf_type, (uintmax_t)cfst.cf_mode, (uintmax_t)cfst.cf_lmode,
             (uintmax_t)cfst.cf_uid, (uintmax_t)cfst.cf_gid, (intmax_t)cfst.cf_size,
             (intmax_t) cfst.cf_atime, (intmax_t) cfst.cf_mtime, (intmax_t) cfst.cf_ctime,
             cfst.cf_makeholes, cfst.cf_ino, cfst.cf_nlink, (intmax_t) cfst.cf_dev);

    SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);

    memset(sendbuffer, 0, CF_BUFSIZE);

    if (cfst.cf_readlink != NULL)
    {
        strcpy(sendbuffer, "OK:");
        strcat(sendbuffer, cfst.cf_readlink);
    }
    else
    {
        sprintf(sendbuffer, "OK:");
    }

    SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
    return 0;
}

/***************************************************************/

static void CfGetFile(ServerFileGetState *args)
{
    int sd, fd;
    off_t n_read, total = 0, sendlen = 0, count = 0;
    char sendbuffer[CF_BUFSIZE + 256], filename[CF_BUFSIZE];
    struct stat sb;
    int blocksize = 2048;

    sd = (args->connect)->sd_reply;

    TranslatePath(filename, args->replyfile);

    cfstat(filename, &sb);

    CfDebug("CfGetFile(%s on sd=%d), size=%" PRIdMAX "\n", filename, sd, (intmax_t) sb.st_size);

/* Now check to see if we have remote permission */

    if (!TransferRights(filename, sd, args, sendbuffer, &sb))
    {
        RefuseAccess(args->connect, sendbuffer, args->buf_size, "");
        snprintf(sendbuffer, CF_BUFSIZE, "%s", CF_FAILEDSTR);
        SendSocketStream(sd, sendbuffer, args->buf_size, 0);
    }

/* File transfer */

    if ((fd = SafeOpen(filename)) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "open", "Open error of file [%s]\n", filename);
        snprintf(sendbuffer, CF_BUFSIZE, "%s", CF_FAILEDSTR);
        SendSocketStream(sd, sendbuffer, args->buf_size, 0);
    }
    else
    {
        int div = 3;

        if (sb.st_size > 10485760L) /* File larger than 10 MB, checks every 64kB */
        {
            div = 32;
        }

        while (true)
        {
            memset(sendbuffer, 0, CF_BUFSIZE);

            CfDebug("Now reading from disk...\n");

            if ((n_read = read(fd, sendbuffer, blocksize)) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "read", "read failed in GetFile");
                break;
            }

            if (n_read == 0)
            {
                break;
            }
            else
            {
                off_t savedlen = sb.st_size;

                /* check the file is not changing at source */

                if (count++ % div == 0)   /* Don't do this too often */
                {
                    if (stat(filename, &sb))
                    {
                        CfOut(OUTPUT_LEVEL_ERROR, "send", "Cannot stat file %s: (errno=%d) %s",
                              filename, errno, strerror(errno));
                        break;
                    }
                }

                if (sb.st_size != savedlen)
                {
                    snprintf(sendbuffer, CF_BUFSIZE, "%s%s: %s", CF_CHANGEDSTR1, CF_CHANGEDSTR2, filename);

                    if (SendSocketStream(sd, sendbuffer, blocksize, 0) == -1)
                    {
                        CfOut(OUTPUT_LEVEL_VERBOSE, "send", "Send failed in GetFile");
                    }

                    CfDebug("Aborting transfer after %" PRIdMAX ": file is changing rapidly at source.\n", (intmax_t)total);
                    break;
                }

                if ((savedlen - total) / blocksize > 0)
                {
                    sendlen = blocksize;
                }
                else if (savedlen != 0)
                {
                    sendlen = (savedlen - total);
                }
            }

            total += n_read;

            if (SendSocketStream(sd, sendbuffer, sendlen, 0) == -1)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "send", "Send failed in GetFile");
                break;
            }
        }

        close(fd);
    }

    CfDebug("Done with GetFile()\n");
}

/***************************************************************/

static void CfEncryptGetFile(ServerFileGetState *args)
/* Because the stream doesn't end for each file, we need to know the
   exact number of bytes transmitted, which might change during
   encryption, hence we need to handle this with transactions */
{
    int sd, fd, n_read, cipherlen, finlen;
    off_t total = 0, count = 0;
    char sendbuffer[CF_BUFSIZE + 256], out[CF_BUFSIZE], filename[CF_BUFSIZE];
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };
    int blocksize = CF_BUFSIZE - 4 * CF_INBAND_OFFSET;
    EVP_CIPHER_CTX ctx;
    char *key, enctype;
    struct stat sb;

    sd = (args->connect)->sd_reply;
    key = (args->connect)->session_key;
    enctype = (args->connect)->encryption_type;

    TranslatePath(filename, args->replyfile);

    cfstat(filename, &sb);

    CfDebug("CfEncryptGetFile(%s on sd=%d), size=%" PRIdMAX "\n", filename, sd, (intmax_t) sb.st_size);

/* Now check to see if we have remote permission */

    if (!TransferRights(filename, sd, args, sendbuffer, &sb))
    {
        RefuseAccess(args->connect, sendbuffer, args->buf_size, "");
        FailedTransfer(sd, sendbuffer, filename);
    }

    EVP_CIPHER_CTX_init(&ctx);

    if ((fd = SafeOpen(filename)) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "open", "Open error of file [%s]\n", filename);
        FailedTransfer(sd, sendbuffer, filename);
    }
    else
    {
        int div = 3;

        if (sb.st_size > 10485760L) /* File larger than 10 MB, checks every 64kB */
        {
            div = 32;
        }

        while (true)
        {
            memset(sendbuffer, 0, CF_BUFSIZE);

            if ((n_read = read(fd, sendbuffer, blocksize)) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "read", "read failed in EncryptGetFile");
                break;
            }

            off_t savedlen = sb.st_size;

            if (count++ % div == 0)       /* Don't do this too often */
            {
                CfDebug("Restatting %s - size %d\n", filename, n_read);
                if (stat(filename, &sb))
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "send", "Cannot stat file %s: (errno=%d) %s",
                            filename, errno, strerror(errno));
                    break;
                }
            }

            if (sb.st_size != savedlen)
            {
                AbortTransfer(sd, sendbuffer, filename);
                break;
            }

            total += n_read;

            if (n_read > 0)
            {
                EVP_EncryptInit_ex(&ctx, CfengineCipher(enctype), NULL, key, iv);

                if (!EVP_EncryptUpdate(&ctx, out, &cipherlen, sendbuffer, n_read))
                {
                    FailedTransfer(sd, sendbuffer, filename);
                    EVP_CIPHER_CTX_cleanup(&ctx);
                    close(fd);
                    return;
                }

                if (!EVP_EncryptFinal_ex(&ctx, out + cipherlen, &finlen))
                {
                    FailedTransfer(sd, sendbuffer, filename);
                    EVP_CIPHER_CTX_cleanup(&ctx);
                    close(fd);
                    return;
                }
            }

            if (total >= savedlen)
            {
                if (SendTransaction(sd, out, cipherlen + finlen, CF_DONE) == -1)
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "send", "Send failed in GetFile");
                    EVP_CIPHER_CTX_cleanup(&ctx);
                    close(fd);
                    return;
                }
                break;
            }
            else
            {
                if (SendTransaction(sd, out, cipherlen + finlen, CF_MORE) == -1)
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "send", "Send failed in GetFile");
                    close(fd);
                    EVP_CIPHER_CTX_cleanup(&ctx);
                    return;
                }
            }
        }
    }

    EVP_CIPHER_CTX_cleanup(&ctx);
    close(fd);
}

/**************************************************************/

static void CompareLocalHash(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer)
{
    unsigned char digest1[EVP_MAX_MD_SIZE + 1], digest2[EVP_MAX_MD_SIZE + 1];
    char filename[CF_BUFSIZE], rfilename[CF_BUFSIZE];
    char *sp;
    int i;

/* TODO - when safe change this proto string to sha2 */

    sscanf(recvbuffer, "MD5 %[^\n]", rfilename);

    sp = recvbuffer + strlen(recvbuffer) + CF_SMALL_OFFSET;

    for (i = 0; i < CF_DEFAULT_DIGEST_LEN; i++)
    {
        digest1[i] = *sp++;
    }

    memset(sendbuffer, 0, CF_BUFSIZE);

    TranslatePath(filename, rfilename);

    HashFile(filename, digest2, CF_DEFAULT_DIGEST);

    if ((HashesMatch(digest1, digest2, CF_DEFAULT_DIGEST)) || (HashesMatch(digest1, digest2, HASH_METHOD_MD5)))
    {
        sprintf(sendbuffer, "%s", CFD_FALSE);
        CfDebug("Hashes matched ok\n");
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
    }
    else
    {
        sprintf(sendbuffer, "%s", CFD_TRUE);
        CfDebug("Hashes didn't match\n");
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
    }
}

/**************************************************************/

static void GetServerLiteral(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer, int encrypted)
{
    char handle[CF_BUFSIZE], out[CF_BUFSIZE];
    int cipherlen;

    sscanf(recvbuffer, "VAR %255[^\n]", handle);

    if (ReturnLiteralData(handle, out))
    {
        memset(sendbuffer, 0, CF_BUFSIZE);
        snprintf(sendbuffer, CF_BUFSIZE - 1, "%s", out);
    }
    else
    {
        memset(sendbuffer, 0, CF_BUFSIZE);
        snprintf(sendbuffer, CF_BUFSIZE - 1, "BAD: Not found");
    }

    if (encrypted)
    {
        cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, strlen(sendbuffer) + 1);
        SendTransaction(conn->sd_reply, out, cipherlen, CF_DONE);
    }
    else
    {
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
    }
}

/********************************************************************/

static int GetServerQuery(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer)
{
    char query[CF_BUFSIZE];

    query[0] = '\0';
    sscanf(recvbuffer, "QUERY %255[^\n]", query);

    if (strlen(query) == 0)
    {
        return false;
    }
    
#ifdef HAVE_NOVA
    return Nova_ReturnQueryData(conn, query);
#else
    return false;
#endif
}

/**************************************************************/

static void ReplyServerContext(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer, int encrypted,
                               Item *classes)
{
    char out[CF_BUFSIZE];
    int cipherlen;
    Item *ip;

    memset(sendbuffer, 0, CF_BUFSIZE);

    for (ip = classes; ip != NULL; ip = ip->next)
    {
        if (strlen(sendbuffer) + strlen(ip->name) < CF_BUFSIZE - 3)
        {
            strcat(sendbuffer, ip->name);
            strcat(sendbuffer, ",");
        }
        else
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Overflow in context grab");
            break;
        }
    }

    DeleteItemList(classes);

    if (encrypted)
    {
        cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, strlen(sendbuffer) + 1);
        SendTransaction(conn->sd_reply, out, cipherlen, CF_DONE);
    }
    else
    {
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
    }
}

/**************************************************************/

static int CfOpenDirectory(ServerConnectionState *conn, char *sendbuffer, char *oldDirname)
{
    Dir *dirh;
    const struct dirent *dirp;
    int offset;
    char dirname[CF_BUFSIZE];

    TranslatePath(dirname, oldDirname);

    CfDebug("CfOpenDirectory(%s)\n", dirname);

    if (!IsAbsoluteFileName(dirname))
    {
        sprintf(sendbuffer, "BAD: request to access a non-absolute filename\n");
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
        return -1;
    }

    if ((dirh = OpenDirLocal(dirname)) == NULL)
    {
        CfDebug("cfengine, couldn't open dir %s\n", dirname);
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: cfengine, couldn't open dir %s\n", dirname);
        SendTransaction(conn->sd_reply, sendbuffer, 0, CF_DONE);
        return -1;
    }

/* Pack names for transmission */

    memset(sendbuffer, 0, CF_BUFSIZE);

    offset = 0;

    for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
    {
        if (strlen(dirp->d_name) + 1 + offset >= CF_BUFSIZE - CF_MAXLINKSIZE)
        {
            SendTransaction(conn->sd_reply, sendbuffer, offset + 1, CF_MORE);
            offset = 0;
            memset(sendbuffer, 0, CF_BUFSIZE);
        }

        strncpy(sendbuffer + offset, dirp->d_name, CF_MAXLINKSIZE);
        offset += strlen(dirp->d_name) + 1;     /* + zero byte separator */
    }

    strcpy(sendbuffer + offset, CFD_TERMINATOR);
    SendTransaction(conn->sd_reply, sendbuffer, offset + 2 + strlen(CFD_TERMINATOR), CF_DONE);
    CfDebug("END CfOpenDirectory(%s)\n", dirname);
    CloseDir(dirh);
    return 0;
}

/**************************************************************/

static int CfSecOpenDirectory(ServerConnectionState *conn, char *sendbuffer, char *dirname)
{
    Dir *dirh;
    const struct dirent *dirp;
    int offset, cipherlen;
    char out[CF_BUFSIZE];

    CfDebug("CfSecOpenDirectory(%s)\n", dirname);

    if (!IsAbsoluteFileName(dirname))
    {
        sprintf(sendbuffer, "BAD: request to access a non-absolute filename\n");
        cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, strlen(sendbuffer) + 1);
        SendTransaction(conn->sd_reply, out, cipherlen, CF_DONE);
        return -1;
    }

    if ((dirh = OpenDirLocal(dirname)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Couldn't open dir %s\n", dirname);
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: cfengine, couldn't open dir %s\n", dirname);
        cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, strlen(sendbuffer) + 1);
        SendTransaction(conn->sd_reply, out, cipherlen, CF_DONE);
        return -1;
    }

/* Pack names for transmission */

    memset(sendbuffer, 0, CF_BUFSIZE);

    offset = 0;

    for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
    {
        if (strlen(dirp->d_name) + 1 + offset >= CF_BUFSIZE - CF_MAXLINKSIZE)
        {
            cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, offset + 1);
            SendTransaction(conn->sd_reply, out, cipherlen, CF_MORE);
            offset = 0;
            memset(sendbuffer, 0, CF_BUFSIZE);
            memset(out, 0, CF_BUFSIZE);
        }

        strncpy(sendbuffer + offset, dirp->d_name, CF_MAXLINKSIZE);
        /* + zero byte separator */
        offset += strlen(dirp->d_name) + 1;
    }

    strcpy(sendbuffer + offset, CFD_TERMINATOR);

    cipherlen =
        EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, offset + 2 + strlen(CFD_TERMINATOR));
    SendTransaction(conn->sd_reply, out, cipherlen, CF_DONE);
    CfDebug("END CfSecOpenDirectory(%s)\n", dirname);
    CloseDir(dirh);
    return 0;
}

/***************************************************************/

static void Terminate(int sd)
{
    char buffer[CF_BUFSIZE];

    memset(buffer, 0, CF_BUFSIZE);

    strcpy(buffer, CFD_TERMINATOR);

    if (SendTransaction(sd, buffer, strlen(buffer) + 1, CF_DONE) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "send", "Unable to reply with terminator");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Unable to reply with terminator...\n");
    }
}

/***************************************************************/

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

static int OptionFound(char *args, char *pos, char *word)
/*
 * Returns true if the current position 'pos' in buffer
 * 'args' corresponds to the word 'word'.  Words are
 * separated by spaces.
 */
{
    size_t len;

    if (pos < args)
    {
        return false;
    }

/* Single options do not have to have spaces between */

    if ((strlen(word) == 2) && (strncmp(pos, word, 2) == 0))
    {
        return true;
    }

    len = strlen(word);

    if (strncmp(pos, word, len) != 0)
    {
        return false;
    }

    if (pos == args)
    {
        return true;
    }
    else if ((*(pos - 1) == ' ') && ((pos[len] == ' ') || (pos[len] == '\0')))
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**************************************************************/

static void RefuseAccess(ServerConnectionState *conn, char *sendbuffer, int size, char *errmesg)
{
    char *hostname, *username, *ipaddr;
    char *def = "?";

    if (strlen(conn->hostname) == 0)
    {
        hostname = def;
    }
    else
    {
        hostname = conn->hostname;
    }

    if (strlen(conn->username) == 0)
    {
        username = def;
    }
    else
    {
        username = conn->username;
    }

    if (strlen(conn->ipaddr) == 0)
    {
        ipaddr = def;
    }
    else
    {
        ipaddr = conn->ipaddr;
    }

    snprintf(sendbuffer, CF_BUFSIZE, "%s", CF_FAILEDSTR);
    SendTransaction(conn->sd_reply, sendbuffer, size, CF_DONE);

    CfOut(OUTPUT_LEVEL_INFORM, "", "From (host=%s,user=%s,ip=%s)", hostname, username, ipaddr);

    if (strlen(errmesg) > 0)
    {
        if (LOGCONNS)
        {
            CfOut(OUTPUT_LEVEL_LOG, "", "REFUSAL of request from connecting host: (%s)", errmesg);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "REFUSAL of request from connecting host: (%s)", errmesg);
        }
    }
}

/***************************************************************/

static int TransferRights(char *filename, int sd, ServerFileGetState *args, char *sendbuffer, struct stat *sb)
{
#ifdef __MINGW32__
    SECURITY_DESCRIPTOR *secDesc;
    SID *ownerSid;

    if (GetNamedSecurityInfo
        (filename, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, (PSID *) & ownerSid, NULL, NULL, NULL,
         (void **)&secDesc) == ERROR_SUCCESS)
    {
        if (IsValidSid((args->connect)->sid) && EqualSid(ownerSid, (args->connect)->sid))
        {
            CfDebug("Caller %s is the owner of the file\n", (args->connect)->username);
        }
        else
        {
            // If the process doesn't own the file, we can access if we are
            // root AND granted root map

            LocalFree(secDesc);

            if (args->connect->maproot)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Caller %s not owner of \"%s\", but mapping privilege\n",
                      (args->connect)->username, filename);
                return true;
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "!! Remote user denied right to file \"%s\" (consider maproot?)", filename);
                return false;
            }
        }

        LocalFree(secDesc);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_ERROR, "GetNamedSecurityInfo", "!! Could not retreive existing owner of \"%s\"", filename);
        return false;
    }

#else

    uid_t uid = (args->connect)->uid;

    if ((uid != 0) && (!args->connect->maproot))    /* should remote root be local root */
    {
        if (sb->st_uid == uid)
        {
            CfDebug("Caller %s is the owner of the file\n", (args->connect)->username);
        }
        else
        {
            if (sb->st_mode & S_IROTH)
            {
                CfDebug("Caller %s not owner of the file but permission granted\n", (args->connect)->username);
            }
            else
            {
                CfDebug("Caller %s is not the owner of the file\n", (args->connect)->username);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "!! Remote user denied right to file \"%s\" (consider maproot?)", filename);
                return false;
            }
        }
    }
#endif

    return true;
}

/***************************************************************/

static void AbortTransfer(int sd, char *sendbuffer, char *filename)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Aborting transfer of file due to source changes\n");

    snprintf(sendbuffer, CF_BUFSIZE, "%s%s: %s", CF_CHANGEDSTR1, CF_CHANGEDSTR2, filename);

    if (SendTransaction(sd, sendbuffer, 0, CF_DONE) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "send", "Send failed in GetFile");
    }
}

/***************************************************************/

static void FailedTransfer(int sd, char *sendbuffer, char *filename)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Transfer failure\n");

    snprintf(sendbuffer, CF_BUFSIZE, "%s", CF_FAILEDSTR);

    if (SendTransaction(sd, sendbuffer, 0, CF_DONE) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "send", "Send failed in GetFile");
    }
}

/***************************************************************/

static void ReplyNothing(ServerConnectionState *conn)
{
    char buffer[CF_BUFSIZE];

    snprintf(buffer, CF_BUFSIZE, "Hello %s (%s), nothing relevant to do here...\n\n", conn->hostname, conn->ipaddr);

    if (SendTransaction(conn->sd_reply, buffer, 0, CF_DONE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "send", "Unable to send transaction");
    }
}

/***************************************************************/

static int CheckStoreKey(ServerConnectionState *conn, RSA *key)
{
    RSA *savedkey;
    char udigest[CF_MAXVARSIZE];

    ThreadLock(cft_output);
    snprintf(udigest, CF_MAXVARSIZE - 1, "%s", HashPrint(CF_DEFAULT_DIGEST, conn->digest));
    ThreadUnlock(cft_output);

    if ((savedkey = HavePublicKey(conn->username, MapAddress(conn->ipaddr), udigest)))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "A public key was already known from %s/%s - no trust required\n", conn->hostname,
              conn->ipaddr);

        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Adding IP %s to SkipVerify - no need to check this if we have a key\n", conn->ipaddr);
        IdempPrependItem(&SV.skipverify, MapAddress(conn->ipaddr), NULL);

        if ((BN_cmp(savedkey->e, key->e) == 0) && (BN_cmp(savedkey->n, key->n) == 0))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "The public key identity was confirmed as %s@%s\n", conn->username, conn->hostname);
            SendTransaction(conn->sd_reply, "OK: key accepted", 0, CF_DONE);
            RSA_free(savedkey);
            return true;
        }
    }

/* Finally, if we're still here, we should consider trusting a new key ... */

    if ((SV.trustkeylist != NULL) && (IsMatchItemIn(SV.trustkeylist, MapAddress(conn->ipaddr))))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s/%s was found in the list of hosts to trust\n", conn->hostname, conn->ipaddr);
        conn->trust = true;
        /* conn->maproot = false; ?? */
        SendTransaction(conn->sd_reply, "OK: unknown key was accepted on trust", 0, CF_DONE);
        SavePublicKey(conn->username, MapAddress(conn->ipaddr), udigest, key);
        return true;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "No previous key found, and unable to accept this one on trust\n");
        SendTransaction(conn->sd_reply, "BAD: key could not be accepted on trust", 0, CF_DONE);
        return false;
    }
}

/***************************************************************/
/* Toolkit/Class: conn                                         */
/***************************************************************/

static ServerConnectionState *NewConn(int sd)
{
    ServerConnectionState *conn;
    struct sockaddr addr;
    socklen_t size = sizeof(addr);

    if (getsockname(sd, &addr, &size) == -1)
       {
       return NULL;
       }
    
    ThreadLock(cft_system);

    conn = xmalloc(sizeof(ServerConnectionState));

    ThreadUnlock(cft_system);

    conn->sd_reply = sd;
    conn->id_verified = false;
    conn->rsa_auth = false;
    conn->trust = false;
    conn->hostname[0] = '\0';
    conn->ipaddr[0] = '\0';
    conn->username[0] = '\0';
    conn->session_key = NULL;
    conn->encryption_type = 'c';

    CfDebug("*** New socket [%d]\n", sd);

    return conn;
}

/***************************************************************/

static void DeleteConn(ServerConnectionState *conn)
{
    CfDebug("***Closing socket %d from %s\n", conn->sd_reply, conn->ipaddr);

    cf_closesocket(conn->sd_reply);

    if (conn->session_key != NULL)
    {
        free(conn->session_key);
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

    free((char *) conn);
}

/***************************************************************/
/* ERS                                                         */
/***************************************************************/

static int SafeOpen(char *filename)
{
    int fd;

    ThreadLock(cft_system);

    fd = open(filename, O_RDONLY);

    ThreadUnlock(cft_system);
    return fd;
}

/***************************************************************/

static int cfscanf(char *in, int len1, int len2, char *out1, char *out2, char *out3)
{
    int len3 = 0;
    char *sp;

    sp = in;
    memcpy(out1, sp, len1);
    out1[len1] = '\0';

    sp += len1 + 1;
    memcpy(out2, sp, len2);

    sp += len2 + 1;
    len3 = strlen(sp);
    memcpy(out3, sp, len3);
    out3[len3] = '\0';

    return (len1 + len2 + len3 + 2);
}
