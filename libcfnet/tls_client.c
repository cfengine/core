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


#include <cfnet.h>

#include <logging.h>
#include <misc_lib.h>

#include <tls_client.h>
#include <tls_generic.h>
#include <net.h>                     /* SendTransaction, ReceiveTransaction */

/* TODO move crypto.h to libutils */
#include <crypto.h>                        /* PRIVKEY,PUBKEY,LoadSecretKeys */
#include <bootstrap.h>                     /* ReadPolicyServerFile */
extern char CFWORKDIR[];


/* Global SSL context for client connections over new TLS protocol. */
static SSL_CTX *SSLCLIENTCONTEXT = NULL;
static X509 *SSLCLIENTCERT = NULL;


/**
 * @warning Make sure you've called CryptoInitialize() first!
 */
bool TLSClientInitialize()
{
    int ret;
    static bool is_initialised = false;

    if (is_initialised)
    {
        return true;
    }

    /* OpenSSL is needed for our new protocol over TLS. */
    SSL_library_init();
    SSL_load_error_strings();

    SSLCLIENTCONTEXT = SSL_CTX_new(SSLv23_client_method());
    if (SSLCLIENTCONTEXT == NULL)
    {
        Log(LOG_LEVEL_ERR, "SSL_CTX_new: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err1;
    }

    /* Use only TLS v1 or later.
       TODO option for SSL_OP_NO_TLSv{1,1_1} */
    SSL_CTX_set_options(SSLCLIENTCONTEXT,
                        SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

    /* Never bother with retransmissions, SSL_write() should
     * always either write the whole amount or fail. */
    SSL_CTX_set_mode(SSLCLIENTCONTEXT, SSL_MODE_AUTO_RETRY);

    /*
     * Create cert into memory and load it into SSL context.
     */

    if (PRIVKEY == NULL || PUBKEY == NULL)
    {
        Log(LOG_LEVEL_WARNING,
            "No public/private key pair is loaded, trying to reload");
        LoadSecretKeys(ReadPolicyServerFile(CFWORKDIR));
        if (PRIVKEY == NULL || PUBKEY == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "No public/private key pair found");
            goto err2;
        }
    }

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
        SSLCLIENTCERT = x509;

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

    SSL_CTX_use_certificate(SSLCLIENTCONTEXT, SSLCLIENTCERT);

    ret = SSL_CTX_use_RSAPrivateKey(SSLCLIENTCONTEXT, PRIVKEY);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Failed to use RSA private key: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err3;
    }

    /* Verify cert consistency. */
    ret = SSL_CTX_check_private_key(SSLCLIENTCONTEXT);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Inconsistent key and TLS cert: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err3;
    }

    /* Set options to always request a certificate from the peer, either we
     * are client or server. */
    SSL_CTX_set_verify(SSLCLIENTCONTEXT, SSL_VERIFY_PEER, NULL);
    /* Always accept that certificate, we do proper checking after TLS
     * connection is established since OpenSSL can't pass a connection
     * specific pointer to the callback (so we would have to lock).  */
    SSL_CTX_set_cert_verify_callback(SSLCLIENTCONTEXT, TLSVerifyCallback, NULL);

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
 * @return >0: the version that was negotiated
 *          0: no agreement on version was reached
 *         -1: error
 */
int TLSClientNegotiateProtocol(const ConnectionInfo *conn_info)
{
    int ret;
    char input[CF_SMALLBUF] = "";

    /* Receive CFE_v%d ... */
    ret = TLSRecvLines(conn_info->ssl, input, sizeof(input));

    /* Send "CFE_v%d cf-agent version". */
    char version_string[128];
    int len = snprintf(version_string, sizeof(version_string),
                       "CFE_v%d %s %s\n",
                       CFNET_PROTOCOL_VERSION, "cf-agent", VERSION);

    ret = TLSSend(conn_info->ssl, version_string, len);
    if (ret != len)
    {
        Log(LOG_LEVEL_ERR, "Connection was hung up!");
        return -1;
    }

    /* Receive OK */
    ret = TLSRecvLines(conn_info->ssl, input, sizeof(input));
    if (strncmp(input, "OK", strlen("OK")) == 0)
        return 1;
    else
        return 0;
}

int TLSClientSendIdentity(const ConnectionInfo *conn_info, const char *username)
{
    char line[1024] = "IDENTITY";
    size_t line_len = strlen(line);
    int ret;

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
        return -1;
    }

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
    /* SSL Context might not be initialised up to now due to lack of keys, as
     * they might be generated as part of the policy (e.g. failsafe.cf). */
    if (!TLSClientInitialize())
    {
        return -1;
    }
    assert(SSLCLIENTCONTEXT != NULL && PRIVKEY != NULL && PUBKEY != NULL);

    if (conn_info->type == CF_PROTOCOL_TLS)
    {
        Log(LOG_LEVEL_WARNING,
            "We are already on TLS mode, skipping initialization");
        return 0;
    }

    conn_info->type = CF_PROTOCOL_TLS;
    conn_info->ssl = SSL_new(SSLCLIENTCONTEXT);
    if (conn_info->ssl == NULL)
    {
        Log(LOG_LEVEL_ERR, "SSL_new: %s",
            ERR_reason_error_string(ERR_get_error()));
        return -1;
    }

    /* Initiate the TLS handshake over the already open TCP socket. */
    SSL_set_fd(conn_info->ssl, conn_info->sd);

    int ret = SSL_connect(conn_info->ssl);
    if (ret <= 0)
    {
        TLSLogError(conn_info->ssl, LOG_LEVEL_ERR,
                    "Connection handshake", ret);
        return -1;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "TLS session established, checking trust...");
    }

    return 0;
}
