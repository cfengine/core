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

#include "tls.h"
#include "cf3.defs.h"
#include "alloc.h"
#include "logging.h"
#include "misc_lib.h"

#define RSA_SERVER_CERT "/var/cfengine/server.crt"
#define RSA_SERVER_KEY "/var/cfengine/server.key"

int SendTransaction(ConnectionInfo *connection, char *buffer, int len, char status);
int ServerStartTLS(ConnectionInfo *connection)
{
    int result = 0;
    char buffer[CF_BUFSIZE];
    snprintf(buffer, CF_BUFSIZE, "ACK");

    /*
     * We prepare everything before sending the ACK
     */

    /*
     * Before using OpenSSL calls we need to make sure that everything is in place.
     */
    SSL_library_init();
    SSL_load_error_strings();
    /*
     * Now we can use OpenSSL calls.
     */
    int sd = connection->physical.sd;
    TLSInfo *tlsInfo = (TLSInfo *)xmalloc(sizeof(TLSInfo));
    tlsInfo->method = TLSv1_server_method();
    tlsInfo->context = SSL_CTX_new(tlsInfo->method);

    if (!tlsInfo->context)
    {
        ERR_print_errors_fp(stderr);
        Log(LOG_LEVEL_CRIT, "Unable to create the SSL context");
        free (tlsInfo);
        return -1;
    }
    else
    {
        Log(LOG_LEVEL_CRIT, "SSL Context created");
    }
    /*
     * Initialize our certificate
     */
    if (SSL_CTX_use_certificate_file(tlsInfo->context, RSA_SERVER_CERT, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(tlsInfo->context);
        free (tlsInfo);
        return -1;
    }
    else
    {
        Log(LOG_LEVEL_CRIT, "SSL Certificate loaded");
    }
    /*
     * Initialize the key attached to the certificate
     */
    if (SSL_CTX_use_PrivateKey_file(tlsInfo->context, RSA_SERVER_KEY, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(tlsInfo->context);
        free (tlsInfo);
        return -1;
    }
    else
    {
        Log(LOG_LEVEL_CRIT, "SSL key loaded");
    }
    /*
     * Check that the key and certificate matches
     */
    if (!SSL_CTX_check_private_key(tlsInfo->context))
    {
        Log(LOG_LEVEL_CRIT, "Certificate and private key do not match");
        return -1;
    }
    else
    {
        Log(LOG_LEVEL_CRIT, "SSL key and certificate match");
    }
    /*
     * Continue the TLS negotiation.
     */
    tlsInfo->ssl = SSL_new(tlsInfo->context);

    if (!tlsInfo->ssl)
    {
        ERR_print_errors_fp(stderr);
        Log(LOG_LEVEL_CRIT, "Unable to create the SSL object");
        SSL_CTX_free (tlsInfo->context);
        free (tlsInfo);
        return -1;
    }
    else
    {
        Log(LOG_LEVEL_CRIT, "SSL object created");
    }

    /*
     * Now we are ready to tell the client to try the TLS initialization.
     */
    result = SendTransaction(connection, buffer, 0, CF_DONE);
    if (result == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to send transaction, aborting connection. (send: %s)", GetErrorStr());
        /*
         * It is not as easy as closing the socket. We need to come up with a proper
         * way to bring down the connection.
         */
        return -1;
    }
    else
    {
        Log(LOG_LEVEL_CRIT, "Sent reply to client");
    }

    SSL_set_fd(tlsInfo->ssl, sd);

    /*
     * Now we wait for the client to send us the TLS request.
     */
    int total_tries = 0;
    do {
        result = SSL_accept(tlsInfo->ssl);
        if (result <= 0)
        {
            /*
             * Identify the problem and if possible try to fix it.
             */
            int error = SSL_get_error(tlsInfo->ssl, result);
            if ((SSL_ERROR_WANT_WRITE == error) || (SSL_ERROR_WANT_READ == error))
            {
                Log(LOG_LEVEL_DEBUG, "Recoverable error in TLS handshake, trying to fix it");
                /*
                 * We can try to fix this.
                 * This error means that there was not enough data in the buffer, using select
                 * to wait until we get more data.
                 */
                fd_set rfds;
                struct timeval tv;
                int tries = 0;

                do {
                    SET_DEFAULT_TLS_TIMEOUT(tv);
                    FD_ZERO(&rfds);
                    FD_SET(connection->physical.sd, &rfds);

                    result = select(connection->physical.sd+1, &rfds, NULL, NULL, &tv);
                    if (result > 0)
                    {
                        /*
                         * Ready to receive data
                         */
                        break;
                    }
                    else
                    {
                        Log(LOG_LEVEL_DEBUG, "select(2) timed out, retrying (tries: %d)", tries);
                        ++tries;
                    }
                } while (tries <= DEFAULT_TLS_TRIES);
            }
            else
            {
                /*
                 * Unrecoverable error
                 */
                Log(LOG_LEVEL_DEBUG, "Unrecoverable error in TLS handshake (error: %d)", error);
                SSL_free (tlsInfo->ssl);
                SSL_CTX_free (tlsInfo->context);
                free (tlsInfo);
                return -1;
            }
        }
        else
        {
            /*
             * TLS channel established, start talking!
             */
            Log (LOG_LEVEL_INFO, "TLS connection established");
            connection->type = CFEngine_TLS;
            connection->physical.tls = tlsInfo;
            break;
        }
        ++total_tries;
    } while (total_tries <= DEFAULT_TLS_TRIES);
    return 0;
}

int SendTLS(SSL *ssl, const char *buffer, int length)
{
    if (!ssl || !buffer || (length < 0))
    {
        return -1;
    }
    Log(LOG_LEVEL_CRIT, "Trying to send %d bytes using TLS", length);
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
            Log(LOG_LEVEL_ERR, "Sent %d bytes using TLS", sent);
            break;
        }
        ++total_tries;
    } while (total_tries <= DEFAULT_TLS_TRIES);
    return sent;
}

int ReceiveTLS(SSL *ssl, char *buffer, int length)
{
    if (!ssl || !buffer || (length < 0))
    {
        return -1;
    }
    Log(LOG_LEVEL_CRIT, "Trying to read %d bytes using TLS", length);
    int total_tries = 0;
    int received = 0;
    do {
        received = SSL_read(ssl, buffer, length);
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
            Log(LOG_LEVEL_ERR, "Received %d bytes using TLS", received);
            break;
        }
        ++total_tries;
    } while (total_tries <= DEFAULT_TLS_TRIES);
    buffer[received] = '\0';
    return received;
}
