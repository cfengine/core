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


#include <server_tls.h>
#include <server_common.h>

#include <crypto.h>                                        /* DecryptString */
#include <conversion.h>
#include <signals.h>
#include <item_lib.h>                 /* IsMatchItemIn */
#include <lastseen.h>                 /* LastSaw1 */
#include <net.h>                      /* SendTransaction,ReceiveTransaction */
#include <tls_generic.h>              /* TLSSend */
#include <cf-serverd-enterprise-stubs.h>


static SSL_CTX *SSLSERVERCONTEXT = NULL;
static X509 *SSLSERVERCERT = NULL;


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

    /* Never bother with retransmissions, SSL_write() should
     * always either write the whole amount or fail. */
    SSL_CTX_set_mode(SSLSERVERCONTEXT, SSL_MODE_AUTO_RETRY);

    /*
     * Create cert into memory and load it into SSL context.
     */

    if (PRIVKEY == NULL || PUBKEY == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "No public/private key pair is loaded, create one with cf-key");
        goto err2;
    }
    assert(SSLSERVERCERT == NULL);
    /* Generate self-signed cert valid from now to 50 years later. */
    {
        X509 *x509 = X509_new();
        X509_gmtime_adj(X509_get_notBefore(x509), 0);
        X509_time_adj(X509_get_notAfter(x509), 60*60*24*365*50, NULL);
        EVP_PKEY *pkey = EVP_PKEY_new();
        EVP_PKEY_set1_RSA(pkey, PRIVKEY);
        X509_NAME *name = X509_get_subject_name(x509);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   (const char *) "",
                                   -1, -1, 0);
        X509_set_issuer_name(x509, name);
        X509_set_pubkey(x509, pkey);

        const EVP_MD *md = EVP_get_digestbyname("sha384");
        if (md == NULL)
        {
            Log(LOG_LEVEL_ERR, "Uknown digest algorithm %s",
                "sha384");
            return false;
        }
        ret = X509_sign(x509, pkey, md);

        EVP_PKEY_free(pkey);
        SSLSERVERCERT = x509;

        if (ret <= 0)
        {
            Log(LOG_LEVEL_ERR,
                "Couldn't sign the public key for the TLS handshake: %s",
                ERR_reason_error_string(ERR_get_error()));
            goto err3;
        }
    }
    /* Log(LOG_LEVEL_ERR, "generate cert from priv key: %s", */
    /*     ERR_reason_error_string(ERR_get_error())); */

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
    assert(conn_info->type == CF_PROTOCOL_UNDEFINED);

    const int peek_size = CF_INBAND_OFFSET + sizeof("CAUTH");

    char buf[peek_size];
    ssize_t got = recv(conn_info->sd, buf, sizeof(buf), MSG_PEEK);
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
            "Peer sent only %zd bytes! Considering the protocol as Classic",
            got);
        conn_info->type = CF_PROTOCOL_CLASSIC;
    }
    else if (got == peek_size &&
             memcmp(&buf[CF_INBAND_OFFSET], "CAUTH", strlen("CAUTH")) == 0)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Peeked CAUTH in TCP stream, considering the protocol as Classic");
        conn_info->type = CF_PROTOCOL_CLASSIC;
    }
    else                                   /* got==peek_size && not "CAUTH" */
    {
        Log(LOG_LEVEL_VERBOSE,
            "Peeked nothing important in TCP stream, considering the protocol as TLS");
        LogRaw(LOG_LEVEL_DEBUG, "Peeked data: ", buf, sizeof(buf));
        conn_info->type = CF_PROTOCOL_TLS;
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

    ret = TLSSend(conn_info->ssl, version_string, len);
    if (ret != len)
    {
        Log(LOG_LEVEL_ERR, "Connection was hung up!");
        return -1;
    }

    /* Receive CFE_v%d ... */
    ret = TLSRecvLines(conn_info->ssl, input, sizeof(input));
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
        TLSSend(conn_info->ssl, s, sizeof(s)-1);
        return version_received;
    }
    else
    {
        char s[] = "BAD unsupported protocol version\n";
        TLSSend(conn_info->ssl, s, sizeof(s)-1);
        Log(LOG_LEVEL_ERR,
            "Client advertises unsupported protocol version: %d", version_received);
        return 0;
    }
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

    ret = TLSRecvLines(conn_info->ssl, line, sizeof(line));
    if (ret <= 0)
    {
        return -1;
    }

    /* Assert sscanf() is safe to use. */
    assert(sizeof(word1)>=sizeof(line) && sizeof(word2)>=sizeof(line));

    ret = sscanf(line, "IDENTITY %[^=]=%s %n", word1, word2, &chars_read);
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
        ret = sscanf(&line[line_pos], " %[^=]=%s %n", word1, word2, &chars_read);
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

    ret = TLSSend(conn->conn_info.ssl, s, len);
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

    conn->conn_info.ssl = SSL_new(SSLSERVERCONTEXT);
    if (conn->conn_info.ssl == NULL)
    {
        Log(LOG_LEVEL_ERR, "SSL_new: %s",
            ERR_reason_error_string(ERR_get_error()));
        return -1;
    }

    /* Now we are letting OpenSSL take over the open socket. */
    SSL_set_fd(conn->conn_info.ssl, conn->conn_info.sd);

    ret = SSL_accept(conn->conn_info.ssl);
    if (ret <= 0)
    {
        TLSLogError(conn->conn_info.ssl, LOG_LEVEL_ERR,
                    "Connection handshake", ret);
        return -1;
    }

    Log (LOG_LEVEL_VERBOSE, "TLS session established, checking trust...");

    /* Send/Receive "CFE_v%d" version string and agree on version. */
    ret = ServerNegotiateProtocol(&conn->conn_info);
    if (ret <= 0)
    {
        return -1;
    }

    /* Receive IDENTITY USER=asdf plain string. */
    ret = ServerIdentifyClient(&conn->conn_info, conn->username,
                               sizeof(conn->username));
    if (ret != 1)
    {
        return -1;
    }

    /* We *now* (maybe a bit late) verify the key that the client sent us in
     * the TLS handshake, since we need the username to do so. TODO in the
     * future store keys irrelevant of username, so that we can match them
     * before IDENTIFY. */
    ret = TLSVerifyPeer(&conn->conn_info, conn->ipaddr, conn->username);
    if (ret == -1)                                      /* error */
    {
        return -1;
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
            (IsMatchItemIn(conn->ctx, SV.trustkeylist, MapAddress(conn->ipaddr))))
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
            return -1;
        }
    }

    /* skipping CAUTH */
    conn->id_verified = 1;
    /* skipping SAUTH, allow access to read-only files */
    conn->rsa_auth = 1;
    LastSaw1(conn->ipaddr, conn->conn_info.remote_keyhash_str,
             LAST_SEEN_ROLE_ACCEPT);

    ServerSendWelcome(conn);

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
    PROTOCOL_COMMAND_SYNC,
    PROTOCOL_COMMAND_MD5,
    PROTOCOL_COMMAND_VERSION,
    PROTOCOL_COMMAND_VAR,
    PROTOCOL_COMMAND_CONTEXT,
    PROTOCOL_COMMAND_QUERY,
    PROTOCOL_COMMAND_CALL_ME_BACK,
    PROTOCOL_COMMAND_BAD
} ProtocolCommandNew;

static const char *PROTOCOL_NEW[PROTOCOL_COMMAND_BAD + 1] =
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


/****************************************************************************/
bool BusyWithNewProtocol(EvalContext *ctx, ServerConnectionState *conn)
{
    time_t tloc, trem = 0;
    char recvbuffer[CF_BUFSIZE + CF_BUFEXT], sendbuffer[CF_BUFSIZE];
    char filename[CF_BUFSIZE], args[CF_BUFSIZE], out[CF_BUFSIZE];
    long time_no_see = 0;
    unsigned int len = 0;
    int drift, received;
    ServerFileGetState get_args;
    Item *classes;

    /* We never double encrypt within the TLS layer */
    const int encrypted = 0;


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

    switch (GetCommandNew(recvbuffer))
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
            Log(LOG_LEVEL_VERBOSE, "AccessControl: access denied");
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

    case PROTOCOL_COMMAND_MD5:

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        CompareLocalHash(conn, sendbuffer, recvbuffer);
        return true;

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

    case PROTOCOL_COMMAND_QUERY:

        if (!conn->id_verified)
        {
            Log(LOG_LEVEL_INFO, "ID not verified");
            RefuseAccess(conn, 0, recvbuffer);
            return true;
        }

        if (!LiteralAccessControl(ctx, recvbuffer, conn, encrypted))
        {
            Log(LOG_LEVEL_INFO, "Query access failure");
            RefuseAccess(conn, 0, recvbuffer);
            return false;
        }

        if (GetServerQuery(conn, recvbuffer, encrypted))
        {
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
        DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

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
        return ReceiveCollectCall(conn);

    case PROTOCOL_COMMAND_BAD:
        Log(LOG_LEVEL_WARNING, "Unexpected protocol command: %s", recvbuffer);
    }

    sprintf(sendbuffer, "BAD: Request denied\n");
    SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
    Log(LOG_LEVEL_INFO, "Closing connection, due to request: '%s'", recvbuffer);
    return false;
}
