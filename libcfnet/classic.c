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


#include "classic.h"

#include "cfnet.h"
#include "logging.h"
#include "misc_lib.h"

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

/**
 * @return 0 if socket is closed.
 */
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
            Log(LOG_LEVEL_ERR, "Couldn't receive. (recv: %s)", GetErrorStr());
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

int SendSocketStream(int sd, char buffer[CF_BUFSIZE], int tosend)
{
    int sent, already = 0;

    do
    {
        sent = send(sd, buffer + already, tosend - already, 0);

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
