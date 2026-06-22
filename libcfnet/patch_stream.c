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

#include <patch_stream.h>

#include <stream_protocol.h>
#include <logging.h>
#include <alloc.h>

bool PatchStreamRefuse(SSL *conn)
{
    assert(conn != NULL);

    /* The client sends nothing after the request line, so there is no need
     * to flush the stream before sending the error message. */
    return ProtocolSendError(conn, false, ERROR_MSG_UNSPECIFIED_SERVER_REFUSAL);
}

bool PatchStreamServe(SSL *conn, const void *data, size_t len)
{
    assert(conn != NULL);
    assert(data != NULL || len == 0);

    const char *buf = data;
    size_t offset = 0;
    bool eof = false;

    while (!eof)
    {
        size_t chunk = len - offset;
        if (chunk > PROTOCOL_MESSAGE_SIZE)
        {
            chunk = PROTOCOL_MESSAGE_SIZE;
        }
        eof = (offset + chunk == len);

        if (!ProtocolSendMessage(conn, buf + offset, chunk, eof))
        {
            /* Error is already logged in ProtocolSendMessage() */
            return false;
        }
        offset += chunk;
    }

    return true;
}

bool PatchStreamFetch(SSL *conn, char **data, size_t *len)
{
    assert(conn != NULL);
    assert(data != NULL);
    assert(len != NULL);

    char *buf = NULL;
    size_t buf_len = 0;

    char msg[PROTOCOL_MESSAGE_SIZE];
    size_t msg_len;
    bool eof = false;

    while (!eof)
    {
        if (!ProtocolRecvMessage(conn, msg, &msg_len, &eof))
        {
            /* Error is already logged in ProtocolRecvMessage() */
            free(buf);
            return false;
        }

        /* Message length can be 0 if a PDU contains only a header. Usually
         * happens when sending End-of-File flag. */
        if (msg_len > 0)
        {
            if (msg_len > PATCH_STREAM_MAX_SIZE - buf_len)
            {
                Log(LOG_LEVEL_ERR,
                    "Refusing to fetch patch: size exceeds maximum of %d bytes",
                    PATCH_STREAM_MAX_SIZE);
                free(buf);
                return false;
            }

            buf = xrealloc(buf, buf_len + msg_len);
            memcpy(buf + buf_len, msg, msg_len);
            buf_len += msg_len;
        }
    }

    *data = buf;
    *len = buf_len;
    return true;
}
