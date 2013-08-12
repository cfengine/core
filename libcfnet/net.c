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

#include "net.h"
#include "classic.h"
#include "tls_generic.h"

#include "logging.h"
#include "misc_lib.h"

/*************************************************************************/

/**
 * @param len is the number of bytes to send, or 0 if buffer is a
 *        '\0'-terminated string so strlen(buffer) can used.
 */
int SendTransaction(const ConnectionInfo *conn_info, const char *buffer, int len, char status)
{
    char work[CF_BUFSIZE] = { 0 };
    int ret;

    if (len == 0)
    {
        len = strlen(buffer);
    }

    if (len > CF_BUFSIZE - CF_INBAND_OFFSET)
    {
        Log(LOG_LEVEL_ERR, "SendTransaction: len (%d) > %d - %d",
            len, CF_BUFSIZE, CF_INBAND_OFFSET);
        return -1;
    }

    snprintf(work, CF_INBAND_OFFSET, "%c %d", status, len);

    memcpy(work + CF_INBAND_OFFSET, buffer, len);

    Log(LOG_LEVEL_DEBUG, "SendTransaction header:'%s'", work);
    LogRaw(LOG_LEVEL_DEBUG, "SendTransaction data: ",
           work + CF_INBAND_OFFSET, len);

    switch(conn_info->type)
    {
    case CF_PROTOCOL_CLASSIC:
        ret = SendSocketStream(conn_info->sd, work,
                               len + CF_INBAND_OFFSET);
        break;
    case CF_PROTOCOL_TLS:
        ret = TLSSend(conn_info->ssl, work, len + CF_INBAND_OFFSET);
        break;
    default:
        UnexpectedError("SendTransaction: ProtocolVersion %d!",
                        conn_info->type);
        ret = -1;
    }

    if (ret == -1)
        return -1;
    else
        return 0;
}

/*************************************************************************/

int ReceiveTransaction(const ConnectionInfo *conn_info, char *buffer, int *more)
{
    char proto[CF_INBAND_OFFSET + 1] = { 0 };
    char status = 'x';
    unsigned int len = 0;
    int ret;

    /* Get control channel. */
    switch(conn_info->type)
    {
    case CF_PROTOCOL_CLASSIC:
        ret = RecvSocketStream(conn_info->sd, proto, CF_INBAND_OFFSET);
        break;
    case CF_PROTOCOL_TLS:
        ret = TLSRecv(conn_info->ssl, proto, CF_INBAND_OFFSET);
        break;
    default:
        UnexpectedError("ReceiveTransaction: ProtocolVersion %d!",
                        conn_info->type);
        ret = -1;
    }
    if (ret == -1 || ret == 0)
        return ret;

    LogRaw(LOG_LEVEL_DEBUG, "ReceiveTransaction header: ",
           proto, CF_INBAND_OFFSET);

    ret = sscanf(proto, "%c %u", &status, &len);
    if (ret != 2)
    {
        Log(LOG_LEVEL_ERR,
            "ReceiveTransaction: Bad packet -- bogus header '%s'", proto);
        return -1;
    }

    if (len > CF_BUFSIZE - CF_INBAND_OFFSET)
    {
        Log(LOG_LEVEL_ERR,
            "ReceiveTransaction: Bad packet -- too long (len=%d)", len);
        return -1;
    }

    if (more != NULL)
    {
        if (status == 'm')
            *more = true;
        else
            *more = false;
    }

    /* Get data. */
    switch(conn_info->type)
    {
    case CF_PROTOCOL_CLASSIC:
        ret = RecvSocketStream(conn_info->sd, buffer, len);
        break;
    case CF_PROTOCOL_TLS:
        ret = TLSRecv(conn_info->ssl, buffer, len);
        break;
    default:
        UnexpectedError("ReceiveTransaction: ProtocolVersion %d!",
                        conn_info->type);
        ret = -1;
    }

    LogRaw(LOG_LEVEL_DEBUG, "ReceiveTransaction data: ", buffer, ret);

    return ret;
}

/*************************************************************************/

/*
  NB: recv() timeout interpretation differs under Windows: setting tv_sec to
  50 (and tv_usec to 0) results in a timeout of 0.5 seconds on Windows, but
  50 seconds on Linux.
*/

#ifdef __linux__

int SetReceiveTimeout(int fd, const struct timeval *tv)
{
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)tv, sizeof(struct timeval)))
    {
        return -1;
    }

    return 0;
}

#else

int SetReceiveTimeout(ARG_UNUSED int fd, ARG_UNUSED const struct timeval *tv)
{
    return 0;
}

#endif
