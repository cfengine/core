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
bool cfnet_init(const char *tls_min_version, const char *ciphers)
{
    CryptoInitialize();
    return TLSClientInitialize(tls_min_version, ciphers);
}

void cfnet_shut()
{
    TLSDeInitialize();
    CryptoDeInitialize();
}

bool cfnet_IsInitialized()
{
    return TLSClientIsInitialized();
}

int CFENGINE_PORT = 5308;
char CFENGINE_PORT_STR[16] = "5308";

void DetermineCfenginePort()
{
    assert(sizeof(CFENGINE_PORT_STR) >= PRINTSIZE(CFENGINE_PORT));
    /* ... but we leave more space, as service names can be longer */

    errno = 0;
    struct servent *server = getservbyname(CFENGINE_SERVICE, "tcp");
    if (server != NULL)
    {
        CFENGINE_PORT = ntohs(server->s_port);
        snprintf(CFENGINE_PORT_STR, sizeof(CFENGINE_PORT_STR),
                 "%d", CFENGINE_PORT);
    }
    else if (errno == 0)
    {
        Log(LOG_LEVEL_VERBOSE,
            "No registered cfengine service, using default");
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE,
            "Unable to query services database, using default. (getservbyname: %s)",
            GetErrorStr());
    }
    Log(LOG_LEVEL_VERBOSE, "Default port for cfengine is %d", CFENGINE_PORT);
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

    const char *key_hash = KeyPrintableHash(conn_info->remote_key);

    if (ret == 1)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Server is TRUSTED, received key '%s' MATCHES stored one.",
            key_hash);
    }
    else   /* ret == 0 */
    {
        if (trust_server)             /* We're most probably bootstrapping. */
        {
            Log(LOG_LEVEL_NOTICE, "Trusting new key: %s", key_hash);
            SavePublicKey(username, KeyPrintableHash(conn_info->remote_key),
                          KeyRSA(conn_info->remote_key));
        }
        else
        {
            Log(LOG_LEVEL_ERR,
                "TRUST FAILED, server presented untrusted key: %s", key_hash);
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
    GetCurrentUserName(conn->username, sizeof(conn->username));
#else
    /* Always say "root" as username from windows. */
    strlcpy(conn->username, "root", sizeof(conn->username));
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
    case CF_PROTOCOL_UNDEFINED:
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
        cipherlen = EncryptString(out, in, strlen(in) + 1, conn->encryption_type, conn->session_key);
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

    Item *start = NULL, *end = NULL;                  /* NULL is empty list */
    while (true)
    {
        /* TODO check the CF_MORE flag, no need for CFD_TERMINATOR. */
        int nbytes = ReceiveTransaction(conn->conn_info, recvbuffer, NULL);

        /* If recv error or socket closed before receiving CFD_TERMINATOR. */
        if (nbytes == -1)
        {
            /* TODO mark connection in the cache as closed. */
            goto err;
        }

        if (encrypt)
        {
            memcpy(in, recvbuffer, nbytes);
            DecryptString(conn->encryption_type, in, recvbuffer,
                          conn->session_key, nbytes);
        }

        if (recvbuffer[0] == '\0')
        {
            Log(LOG_LEVEL_ERR,
                "Empty%s server packet when listing directory '%s'!",
                (start == NULL) ? " first" : "",
                dirname);
            goto err;
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
            EncryptString(out, in,
                          strlen(in) + CF_SMALL_OFFSET + CF_DEFAULT_DIGEST_LEN,
                          conn->encryption_type, conn->session_key);
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
        Log(LOG_LEVEL_VERBOSE, "Networking error, assuming different checksum");
        return true;
    }

    if (ReceiveTransaction(conn->conn_info, recvbuffer, NULL) == -1)
    {
        /* TODO mark connection in the cache as closed. */
        Log(LOG_LEVEL_ERR, "Failed receive. (ReceiveTransaction: %s)", GetErrorStr());
        Log(LOG_LEVEL_VERBOSE, "No answer from host, assuming different checksum");
        return true;
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
    int dd, blocksize = 2048, n_read = 0, plainlen, more = true, finlen, cnt = 0;
    int tosend, cipherlen = 0;
    char *buf, in[CF_BUFSIZE], out[CF_BUFSIZE], workbuf[CF_BUFSIZE], cfchangedstr[265];
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };
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
    cipherlen = EncryptString(out, in, strlen(in) + 1, conn->encryption_type, conn->session_key);
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

    bool   last_write_made_hole = false;
    size_t n_wrote_total        = 0;

    while (more)
    {
        if ((cipherlen = ReceiveTransaction(conn->conn_info, buf, &more)) == -1)
        {
            free(buf);
            return false;
        }

        cnt++;

        /* If the first thing we get is an error message, break. */

        if (n_wrote_total == 0 &&
            strncmp(buf + CF_INBAND_OFFSET, CF_FAILEDSTR, strlen(CF_FAILEDSTR)) == 0)
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

        n_read = plainlen + finlen;

        bool w_ok = FileSparseWrite(dd, workbuf, n_read,
                                    &last_write_made_hole);
        if (!w_ok)
        {
            Log(LOG_LEVEL_ERR,
                "Local disk write failed copying '%s:%s' to '%s'",
                conn->this_server, source, dest);
            free(buf);
            unlink(dest);
            close(dd);
            conn->error = true;
            EVP_CIPHER_CTX_cleanup(&crypto_ctx);
            return false;
        }

        n_wrote_total += n_read;
    }

    const bool do_sync = false;

    bool ret = FileSparseClose(dd, dest, do_sync,
                               n_wrote_total, last_write_made_hole);
    if (!ret)
    {
        unlink(dest);
        free(buf);
        EVP_CIPHER_CTX_cleanup(&crypto_ctx);
        return false;
    }

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

/* TODO finalise socket or TLS session in all cases that this function fails
 * and the transaction protocol is out of sync. */
int CopyRegularFileNet(const char *source, const char *dest, off_t size,
                       bool encrypt, AgentConnection *conn)
{
    char *buf, workbuf[CF_BUFSIZE], cfchangedstr[265];
    const int buf_size = 2048;

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

    size_t n_wrote_total        = 0;
    bool   last_write_made_hole = false;
    while (n_wrote_total < size)
    {
        int toget = MIN(size - n_wrote_total, buf_size);

        assert(toget > 0);

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

        /* TODO what if 0 < n_read < toget? Might happen with TLS. */

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

        if (n_wrote_total == 0
            && strncmp(buf, CF_FAILEDSTR, strlen(CF_FAILEDSTR) == 0))
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

        bool w_ok = FileSparseWrite(dd, buf, n_read,
                                    &last_write_made_hole);
        if (!w_ok)
        {
            Log(LOG_LEVEL_ERR,
                "Local disk write failed copying '%s:%s' to '%s'",
                conn->this_server, source, dest);
            free(buf);
            unlink(dest);
            close(dd);
            FlushFileStream(conn->conn_info->sd, size - n_wrote_total - n_read);
            conn->error = true;
            return false;
        }

        n_wrote_total += n_read;
    }

    const bool do_sync = false;

    bool ret = FileSparseClose(dd, dest, do_sync,
                               n_wrote_total, last_write_made_hole);
    if (!ret)
    {
        unlink(dest);
        free(buf);
        FlushFileStream(conn->conn_info->sd, size - n_wrote_total);
        return false;
    }

    free(buf);
    return true;
}
