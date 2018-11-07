/*
   Copyright 2018 Northern.tech AS

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

#include <logging.h>                                            /* LogLevel */
#include <misc_lib.h>
#include <string_lib.h>

/* TODO move crypto.h to libutils */
#include <crypto.h>                                    /* HavePublicKeyByIP */
#include <files_hashes.h>                              /* HashPubKey */

#include <assert.h>

/* known TLS versions */
enum tls_version {
    TLS_1_0 = 0,
    TLS_1_1 = 1,
    TLS_1_2 = 2,
    TLS_1_3 = 3,
};
#define TLS_LAST TLS_1_3

/* determine the highest TLS version supported by the available/used version of
 * OpenSSL */
#if defined(SSL_OP_NO_TLSv1_3)
#define HAVE_TLS_1_3
#define TLS_HIGHEST_SUPPORTED TLS_1_3
#elif defined(SSL_OP_NO_TLSv1_2)
#define TLS_HIGHEST_SUPPORTED TLS_1_2
#elif defined(SSL_OP_NO_TLSv1_1)
#define TLS_HIGHEST_SUPPORTED TLS_1_1
#else
#define TLS_HIGHEST_SUPPORTED TLS_1_0
#endif

/* the lowest version of TLS we always require */
#define TLS_LOWEST_REQUIRED TLS_1_0

#ifndef SSL_OP_NO_TLSv1_3
#define SSL_OP_NO_TLSv1_3 0     /* no-op when ORed with bit flags */
#endif
#ifndef SSL_OP_NO_TLSv1_2
#define SSL_OP_NO_TLSv1_2 0
#endif
#ifndef SSL_OP_NO_TLSv1_1
#define SSL_OP_NO_TLSv1_1 0
#endif

static const char *const tls_version_strings[TLS_LAST + 1] = {"1.0", "1.1", "1.2", "1.3"};
static unsigned int tls_disable_flags[TLS_LAST + 1] = {0, SSL_OP_NO_TLSv1_1, SSL_OP_NO_TLSv1_2, SSL_OP_NO_TLSv1_3};

int CONNECTIONINFO_SSL_IDX = -1;


const char *TLSErrorString(intmax_t errcode)
{
    const char *errmsg = ERR_reason_error_string((unsigned long) errcode);
    return (errmsg != NULL) ? errmsg : "no error message";
}

bool TLSGenericInitialize()
{
    static bool is_initialised = false;

    /* We must make sure that SSL_get_ex_new_index() is called only once! */
    if (is_initialised)
    {
        return true;
    }

    /* OpenSSL is needed for TLS. */
    SSL_library_init();
    SSL_load_error_strings();

    /* Register a unique place to store ConnectionInfo within SSL struct. */
    CONNECTIONINFO_SSL_IDX =
        SSL_get_ex_new_index(0, "Pointer to ConnectionInfo",
                             NULL, NULL, NULL);

    is_initialised = true;
    return true;
}

/**
 * @retval 1 equal
 * @retval 0 not equal
 * @retval -1 error
 */
static int CompareCertToRSA(X509 *cert, RSA *rsa_key)
{
    int ret;
    int retval = -1;                                            /* ERROR */

    EVP_PKEY *cert_pkey = X509_get_pubkey(cert);
    if (cert_pkey == NULL)
    {
        Log(LOG_LEVEL_ERR, "X509_get_pubkey: %s",
            TLSErrorString(ERR_get_error()));
        goto ret1;
    }
    if (EVP_PKEY_base_id(cert_pkey) != EVP_PKEY_RSA)
    {
        Log(LOG_LEVEL_ERR,
            "Received key of unknown type, only RSA currently supported!");
        goto ret2;
    }

    RSA *cert_rsa_key = EVP_PKEY_get1_RSA(cert_pkey);
    if (cert_rsa_key == NULL)
    {
        Log(LOG_LEVEL_ERR, "TLSVerifyPeer: EVP_PKEY_get1_RSA failed!");
        goto ret2;
    }

    EVP_PKEY *rsa_pkey = EVP_PKEY_new();
    if (rsa_pkey == NULL)
    {
        Log(LOG_LEVEL_ERR, "TLSVerifyPeer: EVP_PKEY_new allocation failed!");
        goto ret3;
    }

    ret = EVP_PKEY_set1_RSA(rsa_pkey, rsa_key);
    if (ret == 0)
    {
        Log(LOG_LEVEL_ERR, "TLSVerifyPeer: EVP_PKEY_set1_RSA failed!");
        goto ret4;
    }

    ret = EVP_PKEY_cmp(cert_pkey, rsa_pkey);
    if (ret == 1)
    {
        Log(LOG_LEVEL_DEBUG,
            "Public key to certificate compare equal");
        retval = 1;                                             /* EQUAL */
    }
    else if (ret == 0 || ret == -1)
    {
        Log(LOG_LEVEL_DEBUG,
            "Public key to certificate compare different");
        retval = 0;                                            /* NOT EQUAL */
    }
    else
    {
        Log(LOG_LEVEL_ERR, "OpenSSL EVP_PKEY_cmp: %d %s",
            ret, TLSErrorString(ERR_get_error()));
    }

  ret4:
    EVP_PKEY_free(rsa_pkey);
  ret3:
    RSA_free(cert_rsa_key);
  ret2:
    EVP_PKEY_free(cert_pkey);

  ret1:
    return retval;
}

/**
 * The only thing we make sure here is that any key change is not allowed. All
 * the rest of authentication happens separately *after* the initial
 * handshake, thus *after* this callback has returned successfully and TLS
 * session has been established.
 * @return 0 on error, 1 on success
 * @note This is an SSL callback, return type has to be int, not bool
 */
int TLSVerifyCallback(X509_STORE_CTX *store_ctx,
                      void *arg ARG_UNUSED)
{

    /* It's kind of tricky to get custom connection-specific info in this
     * callback. We first acquire a pointer to the SSL struct of the
     * connection and... */
    int ssl_idx = SSL_get_ex_data_X509_STORE_CTX_idx();
    SSL *ssl = X509_STORE_CTX_get_ex_data(store_ctx, ssl_idx);
    if (ssl == NULL)
    {
        UnexpectedError("No SSL context during handshake, denying!");
        return 0;
    }

    /* ...and then we ask for the custom data we attached there. */
    ConnectionInfo *conn_info = SSL_get_ex_data(ssl, CONNECTIONINFO_SSL_IDX);
    if (conn_info == NULL)
    {
        UnexpectedError("No conn_info at index %d", CONNECTIONINFO_SSL_IDX);
        return 0;
    }

    /* From that data get the key if the connection is already established. */
    RSA *already_negotiated_key = KeyRSA(conn_info->remote_key);
    /* Is there an already negotiated certificate? */
    X509 *previous_tls_cert = SSL_get_peer_certificate(ssl);

    if (previous_tls_cert == NULL)
    {
        Log(LOG_LEVEL_DEBUG, "TLSVerifyCallback: no ssl->peer_cert");
        if (already_negotiated_key == NULL)
        {
            Log(LOG_LEVEL_DEBUG, "TLSVerifyCallback: no conn_info->key");
            Log(LOG_LEVEL_DEBUG,
                "This must be the initial TLS handshake, accepting");
            return 1;                                   /* ACCEPT HANDSHAKE */
        }
        else
        {
            UnexpectedError("Initial handshake, but old keys differ, denying!");
            return 0;
        }
    }
    else                                     /* TLS renegotiation handshake */
    {
        if (already_negotiated_key == NULL)
        {
            Log(LOG_LEVEL_DEBUG, "TLSVerifyCallback: no conn_info->key");
            Log(LOG_LEVEL_ERR,
                "Renegotiation handshake before trust was established, denying!");
            X509_free(previous_tls_cert);
            return 0;                                           /* fishy */
        }
        else
        {
            /* previous_tls_cert key should match already_negotiated_key. */
            if (CompareCertToRSA(previous_tls_cert,
                                 already_negotiated_key) != 1)
            {
                UnexpectedError("Renegotiation caused keys to differ, denying!");
                X509_free(previous_tls_cert);
                return 0;
            }
            else
            {
                /* THIS IS THE ONLY WAY TO CONTINUE */
            }
        }
    }

    assert(previous_tls_cert != NULL);
    assert(already_negotiated_key != NULL);

    /* At this point we have ensured that previous_tls_cert->key is equal
     * to already_negotiated_key, so we might as well forget the former. */
    X509_free(previous_tls_cert);

    /* We want to compare already_negotiated_key to the one the peer
     * negotiates in this TLS renegotiation. So, extract first certificate out
     * of the chain the peer sent. It should be the only one since we do not
     * support certificate chains, we just want the RSA key. */
    STACK_OF(X509) *chain = X509_STORE_CTX_get_chain(store_ctx);
    if (chain == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "No certificate chain inside TLS handshake, denying!");
        return 0;
    }

    int chain_len = sk_X509_num(chain);
    if (chain_len != 1)
    {
        Log(LOG_LEVEL_ERR,
            "More than one certificate presented in TLS handshake, refusing handshake!");
        return 0;
    }

    X509 *new_cert = sk_X509_value(chain, 0);
    if (new_cert == NULL)
    {
        UnexpectedError("NULL certificate at the beginning of chain!");
        return 0;
    }

    if (CompareCertToRSA(new_cert, already_negotiated_key) != 1)
    {
        Log(LOG_LEVEL_ERR,
            "Peer attempted to change key during TLS renegotiation, denying!");
        return 0;
    }

    Log(LOG_LEVEL_DEBUG,
        "TLS renegotiation occurred but keys are still the same, accepting");
    return 1;                                           /* ACCEPT HANDSHAKE */
}

/**
 * @retval 1 if the public key used by the peer in the TLS handshake is the
 *         same with the one stored for that host.
 * @retval 0 if stored key for the host is missing or differs from the one
 *         received.
 * @retval -1 in case of other error (error will be Log()ed).
 * @note When return value is != -1 (so no error occurred) the #conn_info struct
 *       should have been populated, with key received and its hash.
 */
int TLSVerifyPeer(ConnectionInfo *conn_info, const char *remoteip, const char *username)
{
    int ret, retval;

    X509 *received_cert = SSL_get_peer_certificate(conn_info->ssl);
    if (received_cert == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "No certificate presented by remote peer (openssl: %s)",
            TLSErrorString(ERR_get_error()));
        retval = -1;
        goto ret1;
    }

    EVP_PKEY *received_pubkey = X509_get_pubkey(received_cert);
    if (received_pubkey == NULL)
    {
        Log(LOG_LEVEL_ERR, "X509_get_pubkey: %s",
            TLSErrorString(ERR_get_error()));
        retval = -1;
        goto ret2;
    }
    if (EVP_PKEY_base_id(received_pubkey) != EVP_PKEY_RSA)
    {
        Log(LOG_LEVEL_ERR,
            "Received key of unknown type, only RSA currently supported!");
        retval = -1;
        goto ret3;
    }

    RSA *remote_key = EVP_PKEY_get1_RSA(received_pubkey);
    if (remote_key == NULL)
    {
        Log(LOG_LEVEL_ERR, "TLSVerifyPeer: EVP_PKEY_get1_RSA failed!");
        retval = -1;
        goto ret3;
    }

    Key *key = KeyNew(remote_key, CF_DEFAULT_DIGEST);
    conn_info->remote_key = key;

    /*
     * Compare the key received with the one stored.
     */
    const char *key_hash = KeyPrintableHash(key);
    RSA *expected_rsa_key = HavePublicKey(username, remoteip, key_hash);

    if (expected_rsa_key == NULL)
    {
        /* TODO LOG_LEVEL_NOTICE once cf-serverd logs to a different file. */
        Log(LOG_LEVEL_VERBOSE,
            "Received key '%s' not found in ppkeys", key_hash);
        retval = 0;
        goto ret4;
    }

    EVP_PKEY *expected_pubkey = EVP_PKEY_new();
    if (expected_pubkey == NULL)
    {
        Log(LOG_LEVEL_ERR, "TLSVerifyPeer: EVP_PKEY_new allocation failed!");
        retval = -1;
        goto ret5;
    }

    ret = EVP_PKEY_set1_RSA(expected_pubkey, expected_rsa_key);
    if (ret == 0)
    {
        Log(LOG_LEVEL_ERR, "TLSVerifyPeer: EVP_PKEY_set1_RSA failed!");
        retval = -1;
        goto ret6;
    }

    ret = EVP_PKEY_cmp(received_pubkey, expected_pubkey);
    if (ret == 1)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Received public key compares equal to the one we have stored");
        retval = 1;               /* TRUSTED KEY, equal to the expected one */
        goto ret6;
    }
    else if (ret == 0 || ret == -1)
    {
        Log(LOG_LEVEL_NOTICE,
            "Received key '%s' compares different to the one in ppkeys",
            key_hash);
        retval = 0;
        goto ret6;
    }
    else
    {
        Log(LOG_LEVEL_ERR, "OpenSSL EVP_PKEY_cmp: %d %s",
            ret, TLSErrorString(ERR_get_error()));
        retval = -1;
        goto ret6;
    }

    UnexpectedError("Unreachable!");
    return 0;

  ret6:
    EVP_PKEY_free(expected_pubkey);
  ret5:
    RSA_free(expected_rsa_key);
  ret4:
    if (retval == -1)
    {
        /* We won't be needing conn_info->remote_key */
        KeyDestroy(&key);
        conn_info->remote_key = NULL;
    }
  ret3:
    EVP_PKEY_free(received_pubkey);
  ret2:
    X509_free(received_cert);
  ret1:
    return retval;
}

/**
 * @brief Generate and return a dummy in-memory X509 certificate signed with
 *        the private key passed. It is valid from now to 50 years later...
 */
X509 *TLSGenerateCertFromPrivKey(RSA *privkey)
{
    int ret;
    X509 *x509 = X509_new();
    if (x509 == NULL)
    {
        Log(LOG_LEVEL_ERR, "X509_new: %s",
            TLSErrorString(ERR_get_error()));
        goto err1;
    }

    ASN1_TIME *t1 = X509_gmtime_adj(X509_get_notBefore(x509), 0);
    ASN1_TIME *t2 = X509_gmtime_adj(X509_get_notAfter(x509), 60*60*24*365*10);
    if (t1 == NULL || t2 == NULL)
    {
        Log(LOG_LEVEL_ERR, "X509_gmtime_adj: %s",
            TLSErrorString(ERR_get_error()));
        goto err2;
    }

    EVP_PKEY *pkey = EVP_PKEY_new();
    if (pkey == NULL)
    {
        Log(LOG_LEVEL_ERR, "EVP_PKEY_new: %s",
            TLSErrorString(ERR_get_error()));
        goto err2;
    }

    ret = EVP_PKEY_set1_RSA(pkey, privkey);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "EVP_PKEY_set1_RSA: %s",
            TLSErrorString(ERR_get_error()));
        goto err3;
    }

    X509_NAME *name = X509_get_subject_name(x509);
    if (name == NULL)
    {
        Log(LOG_LEVEL_ERR, "X509_get_subject_name: %s",
            TLSErrorString(ERR_get_error()));
        goto err3;
    }

    ret = 0;
    ret += X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                      (const char *) "a",
                                      -1, -1, 0);
    ret += X509_set_issuer_name(x509, name);
    ret += X509_set_pubkey(x509, pkey);
    if (ret < 3)
    {
        Log(LOG_LEVEL_ERR, "Failed to set certificate details: %s",
            TLSErrorString(ERR_get_error()));
        goto err3;
    }

    const EVP_MD *md = EVP_get_digestbyname("sha384");
    if (md == NULL)
    {
        Log(LOG_LEVEL_ERR, "OpenSSL: Unknown digest algorithm %s",
            "sha384");
        goto err3;
    }

    if (getenv("CFENGINE_TEST_PURIFY_OPENSSL") != NULL)
    {
        RSA_blinding_off(privkey);
    }

    /* Not really needed since the other side does not
       verify the signature. */
    ret = X509_sign(x509, pkey, md);
    /* X509_sign obscurely returns the length of the signature... */
    if (ret == 0)
    {
        Log(LOG_LEVEL_ERR, "X509_sign: %s",
            TLSErrorString(ERR_get_error()));
        goto err3;
    }

    EVP_PKEY_free(pkey);

    assert(x509 != NULL);
    return x509;


  err3:
    EVP_PKEY_free(pkey);
  err2:
    X509_free(x509);
  err1:
    return NULL;
}

static const char *TLSPrimarySSLError(int code)
{
    switch (code)
    {
    case SSL_ERROR_NONE:
        return "TLSGetSSLErrorString: No SSL error!";
    case SSL_ERROR_ZERO_RETURN:
        return "TLS session has been terminated (SSL_ERROR_ZERO_RETURN)";
    case SSL_ERROR_WANT_READ:
        return "SSL_ERROR_WANT_READ";
    case SSL_ERROR_WANT_WRITE:
        return "SSL_ERROR_WANT_WRITE";
    case SSL_ERROR_WANT_CONNECT:
        return "SSL_ERROR_WANT_CONNECT";
    case SSL_ERROR_WANT_ACCEPT:
        return "SSL_ERROR_WANT_ACCEPT";
    case SSL_ERROR_WANT_X509_LOOKUP:
        return "SSL_ERROR_WANT_X509_LOOKUP";
    case SSL_ERROR_SYSCALL:
        return "SSL_ERROR_SYSCALL";
    case SSL_ERROR_SSL:
        return "SSL_ERROR_SSL";
    }
    return "Unknown OpenSSL error code!";
}

/**
 * @brief OpenSSL is missing an SSL_reason_error_string() like
 *        ERR_reason_error_string().  Provide missing functionality here,
 *        since it's kind of complicated.
 * @param #prepend String to prepend to the SSL error.
 * @param #code Return code from the OpenSSL function call.
 * @warning Use only for SSL_connect(), SSL_accept(), SSL_do_handshake(),
 *          SSL_read(), SSL_peek(), SSL_write(), see SSL_get_error man page.
 */
int TLSLogError(SSL *ssl, LogLevel level, const char *prepend, int retcode)
{
    assert(prepend != NULL);

    /* For when retcode==SSL_ERROR_SYSCALL. */
    const char *syserr = (errno != 0) ? GetErrorStr() : "";
    int errcode         = SSL_get_error(ssl, retcode);
    const char *errstr1 = TLSPrimarySSLError(errcode);
    /* For SSL_ERROR_SSL, SSL_ERROR_SYSCALL (man SSL_get_error). It's not
     * useful for SSL_read() and SSL_write(). */
    const char *errstr2 = ERR_reason_error_string(ERR_get_error());

    /* We know the socket is always blocking. However our blocking sockets
     * have a timeout set via means of setsockopt(SO_RCVTIMEO), so internally
     * OpenSSL can still get the EWOULDBLOCK error code from recv(). In that
     * case OpenSSL gives us SSL_ERROR_WANT_READ despite the socket being
     * blocking. So we log a proper error message! */
    if (errcode == SSL_ERROR_WANT_READ)
    {
        Log(level, "%s: receive timeout", prepend);
    }
    else if (errcode == SSL_ERROR_WANT_WRITE)
    {
        Log(level, "%s: send timeout", prepend);
    }
    /* if we got SSL_ERROR_SYSCALL and ERR_get_error() returned 0 then take
     * ret into account (man SSL_get_error). */
    else if (errcode == SSL_ERROR_SYSCALL && errstr2 == NULL &&
             (retcode == 0 || retcode == -1))
    {
        /* This is not described in SSL_get_error manual, but play it safe. */
        if ((SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN) != 0)
        {
            Log(level, "%s: remote peer terminated TLS session",
                prepend);
        }
        /* "an EOF was observed that violates the protocol" */
        else if (retcode == 0)
        {
            Log(level, "%s: socket closed", prepend);
        }
        /* "the underlying BIO reported an I/O error" */
        else if (retcode == -1)
        {
            Log(level, "%s: underlying network error (%s)", prepend, syserr);
        }
    }
    else                                 /* generic error printing fallback */
    {
        Log(level, "%s: (%d %s) %s %s",
            prepend, retcode, errstr1,
            (errstr2 == NULL) ? "" : errstr2,          /* most likely empty */
            syserr);
    }

    return errcode;
}

static void assert_SSLIsBlocking(const SSL *ssl)
{
#if !defined(NDEBUG) && !defined(__MINGW32__)
    int fd = SSL_get_fd(ssl);
    if (fd >= 0)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags != -1 && (flags & O_NONBLOCK) != 0)
        {
            ProgrammingError("OpenSSL socket is non-blocking!");
        }
    }
#else // silence compiler warning
    ssl = NULL;
#endif
}

/**
 * @brief Sends the data stored on the buffer using a TLS session.
 * @param ssl SSL information.
 * @param buffer Data to send.
 * @param length Length of the data to send.
 * @return The length of the data sent (always equals #length if SSL is set
 *         up correctly, see note), or -1 in case of error, or 0 for connection
 *         closed.
 *
 * @note Use only for *blocking* sockets. Set
 *       SSL_CTX_set_mode(SSL_MODE_AUTO_RETRY) and make sure you haven't
 *       turned on SSL_MODE_ENABLE_PARTIAL_WRITE so that either the
 *       operation is completed (retval==length) or an error occurred.
 *
 * @TODO ERR_get_error is only meaningful for some error codes, so check and
 *       return empty string otherwise.
 */
int TLSSend(SSL *ssl, const char *buffer, int length)
{
    assert(length >= 0);
    assert_SSLIsBlocking(ssl);

    if (length == 0)
    {
        UnexpectedError("TLSSend: Zero length buffer!");
        return 0;
    }

    EnforceBwLimit(length);

    int sent = SSL_write(ssl, buffer, length);
    if (sent == 0)
    {
        if ((SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN) != 0)
        {
            Log(LOG_LEVEL_ERR,
                "Remote peer terminated TLS session (SSL_write)");
            return 0;
        }
        else
        {
            TLSLogError(ssl, LOG_LEVEL_ERR,
                        "Connection unexpectedly closed (SSL_write)",
                        sent);
            return 0;
        }
    }
    if (sent < 0)
    {
        TLSLogError(ssl, LOG_LEVEL_ERR, "SSL_write", sent);
        return -1;
    }

    return sent;
}

/**
 * @brief Receives at most #length bytes of data from the SSL session
 *        and stores it in the buffer.
 * @param ssl SSL information.
 * @param buffer Buffer, of size at least #toget + 1 to store received data.
 * @param toget Length of the data to receive, must be < CF_BUFSIZE.
 *
 * @return The length of the received data, which should be equal or less
 *         than the requested amount.
 *         -1 in case of timeout or error - SSL session is unusable
 *         0  if connection was closed
 *
 * @note Use only for *blocking* sockets. Set
 *       SSL_CTX_set_mode(SSL_MODE_AUTO_RETRY) to make sure that either
 *       operation completed or an error occurred.
 * @note Still, it may happen for #retval to be less than #toget, if the
 *       opposite side completed a TLSSend() with number smaller than #toget.
 */
int TLSRecv(SSL *ssl, char *buffer, int toget)
{
    assert(toget > 0);
    assert(toget < CF_BUFSIZE);
    assert_SSLIsBlocking(ssl);

    int received = SSL_read(ssl, buffer, toget);
    if (received < 0)
    {
        int errcode = TLSLogError(ssl, LOG_LEVEL_ERR, "SSL_read", received);

        /* SSL_read() might get an internal recv() timeout, since we've set
         * SO_RCVTIMEO. In that case, the internal socket returns EAGAIN or
         * EWOULDBLOCK and SSL_read() returns SSL_ERROR_WANT_READ. */
        if (errcode == SSL_ERROR_WANT_READ)               /* recv() timeout */
        {
            /* Make sure that the peer will send us no more data. */
            SSL_shutdown(ssl);
            shutdown(SSL_get_fd(ssl), SHUT_RDWR);

            /* Empty possible SSL_read() buffers. */

            int ret = 1;
            int bytes_still_buffered = SSL_pending(ssl);
            while (bytes_still_buffered > 0 && ret > 0)
            {
                char tmpbuf[bytes_still_buffered];
                ret = SSL_read(ssl, tmpbuf, bytes_still_buffered);
                bytes_still_buffered -= ret;
            }
        }

        return -1;
    }
    else if (received == 0)
    {
        if ((SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN) != 0)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Remote peer terminated TLS session (SSL_read)");
        }
        else
        {
            TLSLogError(ssl, LOG_LEVEL_ERR,
                        "Connection unexpectedly closed (SSL_read)",
                        received);
        }
    }

    assert(received < CF_BUFSIZE);
    buffer[received] = '\0';

    return received;
}

/**
 * @brief Repeat receiving until last byte received is '\n'.
 *
 * @param #buf on return will contain all received lines, and '\0' will be
 *             appended to it.
 * @return Return value is #buf 's length (excluding manually appended '\0')
 *         or -1 in case of error.
 *
 * @note This function is intended for line-oriented communication, this means
 *       the peer sends us one line (or a bunch of lines) and waits for reply,
 *       so that '\n' is the last character in the underlying SSL_read().
 */
int TLSRecvLines(SSL *ssl, char *buf, size_t buf_size)
{
    int ret;
    size_t got = 0;
    buf_size -= 1;               /* Reserve one space for terminating '\0' */

    /* Repeat until we receive end of line. */
    do
    {
        buf[got] = '\0';
        ret = TLSRecv(ssl, &buf[got], buf_size - got);
        if (ret <= 0)
        {
            Log(LOG_LEVEL_ERR,
                "Connection was hung up while receiving line: %s",
                buf);
            return -1;
        }
        got += ret;
    } while ((buf[got-1] != '\n') && (got < buf_size));
    assert(got <= buf_size);

    /* Append '\0', there is room because buf_size has been decremented. */
    buf[got] = '\0';

    if ((got == buf_size) && (buf[got-1] != '\n'))
    {
        Log(LOG_LEVEL_ERR,
            "Received line too long, hanging up! Length %zu, line: %s",
            got, buf);
        return -1;
    }

    LogRaw(LOG_LEVEL_DEBUG, "TLSRecvLines(): ", buf, got);

    return (got <= INT_MAX) ? (int) got : -1;
}

/**
 * Set safe OpenSSL defaults commonly used by both clients and servers.
 *
 * @param min_version the minimum acceptable TLS version for incoming or
 *        outgoing connections (depending on ssl_ctx), for example
 *        "1", "1.1", "1.2".
 */
void TLSSetDefaultOptions(SSL_CTX *ssl_ctx, const char *min_version)
{
#if HAVE_DECL_SSL_CTX_CLEAR_OPTIONS
    /* Clear all flags, we do not want compatibility tradeoffs like
     * SSL_OP_LEGACY_SERVER_CONNECT. */
    SSL_CTX_clear_options(ssl_ctx, SSL_CTX_get_options(ssl_ctx));
#else
    /* According to OpenSSL code v.0.9.8m, the first option to be added
     * by default (SSL_OP_LEGACY_SERVER_CONNECT) was added at the same
     * time SSL_CTX_clear_options appeared. Therefore, it is OK not to
     * clear options if they are not set.
     * If this assertion is proven to be false, output a clear warning
     * to let the user know what happens. */
    if (SSL_CTX_get_options(ssl_ctx) != 0)
    {
      Log(LOG_LEVEL_WARNING,
          "This version of CFEngine was compiled against OpenSSL < 0.9.8m, "
          "using it with a later OpenSSL version is insecure. "
          "The current version uses compatibility workarounds that may allow "
          "CVE-2009-3555 exploitation.");
      Log(LOG_LEVEL_WARNING, "Please update your CFEngine package or "
          "compile it against your current OpenSSL version.");
    }
#endif

    /* In any case use only TLS v1 or later. */
    long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;

    enum tls_version min_tls_version = TLS_LOWEST_REQUIRED;
    if (!NULL_OR_EMPTY(min_version))
    {
        bool found = false;
        for (enum tls_version v = TLS_1_0; !found && v <= TLS_LAST; v++)
        {
            if (StringSafeEqual(min_version, tls_version_strings[v]))
            {
                found = true;
                if (v < TLS_LOWEST_REQUIRED)
                {
                    Log(LOG_LEVEL_WARNING, "Minimum requested TLS version is %s,"
                        " but minimum version required by CFEngine is %s."
                        " Using the minimum required version.",
                        min_version, tls_version_strings[TLS_LOWEST_REQUIRED]);
                    min_tls_version = TLS_LOWEST_REQUIRED;
                }
                else if (v > TLS_HIGHEST_SUPPORTED)
                {
                    Log(LOG_LEVEL_WARNING, "Minimum requested TLS version is %s,"
                        " but maximum version supported by OpenSSL is %s."
                        " Using the maximum supported version.",
                        min_version, tls_version_strings[TLS_HIGHEST_SUPPORTED]);
                    min_tls_version = TLS_HIGHEST_SUPPORTED;
                }
                else
                {
                    min_tls_version = v;
                }
            }
        }
        if (!found)
        {
            Log(LOG_LEVEL_WARNING,
                "Unrecognized requested minimum TLS version '%s',"
                " using the minimum required version %s.",
                min_version, tls_version_strings[TLS_LOWEST_REQUIRED]);
            min_tls_version = TLS_LOWEST_REQUIRED;
        }
    }

    Log(LOG_LEVEL_VERBOSE,
        "Setting minimum acceptable TLS version: %s", tls_version_strings[min_tls_version]);

    /* disable all the lower versions than the minimum requested/determined */
    for (enum tls_version v = TLS_1_0; v < min_tls_version; v++)
    {
        options |= tls_disable_flags[v];
    }

    /* No session resumption or renegotiation for now. */
    options |= SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;

#ifdef SSL_OP_NO_TICKET
    /* Disable another way of resumption, session tickets (RFC 5077). */
    options |= SSL_OP_NO_TICKET;
#endif

    SSL_CTX_set_options(ssl_ctx, options);


    /* Disable both server-side and client-side session caching, to
       complement the previous options. Safe for now, might enable for
       performance in the future. */
    SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_OFF);


    /* Never bother with retransmissions, SSL_write() should
     * always either write the whole amount or fail. */
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);

    /* Set options to always request a certificate from the peer,
       either we are client or server. */
    SSL_CTX_set_verify(ssl_ctx,
                       SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       NULL);
    /* Always accept that certificate, we do proper checking after TLS
     * connection is established since OpenSSL can't pass a connection
     * specific pointer to the callback (so we would have to lock).  */
    SSL_CTX_set_cert_verify_callback(ssl_ctx, TLSVerifyCallback, NULL);
}

bool TLSSetCipherList(SSL_CTX *ssl_ctx, const char *cipher_list)
{
    assert(ssl_ctx);

    if (cipher_list == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Using the OpenSSL's default cipher list");
        /* nothing more to do */
        return true;
    }

    Log(LOG_LEVEL_VERBOSE, "Setting cipher list for TLS connections to: %s",
        cipher_list);

    const size_t max_len = strlen(cipher_list) + 1; /* NUL byte */
    size_t n_specs = StringCountTokens(cipher_list, max_len, ":");

    /* TLS 1.3 defines cipher suites, they start with "TLS_" */
    char ciphers[max_len];
    size_t ciphers_len = 0;

    char cipher_suites[max_len];
    size_t cipher_suites_len = 0;

    for (size_t i = 0; i < n_specs; i++)
    {
        StringRef spec_ref = StringGetToken(cipher_list, max_len, i, ":");
        if (StringStartsWith(spec_ref.data, "TLS_"))
        {
            StrCat(cipher_suites, max_len, &cipher_suites_len, spec_ref.data, spec_ref.len + 1);
        }
        else
        {
            StrCat(ciphers, max_len, &ciphers_len, spec_ref.data, spec_ref.len + 1);
        }
    }

    if (ciphers_len != 0)       /* TLS <= 1.2 ciphers */
    {
        int ret = SSL_CTX_set_cipher_list(ssl_ctx, ciphers);
        if (ret != 1)
        {
            Log(LOG_LEVEL_ERR, "No valid ciphers in the cipher list: %s", cipher_list);
            return false;
        }
    }

#ifdef HAVE_TLS_1_3
    if (cipher_suites_len != 0) /* TLS >= 1.3 ciphers */
    {
        int ret = SSL_CTX_set_ciphersuites(ssl_ctx, cipher_suites);
        if (ret != 1)
        {
            Log(LOG_LEVEL_ERR, "No valid cipher suites in the list: %s", cipher_list);
            return false;
        }
    }
    else
    {
        /* Allowed ciphers specified, but no TLS 1.3 ciphersuites among them.
           Let's disable TLS 1.3 because otherwise OpenSSL uses the default
           ciphersuites for TLS 1.3 and thus effectively extends the specified
           list of allowed ciphers behind our back. */
        Log(LOG_LEVEL_WARNING,
            "Disabling TLS 1.3 because no TLS 1.3 ciphersuites specified in allowed ciphers: '%s'",
            cipher_list);
        SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_TLSv1_3);
    }
#else
    if (cipher_suites_len != 0)
    {
        Log(LOG_LEVEL_WARNING,
            "Ignoring requested TLS 1.3 ciphersuites '%s', TLS 1.3 not supported",
            cipher_suites);
    }
#endif

    return true;
}
