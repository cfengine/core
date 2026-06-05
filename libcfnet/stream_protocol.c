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

#include <platform.h>

#include <stream_protocol.h>

#include <logging.h>
#include <stdint.h>
#include <stdarg.h>

/**
 * @brief Send a message using the stream protocol
 * @warning You probably want to use ProtocolSendMessage() or
 *          ProtocolSendError() instead
 *
 * @param conn The SSL connection object
 * @param msg The message to send
 * @param len The length of the message to send (must be less or equal to
 *            PROTOCOL_MESSAGE_SIZE Bytes)
 * @param eof Set to true if this is the last message in a transaction,
 *            otherwise false
 * @param err Set to true if transaction must be canceled (e.g., due to an
 *            unexpected error), otherwise false
 * @note If the err parameter is set to true, the expected return value is
 *       still true.
 * @return true on success, otherwise false
 */
static bool __ProtocolSendMessage(
    SSL *conn, const char *msg, size_t len, bool eof, bool err)
{
    assert(conn != NULL);
    assert(msg != NULL || len == 0);
    assert(len <= PROTOCOL_MESSAGE_SIZE);

    /* Set message length */
    assert(sizeof(len) >= 3); /* It's probably guaranteed, but let's make sure
                               * to avoid potentially nasty surprises */
    uint16_t header = len << 4;

    /* Set Error flag */
    if (err)
    {
        header |= (1 << 0);
    }

    /* Set End-of-File flag */
    if (eof)
    {
        header |= (1 << 1);
    }

    /* Send header */
    header = htons(header);
    int ret = TLSSend(conn, (char *) &header, PROTOCOL_HEADER_SIZE);
    if (ret != PROTOCOL_HEADER_SIZE)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to send message header during stream: "
            "Expected to send %d bytes, but sent %d bytes",
            PROTOCOL_HEADER_SIZE,
            ret);
        return false;
    }

    if (len > 0)
    {
        /* Send payload */
        ret = TLSSend(conn, msg, len);
        if (ret != (int) len)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to send message payload during stream: "
                "Expected to send %zu bytes, but sent %d bytes",
                len,
                ret);
            return false;
        }
    }

    return true;
}

bool ProtocolSendMessage(SSL *conn, const char *msg, size_t len, bool eof)
{
    assert(conn != NULL);
    assert(msg != NULL || len == 0);

    return __ProtocolSendMessage(conn, msg, len, eof, false);
}

bool ProtocolRecvMessage(SSL *conn, char *msg, size_t *len, bool *eof)
{
    assert(conn != NULL);
    assert(msg != NULL);
    assert(len != NULL);
    assert(eof != NULL);

    /* TLSRecv() expects a buffer this size */
    char recv_buffer[CF_BUFSIZE];

    /* Receive header */
    int ret = TLSRecv(conn, recv_buffer, PROTOCOL_HEADER_SIZE);
    if (ret != PROTOCOL_HEADER_SIZE)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to receive message header during stream: "
            "Expected to receive %d bytes, but received %d bytes",
            PROTOCOL_HEADER_SIZE,
            ret);
        return false;
    }

    /* Why not receive the bytes directly into header in the TLSRecv()?
     * Because it actually writes a NUL-Byte after the requested bytes which
     * would cause memory violations. */
    uint16_t header;
    memcpy(&header, recv_buffer, PROTOCOL_HEADER_SIZE);
    header = ntohs(header);

    /* Extract Error flag */
    bool err = header & (1 << 0);

    /* Extract End-of-File flag */
    *eof = header & (1 << 1);

    /* Extract message length */
    assert(sizeof(*len) >= 2); /* It's probably guaranteed, but let's make
                                * sure to avoid potentially nasty surprises */
    *len = header >> 4;

    /* Read payload */
    if (*len > 0)
    {
        /* The TLSRecv() function's doc string says that the returned value
         * may be less than the requested length if the other side completed a
         * send with less bytes. I take it that this means that there is no
         * short reads/recvs. Furthermore, TLSSend() says that its return
         * value is always equal to the requested length as long as TLS is
         * setup correctly. I take it that the same is true for TLSRecv().
         * Hence, we will interpret a shorter read than what we expect as an
         * error. */
        ret = TLSRecv(conn, recv_buffer, *len);
        if (ret != *len)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to receive message payload during stream: "
                "Expected to receive %zu bytes, but received %d bytes",
                *len,
                ret);
            return false;
        }
        memcpy(msg, recv_buffer, *len);

        if (err)
        {
            /* If the error flag is set, then the payload contains an error
             * message of 'len' bytes. */
            assert(*len < sizeof(recv_buffer));
            recv_buffer[*len] = '\0'; /* Set terminating null-byte */
            Log(LOG_LEVEL_ERR, "Remote stream error: %s", recv_buffer);
        }
    }

    return !err;
}

bool ProtocolFlushStream(SSL *conn)
{
    assert(conn != NULL);

    char msg[PROTOCOL_MESSAGE_SIZE];
    size_t len;
    bool eof;
    while (ProtocolRecvMessage(conn, msg, &len, &eof))
    {
        if (eof)
        {
            return true;
        }
    }

    /* Error is already logged in ProtocolRecvMessage() */
    return false;
}

bool ProtocolSendError(SSL *conn, bool flush, const char *fmt, ...)
{
    assert(conn != NULL);
    assert(fmt != NULL);

    va_list ap;
    char msg[PROTOCOL_MESSAGE_SIZE];

    va_start(ap, fmt);
    int len = vsnprintf(msg, PROTOCOL_MESSAGE_SIZE, fmt, ap);
    va_end(ap);

    assert(len >= 0); /* Let's make sure we detect this in debug builds */
    if (len < 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to format error message during stream");
        len = 0; /* We still want to send the header */
    }
    else if (len >= PROTOCOL_MESSAGE_SIZE)
    {
        Log(LOG_LEVEL_WARNING,
            "Error message truncated during stream: "
            "Message is %d bytes, but maximum message size is %d bytes",
            len,
            PROTOCOL_MESSAGE_SIZE);
        /* Add dots to indicate message truncation. We don't need the
         * terminating NULL-byte in the buffer. Furthermore, TLSRecv() will
         * append one, upon receiving the message */
        msg[PROTOCOL_MESSAGE_SIZE - 1] = '.';
        msg[PROTOCOL_MESSAGE_SIZE - 2] = '.';
        msg[PROTOCOL_MESSAGE_SIZE - 3] = '.';
        len = PROTOCOL_MESSAGE_SIZE;
    }

    if (flush)
    {
        ProtocolFlushStream(conn);
    }

    return __ProtocolSendMessage(conn, msg, (size_t) len, false, true);
}
