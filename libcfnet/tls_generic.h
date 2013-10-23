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


#ifndef CFENGINE_TLS_GENERIC_H
#define CFENGINE_TLS_GENERIC_H


#include <cfnet.h>

#include <logging.h>                                            /* LogLevel */


int TLSVerifyCallback(X509_STORE_CTX *ctx, void *arg);
/**
 * @return 1 if the certificate received during the TLS handshake is valid
 *         signed and its public key is the same with the stored one for that
 *         host.
 * @return 0 if stored key for the host is missing or differs from the one
 *         received.
 * @return -1 in case of other error (error will be Log()ed).
 * @note When return value is != -1 (so no error occured) the #conn_info struct
 *       should have been populated, with key received and its hash.
 */
int TLSVerifyPeer(ConnectionInfo *conn_info, const char *remoteip, const char *username);
/**
 * @brief OpenSSL is missing an SSL_reason_error_string() like
 *        ERR_reason_error_string().  Provide missing functionality here,
 *        since it's kind of complicated.
 * @param #prepend String to prepend to the SSL error.
 * @param #code Return code from the OpenSSL function call.
 * @warning Use only for SSL_connect(), SSL_accept(), SSL_do_handshake(),
 *          SSL_read(), SSL_peek(), SSL_write(), see SSL_get_error man page.
 */
void TLSLogError(SSL *ssl, LogLevel level, const char *prepend, int code);
/**
 * @brief Sends the data stored on the buffer using a TLS session.
 * @param ssl SSL information.
 * @param buffer Data to send.
 * @param length Length of the data to send.
 * @return The length of the data sent (which could be smaller than the
 *         requested length) or -1 in case of error.
 * @note Use only for *blocking* sockets. Set
 *       SSL_CTX_set_mode(SSL_MODE_AUTO_RETRY) to make sure that either
 *       operation completed or an error occured.
 */
int TLSSend(SSL *ssl, const char *buffer, int length);
/**
 * @brief Receives data from the SSL session and stores it on the buffer.
 * @param ssl SSL information.
 * @param buffer Buffer to store the received data.
 * @param length Length of the data to receive.
 * @return The length of the received data, which could be smaller or equal
 *         than the requested or -1 in case of error or 0 if connection was
 *         closed.
 * @note Use only for *blocking* sockets. Set
 *       SSL_CTX_set_mode(SSL_MODE_AUTO_RETRY) to make sure that either
 *       operation completed or an error occured.
 */
int TLSRecv(SSL *ssl, char *buffer, int length);
/**
 * @brief Receives character until a new line is found.
 * @return Line is '\0'-terminated and put in #line. Return value is line
 *         length (including '\0') or -1 in case of error.
 *
 * @note This function is intended for line-oriented communication, this means
 *       the peer sends us one line and waits for reply, so that '\n' is the
 *       last character in the underlying SSL_read().
 */
int TLSRecvLine(SSL *ssl, char *buf, size_t buf_size);

#endif
