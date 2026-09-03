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

#include <wakeup_channel.h>
#include <logging.h>
#include <prototypes3.h>       /* cf_closesocket() */

bool WakeupChannelOpen(WakeupChannel *channel)
{
    assert(channel != NULL);

    channel->fds[0] = -1;
    channel->fds[1] = -1;

    /* Windows' select() only works on sockets, and a plain pipe(2) isn't
     * one, so this always goes through socketpair() -- which works just as
     * well on POSIX -- rather than keeping two code paths in sync. This
     * mirrors MakeSignalPipe() in libpromises/signals.c. */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, channel->fds) != 0)
    {
        Log(LOG_LEVEL_ERR, "Could not create wakeup channel (socketpair: '%s')", GetErrorStr());
        return false;
    }

    for (int i = 0; i < 2; i++)
    {
#ifdef __MINGW32__
        u_long enable = 1;
        int ret = ioctlsocket(channel->fds[i], FIONBIO, &enable);
#define CNTLNAME "ioctlsocket"
#else
        int ret = fcntl(channel->fds[i], F_SETFL, O_NONBLOCK);
#define CNTLNAME "fcntl"
#endif
        if (ret != 0)
        {
            Log(LOG_LEVEL_ERR, "Could not set wakeup channel to non-blocking (" CNTLNAME ": '%s')",
                GetErrorStr());
            WakeupChannelClose(channel);
            return false;
        }
#undef CNTLNAME
    }

    return true;
}

int WakeupChannelReadFd(const WakeupChannel *channel)
{
    assert(channel != NULL);
    return channel->fds[0];
}

void WakeupChannelNotify(const WakeupChannel *channel)
{
    assert(channel != NULL);

    /* One byte is enough to wake the reader up; if the channel happens to
     * already be full, the reader is already guaranteed to wake up because
     * of what's queued, so a transient EAGAIN/EWOULDBLOCK here is fine. */
    unsigned char byte = 1;
    send(channel->fds[1], (const char *) &byte, sizeof(byte), 0);
}

void WakeupChannelDrain(const WakeupChannel *channel)
{
    assert(channel != NULL);

    unsigned char buf;
    while (recv(channel->fds[0], (char *) &buf, sizeof(buf), 0) > 0) { /* drain */ }
}

void WakeupChannelClose(WakeupChannel *channel)
{
    assert(channel != NULL);

    for (int i = 0; i < 2; i++)
    {
        if (channel->fds[i] != -1)
        {
            cf_closesocket(channel->fds[i]);
            channel->fds[i] = -1;
        }
    }
}
