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

#include <signals.h>

static bool PENDING_TERMINATION = false;

/********************************************************************/

bool IsPendingTermination(void)
{
    return PENDING_TERMINATION;
}

/********************************************************************/

static int SIGNAL_PIPE[2] = { -1, -1 };

/**
 * Make a pipe that can be used to flag that a signal has arrived.
 * Using a pipe avoids race conditions, since it saves its values until emptied.
 * Use GetSignalPipe() to get the pipe.
 * Note that we use a real socket as the pipe, because Windows only supports
 * using select() with real sockets. This means also using send() and recv()
 * instead of write() and read().
 */
void MakeSignalPipe()
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, SIGNAL_PIPE) != 0)
    {
        Log(LOG_LEVEL_CRIT, "Could not create internal communication pipe. Cannot continue. (socketpair: '%s')",
            GetErrorStr());
        exit(EXIT_FAILURE);
    }

    for (int c = 0; c < 2; c++)
    {
#ifndef __MINGW32__
        if (fcntl(SIGNAL_PIPE[c], F_SETFL, O_NONBLOCK) != 0)
        {
            Log(LOG_LEVEL_CRIT, "Could not create internal communication pipe. Cannot continue. (fcntl: '%s')",
                GetErrorStr());
            exit(EXIT_FAILURE);
        }
#else // __MINGW32__
        u_long enable = 1;
        if (ioctlsocket(SIGNAL_PIPE[c], FIONBIO, &enable) != 0)
        {
            Log(LOG_LEVEL_CRIT, "Could not create internal communication pipe. Cannot continue. (ioctlsocket: '%s')",
                GetErrorStr());
            exit(EXIT_FAILURE);
        }
#endif // __MINGW32__
    }
}

/**
 * Gets the signal pipe, which is non-blocking.
 * Each byte read corresponds to one arrived signal.
 * Note: Use recv() to read from the pipe, not read().
 */
int GetSignalPipe()
{
    return SIGNAL_PIPE[0];
}

void HandleSignalsForAgent(int signum)
{
    if ((signum == SIGTERM) || (signum == SIGINT))
    {
        /* TODO don't exit from the signal handler, just set a flag. Reason is
         * that all the atexit() hooks we register are not reentrant. */
        exit(EXIT_SUCCESS);
    }
    else if (signum == SIGUSR1)
    {
        LogSetGlobalLevel(LOG_LEVEL_DEBUG);
    }
    else if (signum == SIGUSR2)
    {
        LogSetGlobalLevel(LOG_LEVEL_NOTICE);
    }

    unsigned char sig = (unsigned char)signum;
    if (SIGNAL_PIPE[1] >= 0)
    {
        // send() is async-safe, according to POSIX.
        if (send(SIGNAL_PIPE[1], &sig, 1, 0) < 0)
        {
            // These signal contention. Everything else is an error.
            if (errno != EAGAIN
#ifndef __MINGW32__
                && errno != EWOULDBLOCK
#endif
                )
            {
                // This is not async safe, but if we get in here there's something really weird
                // going on.
                Log(LOG_LEVEL_CRIT, "Could not write to signal pipe. Unsafe to continue. (write: '%s')",
                    GetErrorStr());
                _exit(EXIT_FAILURE);
            }
        }
    }

/* Reset the signal handler */
    signal(signum, HandleSignalsForAgent);
}

/********************************************************************/

void HandleSignalsForDaemon(int signum)
{
    if ((signum == SIGTERM) || (signum == SIGINT) || (signum == SIGHUP) || (signum == SIGSEGV) || (signum == SIGKILL)
        || (signum == SIGPIPE))
    {
        PENDING_TERMINATION = true;
    }
    else if (signum == SIGUSR1)
    {
        LogSetGlobalLevel(LOG_LEVEL_DEBUG);
    }
    else if (signum == SIGUSR2)
    {
        LogSetGlobalLevel(LOG_LEVEL_NOTICE);
    }

    unsigned char sig = (unsigned char)signum;
    if (SIGNAL_PIPE[1] >= 0)
    {
        // send() is async-safe, according to POSIX.
        if (send(SIGNAL_PIPE[1], &sig, 1, 0) < 0)
        {
            // These signal contention. Everything else is an error.
            if (errno != EAGAIN
#ifndef __MINGW32__
                && errno != EWOULDBLOCK
#endif
                )
            {
                // This is not async safe, but if we get in here there's something really weird
                // going on.
                Log(LOG_LEVEL_CRIT, "Could not write to signal pipe. Unsafe to continue. (write: '%s')",
                    GetErrorStr());
                _exit(EXIT_FAILURE);
            }
        }
    }

/* Reset the signal handler */
    signal(signum, HandleSignalsForDaemon);
}
