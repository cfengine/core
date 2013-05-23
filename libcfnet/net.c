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

#include "logging.h"
#include "misc_lib.h"

/*************************************************************************/

static bool LastRecvTimedOut(void)
{
#ifndef __MINGW32__
	if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
	{
		return true;
	}
#else
	int lasterror = GetLastError();

	if (lasterror == EAGAIN || lasterror == WSAEWOULDBLOCK)
	{
		return true;
	}
#endif

	return false;
}

int SendTransaction(int sd, char *buffer, int len, char status)
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

    if (SendSocketStream(sd, work, wlen + CF_INBAND_OFFSET, 0) == -1)
    {
        return -1;
    }

    return 0;
}

/*************************************************************************/

int ReceiveTransaction(int sd, char *buffer, int *more)
{
    char proto[CF_INBAND_OFFSET + 1];
    char status = 'x';
    unsigned int len = 0;

    memset(proto, 0, CF_INBAND_OFFSET + 1);

    if (RecvSocketStream(sd, proto, CF_INBAND_OFFSET) == -1) /* Get control channel */
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

    return RecvSocketStream(sd, buffer, len);
}

/*************************************************************************/

int RecvSocketStream(int sd, char buffer[CF_BUFSIZE], int toget)
{
    int already, got;

    if (toget > CF_BUFSIZE - 1)
    {
        Log(LOG_LEVEL_ERR, "Bad software request for overfull buffer");
        return -1;
    }

    for (already = 0; already != toget; already += got)
    {
        got = recv(sd, buffer + already, toget - already, 0);

        if ((got == -1) && (errno == EINTR))
        {
            continue;
        }

        if ((got == -1) && (LastRecvTimedOut()))
        {
            Log(LOG_LEVEL_ERR, "Timeout - remote end did not respond with the expected amount of data (received=%d, expecting=%d). (recv: %s)",
                already, toget, GetErrorStr());
            return -1;
        }

        if (got == -1)
        {
            Log(LOG_LEVEL_ERR, "Couldn't receceive. (recv: %s)", GetErrorStr());
            return -1;
        }

        if (got == 0)           /* doesn't happen unless sock is closed */
        {
            break;
        }
    }

    buffer[already] = '\0';
    return already;
}

/*************************************************************************/

int SendSocketStream(int sd, char buffer[CF_BUFSIZE], int tosend, int flags)
{
    int sent, already = 0;

    do
    {
        sent = send(sd, buffer + already, tosend - already, flags);

        if ((sent == -1) && (errno == EINTR))
        {
            continue;
        }

        if (sent == -1)
        {
            Log(LOG_LEVEL_VERBOSE, "Couldn't send. (send: %s)", GetErrorStr());
            return -1;
        }

        already += sent;
    }
    while (already < tosend);

    return already;
}

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
