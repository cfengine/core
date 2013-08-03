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


#include "cfnet.h"

#include "logging.h"
#include "misc_lib.h"

#include "tls_generic.h"
#include "net.h"                     /* SendTransaction, ReceiveTransaction */

extern RSA* PRIVKEY;                      /* TODO move crypto.h to libutils */


/* Global SSL context for client connections over new TLS protocol. */
static SSL_CTX *SSLCLIENTCONTEXT = NULL;
static X509 *SSLCLIENTCERT = NULL;


/**
 * @warning Make sure you've called CryptoInitialize() first!
 */
bool TLSClientInitialize()
{
    int ret;

    /* OpenSSL is needed for our new protocol over TLS. */
    SSL_library_init();
    SSL_load_error_strings();

    assert(SSLCLIENTCONTEXT == NULL);
    SSLCLIENTCONTEXT = SSL_CTX_new(SSLv23_client_method());
    if (SSLCLIENTCONTEXT == NULL)
    {
        Log(LOG_LEVEL_ERR, "SSL_CTX_new: %s",
            ERR_reason_error_string(ERR_get_error()));
        return false;
    }

    /* Use only TLS v1 or later.
       TODO option for SSL_OP_NO_TLSv{1,1_1} */
    SSL_CTX_set_options(SSLCLIENTCONTEXT,
                        SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

    /* Never bother with retransmissions, SSL_write() and SSL_read() should
     * always either write/read the whole amount or fail. */
    SSL_CTX_set_mode(SSLCLIENTCONTEXT, SSL_MODE_AUTO_RETRY);

    /*
     * Create cert into memory and load it into context.
     */

    if (PRIVKEY == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "No public/private key pair is loaded, create one with cf-key");
        return false;
    }

    /* Generate self-signed cert valid from now to 100 years later. */
    X509 *x509 = X509_new();
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_time_adj_ex(X509_get_notAfter(x509), 365*100, 0, NULL);
    EVP_PKEY *pkey = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pkey, PRIVKEY);
    X509_NAME *name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               (const char *) "ouripaddress",   /* TODO */
                               -1, -1, 0);
    X509_set_issuer_name(x509, name);
    X509_set_pubkey(x509, pkey);
    X509_sign(x509, pkey, EVP_sha384());

    /* Log(LOG_LEVEL_ERR, "generate cert from priv key: %s", */
    /*     ERR_reason_error_string(ERR_get_error())); */

    SSL_CTX_use_certificate(SSLCLIENTCONTEXT, x509);
    SSLCLIENTCERT = x509;

    ret = SSL_CTX_use_RSAPrivateKey(SSLCLIENTCONTEXT, PRIVKEY);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Failed to use RSA private key: %s",
            ERR_reason_error_string(ERR_get_error()));
        return false;
    }
    ret = SSL_CTX_check_private_key(SSLCLIENTCONTEXT);    /* verify cert consistency */
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Inconsistent key and TLS cert: %s",
            ERR_reason_error_string(ERR_get_error()));
        return false;
    }

    /* Set options to always request a certificate from the peer, either we
     * are client or server. */
    SSL_CTX_set_verify(SSLCLIENTCONTEXT, SSL_VERIFY_PEER, NULL);
    /* Always accept that certificate, we do proper checking after TLS
     * connection is established since OpenSSL can't pass a connection
     * specific pointer to the callback (so we would have to lock).  */
    SSL_CTX_set_cert_verify_callback(SSLCLIENTCONTEXT, TLSVerifyCallback, NULL);

    return true;
}

void TLSDeInitialize()
{
    SSL_CTX_free(SSLCLIENTCONTEXT);
}

/*
 * The tricky part about enabling TLS is that the server might disconnect us if
 * it does not support the STARTTLS command. Once disconnected we will need to
 * reconnect and make sure everything works again.
 */
int TLSTry(ConnectionInfo *conn_info)
{
    char buffer[CF_BUFSIZE] = "";
    int result = 0;

    assert(SSLCLIENTCONTEXT != NULL);         /* TLSInitialize() has been called. */

    if (conn_info->type == CF_PROTOCOL_TLS)
    {
        Log(LOG_LEVEL_ERR, "We are already on TLS mode, skipping initialization");
        return 0;
    }

    result = SendTransaction(conn_info, "STARTTLS", 0, CF_DONE);
    if (result < 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to start TLS");
        return -1;
    }

    result = ReceiveTransaction(conn_info, buffer, NULL);
    if (strcmp(buffer, "ACK") == 0)
    {
        Log(LOG_LEVEL_VERBOSE,
            "STARTTLS accepted by server, initiating TLS handshake");

        /* Let OpenSSL take over existing connection. */
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

        /* Now we send the TLS request to the server. */
        int total_tries = 0;
        do
        {
            result = SSL_connect(conn_info->ssl);
            if (result <= 0)
            {
                Log(LOG_LEVEL_ERR, "Problems with TLS negotiation, trying again");
                /*
                 * Identify the problem and if possible try to fix it.
                 */
                int error = SSL_get_error(conn_info->ssl, result);
                if ((SSL_ERROR_WANT_WRITE == error) || (SSL_ERROR_WANT_READ == error))
                {
                    Log(LOG_LEVEL_ERR, "Recoverable error in TLS handshake, trying to fix it");
                    /*
                     * We can try to fix this.
                     * This error means that there was not enough data in the buffer, using select
                     * to wait until we get more data.
                     */
                    fd_set wfds;
                    struct timeval tv;
                    int tries = 0;

                    do {
                        SET_DEFAULT_TLS_TIMEOUT(tv);
                        FD_ZERO(&wfds);
                        FD_SET(conn_info->sd, &wfds);

                        result = select(conn_info->sd + 1, NULL, &wfds, NULL, &tv);
                        if (result > 0)
                        {
                            Log (LOG_LEVEL_INFO, "TLS connection established");
                            break;
                        }
                        else
                        {
                            Log(LOG_LEVEL_ERR, "select(2) timed out, retrying (tries: %d)", tries);
                            ++tries;
                        }
                    } while (tries <= DEFAULT_TLS_TRIES);
                }
                else
                {
                    /*
                     * Unrecoverable error
                     */
                    int e = ERR_get_error();
                    Log(LOG_LEVEL_ERR, "TLS handshake err %d %d: %s",
                        error, e, ERR_reason_error_string(e));
                    return -1;
                }
            }
            else
            {
                Log (LOG_LEVEL_VERBOSE, "TLS session established, checking trust...");
                break;
            }
            ++total_tries;
        }
        while (total_tries <= DEFAULT_TLS_TRIES);
    }
    else
    {
        Log(LOG_LEVEL_WARNING, "Server rejected STARTTLS, reply: %s", buffer);
        return -1;
    }

    return 0;
}
