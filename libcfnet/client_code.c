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

#include <cfnet.h>                                 /* struct ConnectionInfo */
#include <client_code.h>
#include <communication.h>
#include <connection_info.h>
#include <classic.h>                  /* RecvSocketStream */
#include <net.h>                      /* SendTransaction,ReceiveTransaction */
#include <tls_client.h>               /* TLSTry */
#include <tls_generic.h>              /* TLSVerifyPeer */
#include <dir.h>
#include <unix.h>
#include <dir_priv.h>                          /* AllocateDirentForFilename */
#include <client_protocol.h>
#include <crypto.h>         /* CryptoInitialize,SavePublicKey,EncryptString */
#include <logging.h>
#include <files_hashes.h>                                       /* HashFile */
#include <mutex.h>                                            /* ThreadLock */
#include <files_lib.h>                               /* FullWrite,safe_open */
#include <string_lib.h>                           /* MemSpan,MemSpanInverse */
#include <misc_lib.h>                                   /* ProgrammingError */
#include <printsize.h>                                         /* PRINTSIZE */

#include <lastseen.h>                                            /* LastSaw */


#define CFENGINE_SERVICE "cfengine"


/**
 * Initialize client's network library.
 */
bool cfnet_init()
{
    CryptoInitialize();

    if (TLSClientInitialize())
        return true;
    else
        return false;
}

void cfnet_shut()
{
    TLSDeInitialize();
    CryptoDeInitialize();
}


int CFENGINE_PORT = 5308;
char CFENGINE_PORT_STR[16] = "5308";

void DetermineCfenginePort()
{
    struct servent *server;
    assert(sizeof(CFENGINE_PORT_STR) >= PRINTSIZE(CFENGINE_PORT));
    /* ... but we leave more space, as service names can be longer */

    errno = 0;
    if ((server = getservbyname(CFENGINE_SERVICE, "tcp")) != NULL)
    {
        CFENGINE_PORT = ntohs(server->s_port);
        snprintf(CFENGINE_PORT_STR, sizeof(CFENGINE_PORT_STR),
                 "%d", CFENGINE_PORT);
    }
    else
    {
        if (errno == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "No registered cfengine service, using default");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Unable to query services database, using default. (getservbyname: %s)", GetErrorStr());
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Setting cfengine default port to %d", CFENGINE_PORT);
}

/**
 * @return 1 success, 0 auth/ID error, -1 other error
 */
int TLSConnect(ConnectionInfo *conn_info, bool trust_server,
               const char *ipaddr, const char *username)
{
    int ret;

    ret = TLSTry(conn_info);
    if (ret == -1)
    {
        return -1;
    }

    /* TODO username is local, fix. */
    ret = TLSVerifyPeer(conn_info, ipaddr, username);

    if (ret == -1)                                      /* error */
    {
        return -1;
    }

    if (ret == 1)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Server is TRUSTED, received key %s MATCHES stored one.",
            KeyPrintableHash(conn_info->remote_key));
    }
    else   /* ret == 0 */
    {
        if (trust_server)             /* We're most probably bootstrapping. */
        {
            Log(LOG_LEVEL_NOTICE, "Trusting new key: %s",
                KeyPrintableHash(conn_info->remote_key));
            SavePublicKey(username, KeyPrintableHash(conn_info->remote_key),
                          KeyRSA(conn_info->remote_key));
        }
        else
        {
            Log(LOG_LEVEL_ERR,
                "TRUST FAILED, server presented an untrusted key, dropping connection!");
            Log(LOG_LEVEL_ERR,
                "Rebootstrap the client if you really want to start trusting this new key");
            return -1;
        }
    }

    /* TLS CONNECTION IS ESTABLISHED, negotiate protocol version and send
     * identification data. */
    ret = TLSClientIdentificationDialog(conn_info, username);

    return ret;
}

/**
 * @NOTE if #flags.protocol_version is CF_PROTOCOL_UNDEFINED, then classic
 *       protocol is used by default.
 */
AgentConnection *ServerConnection(const char *server, const char *port,
                                  unsigned int connect_timeout,
                                  ConnectionFlags flags, int *err)
{
    AgentConnection *conn = NULL;
    int ret;
    *err = 0;

    conn = NewAgentConn(server, port, flags);

#if !defined(__MINGW32__)
    signal(SIGPIPE, SIG_IGN);

    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);

    /* FIXME: username is local */
    GetCurrentUserName(conn->username, CF_SMALLBUF);
#else
    /* Always say "root" as username from windows. */
    snprintf(conn->username, CF_SMALLBUF, "root");
#endif

    if (port == NULL || *port == '\0')
    {
        port = CFENGINE_PORT_STR;
    }

    char txtaddr[CF_MAX_IP_LEN] = "";
    conn->conn_info->sd = SocketConnect(server, port, connect_timeout,
                                        flags.force_ipv4,
                                        txtaddr, sizeof(txtaddr));
    if (conn->conn_info->sd == -1)
    {
        Log(LOG_LEVEL_INFO, "No server is responding on port: %s",
            port);
        DisconnectServer(conn);
        *err = -1;
        return NULL;
    }

    assert(sizeof(conn->remoteip) >= sizeof(txtaddr));
    strcpy(conn->remoteip, txtaddr);

    switch (flags.protocol_version)
    {
    case CF_PROTOCOL_TLS:

        /* Set the version to request during protocol negotiation. After
         * TLSConnect() it will have the version we finally ended up with. */
        conn->conn_info->protocol = CF_PROTOCOL_LATEST;

        ret = TLSConnect(conn->conn_info, flags.trust_server,
                         conn->remoteip, conn->username);

        if (ret == -1)                                      /* Error */
        {
            DisconnectServer(conn);
            *err = -1;
            return NULL;
        }
        else if (ret == 0)                             /* Auth/ID error */
        {
            DisconnectServer(conn);
            errno = EPERM;
            *err = -2;
            return NULL;
        }
        assert(ret == 1);

        conn->conn_info->status = CONNECTIONINFO_STATUS_ESTABLISHED;
        LastSaw1(conn->remoteip, KeyPrintableHash(conn->conn_info->remote_key),
                 LAST_SEEN_ROLE_CONNECT);
        break;

    case CF_PROTOCOL_UNDEFINED:
    case CF_PROTOCOL_CLASSIC:

        conn->conn_info->protocol = CF_PROTOCOL_CLASSIC;
        conn->encryption_type = CfEnterpriseOptions();

        if (!IdentifyAgent(conn->conn_info))
        {
            Log(LOG_LEVEL_ERR, "Id-authentication for '%s' failed", VFQNAME);
            errno = EPERM;
            DisconnectServer(conn);
            *err = -2; // auth err
            return NULL;
        }

        if (!AuthenticateAgent(conn, flags.trust_server))
        {
            Log(LOG_LEVEL_ERR, "Authentication dialogue with '%s' failed", server);
            errno = EPERM;
            DisconnectServer(conn);
            *err = -2; // auth err
            return NULL;
        }
        conn->conn_info->status = CONNECTIONINFO_STATUS_ESTABLISHED;
        break;

    default:
        ProgrammingError("ServerConnection: ProtocolVersion %d!",
                         flags.protocol_version);
    }

    conn->authenticated = true;
    return conn;
}

/*********************************************************************/

void DisconnectServer(AgentConnection *conn)
{
    /* Socket needs to be closed even after SSL_shutdown. */
    if (conn->conn_info->sd >= 0)                 /* Not INVALID or OFFLINE */
    {
        if (conn->conn_info->protocol >= CF_PROTOCOL_TLS &&
            conn->conn_info->ssl != NULL)
        {
            SSL_shutdown(conn->conn_info->ssl);
        }

        cf_closesocket(conn->conn_info->sd);
        conn->conn_info->sd = SOCKET_INVALID;
        Log(LOG_LEVEL_VERBOSE, "Connection to %s is closed", conn->remoteip);
    }
    DeleteAgentConn(conn);
}

/*********************************************************************/

static void NewStatCache(Stat *data, AgentConnection *conn)
{
    Stat *sp = xmemdup(data, sizeof(Stat));
    sp->next = conn->cache;
    conn->cache = sp;
}

static int FSWrite(const char *destination, int dd, const char *buf, size_t n_write)
{
    const void *cur = buf;
    const void *end = buf + n_write;

    while (cur < end)
    {
        const void *skip_span = MemSpan(cur, 0, end - cur);
        if (skip_span > cur)
        {
            if (lseek(dd, skip_span - cur, SEEK_CUR) < 0)
            {
                Log(LOG_LEVEL_ERR, "Copy failed (no space?) while copying to '%s' from network '%s'", destination, GetErrorStr());
                return false;
            }

            cur = skip_span;
        }

        const void *copy_span = MemSpanInverse(cur, 0, end - cur);
        if (copy_span > cur)
        {
            if (FullWrite(dd, cur, copy_span - cur) < 0)
            {
                Log(LOG_LEVEL_ERR, "Copy failed (no space?) while copying to '%s' from network '%s'", destination, GetErrorStr());
                return false;
            }

            cur = copy_span;
        }
    }

    return true;
}

static int CacheStat(const char *file, struct stat *statbuf, const char *stattype, AgentConnection *conn)
{
    Stat *sp;

    for (sp = conn->cache; sp != NULL; sp = sp->next)
    {
        /* TODO differentiate ports etc in this stat cache! */

        if ((strcmp(conn->this_server, sp->cf_server) == 0) && (strcmp(file, sp->cf_filename) == 0))
        {
            if (sp->cf_failed)  /* cached failure from cfopendir */
            {
                errno = EPERM;
                return -1;
            }

            if ((strcmp(stattype, "link") == 0) && (sp->cf_lmode != 0))
            {
                statbuf->st_mode = sp->cf_lmode;
            }
            else
            {
                statbuf->st_mode = sp->cf_mode;
            }

            statbuf->st_uid = sp->cf_uid;
            statbuf->st_gid = sp->cf_gid;
            statbuf->st_size = sp->cf_size;
            statbuf->st_atime = sp->cf_atime;
            statbuf->st_mtime = sp->cf_mtime;
            statbuf->st_ctime = sp->cf_ctime;
            statbuf->st_ino = sp->cf_ino;
            statbuf->st_dev = sp->cf_dev;
            statbuf->st_nlink = sp->cf_nlink;

            return 0;
        }
    }

    return 1;
}

int cf_remote_stat(const char *file, struct stat *buf, const char *stattype, bool encrypt, AgentConnection *conn)
/* If a link, this reads readlink and sends it back in the same
   package. It then caches the value for each copy command */
{
    char sendbuffer[CF_BUFSIZE];
    char recvbuffer[CF_BUFSIZE];
    char in[CF_BUFSIZE], out[CF_BUFSIZE];
    int ret, tosend, cipherlen;
    time_t tloc;

    memset(recvbuffer, 0, CF_BUFSIZE);

    if (strlen(file) > CF_BUFSIZE - 30)
    {
        Log(LOG_LEVEL_ERR, "Filename too long");
        return -1;
    }

    ret = CacheStat(file, buf, stattype, conn);

    if (ret != 1)
    {
        return ret;
    }

    if ((tloc = time((time_t *) NULL)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't read system clock");
    }

    sendbuffer[0] = '\0';

    /* We encrypt only for CLASSIC protocol. The TLS protocol is always over
     * encrypted layer, so it does not support encrypted (S*) commands. */
    encrypt = encrypt && conn->conn_info->protocol == CF_PROTOCOL_CLASSIC;

    if (encrypt)
    {
        if (conn->session_key == NULL)
        {
            Log(LOG_LEVEL_ERR, "Cannot do encrypted copy without keys (use cf-key)");
            return -1;
        }

        snprintf(in, CF_BUFSIZE - 1, "SYNCH %jd STAT %s", (intmax_t) tloc, file);
        cipherlen = EncryptString(conn->encryption_type, in, out, conn->session_key, strlen(in) + 1);
        snprintf(sendbuffer, CF_BUFSIZE - 1, "SSYNCH %d", cipherlen);
        memcpy(sendbuffer + CF_PROTO_OFFSET, out, cipherlen);
        tosend = cipherlen + CF_PROTO_OFFSET;
    }
    else
    {
        snprintf(sendbuffer, CF_BUFSIZE, "SYNCH %jd STAT %s", (intmax_t) tloc, file);
        tosend = strlen(sendbuffer);
    }

    if (SendTransaction(conn->conn_info, sendbuffer, tosend, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_INFO, "Transmission failed/refused talking to %.255s:%.255s. (stat: %s)",
            conn->this_server, file, GetErrorStr());
        return -1;
    }

    if (ReceiveTransaction(conn->conn_info, recvbuffer, NULL) == -1)
    {
        /* TODO mark connection in the cache as closed. */
        return -1;
    }

    if (strstr(recvbuffer, "unsynchronized"))
    {
        Log(LOG_LEVEL_ERR,
            "Clocks differ too much to do copy by date (security), server reported: %s",
            recvbuffer + strlen("BAD: "));
        return -1;
    }

    if (BadProtoReply(recvbuffer))
    {
        Log(LOG_LEVEL_VERBOSE, "Server returned error: %s",
            recvbuffer + strlen("BAD: "));
        errno = EPERM;
        return -1;
    }

    if (OKProtoReply(recvbuffer))
    {
        Stat cfst;

        // use intmax_t here to provide enough space for large values coming over the protocol
        intmax_t d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12 = 0, d13 = 0;
        ret = sscanf(recvbuffer, "OK: "
               "%1" PRIdMAX     // 01 cfst.cf_type
               " %5" PRIdMAX    // 02 cfst.cf_mode
               " %14" PRIdMAX   // 03 cfst.cf_lmode
               " %14" PRIdMAX   // 04 cfst.cf_uid
               " %14" PRIdMAX   // 05 cfst.cf_gid
               " %18" PRIdMAX   // 06 cfst.cf_size
               " %14" PRIdMAX   // 07 cfst.cf_atime
               " %14" PRIdMAX   // 08 cfst.cf_mtime
               " %14" PRIdMAX   // 09 cfst.cf_ctime
               " %1" PRIdMAX    // 10 cfst.cf_makeholes
               " %14" PRIdMAX   // 11 cfst.cf_ino
               " %14" PRIdMAX   // 12 cfst.cf_nlink
               " %18" PRIdMAX,  // 13 cfst.cf_dev
               &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9, &d10, &d11, &d12, &d13);

        if (ret < 13)
        {
            Log(LOG_LEVEL_ERR, "Cannot read SYNCH reply from '%s', only %d/13 items parsed", conn->remoteip, ret );
            return -1;
        }

        cfst.cf_type = (FileType) d1;
        cfst.cf_mode = (mode_t) d2;
        cfst.cf_lmode = (mode_t) d3;
        cfst.cf_uid = (uid_t) d4;
        cfst.cf_gid = (gid_t) d5;
        cfst.cf_size = (off_t) d6;
        cfst.cf_atime = (time_t) d7;
        cfst.cf_mtime = (time_t) d8;
        cfst.cf_ctime = (time_t) d9;
        cfst.cf_makeholes = (char) d10;
        cfst.cf_ino = d11;
        cfst.cf_nlink = d12;
        cfst.cf_dev = (dev_t)d13;

        /* Use %?d here to avoid memory overflow attacks */

        memset(recvbuffer, 0, CF_BUFSIZE);

        if (ReceiveTransaction(conn->conn_info, recvbuffer, NULL) == -1)
        {
            /* TODO mark connection in the cache as closed. */
            return -1;
        }

        if (strlen(recvbuffer) > 3)
        {
            cfst.cf_readlink = xstrdup(recvbuffer + 3);
        }
        else
        {
            cfst.cf_readlink = NULL;
        }

        switch (cfst.cf_type)
        {
        case FILE_TYPE_REGULAR:
            cfst.cf_mode |= (mode_t) S_IFREG;
            break;
        case FILE_TYPE_DIR:
            cfst.cf_mode |= (mode_t) S_IFDIR;
            break;
        case FILE_TYPE_CHAR_:
            cfst.cf_mode |= (mode_t) S_IFCHR;
            break;
        case FILE_TYPE_FIFO:
            cfst.cf_mode |= (mode_t) S_IFIFO;
            break;
        case FILE_TYPE_SOCK:
            cfst.cf_mode |= (mode_t) S_IFSOCK;
            break;
        case FILE_TYPE_BLOCK:
            cfst.cf_mode |= (mode_t) S_IFBLK;
            break;
        case FILE_TYPE_LINK:
            cfst.cf_mode |= (mode_t) S_IFLNK;
            break;
        }

        cfst.cf_filename = xstrdup(file);
        cfst.cf_server = xstrdup(conn->this_server);
        cfst.cf_failed = false;

        if (cfst.cf_lmode != 0)
        {
            cfst.cf_lmode |= (mode_t) S_IFLNK;
        }

        NewStatCache(&cfst, conn);

        if ((cfst.cf_lmode != 0) && (strcmp(stattype, "link") == 0))
        {
            buf->st_mode = cfst.cf_lmode;
        }
        else
        {
            buf->st_mode = cfst.cf_mode;
        }

        buf->st_uid = cfst.cf_uid;
        buf->st_gid = cfst.cf_gid;
        buf->st_size = cfst.cf_size;
        buf->st_mtime = cfst.cf_mtime;
        buf->st_ctime = cfst.cf_ctime;
        buf->st_atime = cfst.cf_atime;
        buf->st_ino = cfst.cf_ino;
        buf->st_dev = cfst.cf_dev;
        buf->st_nlink = cfst.cf_nlink;

        return 0;
    }

    Log(LOG_LEVEL_ERR, "Transmission refused or failed statting '%s', got '%s'", file, recvbuffer);
    errno = EPERM;
    return -1;
}

/*********************************************************************/

/* TODO only a server_name is not enough for stat'ing of files... */
const Stat *StatCacheLookup(AgentConnection *conn, const char *server_name, const char *file_name)
{
    for (const Stat *sp = conn->cache; sp != NULL; sp = sp->next)
    {
        if (strcmp(server_name, sp->cf_server) == 0 &&
            strcmp(file_name, sp->cf_filename) == 0)
        {
            return sp;
        }
    }

    return NULL;
}

/* Returning NULL (an empty list) does not mean empty directory but ERROR,
 * since every directory has to contain at least . and .. */
Item *RemoteDirList(const char *dirname, bool encrypt, AgentConnection *conn)
{
    char sendbuffer[CF_BUFSIZE];
    char recvbuffer[CF_BUFSIZE];
    char in[CF_BUFSIZE];
    char out[CF_BUFSIZE];
    int cipherlen = 0, tosend;

    if (strlen(dirname) > CF_BUFSIZE - 20)
    {
        Log(LOG_LEVEL_ERR, "Directory name too long");
        return NULL;
    }

    /* We encrypt only for CLASSIC protocol. The TLS protocol is always over
     * encrypted layer, so it does not support encrypted (S*) commands. */
    encrypt = encrypt && conn->conn_info->protocol == CF_PROTOCOL_CLASSIC;

    if (encrypt)
    {
        if (conn->session_key == NULL)
        {
            Log(LOG_LEVEL_ERR, "Cannot do encrypted copy without keys (use cf-key)");
            return NULL;
        }

        snprintf(in, CF_BUFSIZE, "OPENDIR %s", dirname);
        cipherlen = EncryptString(conn->encryption_type, in, out, conn->session_key, strlen(in) + 1);
        snprintf(sendbuffer, CF_BUFSIZE - 1, "SOPENDIR %d", cipherlen);
        memcpy(sendbuffer + CF_PROTO_OFFSET, out, cipherlen);
        tosend = cipherlen + CF_PROTO_OFFSET;
    }
    else
    {
        snprintf(sendbuffer, CF_BUFSIZE, "OPENDIR %s", dirname);
        tosend = strlen(sendbuffer);
    }

    if (SendTransaction(conn->conn_info, sendbuffer, tosend, CF_DONE) == -1)
    {
        return NULL;
    }

    Item *start = NULL, *end = NULL;                  /* NULL == empty list */
    while (true)
    {
        /* TODO check the CF_MORE flag, no need for CFD_TERMINATOR. */
        int nbytes = ReceiveTransaction(conn->conn_info, recvbuffer, NULL);

        /* If recv error or socket closed before receiving CFD_TERMINATOR. */
        if (nbytes == -1 || nbytes == 0)
        {
            /* TODO mark connection in the cache as closed. */
            goto err;
        }

        if (recvbuffer[0] == '\0')
        {
            Log(LOG_LEVEL_ERR,
                "Empty%s server packet when listing directory '%s'!",
                (start == NULL) ? " first" : "",
                dirname);
            goto err;
        }

        if (encrypt)
        {
            memcpy(in, recvbuffer, nbytes);
            DecryptString(conn->encryption_type, in, recvbuffer,
                          conn->session_key, nbytes);
        }

        if (FailedProtoReply(recvbuffer))
        {
            Log(LOG_LEVEL_INFO, "Network access to '%s:%s' denied", conn->this_server, dirname);
            goto err;
        }

        if (BadProtoReply(recvbuffer))
        {
            Log(LOG_LEVEL_INFO, "%s", recvbuffer + strlen("BAD: "));
            goto err;
        }

        /* Double '\0' means end of packet. */
        for (char *sp = recvbuffer; *sp != '\0'; sp += strlen(sp) + 1)
        {
            if (strcmp(sp, CFD_TERMINATOR) == 0)      /* end of all packets */
            {
                return start;
            }

            Item *ip = xcalloc(1, sizeof(Item));
            ip->name = (char *) AllocateDirentForFilename(sp);

            if (start == NULL)  /* First element */
            {
                start = ip;
                end = ip;
            }
            else
            {
                end->next = ip;
                end = ip;
            }
        }
    }

    return start;

  err:                                                         /* free list */
    for (Item *ip = start; ip != NULL; ip = start)
    {
        start = ip->next;
        free(ip->name);
        free(ip);
    }

    return NULL;
}

/*********************************************************************/

int CompareHashNet(const char *file1, const char *file2, bool encrypt, AgentConnection *conn)
{
    unsigned char d[EVP_MAX_MD_SIZE + 1];
    char *sp, sendbuffer[CF_BUFSIZE], recvbuffer[CF_BUFSIZE], in[CF_BUFSIZE], out[CF_BUFSIZE];
    int i, tosend, cipherlen;

    HashFile(file2, d, CF_DEFAULT_DIGEST);

    memset(recvbuffer, 0, CF_BUFSIZE);

    /* We encrypt only for CLASSIC protocol. The TLS protocol is always over
     * encrypted layer, so it does not support encrypted (S*) commands. */
    encrypt = encrypt && conn->conn_info->protocol == CF_PROTOCOL_CLASSIC;

    if (encrypt)
    {
        snprintf(in, CF_BUFSIZE, "MD5 %s", file1);

        sp = in + strlen(in) + CF_SMALL_OFFSET;

        for (i = 0; i < CF_DEFAULT_DIGEST_LEN; i++)
        {
            *sp++ = d[i];
        }

        cipherlen =
            EncryptString(conn->encryption_type, in, out, conn->session_key,
                          strlen(in) + CF_SMALL_OFFSET + CF_DEFAULT_DIGEST_LEN);
        snprintf(sendbuffer, CF_BUFSIZE, "SMD5 %d", cipherlen);
        memcpy(sendbuffer + CF_PROTO_OFFSET, out, cipherlen);
        tosend = cipherlen + CF_PROTO_OFFSET;
    }
    else
    {
        snprintf(sendbuffer, CF_BUFSIZE, "MD5 %s", file1);
        sp = sendbuffer + strlen(sendbuffer) + CF_SMALL_OFFSET;

        for (i = 0; i < CF_DEFAULT_DIGEST_LEN; i++)
        {
            *sp++ = d[i];
        }

        tosend = strlen(sendbuffer) + CF_SMALL_OFFSET + CF_DEFAULT_DIGEST_LEN;
    }

    if (SendTransaction(conn->conn_info, sendbuffer, tosend, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed send. (SendTransaction: %s)", GetErrorStr());
        return false;
    }

    if (ReceiveTransaction(conn->conn_info, recvbuffer, NULL) == -1)
    {
        /* TODO mark connection in the cache as closed. */
        Log(LOG_LEVEL_ERR, "Failed receive. (ReceiveTransaction: %s)", GetErrorStr());
        Log(LOG_LEVEL_VERBOSE,  "No answer from host, assuming checksum ok to avoid remote copy for now...");
        return false;
    }

    if (strcmp(CFD_TRUE, recvbuffer) == 0)
    {
        return true;            /* mismatch */
    }
    else
    {
        return false;
    }

/* Not reached */
}

/*********************************************************************/

int EncryptCopyRegularFileNet(const char *source, const char *dest, off_t size, AgentConnection *conn)
{
    int dd, blocksize = 2048, n_read = 0, towrite, plainlen, more = true, finlen, cnt = 0;
    int tosend, cipherlen = 0;
    char *buf, in[CF_BUFSIZE], out[CF_BUFSIZE], workbuf[CF_BUFSIZE], cfchangedstr[265];
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };
    long n_read_total = 0;
    EVP_CIPHER_CTX crypto_ctx;

    snprintf(cfchangedstr, 255, "%s%s", CF_CHANGEDSTR1, CF_CHANGEDSTR2);

    if ((strlen(dest) > CF_BUFSIZE - 20))
    {
        Log(LOG_LEVEL_ERR, "Filename too long");
        return false;
    }

    unlink(dest);                /* To avoid link attacks */

    if ((dd = safe_open(dest, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY, 0600)) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Copy from server '%s' to destination '%s' failed (open: %s)",
            conn->this_server, dest, GetErrorStr());
        unlink(dest);
        return false;
    }

    if (size == 0)
    {
        // No sense in copying an empty file
        close(dd);
        return true;
    }

    workbuf[0] = '\0';
    EVP_CIPHER_CTX_init(&crypto_ctx);

    snprintf(in, CF_BUFSIZE - CF_PROTO_OFFSET, "GET dummykey %s", source);
    cipherlen = EncryptString(conn->encryption_type, in, out, conn->session_key, strlen(in) + 1);
    snprintf(workbuf, CF_BUFSIZE, "SGET %4d %4d", cipherlen, blocksize);
    memcpy(workbuf + CF_PROTO_OFFSET, out, cipherlen);
    tosend = cipherlen + CF_PROTO_OFFSET;

/* Send proposition C0 - query */

    if (SendTransaction(conn->conn_info, workbuf, tosend, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't send data. (SendTransaction: %s)", GetErrorStr());
        close(dd);
        return false;
    }

    buf = xmalloc(CF_BUFSIZE + sizeof(int));

    n_read_total = 0;

    while (more)
    {
        if ((cipherlen = ReceiveTransaction(conn->conn_info, buf, &more)) == -1)
        {
            free(buf);
            return false;
        }

        cnt++;

        /* If the first thing we get is an error message, break. */

        if ((n_read_total == 0) && (strncmp(buf + CF_INBAND_OFFSET, CF_FAILEDSTR, strlen(CF_FAILEDSTR)) == 0))
        {
            Log(LOG_LEVEL_INFO, "Network access to '%s:%s' denied", conn->this_server, source);
            close(dd);
            free(buf);
            return false;
        }

        if (strncmp(buf + CF_INBAND_OFFSET, cfchangedstr, strlen(cfchangedstr)) == 0)
        {
            Log(LOG_LEVEL_INFO, "Source '%s:%s' changed while copying", conn->this_server, source);
            close(dd);
            free(buf);
            return false;
        }

        EVP_DecryptInit_ex(&crypto_ctx, CfengineCipher(CfEnterpriseOptions()), NULL, conn->session_key, iv);

        if (!EVP_DecryptUpdate(&crypto_ctx, workbuf, &plainlen, buf, cipherlen))
        {
            close(dd);
            free(buf);
            return false;
        }

        if (!EVP_DecryptFinal_ex(&crypto_ctx, workbuf + plainlen, &finlen))
        {
            close(dd);
            free(buf);
            return false;
        }

        towrite = n_read = plainlen + finlen;

        n_read_total += n_read;

        if (!FSWrite(dest, dd, workbuf, towrite))
        {
            Log(LOG_LEVEL_ERR, "Local disk write failed copying '%s:%s' to '%s:%s'",
                conn->this_server, source, dest, GetErrorStr());
            if (conn)
            {
                conn->error = true;
            }
            free(buf);
            unlink(dest);
            close(dd);
            EVP_CIPHER_CTX_cleanup(&crypto_ctx);
            return false;
        }
    }

    /* If the file ends with a `hole', something needs to be written at
       the end.  Otherwise the kernel would truncate the file at the end
       of the last write operation. Write a null character and truncate
       it again.  */

    if (ftruncate(dd, n_read_total) < 0)
    {
        Log(LOG_LEVEL_ERR, "Copy failed (no space?) while copying '%s' from network '%s'",
            dest, GetErrorStr());
        free(buf);
        unlink(dest);
        close(dd);
        EVP_CIPHER_CTX_cleanup(&crypto_ctx);
        return false;
    }

    close(dd);
    free(buf);
    EVP_CIPHER_CTX_cleanup(&crypto_ctx);
    return true;
}

static void FlushFileStream(int sd, int toget)
{
    int i;
    char buffer[2];

    Log(LOG_LEVEL_VERBOSE, "Flushing rest of file...%d bytes", toget);

    for (i = 0; i < toget; i++)
    {
        recv(sd, buffer, 1, 0); /* flush to end of current file */
    }
}

int CopyRegularFileNet(const char *source, const char *dest, off_t size,
                       bool encrypt, AgentConnection *conn)
{
    char *buf, workbuf[CF_BUFSIZE], cfchangedstr[265];
    const int buf_size = 2048;

    off_t n_read_total = 0;
    EVP_CIPHER_CTX crypto_ctx;

    /* We encrypt only for CLASSIC protocol. The TLS protocol is always over
     * encrypted layer, so it does not support encrypted (S*) commands. */
    encrypt = encrypt && conn->conn_info->protocol == CF_PROTOCOL_CLASSIC;

    if (encrypt)
    {
        return EncryptCopyRegularFileNet(source, dest, size, conn);
    }

    snprintf(cfchangedstr, 255, "%s%s", CF_CHANGEDSTR1, CF_CHANGEDSTR2);

    if ((strlen(dest) > CF_BUFSIZE - 20))
    {
        Log(LOG_LEVEL_ERR, "Filename too long");
        return false;
    }

    unlink(dest);                /* To avoid link attacks */

    int dd = safe_open(dest, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY, 0600);
    if (dd == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Copy from server '%s' to destination '%s' failed (open: %s)",
            conn->this_server, dest, GetErrorStr());
        unlink(dest);
        return false;
    }



    workbuf[0] = '\0';
    int tosend = snprintf(workbuf, CF_BUFSIZE, "GET %d %s", buf_size, source);
    if (tosend <= 0 || tosend >= CF_BUFSIZE)
    {
        Log(LOG_LEVEL_ERR, "Failed to compose GET command for file %s",
            source);
        close(dd);
        return false;
    }

    /* Send proposition C0 */

    if (SendTransaction(conn->conn_info, workbuf, tosend, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't send GET command");
        close(dd);
        return false;
    }

    buf = xmalloc(CF_BUFSIZE + sizeof(int));    /* Note CF_BUFSIZE not buf_size !! */

    Log(LOG_LEVEL_VERBOSE, "Copying remote file '%s:%s', expecting %jd bytes",
          conn->this_server, source, (intmax_t)size);

    n_read_total = 0;
    while (n_read_total < size)
    {
        int toget = MIN(size - n_read_total, buf_size);

        assert(toget != 0);

        /* Stage C1 - receive */
        int n_read;
        switch(conn->conn_info->protocol)
        {
        case CF_PROTOCOL_CLASSIC:
            n_read = RecvSocketStream(conn->conn_info->sd, buf, toget);
            break;
        case CF_PROTOCOL_TLS:
            n_read = TLSRecv(conn->conn_info->ssl, buf, toget);
            break;
        default:
            UnexpectedError("CopyRegularFileNet: ProtocolVersion %d!",
                            conn->conn_info->protocol);
            n_read = -1;
        }

        if (n_read <= 0)
        {
            /* This may happen on race conditions, where the file has shrunk
             * since we asked for its size in SYNCH ... STAT source */

            Log(LOG_LEVEL_ERR,
                "Error in client-server stream, has %s:%s shrunk? (code %d)",
                conn->this_server, source, n_read);
            close(dd);
            free(buf);
            return false;
        }

        /* If the first thing we get is an error message, break. */

        if ((n_read_total == 0) && (strncmp(buf, CF_FAILEDSTR, strlen(CF_FAILEDSTR)) == 0))
        {
            Log(LOG_LEVEL_INFO, "Network access to '%s:%s' denied",
                conn->this_server, source);
            close(dd);
            free(buf);
            return false;
        }

        if (strncmp(buf, cfchangedstr, strlen(cfchangedstr)) == 0)
        {
            Log(LOG_LEVEL_INFO, "Source '%s:%s' changed while copying",
                conn->this_server, source);
            close(dd);
            free(buf);
            return false;
        }


        /* Check for mismatch between encryption here and on server. */

        int value = -1;
        sscanf(buf, "t %d", &value);

        if ((value > 0) && (strncmp(buf + CF_INBAND_OFFSET, "BAD: ", 5) == 0))
        {
            Log(LOG_LEVEL_INFO, "Network access to cleartext '%s:%s' denied",
                conn->this_server, source);
            close(dd);
            free(buf);
            return false;
        }

        if (!FSWrite(dest, dd, buf, n_read))
        {
            Log(LOG_LEVEL_ERR,
                "Local disk write failed copying '%s:%s' to '%s'. (FSWrite: %s)",
                conn->this_server, source, dest, GetErrorStr());
            if (conn)
            {
                conn->error = true;
            }
            free(buf);
            unlink(dest);
            close(dd);
            FlushFileStream(conn->conn_info->sd, size - n_read_total);
            EVP_CIPHER_CTX_cleanup(&crypto_ctx);
            return false;
        }

        n_read_total += n_read;
    }

    /* If the file ends with a `hole', something needs to be written at
       the end.  Otherwise the kernel would truncate the file at the end
       of the last write operation. Write a null character and truncate
       it again.  */

    if (ftruncate(dd, n_read_total) < 0)
    {
        Log(LOG_LEVEL_ERR, "Copy failed (no space?) while copying '%s' from network '%s'",
            dest, GetErrorStr());
        free(buf);
        unlink(dest);
        close(dd);
        FlushFileStream(conn->conn_info->sd, size - n_read_total);
        return false;
    }

    close(dd);
    free(buf);
    return true;
}
