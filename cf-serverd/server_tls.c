/*
   Copyright 2017 Northern.tech AS

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


#include <server_tls.h>
#include <server_common.h>

#include <openssl/err.h>                                   /* ERR_get_error */

#include <crypto.h>                                        /* DecryptString */
#include <conversion.h>
#include <signals.h>
#include <item_lib.h>                 /* IsMatchItemIn */
#include <lastseen.h>                 /* LastSaw1 */
#include <net.h>                      /* SendTransaction,ReceiveTransaction */
#include <tls_generic.h>              /* TLSSend */
#include <cf-serverd-enterprise-stubs.h>
#include <connection_info.h>
#include <regex.h>                                       /* StringMatchFull */
#include <known_dirs.h>
#include <file_lib.h>                                           /* IsDirReal */

#include "server_access.h"          /* access_CheckResource, acl_CheckExact */


static SSL_CTX *SSLSERVERCONTEXT = NULL;
static X509 *SSLSERVERCERT = NULL;


/**
 * @warning Make sure you've called CryptoInitialize() first!
 */
bool ServerTLSInitialize()
{
    int ret;

    if (!TLSGenericInitialize())
    {
        return false;
    }

    assert(SSLSERVERCONTEXT == NULL);
    SSLSERVERCONTEXT = SSL_CTX_new(SSLv23_server_method());
    if (SSLSERVERCONTEXT == NULL)
    {
        Log(LOG_LEVEL_ERR, "SSL_CTX_new: %s",
            TLSErrorString(ERR_get_error()));
        goto err1;
    }

    TLSSetDefaultOptions(SSLSERVERCONTEXT, SV.allowtlsversion);

    /*
     * CFEngine is not a web server so it does not need to support many
     * ciphers. It only allows a safe but very common subset by default,
     * extensible via "allowciphers" in body server control. By default
     * it allows:
     *
     *     AES256-GCM-SHA384: most high-grade RSA-based cipher from TLSv1.2
     *     AES256-SHA: most backwards compatible but high-grade, from SSLv3
     */
    const char *cipher_list = SV.allowciphers;
    if (cipher_list == NULL)
    {
        cipher_list ="AES256-GCM-SHA384:AES256-SHA";
    }

    Log(LOG_LEVEL_VERBOSE,
        "Setting cipher list for incoming TLS connections to: %s",
        cipher_list);

    ret = SSL_CTX_set_cipher_list(SSLSERVERCONTEXT, cipher_list);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR,
            "No valid ciphers in cipher list: %s",
            cipher_list);
        goto err2;
    }

    if (PRIVKEY == NULL || PUBKEY == NULL)
    {
        Log(LOG_LEVEL_ERR, "No public/private key pair is loaded,"
            " please create one using cf-key");
        goto err2;
    }

    assert(SSLSERVERCERT == NULL);
    /* Create cert into memory and load it into SSL context. */
    SSLSERVERCERT = TLSGenerateCertFromPrivKey(PRIVKEY);
    if (SSLSERVERCERT == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to generate in-memory certificate from private key");
        goto err2;
    }

    SSL_CTX_use_certificate(SSLSERVERCONTEXT, SSLSERVERCERT);

    ret = SSL_CTX_use_RSAPrivateKey(SSLSERVERCONTEXT, PRIVKEY);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Failed to use RSA private key: %s",
            TLSErrorString(ERR_get_error()));
        goto err3;
    }

    /* Verify cert consistency. */
    ret = SSL_CTX_check_private_key(SSLSERVERCONTEXT);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Inconsistent key and TLS cert: %s",
            TLSErrorString(ERR_get_error()));
        goto err3;
    }

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

void ServerTLSDeInitialize()
{
    if (PUBKEY)
    {
        RSA_free(PUBKEY);
        PUBKEY = NULL;
    }

    if (PRIVKEY)
    {
        RSA_free(PRIVKEY);
        PRIVKEY = NULL;
    }

    if (SSLSERVERCERT != NULL)
    {
        X509_free(SSLSERVERCERT);
        SSLSERVERCERT = NULL;
    }

    if (SSLSERVERCONTEXT != NULL)
    {
        SSL_CTX_free(SSLSERVERCONTEXT);
        SSLSERVERCONTEXT = NULL;
    }
}

/**
 * @brief Set the connection type to CLASSIC or TLS.

 * It is performed by peeking into the TLS connection to read the first bytes,
 * and if it's a CAUTH protocol command use the old protocol loop, else use
 * the TLS protocol loop.
 * This must be the first thing we run on an accepted connection.
 *
 * @return -1 in case of error, 1 otherwise.
 */
int ServerTLSPeek(ConnectionInfo *conn_info)
{
    assert(SSLSERVERCONTEXT != NULL);
    assert(PRIVKEY != NULL);
    assert(PUBKEY  != NULL);

    assert(ConnectionInfoProtocolVersion(conn_info) == CF_PROTOCOL_UNDEFINED);

    const int peek_size = CF_INBAND_OFFSET + sizeof("CAUTH");

    char buf[peek_size];
    ssize_t got = recv(ConnectionInfoSocket(conn_info), buf, sizeof(buf), MSG_PEEK);
    assert(got <= peek_size);
    if (got < 0)
    {
        assert(got == -1);
        Log(LOG_LEVEL_ERR, "TCP receive error: %s", GetErrorStr());
        return -1;
    }
    else if (got == 0)
    {
        Log(LOG_LEVEL_INFO,
            "Peer closed TCP connection without sending data!");
        return -1;
    }
    else if (got < peek_size)
    {
        Log(LOG_LEVEL_INFO,
            "Peer sent only %zd bytes! Considering the protocol as Classic",
            got);
        ConnectionInfoSetProtocolVersion(conn_info, CF_PROTOCOL_CLASSIC);
    }
    else if (memcmp(&buf[CF_INBAND_OFFSET], "CAUTH", strlen("CAUTH")) == 0)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Peeked CAUTH in TCP stream, considering the protocol as Classic");
        ConnectionInfoSetProtocolVersion(conn_info, CF_PROTOCOL_CLASSIC);
    }
    else                                   /* got==peek_size && not "CAUTH" */
    {
        Log(LOG_LEVEL_VERBOSE,
            "Peeked nothing important in TCP stream, considering the protocol as TLS");
        ConnectionInfoSetProtocolVersion(conn_info, CF_PROTOCOL_TLS);
    }
    LogRaw(LOG_LEVEL_DEBUG, "Peeked data: ", buf, got);

    return 1;
}

/**
 * 1. Send "CFE_v%d" server hello.
 * 2. Receive two lines: One "CFE_v%d" with the protocol version the client
 *    wishes to have, and one "IDENTITY USERNAME=blah ..." with identification
 *    information for the client.
 *
 * @note For Identification dialog to end successfully, one "OK WELCOME" line
 *       must be sent right after this function, after identity is verified.
 *
 * @TODO More protocol identity. E.g.
 *       IDENTITY USERNAME=xxx HOSTNAME=xxx CUSTOMNAME=xxx
 *
 * @retval true if protocol version was successfully negotiated and IDENTITY
 *         command was parsed correctly. Identity fields (only #username for
 *         now) have the respective string values, or they are empty if field
 *         was not on IDENTITY line.  #conn_info->protocol has been updated
 *         with the negotiated protocol version.
 * @retval false in case of error.
 */
static bool ServerIdentificationDialog(ConnectionInfo *conn_info,
                                       char *username, size_t username_size)
{
    int ret;
    char input[1024] = "";
    /* The only protocol version we support inside TLS, for now. */
    const int SERVER_PROTOCOL_VERSION = CF_PROTOCOL_LATEST;

    /* Send "CFE_v%d cf-serverd version". */
    char version_string[CF_MAXVARSIZE];
    int len = snprintf(version_string, sizeof(version_string),
                       "CFE_v%d cf-serverd %s\n",
                       SERVER_PROTOCOL_VERSION, VERSION);

    ret = TLSSend(conn_info->ssl, version_string, len);
    if (ret != len)
    {
        Log(LOG_LEVEL_NOTICE, "Connection was hung up!");
        return false;
    }

    /* Receive CFE_v%d ... \n IDENTITY USERNAME=... */
    int input_len = TLSRecvLines(conn_info->ssl, input, sizeof(input));
    if (input_len <= 0)
    {
        Log(LOG_LEVEL_NOTICE,
            "Client closed connection early! He probably does not trust our key...");
        return false;
    }

    int version_received = -1;
    ret = sscanf(input, "CFE_v%d", &version_received);
    if (ret != 1)
    {
        Log(LOG_LEVEL_NOTICE,
            "Protocol version negotiation failed! Received: %s",
            input);
        return false;
    }

    /* For now we support only one version inside TLS. */
    /* TODO value should not be hardcoded but compared to enum ProtocolVersion. */
    if (version_received != SERVER_PROTOCOL_VERSION)
    {
        Log(LOG_LEVEL_NOTICE,
            "Client advertises disallowed protocol version: %d",
            version_received);
        return false;
        /* TODO send "BAD ..." ? */
    }

    /* Did we receive 2nd line or do we need to receive again? */
    const char id_line[] = "\nIDENTITY ";
    char *line2 = memmem(input, input_len, id_line, strlen(id_line));
    if (line2 == NULL)
    {
        /* Wait for 2nd line to arrive. */
        input_len = TLSRecvLines(conn_info->ssl, input, sizeof(input));
        if (input_len <= 0)
        {
            Log(LOG_LEVEL_NOTICE,
                "Client closed connection during identification dialog!");
            return false;
        }
        line2 = input;
    }
    else
    {
        line2++;                                               /* skip '\n' */
    }

    /***** Parse all IDENTITY fields from line2 *****/

    char word1[1024], word2[1024];
    int line2_pos = 0, chars_read = 0;

    /* Reset all identity variables, we'll set them according to fields
     * on IDENTITY line. For now only "username" setting exists... */
    username[0] = '\0';

    /* Assert sscanf() is safe to use. */
    assert(sizeof(word1) >= sizeof(input));
    assert(sizeof(word2) >= sizeof(input));

    ret = sscanf(line2, "IDENTITY %[^=]=%s%n", word1, word2, &chars_read);
    while (ret >= 2)
    {
        /* Found USERNAME identity setting */
        if (strcmp(word1, "USERNAME") == 0)
        {
            if ((strlen(word2) < username_size) && (IsUserNameValid(word2) == true))
            {
                strcpy(username, word2);
            }
            else
            {
                Log(LOG_LEVEL_NOTICE, "Received invalid IDENTITY: %s=%s",
                    word1, word2);
                return false;
            }
            Log(LOG_LEVEL_VERBOSE, "Setting IDENTITY: %s=%s",
                word1, word2);
        }
        /* ... else if (strcmp()) for other acceptable IDENTITY parameters. */
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Received unknown IDENTITY parameter: %s=%s",
                word1, word2);
        }

        line2_pos += chars_read;
        ret = sscanf(&line2[line2_pos], " %[^=]=%s%n", word1, word2, &chars_read);
    }

    /* Version client and server agreed on. */
    conn_info->protocol = version_received;

    return true;
}

static int ServerSendWelcome(const ServerConnectionState *conn)
{
    char s[1024] = "OK WELCOME";
    size_t len = strlen(s);
    int ret;

    /* "OK WELCOME" is the important part. The rest is just extra verbosity. */
    if (conn->username[0] != '\0')
    {
        ret = snprintf(&s[len], sizeof(s) - len, " %s=%s",
                           "USERNAME", conn->username);
        if (ret >= sizeof(s) - len)
        {
            Log(LOG_LEVEL_NOTICE, "Sending OK WELCOME message truncated: %s", s);
            return -1;
        }
        len += ret;
    }

    /* Overwrite the terminating '\0', we don't need it anyway. */
    s[len] = '\n';
    len++;

    ret = TLSSend(conn->conn_info->ssl, s, len);
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
    if (conn->conn_info->status != CONNECTIONINFO_STATUS_ESTABLISHED)
    {
        assert(ConnectionInfoSSL(conn->conn_info) == NULL);
        SSL *ssl = SSL_new(SSLSERVERCONTEXT);
        if (ssl == NULL)
        {
            Log(LOG_LEVEL_ERR, "SSL_new: %s",
                TLSErrorString(ERR_get_error()));
            return -1;
        }
        ConnectionInfoSetSSL(conn->conn_info, ssl);

        /* Pass conn_info inside the ssl struct for TLSVerifyCallback(). */
        SSL_set_ex_data(ssl, CONNECTIONINFO_SSL_IDX, conn->conn_info);

        /* Now we are letting OpenSSL take over the open socket. */
        SSL_set_fd(ssl, ConnectionInfoSocket(conn->conn_info));

        int ret = SSL_accept(ssl);
        if (ret <= 0)
        {
            TLSLogError(ssl, LOG_LEVEL_ERR,
                        "Failed to accept TLS connection", ret);
            return -1;
        }

        Log(LOG_LEVEL_VERBOSE, "TLS version negotiated: %8s; Cipher: %s,%s",
            SSL_get_version(ssl),
            SSL_get_cipher_name(ssl),
            SSL_get_cipher_version(ssl));
        Log(LOG_LEVEL_VERBOSE, "TLS session established, checking trust...");

        /* Send/Receive "CFE_v%d" version string, agree on version, receive
           identity (username) of peer. */
        char username[sizeof(conn->username)] = "";
        bool b = ServerIdentificationDialog(conn->conn_info,
                                            username, sizeof(username));
        if (b != true)
        {
            return -1;
        }

        /* We *now* (maybe a bit late) verify the key that the client sent us in
         * the TLS handshake, since we need the username to do so. TODO in the
         * future store keys irrelevant of username, so that we can match them
         * before IDENTIFY. */
        ret = TLSVerifyPeer(conn->conn_info, conn->ipaddr, username);
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
            if ((SV.trustkeylist != NULL) &&
                (IsMatchItemIn(SV.trustkeylist, conn->ipaddr)))
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Peer was found in \"trustkeysfrom\" list");
                Log(LOG_LEVEL_NOTICE, "Trusting new key: %s",
                    KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));

                SavePublicKey(username, KeyPrintableHash(conn->conn_info->remote_key),
                              KeyRSA(ConnectionInfoKey(conn->conn_info)));
            }
            else
            {
                Log(LOG_LEVEL_NOTICE,
                    "TRUST FAILED, peer presented an untrusted key, dropping connection!");
                Log(LOG_LEVEL_VERBOSE,
                    "Add peer to \"trustkeysfrom\" if you really want to start trusting this new key.");
                return -1;
            }
        }

        /* All checks succeeded, set conn->uid (conn->sid for Windows)
         * according to the received USERNAME identity. */
        SetConnIdentity(conn, username);

        /* No CAUTH, SAUTH in non-classic protocol. */
        conn->user_data_set = 1;
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


/**
 * Currently this function returns false when we want the connection
 * closed, and true, when we want to proceed further with requests.
 *
 * @TODO So we need this function to return more than true/false, because now
 * we return true even when access is denied! E.g. return -1 for error, 0 on
 * success, 1 on access denied. It can be an option if connection will close
 * on denial.
 */
bool BusyWithNewProtocol(EvalContext *ctx, ServerConnectionState *conn)
{
    /* The CF_BUFEXT extra space is there to ensure we're not *reading* out of
     * bounds in commands that carry extra binary arguments, like MD5. */
    char recvbuffer[CF_BUFSIZE + CF_BUFEXT] = { 0 };
    /* This size is the max we can SendTransaction(). */
    char sendbuffer[CF_BUFSIZE - CF_INBAND_OFFSET] = { 0 };
    char filename[CF_BUFSIZE + 1];      /* +1 for appending slash sometimes */
    ServerFileGetState get_args = { 0 };

    /* We already encrypt because of the TLS layer, no need to encrypt more. */
    const int encrypted = 0;

    /* Legacy stuff only for old protocol. */
    assert(conn->rsa_auth == 1);
    assert(conn->user_data_set == 1);

    /* Receive up to CF_BUFSIZE - 1 bytes. */
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

    /* TODO break recvbuffer here: command, param1, param2 etc. */

    switch (GetCommandNew(recvbuffer))
    {
    case PROTOCOL_COMMAND_EXEC:
    {
        const size_t EXEC_len = strlen(PROTOCOL_NEW[PROTOCOL_COMMAND_EXEC]);
        /* Assert recvbuffer starts with EXEC. */
        assert(strncmp(PROTOCOL_NEW[PROTOCOL_COMMAND_EXEC],
                       recvbuffer, EXEC_len) == 0);

        char *args = &recvbuffer[EXEC_len];
        args += strspn(args, " \t");                       /* bypass spaces */

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Received:", "EXEC", args);

        bool b = DoExec2(ctx, conn, args,
                         sendbuffer, sizeof(sendbuffer));

        /* In the end we might keep the connection open (return true) to be
         * ready for next requests, but we must always send the TERMINATOR
         * string so that the client can close the connection at will. */
        Terminate(conn->conn_info);

        return b;
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

        size_t zret = ShortcutsExpand(filename, sizeof(filename),
                                     SV.path_shortcuts,
                                     conn->ipaddr, conn->revdns,
                                     KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));
        if (zret == (size_t) -1)
        {
            goto protocol_error;
        }

        zret = PreprocessRequestPath(filename, sizeof(filename));
        if (zret == (size_t) -1)
        {
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        PathRemoveTrailingSlash(filename, strlen(filename));

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Translated to:", "GET", filename);

        if (acl_CheckPath(paths_acl, filename,
                          conn->ipaddr, conn->revdns,
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
        get_args.conn = conn;
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
        size_t zret = ShortcutsExpand(filename, sizeof(filename) - 1,
                                      SV.path_shortcuts,
                                      conn->ipaddr, conn->revdns,
                                      KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));
        if (zret == (size_t) -1)
        {
            goto protocol_error;
        }

        zret = PreprocessRequestPath(filename, sizeof(filename) - 1);
        if (zret == (size_t) -1)
        {
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        /* OPENDIR *must* be directory. */
        PathAppendTrailingSlash(filename, strlen(filename));

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Translated to:", "OPENDIR", filename);

        if (acl_CheckPath(paths_acl, filename,
                          conn->ipaddr, conn->revdns,
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

        if (ret != 2 || filename[0] == '\0')
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
        size_t zret = ShortcutsExpand(filename, sizeof(filename) - 1,
                                      SV.path_shortcuts,
                                      conn->ipaddr, conn->revdns,
                                      KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));
        if (zret == (size_t) -1)
        {
            goto protocol_error;
        }

        zret = PreprocessRequestPath(filename, sizeof(filename) - 1);
        if (zret == (size_t) -1)
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
                          conn->ipaddr, conn->revdns,
                          KeyPrintableHash(ConnectionInfoKey(conn->conn_info)))
            == false)
        {
            Log(LOG_LEVEL_INFO, "access denied to STAT: %s", filename);
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        Log(LOG_LEVEL_DEBUG, "Clocks were off by %ld",
            (long) tloc - (long) trem);

        if (DENYBADCLOCKS && (drift * drift > CLOCK_DRIFT * CLOCK_DRIFT))
        {
            snprintf(sendbuffer, sizeof(sendbuffer),
                     "BAD: Clocks are too far unsynchronized %ld/%ld",
                     (long) tloc, (long) trem);
            Log(LOG_LEVEL_INFO, "denybadclocks %s", sendbuffer);
            SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
            return true;
        }

        StatFile(conn, sendbuffer, filename);

        return true;
    }
    case PROTOCOL_COMMAND_MD5:
    {
        int ret = sscanf(recvbuffer, "MD5 %[^\n]", filename);
        if (ret != 1)
        {
            goto protocol_error;
        }

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Received:", "MD5", filename);

        /* TODO batch all the following in one function since it's very
         * similar in all of GET, OPENDIR and STAT. */

        size_t zret = ShortcutsExpand(filename, sizeof(filename),
                                     SV.path_shortcuts,
                                     conn->ipaddr, conn->revdns,
                                     KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));
        if (zret == (size_t) -1)
        {
            goto protocol_error;
        }

        zret = PreprocessRequestPath(filename, sizeof(filename));
        if (zret == (size_t) -1)
        {
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        PathRemoveTrailingSlash(filename, strlen(filename));

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Translated to:", "MD5", filename);

        if (acl_CheckPath(paths_acl, filename,
                          conn->ipaddr, conn->revdns,
                          KeyPrintableHash(ConnectionInfoKey(conn->conn_info)))
            == false)
        {
            Log(LOG_LEVEL_INFO, "access denied to file: %s", filename);
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        assert(CF_DEFAULT_DIGEST_LEN <= EVP_MAX_MD_SIZE);
        unsigned char digest[EVP_MAX_MD_SIZE + 1];

        assert(CF_BUFSIZE + CF_SMALL_OFFSET + CF_DEFAULT_DIGEST_LEN
               <= sizeof(recvbuffer));
        memcpy(digest, recvbuffer + strlen(recvbuffer) + CF_SMALL_OFFSET,
               CF_DEFAULT_DIGEST_LEN);

        CompareLocalHash(filename, digest, sendbuffer);
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);

        return true;
    }
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
                           conn->ipaddr, conn->revdns,
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

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Received:", "CONTEXT", client_regex);

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
                /* Is this class allowed to be given to the specific
                 * host, according to the regexes in the ACLs? */
                if (acl_CheckRegex(classes_acl, class_name,
                                   conn->ipaddr, conn->revdns,
                                   KeyPrintableHash(ConnectionInfoKey(conn->conn_info)),
                                   NULL)
                    == true)
                {
                    Log(LOG_LEVEL_DEBUG, "Access granted to class: %s",
                        class_name);
                    PrependItem(&matched_classes, class_name, NULL);
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
        int ret1 = sscanf(recvbuffer, "QUERY %255[^\n]", query);
        int ret2 = sscanf(recvbuffer, "QUERY %127s", name);
        if (ret1 != 1 || ret2 != 1)
        {
            goto protocol_error;
        }

        if (acl_CheckExact(query_acl, name,
                           conn->ipaddr, conn->revdns,
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
        /* Server side, handing the collect call off to cf-hub. */

        if (acl_CheckExact(query_acl, "collect_calls",
                           conn->ipaddr, conn->revdns,
                           KeyPrintableHash(ConnectionInfoKey(conn->conn_info)))
            == false)
        {
            Log(LOG_LEVEL_INFO,
                "access denied to Call-Collect, check the ACL for class: collect_calls");
            return false;
        }

        ReceiveCollectCall(conn);
        /* On success that returned true; otherwise, it did all
         * relevant Log()ging.  Either way, we're no longer busy with
         * it and our caller can close the connection: */
        return false;

    case PROTOCOL_COMMAND_BAD:

        Log(LOG_LEVEL_WARNING, "Unexpected protocol command: %s", recvbuffer);
    }

    /* We should only reach this point if something went really bad, and
     * close connection. In all other cases (like access denied) connection
     * shouldn't be closed.
     */

protocol_error:
    strcpy(sendbuffer, "BAD: Request denied");
    SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
    Log(LOG_LEVEL_INFO,
        "Closing connection due to illegal request: %s", recvbuffer);
    return false;
}
