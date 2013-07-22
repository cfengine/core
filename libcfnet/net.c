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
#include "tls.h"

#include "logging.h"
#include "misc_lib.h"

/*************************************************************************/

int SendTransaction(ConnectionInfo *connection, char *buffer, int len, char status)
{
    char work[CF_BUFSIZE];
    int wlen;

    memset(work, 0, sizeof(work));

    if (len == 0)
    {
        wlen = strlen(buffer);
    }
    else
    {
        wlen = len;
    }

    if (wlen > CF_BUFSIZE - CF_INBAND_OFFSET)
    {
        Log(LOG_LEVEL_ERR, "SendTransaction: wlen (%d) > %d - %d", wlen, CF_BUFSIZE, CF_INBAND_OFFSET);
        ProgrammingError("SendTransaction software failure");
    }

    snprintf(work, CF_INBAND_OFFSET, "%c %d", status, wlen);

    memcpy(work + CF_INBAND_OFFSET, buffer, wlen);

    if (CFEngine_Classic == connection->type)
    {
        if (SendSocketStream(connection->physical.sd, work, wlen + CF_INBAND_OFFSET, 0) == -1)
        {
            return -1;
        }
    }
    else if (CFEngine_TLS == connection->type)
    {
        if (SendTLS(connection->physical.tls->ssl, work, wlen + CF_INBAND_OFFSET) == -1)
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }

    return 0;
}

/*************************************************************************/

int ReceiveTransaction(ConnectionInfo *connection, char *buffer, int *more)
{
    char proto[CF_INBAND_OFFSET + 1];
    char status = 'x';
    unsigned int len = 0;
    int result = 0;

    memset(proto, 0, CF_INBAND_OFFSET + 1);

    if (CFEngine_Classic == connection->type)
    {
        if (RecvSocketStream(connection->physical.sd, proto, CF_INBAND_OFFSET) == -1) /* Get control channel */
        {
            return -1;
        }
    }
    else if (CFEngine_TLS == connection->type)
    {
        if (ReceiveTLS(connection->physical.tls->ssl, proto, CF_INBAND_OFFSET) == -1)
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }

    sscanf(proto, "%c %u", &status, &len);

    if (len > CF_BUFSIZE - CF_INBAND_OFFSET)
    {
        Log(LOG_LEVEL_ERR, "Bad transaction packet -- too long (%c %d). proto '%s'", status, len, proto);
        return -1;
    }

    if (strncmp(proto, "CAUTH", 5) == 0)
    {
        return -1;
    }

    if (more != NULL)
    {
        switch (status)
        {
        case 'm':
            *more = true;
            break;
        default:
            *more = false;
        }
    }

    if (CFEngine_Classic == connection->type)
    {
        result = RecvSocketStream(connection->physical.sd, buffer, len);
    }
    else if (CFEngine_TLS == connection->type)
    {
        result = ReceiveTLS(connection->physical.tls->ssl, buffer, len);
    }
    else
    {
        return -1;
    }

    return result;
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
