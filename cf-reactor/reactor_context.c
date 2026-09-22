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

#include <reactor_context.h>
#include <prototypes3.h>        /* ReactorNova*() */
#include <signals.h>            /* GetSignalPipe() */
#include <watcher.h>
#include <alloc.h>

#define INIT_FD_COUNT 8

static ReactorFd *ReactorFdNew(int fd, ReactorFdType type)
{
    ReactorFd *rfd = xmalloc(sizeof(ReactorFd));
    rfd->fd = fd;
    rfd->type = type;
    return rfd;
}

bool ReactorContextInitialize(ReactorContext *reactor_context)
{
    assert(reactor_context != NULL);

    reactor_context->fds = SeqNew(INIT_FD_COUNT, free);

    WatcherRegistryInitialize();

    // Initialize Nova fds
    {
        reactor_context->max_nova_fds = ReactorNovaMaxFds();
        int *nova_fds = (int *) xmalloc(reactor_context->max_nova_fds * sizeof(int));
        size_t num_nova_fds = 0;

        if (!ReactorNovaInitialize(nova_fds, reactor_context->max_nova_fds, &num_nova_fds))
        {
            free(nova_fds);
            SeqDestroy(reactor_context->fds);
            reactor_context->fds = NULL;
            return false;
        }

        // num_nova_fds can never end up less than max_nova_fds here: Nova
        // asserts internally that it always reports back the same fd count
        // it advertises as its max (see the assert on poll_fd_idx in
        // SetupEventProcessing(), nova/reactor-plugin/cf-reactor.c),
        // so this loop always appends exactly max_nova_fds entries.
        for (size_t i = 0; i < num_nova_fds; i++)
        {
            SeqAppend(reactor_context->fds, ReactorFdNew(nova_fds[i], REACTOR_FD_NOVA));
        }
        free(nova_fds);
    }

    // Initialize event watcher fd
    {
        int watcher_fd;
        if (!EventWatcherInitialize(&watcher_fd))
        {
            ReactorNovaFinalize();
            WatcherRegistryFinalize();
            SeqDestroy(reactor_context->fds);
            reactor_context->fds = NULL;
            return false;
        }
        SeqAppend(reactor_context->fds, ReactorFdNew(watcher_fd, REACTOR_FD_WATCHER));
    }

    return true;
}

int ReactorContextSetupFileDescriptors(ReactorContext *reactor_context)
{
    assert(reactor_context != NULL);

    FD_ZERO(&reactor_context->readfds);
    int signal_pipe = GetSignalPipe();
    FD_SET(signal_pipe, &reactor_context->readfds);

    int max_fd = signal_pipe;
    for (size_t i = 0; i < SeqLength(reactor_context->fds); i++)
    {
        const ReactorFd *rfd = SeqAt(reactor_context->fds, i);
        FD_SET(rfd->fd, &reactor_context->readfds);
        max_fd = MAX(rfd->fd, max_fd);
    }
    return max_fd + 1;
}

static bool NovaHasTimedOut(const ReactorContext *reactor_context)
{
    assert(reactor_context != NULL);
    for (size_t i = 0; i < SeqLength(reactor_context->fds); i++)
    {
        const ReactorFd *rfd = SeqAt(reactor_context->fds, i);

        if (rfd->type != REACTOR_FD_NOVA)
        {
            continue;
        }

        if (FD_ISSET(rfd->fd, &reactor_context->readfds))
        {
            return false;
        }
    }
    return true;
}

static int *GetNovaFds(const ReactorContext *reactor_context)
{
    assert(reactor_context != NULL);
    int *nova_fds = (int *) xmalloc(reactor_context->max_nova_fds * sizeof(int));
    size_t num_nova_fds = 0;

    for (size_t i = 0; i < SeqLength(reactor_context->fds); i++)
    {
        const ReactorFd *rfd = SeqAt(reactor_context->fds, i);

        if (rfd->type != REACTOR_FD_NOVA)
        {
            continue;
        }
        // This can never go out of bounds: reactor_context->fds always
        // holds exactly max_nova_fds REACTOR_FD_NOVA entries (see the
        // comment in ReactorContextInitialize()), so num_nova_fds cannot
        // exceed max_nova_fds and nova_fds is always fully populated by
        // the time this function returns.
        assert(num_nova_fds < reactor_context->max_nova_fds);
        nova_fds[num_nova_fds++] = rfd->fd;
    }

    return nova_fds;
}

static void SetNovaFds(ReactorContext *reactor_context, const int *nova_fds)
{
    assert(reactor_context != NULL);
    size_t num_nova_fds = 0;

    for (size_t i = 0; i < SeqLength(reactor_context->fds); i++)
    {
        ReactorFd *rfd = SeqAt(reactor_context->fds, i);

        if (rfd->type != REACTOR_FD_NOVA)
        {
            continue;
        }
        rfd->fd = nova_fds[num_nova_fds++];
    }
}
static int GetWatcherFd(const ReactorContext *reactor_context)
{
    assert(reactor_context != NULL);
    for (size_t i = 0; i < SeqLength(reactor_context->fds); i++)
    {
        const ReactorFd *rfd = SeqAt(reactor_context->fds, i);

        if (rfd->type == REACTOR_FD_WATCHER)
        {
            return rfd->fd;
        }
    }
    ProgrammingError("Reactor was not initialized with event watcher fd");
    return 0;
}

void ReactorContextHandleEvents(ReactorContext *reactor_context, time_t *next_tick)
{
    assert(reactor_context != NULL);

    if (NovaHasTimedOut(reactor_context))
    {
        ReactorNovaHandleTimeout(next_tick);
    }
    else
    {
        int *nova_fds = GetNovaFds(reactor_context);
        ReactorNovaHandleEvents(&reactor_context->readfds, nova_fds, next_tick);
        // ReactorNova replaces the fd of broken connection
        SetNovaFds(reactor_context, nova_fds);

        free(nova_fds);
    }

    /* The signal pipe is always in the watched set so we wake up
    * promptly on a pending signal, but (per its own contract in
    * signals.c) it must be drained or it stays "ready" forever, which
    * would stop select() from ever blocking again. */
    if (FD_ISSET(GetSignalPipe(), &reactor_context->readfds))
    {
        unsigned char buf;
        while (recv(GetSignalPipe(), &buf, 1, 0) > 0) { /* drain */ }
    }

    EventWatcherHandleEvents(GetWatcherFd(reactor_context), &reactor_context->readfds);
}

void ReactorContextFinalize(ReactorContext *reactor_context)
{
    assert(reactor_context != NULL);

    EventWatcherFinalize();
    ReactorNovaFinalize();
    reactor_context->max_nova_fds = 0;

    SeqDestroy(reactor_context->fds);
    reactor_context->fds = NULL;
}
