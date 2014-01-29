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


#include <tls_server.h>
#include <server_common.h>

#include <crypto.h>                                        /* DecryptString */
#include <conversion.h>
#include <signals.h>
#include <item_lib.h>                 /* IsMatchItemIn */
#include <lastseen.h>                 /* LastSaw1 */
#include <net.h>                      /* SendTransaction,ReceiveTransaction */
#include <tls_generic.h>              /* TLSSend */
#include <cf-serverd-enterprise-stubs.h>
#include <connection_info.h>
#include <string_lib.h>                                  /* StringMatchFull */
#include <known_dirs.h>
#include <file_lib.h>                                           /* IsDirReal */

#include "server_access.h"          /* access_CheckResource, acl_CheckExact */


static SSL_CTX *SSLSERVERCONTEXT = NULL; /* GLOBAL_X */
static X509 *SSLSERVERCERT = NULL; /* GLOBAL_X */


/**
 * @warning Make sure you've called CryptoInitialize() first!
 */
bool ServerTLSInitialize()
{
    int ret;

    /* OpenSSL is needed for our new protocol over TLS. */
    SSL_library_init();
    SSL_load_error_strings();

    assert(SSLSERVERCONTEXT == NULL);
    SSLSERVERCONTEXT = SSL_CTX_new(SSLv23_server_method());
    if (SSLSERVERCONTEXT == NULL)
    {
        Log(LOG_LEVEL_ERR, "SSL_CTX_new: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err1;
    }

    /* Use only TLS v1 or later.
       TODO option for SSL_OP_NO_TLSv{1,1_1} */
    SSL_CTX_set_options(SSLSERVERCONTEXT,
                        SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

    /*
     * CFEngine is not a web server so we don't need many ciphers. We only
     * allow a safe but very common subset by default, extensible via
     * "allowciphers" in body server control. By default allow:
     *     AES256-GCM-SHA384: most high-grade RSA-based cipher from TLSv1.2
     *     AES256-SHA: most backwards compatible but high-grade, from SSLv3
     */
    const char *cipher_list = SV.allowciphers;
    if (cipher_list == NULL)
    {
        cipher_list ="AES256-GCM-SHA384:AES256-SHA";
    }

    ret = SSL_CTX_set_cipher_list(SSLSERVERCONTEXT, cipher_list);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR,
            "No valid ciphers in cipher list: %s",
            cipher_list);
    }

    /* Never bother with retransmissions, SSL_write() should
     * always either write the whole amount or fail. */
    SSL_CTX_set_mode(SSLSERVERCONTEXT, SSL_MODE_AUTO_RETRY);

    if (PRIVKEY == NULL || PUBKEY == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "No public/private key pair is loaded, create one with cf-key");
        goto err2;
    }

    assert(SSLSERVERCERT == NULL);
    /* Create cert into memory and load it into SSL context. */
    SSLSERVERCERT = TLSGenerateCertFromPrivKey(PRIVKEY);
    if (SSLSERVERCERT == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to generate in-memory-certificate from private key");
        goto err2;
    }

    SSL_CTX_use_certificate(SSLSERVERCONTEXT, SSLSERVERCERT);

    ret = SSL_CTX_use_RSAPrivateKey(SSLSERVERCONTEXT, PRIVKEY);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Failed to use RSA private key: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err3;
    }

    /* Verify cert consistency. */
    ret = SSL_CTX_check_private_key(SSLSERVERCONTEXT);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Inconsistent key and TLS cert: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err3;
    }

    /* Set options to always request a certificate from the peer, either we
     * are client or server. */
    SSL_CTX_set_verify(SSLSERVERCONTEXT, SSL_VERIFY_PEER, NULL);
    /* Always accept that certificate, we do proper checking after TLS
     * connection is established since OpenSSL can't pass a connection
     * specific pointer to the callback (so we would have to lock).  */
    SSL_CTX_set_cert_verify_callback(SSLSERVERCONTEXT, TLSVerifyCallback, NULL);

    return true;

  err3:
    X509_free(SSLSERVERCERT);
    SSLSERVERCERT = NULL;
  err2:
    SSL_CTX_free(SSLSERVERCONTEXT);
    SSLSERVERCONTEXT = NULL;
  err1:
    return false;
}

/**
 * @brief Set the connection type to CLASSIC or TLS.

 * It is performed by peeking into the TLS connection to read the first bytes,
 * and if it's a CAUTH protocol command use the old protocol loop, else use
 * the TLS protocol loop.
 *
 * @return -1 in case of error, 1 otherwise.
 */
int ServerTLSPeek(ConnectionInfo *conn_info)
{
    assert(SSLSERVERCONTEXT != NULL && PRIVKEY != NULL && PUBKEY != NULL);

    /* This must be the first thing we run on an accepted connection. */
    assert(ConnectionInfoProtocolVersion(conn_info) == CF_PROTOCOL_UNDEFINED);

    const int peek_size = CF_INBAND_OFFSET + sizeof("CAUTH");

    char buf[peek_size];
    ssize_t got = recv(ConnectionInfoSocket(conn_info), buf, sizeof(buf), MSG_PEEK);
    if (got == -1)
    {
        Log(LOG_LEVEL_ERR, "TCP connection: %s", GetErrorStr());
        return -1;
    }
    else if (got == 0)
    {
        Log(LOG_LEVEL_ERR,
            "Peer closed TCP connection without sending data!");
        return -1;
    }
    else if (got < peek_size)
    {
        Log(LOG_LEVEL_WARNING,
            "Peer sent only %lld bytes! Considering the protocol as Classic",
            (long long)got);
        ConnectionInfoSetProtocolVersion(conn_info, CF_PROTOCOL_CLASSIC);
    }
    else if (got == peek_size &&
             memcmp(&buf[CF_INBAND_OFFSET], "CAUTH", strlen("CAUTH")) == 0)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Peeked CAUTH in TCP stream, considering the protocol as Classic");
        ConnectionInfoSetProtocolVersion(conn_info, CF_PROTOCOL_CLASSIC);
    }
    else                                   /* got==peek_size && not "CAUTH" */
    {
        Log(LOG_LEVEL_VERBOSE,
            "Peeked nothing important in TCP stream, considering the protocol as TLS");
        LogRaw(LOG_LEVEL_DEBUG, "Peeked data: ", buf, sizeof(buf));
        ConnectionInfoSetProtocolVersion(conn_info, CF_PROTOCOL_TLS);
    }

    return 1;
}

/**
 * @return >0: the version that was negotiated
 *          0: no agreement on version was reached
 *         -1: error
 */
int ServerNegotiateProtocol(const ConnectionInfo *conn_info)
{
    int ret;
    char input[CF_SMALLBUF] = "";

    /* Send "CFE_v%d cf-serverd version". */
    char version_string[CF_MAXVARSIZE];
    int len = snprintf(version_string, sizeof(version_string),
                       "CFE_v%d cf-serverd %s\n",
                       SERVER_PROTOCOL_VERSION, VERSION);

    ret = TLSSend(ConnectionInfoSSL(conn_info), version_string, len);
    if (ret != len)
    {
        Log(LOG_LEVEL_ERR, "Connection was hung up!");
        return -1;
    }

    /* Receive CFE_v%d ... */
    ret = TLSRecvLine(ConnectionInfoSSL(conn_info), input, sizeof(input));
    if (ret <= 0)
    {
        Log(LOG_LEVEL_ERR,
            "Client closed connection early! He probably does not trust our key...");
        return -1;
    }

    int version_received = -1;
    ret = sscanf(input, "CFE_v%d", &version_received);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR,
            "Protocol version negotiation failed! Received: %s",
            input);
        return -1;
    }

    /* For now we support only one version, so just check they match... */
    if (version_received == SERVER_PROTOCOL_VERSION)
    {
        char s[] = "OK\n";
        TLSSend(ConnectionInfoSSL(conn_info), s, sizeof(s)-1);
    }
    else
    {
        char s[] = "BAD unsupported protocol version\n";
        TLSSend(ConnectionInfoSSL(conn_info), s, sizeof(s)-1);
        Log(LOG_LEVEL_ERR,
            "Client advertises unsupported protocol version: %d", version_received);
        version_received = 0;
    }
    return version_received;
}

/**
 * @brief Return the client's username into the #username variable
 * @TODO More protocol identity: IDENTIFY USERNAME=xxx HOSTNAME=xxx CUSTOMNAME=xxx
 * @return 1 if IDENTITY command was parsed correctly. Identity fields
 *         (only #username for now) have the respective string values, or they are
 *         empty if field was not on IDENTITY line.
 * @return -1 in case of error.
 */
int ServerIdentifyClient(const ConnectionInfo *conn_info,
                         char *username, size_t username_size)
{
    char line[1024], word1[1024], word2[1024];
    int line_pos = 0, chars_read = 0;
    int ret;

    /* Reset all identity variables, we'll set them later according to fields
     * on IDENTITY line. For now only "username" setting exists... */
    username[0] = '\0';

    ret = TLSRecvLine(ConnectionInfoSSL(conn_info), line, sizeof(line));
    if (ret <= 0)
    {
        return -1;
    }

    /* Assert sscanf() is safe to use. */
    assert(sizeof(word1)>=sizeof(line) && sizeof(word2)>=sizeof(line));

    ret = sscanf(line, "IDENTITY %[^=]=%s%n", word1, word2, &chars_read);
    while (ret >= 2)
    {
        /* Found USERNAME identity setting */
        if (strcmp(word1, "USERNAME") == 0)
        {
            if (strlen(word2) < username_size)
            {
                strcpy(username, word2);
            }
            else
            {
                Log(LOG_LEVEL_ERR, "IDENTITY parameter too long: %s=%s",
                    word1, word2);
                return -1;
            }
            Log(LOG_LEVEL_VERBOSE, "Setting IDENTITY: %s=%s",
                word1, word2);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Received unknown IDENTITY parameter: %s=%s",
                word1, word2);
        }

        line_pos += chars_read;
        ret = sscanf(&line[line_pos], " %[^=]=%s%n", word1, word2, &chars_read);
    }

    return 1;
}

int ServerSendWelcome(const ServerConnectionState *conn)
{
    char s[1024] = "OK WELCOME";
    size_t len = strlen(s);
    int ret;

    if (conn->username[0] != '\0')
    {
        ret = snprintf(&s[len], sizeof(s) - len, " %s=%s",
                           "USERNAME", conn->username);
        if (ret >= sizeof(s) - len)
        {
            Log(LOG_LEVEL_ERR, "Sending OK WELCOME message truncated: %s", s);
            return -1;
        }
        len += ret;
    }

    /* Overwrite the terminating '\0', we don't need it anyway. */
    s[len] = '\n';
    len++;

    ret = TLSSend(ConnectionInfoSSL(conn->conn_info), s, len);
    if (ret == -1)
    {
        return -1;
    }

    return 1;
}


/**
 * @brief Accept a TLS connection and authenticate and identify.
 * @note Various fields in #conn are set, like username and keyhash.
 */
int ServerTLSSessionEstablish(ServerConnectionState *conn)
{
    int ret;

    if (ConnectionInfoConnectionStatus(conn->conn_info) != CF_CONNECTION_ESTABLISHED)
    {
        SSL *ssl = SSL_new(SSLSERVERCONTEXT);
        if (ssl == NULL)
        {
            Log(LOG_LEVEL_ERR, "SSL_new: %s",
                ERR_reason_error_string(ERR_get_error()));
            return -1;
        }
        ConnectionInfoSetSSL(conn->conn_info, ssl);

        /* Now we are letting OpenSSL take over the open socket. */
        int sd = ConnectionInfoSocket(conn->conn_info);
        SSL_set_fd(ssl, sd);

        ret = SSL_accept(ssl);
        if (ret <= 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Checking if the accept operation can be retried");
            /* Retry just in case something was problematic at that point in time */
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sd, &rfds);
            struct timeval tv;
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            int ready = select(sd+1, &rfds, NULL, NULL, &tv);

            if (ready > 0)
            {
                Log(LOG_LEVEL_VERBOSE, "The accept operation can be retried");
                ret = SSL_accept(ssl);
                if (ret <= 0)
                {
                    Log(LOG_LEVEL_VERBOSE, "The accept operation was retried and failed");
                    TLSLogError(ssl, LOG_LEVEL_ERR, "Connection handshake server", ret);
                    return -1;
                }
                Log(LOG_LEVEL_VERBOSE, "The accept operation was retried and succeeded");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "The connect operation cannot be retried");
                TLSLogError(ssl, LOG_LEVEL_ERR, "Connection handshake server", ret);
                return -1;
            }
        }

        Log(LOG_LEVEL_VERBOSE, "TLS cipher negotiated: %s, %s",
            SSL_get_cipher_name(ssl),
            SSL_get_cipher_version(ssl));
        Log(LOG_LEVEL_VERBOSE, "TLS session established, checking trust...");

        /* Send/Receive "CFE_v%d" version string and agree on version. */
        ret = ServerNegotiateProtocol(conn->conn_info);
        if (ret <= 0)
        {
            return -1;
        }

        /* Receive IDENTITY USER=asdf plain string. */
        ret = ServerIdentifyClient(conn->conn_info, conn->username,
                                   sizeof(conn->username));
        if (ret != 1)
        {
            return -1;
        }

        /* We *now* (maybe a bit late) verify the key that the client sent us in
         * the TLS handshake, since we need the username to do so. TODO in the
         * future store keys irrelevant of username, so that we can match them
         * before IDENTIFY. */
        ret = TLSVerifyPeer(conn->conn_info, conn->ipaddr, conn->username);
        if (ret == -1)                                      /* error */
        {
            return -1;
        }

        if (ret == 1)                                    /* trusted key */
        {
            Log(LOG_LEVEL_VERBOSE,
                "%s: Client is TRUSTED, public key MATCHES stored one.",
                KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));
        }

        if (ret == 0)                                  /* untrusted key */
        {
            Log(LOG_LEVEL_WARNING,
                "%s: Client's public key is UNKNOWN!",
                KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));

            if ((SV.trustkeylist != NULL) &&
                (IsMatchItemIn(SV.trustkeylist, MapAddress(conn->ipaddr))))
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Host %s was found in the \"trustkeysfrom\" list",
                    conn->ipaddr);
                Log(LOG_LEVEL_WARNING,
                    "%s: Explicitly trusting this key from now on.",
                    KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));

                SavePublicKey("root", KeyPrintableHash(ConnectionInfoKey(conn->conn_info)),
                              KeyRSA(ConnectionInfoKey(conn->conn_info)));
            }
            else
            {
                Log(LOG_LEVEL_ERR, "TRUST FAILED, WARNING: possible MAN IN THE MIDDLE attack, dropping connection!");
                Log(LOG_LEVEL_ERR, "Open server's ACL if you really want to start trusting this new key.");
                return -1;
            }
        }

        /* skipping CAUTH */
        conn->id_verified = 1;
        /* skipping SAUTH, allow access to read-only files */
        conn->rsa_auth = 1;
        LastSaw1(conn->ipaddr, KeyPrintableHash(ConnectionInfoKey(conn->conn_info)),
                 LAST_SEEN_ROLE_ACCEPT);

        ServerSendWelcome(conn);
    }
    return 1;
}

//*******************************************************************
// COMMANDS
//*******************************************************************

typedef enum
{
    PROTOCOL_COMMAND_EXEC = 0,
    PROTOCOL_COMMAND_GET,
    PROTOCOL_COMMAND_OPENDIR,
    PROTOCOL_COMMAND_SYNCH,
    PROTOCOL_COMMAND_MD5,
    PROTOCOL_COMMAND_VERSION,
    PROTOCOL_COMMAND_VAR,
    PROTOCOL_COMMAND_CONTEXT,
    PROTOCOL_COMMAND_QUERY,
    PROTOCOL_COMMAND_CALL_ME_BACK,
    PROTOCOL_COMMAND_BAD
} ProtocolCommandNew;

static const char *const PROTOCOL_NEW[PROTOCOL_COMMAND_BAD + 1] =
{
    "EXEC",
    "GET",
    "OPENDIR",
    "SYNCH",
    "MD5",
    "VERSION",
    "VAR",
    "CONTEXT",
    "QUERY",
    "SCALLBACK",
    NULL
};

static ProtocolCommandNew GetCommandNew(char *str)
{
    int i;
    for (i = 0; PROTOCOL_NEW[i] != NULL; i++)
    {
        int cmdlen = strlen(PROTOCOL_NEW[i]);
        if ((strncmp(str, PROTOCOL_NEW[i], cmdlen) == 0) &&
            (str[cmdlen] == ' ' || str[cmdlen] == '\0'))
        {
            return i;
        }
    }
    assert (i == PROTOCOL_COMMAND_BAD);
    return i;
}


bool BusyWithNewProtocol(EvalContext *ctx, ServerConnectionState *conn)
{
    char recvbuffer[CF_BUFSIZE + CF_BUFEXT];
    char sendbuffer[CF_BUFSIZE] = { 0 };
    char filename[sizeof(recvbuffer)];
    int received;
    ServerFileGetState get_args;

    /* We already encrypt because of the TLS layer, no need to encrypt more. */
    const int encrypted = 0;

    memset(recvbuffer, 0, CF_BUFSIZE + CF_BUFEXT);
    memset(&get_args, 0, sizeof(get_args));

    /* Legacy stuff only for old protocol */
    assert(conn->rsa_auth == 1);
    assert(conn->id_verified == 1);

    received = ReceiveTransaction(conn->conn_info, recvbuffer, NULL);
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

    /* TODO break recvbuffer here: command, param1, param2 etc. */

    switch (GetCommandNew(recvbuffer))
    {
    case PROTOCOL_COMMAND_EXEC:                                 /* TODO always file never directory, no end with '/' */
    {
        char args[256];
        int ret = sscanf(recvbuffer, "EXEC %255[^\n]", args);
        if (ret != 1)                    /* No arguments, use default args. */
        {
            args[0] = '\0';
        }

        if (!AllowedUser(conn->username))
        {
            Log(LOG_LEVEL_INFO, "EXEC denied due to not allowed user: %s",
                conn->username);
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        char arg0[PATH_MAX];
        size_t ret2 = CommandArg0_bound(arg0, CFRUNCOMMAND, sizeof(arg0));
        if (ret2 == (size_t) -1)
        {
            goto protocol_error;
        }

        ret2 = PreprocessRequestPath(arg0, sizeof(arg0));
        if (ret2 == (size_t) -1)
        {
            goto protocol_error;
        }


        /* TODO EXEC should not just use paths_acl access control, but
         * specific "path_exec" ACL. Then different command execution could be
         * allowed per host, and the host could even set argv[0] in his EXEC
         * request, rather than only the arguments. */

        if (acl_CheckPath(paths_acl, arg0,
                          conn->ipaddr, conn->hostname,
                          KeyPrintableHash(ConnectionInfoKey(conn->conn_info)))
            == false)
        {
            Log(LOG_LEVEL_INFO, "EXEC denied due to ACL for file: %s", arg0);
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        if (!MatchClasses(ctx, conn))
        {
            Log(LOG_LEVEL_INFO, "EXEC denied due to failed class match");
            Terminate(conn->conn_info);
            return true;
        }

        DoExec(ctx, conn, args);
        Terminate(conn->conn_info);
        return true;
    }
    case PROTOCOL_COMMAND_VERSION:

        snprintf(sendbuffer, sizeof(sendbuffer), "OK: %s", Version());
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
        return true;

    case PROTOCOL_COMMAND_GET:
    {
        int ret = sscanf(recvbuffer, "GET %d %[^\n]",
                         &(get_args.buf_size), filename);

        if (ret != 2 ||
            get_args.buf_size <= 0 || get_args.buf_size > CF_BUFSIZE)
        {
            goto protocol_error;
        }

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Received:", "GET", filename);

        /* TODO batch all the following in one function since it's very
         * similar in all of GET, OPENDIR and STAT. */

        ret = ShortcutsExpand(filename, sizeof(filename),
                              SV.path_shortcuts,
                              conn->ipaddr, conn->hostname,
                              KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));
        if (ret == (size_t) -1)
        {
            goto protocol_error;
        }

        ret = PreprocessRequestPath(filename, sizeof(filename));
        if (ret == (size_t) -1)
        {
            goto protocol_error;
        }

        PathRemoveTrailingSlash(filename, strlen(filename));

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Translated to:", "GET", filename);

        if (acl_CheckPath(paths_acl, filename,
                          conn->ipaddr, conn->hostname,
                          KeyPrintableHash(ConnectionInfoKey(conn->conn_info)))
            == false)
        {
            Log(LOG_LEVEL_INFO, "access denied to GET: %s", filename);
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        memset(sendbuffer, 0, sizeof(sendbuffer));

        if (get_args.buf_size >= CF_BUFSIZE)
        {
            get_args.buf_size = 2048;
        }

        /* TODO eliminate! */
        get_args.connect = conn;
        get_args.encrypt = false;
        get_args.replybuff = sendbuffer;
        get_args.replyfile = filename;

        CfGetFile(&get_args);

        return true;
    }
    case PROTOCOL_COMMAND_OPENDIR:
    {
        memset(filename, 0, sizeof(filename));
        int ret = sscanf(recvbuffer, "OPENDIR %[^\n]", filename);
        if (ret != 1)
        {
            goto protocol_error;
        }

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Received:", "OPENDIR", filename);

        /* sizeof()-1 because we need one extra byte for
           appending '/' afterwards. */
        ret = ShortcutsExpand(filename, sizeof(filename) - 1,
                              SV.path_shortcuts,
                              conn->ipaddr, conn->hostname,
                              KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));
        if (ret == (size_t) -1)
        {
            goto protocol_error;
        }

        ret = PreprocessRequestPath(filename, sizeof(filename) - 1);
        if (ret == (size_t) -1)
        {
            goto protocol_error;
        }

        /* OPENDIR *must* be directory. */
        PathAppendTrailingSlash(filename, strlen(filename));

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Translated to:", "OPENDIR", filename);

        if (acl_CheckPath(paths_acl, filename,
                          conn->ipaddr, conn->hostname,
                          KeyPrintableHash(ConnectionInfoKey(conn->conn_info)))
            == false)
        {
            Log(LOG_LEVEL_INFO, "access denied to OPENDIR: %s", filename);
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        CfOpenDirectory(conn, sendbuffer, filename);
        return true;
    }
    case PROTOCOL_COMMAND_SYNCH:
    {
        long time_no_see = 0;
        memset(filename, 0, sizeof(filename));
        int ret = sscanf(recvbuffer, "SYNCH %ld STAT %[^\n]",
                         &time_no_see, filename);

        if (ret != 2  || time_no_see == 0 || filename[0] == '\0')
        {
            goto protocol_error;
        }

        time_t tloc = time(NULL);
        if (tloc == -1)
        {
            /* Should never happen. */
            Log(LOG_LEVEL_ERR, "Couldn't read system clock. (time: %s)", GetErrorStr());
            SendTransaction(conn->conn_info, "BAD: clocks out of synch", 0, CF_DONE);
            return true;
        }

        time_t trem = (time_t) time_no_see;
        int drift = (int) (tloc - trem);

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Received:", "STAT", filename);

        /* sizeof()-1 because we need one extra byte for
           appending '/' afterwards. */
        ret = ShortcutsExpand(filename, sizeof(filename) - 1,
                              SV.path_shortcuts,
                              conn->ipaddr, conn->hostname,
                              KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));
        if (ret == (size_t) -1)
        {
            goto protocol_error;
        }

        ret = PreprocessRequestPath(filename, sizeof(filename) - 1);
        if (ret == (size_t) -1)
        {
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        if (IsDirReal(filename) == 1)
        {
            PathAppendTrailingSlash(filename, strlen(filename));
        }
        else
        {
            PathRemoveTrailingSlash(filename, strlen(filename));
        }

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Translated to:", "STAT", filename);

        if (acl_CheckPath(paths_acl, filename,
                          conn->ipaddr, conn->hostname,
                          KeyPrintableHash(ConnectionInfoKey(conn->conn_info)))
            == false)
        {
            Log(LOG_LEVEL_INFO, "access denied to STAT: %s", filename);
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        if (DENYBADCLOCKS && (drift * drift > CLOCK_DRIFT * CLOCK_DRIFT))
        {
            snprintf(sendbuffer, sizeof(sendbuffer),
                     "BAD: Clocks are too far unsynchronized %ld/%ld",
                     (long) tloc, (long) trem);
            Log(LOG_LEVEL_INFO, "denybadclocks %s", sendbuffer);
            SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
            return true;
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "Clocks were off by %ld", (long) tloc - (long) trem);
            StatFile(conn, sendbuffer, filename);
        }

        return true;
    }
    case PROTOCOL_COMMAND_MD5:

        CompareLocalHash(conn, sendbuffer, recvbuffer);
        return true;

    case PROTOCOL_COMMAND_VAR:
    {
        char var[256];
        int ret = sscanf(recvbuffer, "VAR %255[^\n]", var);
        if (ret != 1)
        {
            goto protocol_error;
        }

        /* TODO if this is literals_acl, then when should I check vars_acl? */
        if (acl_CheckExact(literals_acl, var,
                           conn->ipaddr, conn->hostname,
                           KeyPrintableHash(ConnectionInfoKey(conn->conn_info)))
            == false)
        {
            Log(LOG_LEVEL_INFO, "access denied to variable: %s", var);
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        GetServerLiteral(ctx, conn, sendbuffer, recvbuffer, encrypted);
        return true;
    }
    case PROTOCOL_COMMAND_CONTEXT:
    {
        char client_regex[256];
        int ret = sscanf(recvbuffer, "CONTEXT %255[^\n]", client_regex);
        if (ret != 1)
        {
            goto protocol_error;
        }

        /* WARNING: this comes from legacy code and must be killed if we care
         * about performance. We should not accept regular expressions from
         * the client, but this will break backwards compatibility.
         *
         * I replicated the code in raw form here to emphasize complexity,
         * it's the only *slow* command currently in the protocol.  */

        Item *persistent_classes = ListPersistentClasses();
        Item *matched_classes = NULL;

        /* For all persistent classes */
        for (Item *ip = persistent_classes; ip != NULL; ip = ip->next)
        {
            const char *class_name = ip->name;

            /* Does this class match the regex the client sent? */
            if (StringMatchFull(client_regex, class_name))
            {
                /* For all ACLs */
                for (size_t i = 0; i < classes_acl->len; i++)
                {
                    struct resource_acl *racl = &classes_acl->acls[i];

                    /* Does this ACL apply to this host? */
                    if (access_CheckResource(racl, conn->ipaddr, conn->hostname,
                                             KeyPrintableHash(ConnectionInfoKey(conn->conn_info)))
                        == true)
                    {
                        const char *allowed_classes_regex =
                            classes_acl->resource_names->list[i]->str;

                        /* Does this ACL admits access for this class to the
                         * connected host? */
                        if (StringMatchFull(allowed_classes_regex, class_name))
                        {
                            PrependItem(&matched_classes, class_name, NULL);
                        }
                    }
                }
            }
        }

        if (matched_classes == NULL)
        {
            Log(LOG_LEVEL_INFO,
                "No allowed classes for remoteclassesmatching: %s",
                client_regex);
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        ReplyServerContext(conn, encrypted, matched_classes);
        return true;
    }
    case PROTOCOL_COMMAND_QUERY:
    {
        char query[256], name[128];
        int ret1 = sscanf(recvbuffer, "QUERY %256[^\n]", query);
        int ret2 = sscanf(recvbuffer, "QUERY %128s", name);
        if (ret1 != 1 || ret2 != 1)
        {
            goto protocol_error;
        }

        if (acl_CheckExact(query_acl, name,
                           conn->ipaddr, conn->hostname,
                           KeyPrintableHash(ConnectionInfoKey(conn->conn_info)))
            == false)
        {
            Log(LOG_LEVEL_INFO, "access denied to query: %s", query);
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        if (GetServerQuery(conn, recvbuffer, encrypted))
        {
            return true;
        }

        break;
    }
    case PROTOCOL_COMMAND_CALL_ME_BACK:

        if (acl_CheckExact(query_acl, "collect_calls",
                           conn->ipaddr, conn->hostname,
                           KeyPrintableHash(ConnectionInfoKey(conn->conn_info)))
            == false)
        {
            Log(LOG_LEVEL_INFO,
                "access denied to Call-Collect, check the ACL for class: collect_calls");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        return ReceiveCollectCall(conn);


    case PROTOCOL_COMMAND_BAD:

        Log(LOG_LEVEL_WARNING, "Unexpected protocol command: %s", recvbuffer);
    }

    /* We should only reach this point if something went really bad, and
     * close connection. In all other cases (like access denied) connection
     * shouldn't be closed.
     * TODO So we need this function to return more than
     * true/false. -1 for error, 0 on success, 1 on access denied. It can be
     * an option if connection will close on denial. */

protocol_error:
    strcpy(sendbuffer, "BAD: Request denied");
    SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
    Log(LOG_LEVEL_INFO, "Closing connection, due to request: '%s'", recvbuffer);
    return false;
}
