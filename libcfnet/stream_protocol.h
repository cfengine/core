/*
  Copyright 2026 Northern.tech AS

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

#ifndef STREAM_PROTOCOL_H
#define STREAM_PROTOCOL_H

#include <tls_generic.h>
#include <definitions.h>
#include <compiler.h>
#include <stdbool.h>

/*********************************************************/
/* Network protocol                                      */
/*********************************************************/

/**
 * @brief Simple network protocol on top of SSL/TCP. Used for client-server
 * communication during streaming (e.g., file stream and patch stream).
 *
 * @details Header format:
 *   +----------+----------+----------+----------+
 *   | SDU Len. | Reserved | EOF Flag | ERR Flag |
 *   +----------+----------+----------+----------+
 *   | 12 bits  | 2 bits   | 1 bit    | 1 bit    |
 *   +----------+----------+----------+----------+
 *
 * The header consists of 16 bits and the fields are defined as follows:
 * SDU Length           Length of the SDU (i.e. payload) encapsulated within
 *                      this datagram.
 * Reserved             2 bits reserved for future use.
 * End-of-File flag     Signals whether or not the receiver should expect to
 *                      receive more datagrams.
 * Error flag           Signals that the transmission must be canceled due to
 *                      unexpected error.
 *
 * @note If the End-of-File flag is set, there may still be data to process in
 *       in the payload. If the Error flag is set, there may be an error
 *       message in the payload.
 */
#define PROTOCOL_HEADER_SIZE 2

/**
 * @note The TLS Generic API requires that the message length is less than
 *       CF_BUFSIZE. Furthermore, the protocol can only handle up to 4095
 *       Bytes, because it's the largest unsigned integer you can represent
 *       with 12 bits (2^12 - 1 = 4095).
 */
#define PROTOCOL_MESSAGE_SIZE MIN(CF_BUFSIZE - 1, 4095)

/* Common error messages */
#define ERROR_MSG_UNSPECIFIED_SERVER_REFUSAL "Unspecified server refusal"
#define ERROR_MSG_INTERNAL_SERVER_ERROR "Internal server error"
#define ERROR_MSG_INTERNAL_CLIENT_ERROR "Internal client error"

/**
 * @brief Send a message using the stream protocol
 *
 * @param conn The SSL connection object
 * @param msg The message to send
 * @param len The length of the message to send (must be less or equal to
 *            PROTOCOL_MESSAGE_SIZE Bytes)
 * @param eof Set to true if this is the last message in a transaction,
 *            otherwise false
 * @return true on success, otherwise false
 */
bool ProtocolSendMessage(SSL *conn, const char *msg, size_t len, bool eof);

/**
 * @brief Receive a message using the stream protocol
 *
 * @param conn The SSL connection object
 * @param msg The message receive buffer (must be PROTOCOL_MESSAGE_SIZE bytes
 *            large)
 * @param len The length of the received message
 * @param eof Is set to true if this was the last message in the transaction
 * @return true on success, otherwise false
 *
 * @note ProtocolRecvMessage fails if the communication is broken or if we
 *       received an error from the remote host. In both cases, we should not
 *       try to flush the stream.
 */
bool ProtocolRecvMessage(SSL *conn, char *msg, size_t *len, bool *eof);

/**
 * @brief Flush the stream
 *
 * It's used to prevent the remote host from blocking while sending the
 * remaining data after we have experienced an unexpected error and need to
 * abort the stream. Once the stream has been successfully flushed, the
 * remote host will be ready to receive our error message.
 *
 * @param conn The SSL connection object
 * @return true on success, otherwise false
 */
bool ProtocolFlushStream(SSL *conn);

/**
 * @brief Send an error message using the stream protocol
 *
 * @param conn The SSL connection object
 * @param flush Whether or not to flush the stream (see ProtocolFlushStream())
 * @param fmt The format string
 * @param ... The format string arguments
 * @return true on success, otherwise false
 */
bool ProtocolSendError(SSL *conn, bool flush, const char *fmt, ...)
    FUNC_ATTR_PRINTF(3, 4);

#endif // STREAM_PROTOCOL_H
