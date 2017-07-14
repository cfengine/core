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


#include <cfnet.h>

#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include <logging.h>
#include <misc_lib.h>

#include <tls_client.h>
#include <tls_generic.h>
#include <net.h>                     /* SendTransaction, ReceiveTransaction */
/* TODO move crypto.h to libutils */
#include <crypto.h>                                       /* LoadSecretKeys */


extern RSA *PRIVKEY, *PUBKEY;


/**
 * Global SSL context for initiated connections over the TLS protocol. For the
 * agent, they are written only once, *after* the common control bundles have
 * been evaluated.
 *
 * 1. Common bundles evaluation: LoadPolicy()->PolicyResolve()
 * 2. Create TLS contexts:       GenericAgentDiscoverContext()->GenericAgentInitialize()->cfnet_init()
 */
static SSL_CTX *SSLCLIENTCONTEXT = NULL;
static X509 *SSLCLIENTCERT = NULL;


bool TLSClientIsInitialized()
{
    return (SSLCLIENTCONTEXT != NULL);
}

/**
 * @warning Make sure you've called CryptoInitialize() first!
 *
 * @TODO if this function is called a second time, it just returns true, and
 * does not do nothing more. What if the settings (e.g. tls_min_version) have
 * changed? This can happen when cf-serverd reloads policy. Fixing this goes
 * much deeper though, as it would require cf-serverd to call
 * GenericAgentDiscoverContext() when reloading policy.
 */
bool TLSClientInitialize(const char *tls_min_version,
                         const char *ciphers)
{
    int ret;
    static bool is_initialised = false;

    if (is_initialised)
    {
        return true;
    }

    if (PRIVKEY == NULL || PUBKEY == NULL)
    {
        /* VERBOSE in case it's a custom, local-only installation. */
        Log(LOG_LEVEL_VERBOSE, "No public/private key pair is loaded,"
            " please create one using cf-key");
        return false;
    }
    if (!TLSGenericInitialize())
    {
        return false;
    }

    SSLCLIENTCONTEXT = SSL_CTX_new(SSLv23_client_method());
    if (SSLCLIENTCONTEXT == NULL)
    {
        Log(LOG_LEVEL_ERR, "SSL_CTX_new: %s",
            TLSErrorString(ERR_get_error()));
        goto err1;
    }

    TLSSetDefaultOptions(SSLCLIENTCONTEXT, tls_min_version);

    if (ciphers != NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Setting cipher list for outgoing TLS connections to: %s",
            ciphers);

        ret = SSL_CTX_set_cipher_list(SSLCLIENTCONTEXT, ciphers);
        if (ret != 1)
        {
            Log(LOG_LEVEL_ERR,
                "No valid ciphers in cipher list: %s",
                ciphers);
            goto err2;
        }
    }

    /* Create cert into memory and load it into SSL context. */
    SSLCLIENTCERT = TLSGenerateCertFromPrivKey(PRIVKEY);
    if (SSLCLIENTCERT == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to generate in-memory-certificate from private key");
        goto err2;
    }

    SSL_CTX_use_certificate(SSLCLIENTCONTEXT, SSLCLIENTCERT);

    ret = SSL_CTX_use_RSAPrivateKey(SSLCLIENTCONTEXT, PRIVKEY);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Failed to use RSA private key: %s",
            TLSErrorString(ERR_get_error()));
        goto err3;
    }

    /* Verify cert consistency. */
    ret = SSL_CTX_check_private_key(SSLCLIENTCONTEXT);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Inconsistent key and TLS cert: %s",
            TLSErrorString(ERR_get_error()));
        goto err3;
    }

    is_initialised = true;
    return true;

  err3:
    X509_free(SSLCLIENTCERT);
    SSLCLIENTCERT = NULL;
  err2:
    SSL_CTX_free(SSLCLIENTCONTEXT);
    SSLCLIENTCONTEXT = NULL;
  err1:
    return false;
}

void TLSDeInitialize()
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

    if (SSLCLIENTCERT != NULL)
    {
        X509_free(SSLCLIENTCERT);
        SSLCLIENTCERT = NULL;
    }

    if (SSLCLIENTCONTEXT != NULL)
    {
        SSL_CTX_free(SSLCLIENTCONTEXT);
        SSLCLIENTCONTEXT = NULL;
    }
}


/**
 * 1. Receive "CFE_v%d" server hello
 * 2. Send two lines: one "CFE_v%d" with the protocol version we wish to have,
 *    and another with id, e.g. "IDENTITY USERNAME=blah".
 * 3. Receive "OK WELCOME"
 *
 * @return > 0: success. #conn_info->type has been updated with the negotiated
 *              protocol version.
 *           0: server denial
 *          -1: error
 */
int TLSClientIdentificationDialog(ConnectionInfo *conn_info,
                                  const char *username)
{
    char line[1024] = "";
    int ret;

    /* Receive CFE_v%d ... That's the first thing the server sends. */
    ret = TLSRecvLines(conn_info->ssl, line, sizeof(line));

    ProtocolVersion wanted_version;
    if (conn_info->protocol == CF_PROTOCOL_UNDEFINED)
    {
        /* TODO parse CFE_v%d received and use that version if it's lower. */
        wanted_version = CF_PROTOCOL_LATEST;
    }
    else
    {
        wanted_version = conn_info->protocol;
    }

    /* Send "CFE_v%d cf-agent version". */
    char version_string[128];
    int len = snprintf(version_string, sizeof(version_string),
                       "CFE_v%d %s %s\n",
                       wanted_version, "cf-agent", VERSION); /* TODO argv[0] */

    ret = TLSSend(conn_info->ssl, version_string, len);
    if (ret != len)
    {
        Log(LOG_LEVEL_ERR, "Connection was hung up during identification!");
        return -1;
    }

    strcpy(line, "IDENTITY");
    size_t line_len = strlen(line);

    if (username != NULL)
    {
        ret = snprintf(&line[line_len], sizeof(line) - line_len,
                       " USERNAME=%s", username);
        if (ret >= sizeof(line) - line_len)
        {
            Log(LOG_LEVEL_ERR, "Sending IDENTITY truncated: %s", line);
            return -1;
        }
        line_len += ret;
    }

    /* Overwrite the terminating '\0', we don't need it anyway. */
    line[line_len] = '\n';
    line_len++;

    ret = TLSSend(conn_info->ssl, line, line_len);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Connection was hung up during identification! (2)");
        return -1;
    }

    /* Server might hang up here, after we sent identification! We
     * must get the "OK WELCOME" message for everything to be OK. */
    static const char OK[] = "OK WELCOME";
    size_t OK_len = sizeof(OK) - 1;
    ret = TLSRecvLines(conn_info->ssl, line, sizeof(line));
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Connection was hung up during identification! (3)");
        return -1;
    }

    if (ret < OK_len || strncmp(line, OK, OK_len) != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Peer did not accept our identity! Responded: %s",
            line);
        return 0;
    }

    /* Before it contained the protocol version we requested from the server,
     * now we put in the value that was negotiated. */
    conn_info->protocol = wanted_version;

    return 1;
}

/**
 * We directly initiate a TLS handshake with the server. If the server is old
 * version (does not speak TLS) the connection will be denied.
 * @note the socket file descriptor in #conn_info must be connected and *not*
 *       non-blocking
 * @return -1 in case of error
 */
int TLSTry(ConnectionInfo *conn_info)
{
    if (PRIVKEY == NULL || PUBKEY == NULL)
    {
        Log(LOG_LEVEL_ERR, "No public/private key pair is loaded,"
            " please create one using cf-key");
        return -1;
    }
    assert(SSLCLIENTCONTEXT != NULL);

    conn_info->ssl = SSL_new(SSLCLIENTCONTEXT);
    if (conn_info->ssl == NULL)
    {
        Log(LOG_LEVEL_ERR, "SSL_new: %s",
            TLSErrorString(ERR_get_error()));
        return -1;
    }

    /* Pass conn_info inside the ssl struct for TLSVerifyCallback(). */
    SSL_set_ex_data(conn_info->ssl, CONNECTIONINFO_SSL_IDX, conn_info);

    /* Initiate the TLS handshake over the already open TCP socket. */
    SSL_set_fd(conn_info->ssl, conn_info->sd);

    int ret = SSL_connect(conn_info->ssl);
    if (ret <= 0)
    {
        TLSLogError(conn_info->ssl, LOG_LEVEL_ERR,
                    "Failed to establish TLS connection", ret);
        return -1;
    }

    Log(LOG_LEVEL_VERBOSE, "TLS version negotiated: %8s; Cipher: %s,%s",
        SSL_get_version(conn_info->ssl),
        SSL_get_cipher_name(conn_info->ssl),
        SSL_get_cipher_version(conn_info->ssl));
    Log(LOG_LEVEL_VERBOSE, "TLS session established, checking trust...");

    return 0;
}
