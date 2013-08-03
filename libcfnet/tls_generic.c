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

/* TODO move crypto.h to libutils */
#include "crypto.h"                                    /* HavePublicKeyByIP */
#include "files_hashes.h"                              /* HashPubKey */

/**
 * this is an always succeeding callback for SSL_CTX_set_cert_verify_callback().
 *
 * Verifying with a callback is the best way, *but* OpenSSL does not provide a
 * thread-safe way to passing a pointer with custom data (connection info). So
 * we always succeed verification here, and verify properly *after* the
 * handshake is complete.
 */
int TLSVerifyCallback(X509_STORE_CTX *ctx ARG_UNUSED,
                      void *arg ARG_UNUSED)
{
    return 1;
}

/**
 * @return 1 if the certificate received during the TLS handshake is verified
 *         to be the same with the stored one for that host.
 * @return 0 if the certificates differ.
 * @return -1 in case of other error (error will be Log()ed).
 * @note When return value is != -1 (so no error occured) the #conn_info struct
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
            ERR_reason_error_string(ERR_get_error()));
        retval = -1;
        goto ret1;
    }
    EVP_PKEY *received_pubkey = X509_get_pubkey(received_cert);
    if (received_pubkey == NULL)
    {
        Log(LOG_LEVEL_ERR, "X509_get_pubkey: %s",
            ERR_reason_error_string(ERR_get_error()));
        retval = -1;
        goto ret2;
    }
    if (EVP_PKEY_type(received_pubkey->type) != EVP_PKEY_RSA)
    {
        Log(LOG_LEVEL_ERR,
            "Received key of unknown type, only RSA currently supported!");
        retval = -1;
        goto ret2;
    }

    conn_info->remote_key = EVP_PKEY_get1_RSA(received_pubkey);

    /* Store the hash, we need it for various stuff during connection. */
    HashPubKey(conn_info->remote_key, conn_info->remote_keyhash,
               CF_DEFAULT_DIGEST);
    HashPrintSafe(CF_DEFAULT_DIGEST, conn_info->remote_keyhash,
                  conn_info->remote_keyhash_str);

    /*
     * Compare the key received with the one stored.
     */
    RSA *expected_rsa_key = HavePublicKey(username, remoteip,
                                          conn_info->remote_keyhash_str);
    if (expected_rsa_key == NULL)
    {
        /* Log(LOG_LEVEL_ERR, "HavePublicKeyByIP err"); */
        retval = 0;                                        /* KEY NOT FOUND */
        goto ret3;
    }

    /* Avoid dynamic allocation... */
    EVP_PKEY expected_pubkey = { 0 };
    /* EVP_PKEY *expected_pubkey = EVP_PKEY_new(); */
    /* if (expected_pubkey == NULL) */
    /* { */
    /*     Log(LOG_LEVEL_ERR, "EVP_PKEY_new: %s", */
    /*         ERR_reason_error_string(ERR_get_error())); */
    /*     retval = 0; */
    /*     goto ret4; */
    /* } */

    ret = EVP_PKEY_assign_RSA(&expected_pubkey, expected_rsa_key);
    ret = EVP_PKEY_cmp(received_pubkey, &expected_pubkey);
    if (ret == 1)
    {
        retval = 1;                                          /* TRUSTED KEY */
        goto ret5;
    }
    else if (ret == 0 || ret == -1)
    {
        retval = 0;                                        /* UNTRUSTED KEY */
        goto ret5;
    }
    else
    {
        const char *errmsg = ERR_reason_error_string(ERR_get_error());
        Log(LOG_LEVEL_ERR, "OpenSSL EVP_PKEY_cmp: %d %s",
            ret, errmsg ? errmsg : "");
        retval = -1;
        goto ret5;
    }

    UnexpectedError("Unreachable!");
    return 0;

  ret5:
  /*   EVP_PKEY_free(expected_pubkey); */
  /* ret4: */
    RSA_free(expected_rsa_key);
  ret3:
    EVP_PKEY_free(received_pubkey);
  ret2:
    X509_free(received_cert);
  ret1:
    return retval;
}

/**
 * @brief Sends the data stored on the buffer using a TLS session.
 * @param ssl SSL information.
 * @param buffer Data to send.
 * @param length Length of the data to send.
 * @return The length of the data sent (which could be smaller than the requested length) or -1 in case of error.
 */
int TLSSend(SSL *ssl, const char *buffer, int length)
{
    if (!ssl || !buffer || (length < 0))
    {
        return -1;
    }

    /*
     * Technically speaking, the buffer is either sent completely or not sent at all.
     * Therefore it is not needed to count how many bytes we have sent, OpenSSL does that
     * for us.
     */
    int total_tries = 0;
    int sent = 0;
    do {
        sent = SSL_write(ssl, buffer, length);
        if (sent <= 0)
        {
            int error = SSL_get_error(ssl, sent);
            Log(LOG_LEVEL_ERR, "SSL_write failed, retrying (tries: %d)", total_tries);
            if ((SSL_ERROR_WANT_READ == error) || (SSL_ERROR_WANT_WRITE == error))
            {
                /*
                 * We need to retry the operation using exactly the same arguments.
                 * We will use select(2) to wait until the underlying socket is ready.
                 */
                int fd = SSL_get_fd(ssl);
                if (fd < 0)
                {
                    Log(LOG_LEVEL_ERR, "Could not get fd from SSL");
                    return -1;
                }
                fd_set wfds;
                struct timeval tv;
                int result = 0;
                int tries = 0;

                do {
                    SET_DEFAULT_TLS_TIMEOUT(tv);
                    FD_ZERO(&wfds);
                    FD_SET(fd, &wfds);

                    result = select(fd+1, NULL, &wfds, NULL, &tv);
                    if (result > 0)
                    {
                        /*
                         * Ready to send data
                         */
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
                 * Any other error is fatal.
                 */
                Log(LOG_LEVEL_ERR, "Fatal error on SSL_write (error: %d)", error);
                return -1;
            }
        }
        else
        {
            /*
             * We sent more than 0 bytes so we are done.
             */
            Log(LOG_LEVEL_DEBUG, "Sent %d bytes using TLS", sent);
            break;
        }
        ++total_tries;
    } while (total_tries <= DEFAULT_TLS_TRIES);
    return sent;
}

/* TODO TLSRecvPersist(). */

/**
 * @brief Receives data from the SSL session and stores it on the buffer.
 * @param ssl SSL information.
 * @param buffer Buffer to store the received data.
 * @param length Length of the data to receive.
 * @return The length of the received data, which could be smaller than the requested
 *         or -1 in case of error or 0 if connection was closed.
 */
int TLSRecv(SSL *ssl, char *buffer, int length)
{
    if (!ssl || !buffer || (length < 0))
    {
        return -1;
    }

    int total_tries = 0;
    int received = 0;
    do {
        received = SSL_read(ssl, buffer, length);
        if (received == 0 &&
            SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN)
        {
            Log(LOG_LEVEL_VERBOSE, "Remote peer terminated TLS session.");
            return 0;
        }
        if (received <= 0)
        {
            int error = SSL_get_error(ssl, received);
            Log(LOG_LEVEL_ERR, "SSL_read failed, retrying (tries: %d)", total_tries);
            if ((SSL_ERROR_WANT_READ == error) || (SSL_ERROR_WANT_WRITE == error))
            {
                /*
                 * We need to retry the operation using exactly the same arguments.
                 * We will use select(2) to wait until the underlying socket is ready.
                 */
                int fd = SSL_get_fd(ssl);
                if (fd < 0)
                {
                    Log(LOG_LEVEL_ERR, "Could not get fd from SSL");
                    return -1;
                }
                fd_set rfds;
                struct timeval tv;
                int result = 0;
                int tries = 0;

                do {
                    SET_DEFAULT_TLS_TIMEOUT(tv);
                    FD_ZERO(&rfds);
                    FD_SET(fd, &rfds);

                    result = select(fd+1, &rfds, NULL, NULL, &tv);
                    if (result > 0)
                    {
                        /*
                         * Ready to receive data
                         */
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
                 * Any other error is fatal.
                 */
                Log(LOG_LEVEL_ERR, "Fatal error on SSL_read (error: %d)", error);
                return -1;
            }
        }
        else
        {
            /*
             * We received more than 0 bytes so we are done.
             */
            Log(LOG_LEVEL_DEBUG, "Received %d bytes using TLS", received);
            break;
        }
        ++total_tries;
    } while (total_tries <= DEFAULT_TLS_TRIES);
    buffer[received] = '\0';
    return received;
}

/**
 * @brief Repeat receiving until we get a full line (i.e. received buffer ends
 *        with line terminator).
 * @return Line is '\0'-terminated and put in #line. Return value is line
 *         length (including '\0') or -1 in case of error.
 */
int TLSRecvLine(SSL *ssl, char *line, size_t line_size)
{
    int ret;
    int got = 0;
    line_size -= 1;               /* Reserve one space for terminating '\0' */

    /* Repeat until we receive end of line. */
    do
    {
        line[got] = '\0';
        ret = TLSRecv(ssl, &line[got], line_size - got);
        if (ret <= 0)
        {
            Log(LOG_LEVEL_ERR,
                "Connection was hung up while receiving line: %s",
                line);
            return -1;
        }
        got += ret;
    }
    while ((line[got-1] != '\n') && (got < line_size));

    /* Append terminating '\0', there is room because line_size is -1. */
    line[got] = '\0';

    if (got >= line_size)                   /* Can only be got == line_size */
    {
        Log(LOG_LEVEL_ERR,
            "Received line too long, hanging up! Length %d, line: %s",
            got, line);
        return -1;
    }

    Log(LOG_LEVEL_DEBUG, "TLSRecvLine() %d bytes long: %s", got, line);
    return got;
}
