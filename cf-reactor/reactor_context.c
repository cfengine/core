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
#include <file_watcher.h>
#include <alloc.h>

bool ReactorContextInitialize(ReactorContext *ctx)
{
    assert(ctx != NULL);

    WatcherRegistryInitialize();
    // TODO: Register watchers with `WatcherRegister()`

    size_t max_nova_fds = ReactorNovaMaxFds();
    ctx->all_fds_capacity = max_nova_fds + 1;
    ctx->all_fds = xmalloc(ctx->all_fds_capacity * sizeof(int));

    size_t num_nova_fds = 0;
    if (!ReactorNovaInitialize(ctx->all_fds, max_nova_fds, &num_nova_fds))
    {
        WatcherRegistryFinalize();
        free(ctx->all_fds);
        ctx->all_fds = NULL;
        return false;
    }
    ctx->num_nova_fds = num_nova_fds;
    ctx->num_fds = num_nova_fds;

    if (!EventWatcherInitialize(ctx->all_fds, ctx->all_fds_capacity, &ctx->num_fds))
    {
        ReactorNovaFinalize();
        WatcherRegistryFinalize();
        free(ctx->all_fds);
        ctx->all_fds = NULL;
        return false;
    }

    return true;
}

int ReactorContextSetupFileDescriptors(ReactorContext *ctx)
{
    assert(ctx != NULL);

    FD_ZERO(&ctx->readfds);
    int signal_pipe = GetSignalPipe();
    FD_SET(signal_pipe, &ctx->readfds);

    int max_fd = signal_pipe;
    for (size_t i = 0; i < ctx->num_fds; i++)
    {
        FD_SET(ctx->all_fds[i], &ctx->readfds);
        max_fd = MAX(ctx->all_fds[i], max_fd);
    }
    return max_fd + 1;
}

static bool NovaHasTimedOut(ReactorContext *ctx)
{
    assert(ctx != NULL);
    for (size_t i = 0; i < ctx->num_nova_fds; i++)
    {
        if (FD_ISSET(ctx->all_fds[i], &ctx->readfds))
        {
            return false;
        }
    }
    return true;
}

void ReactorContextHandleEvents(ReactorContext *ctx, time_t *next_tick)
{
    assert(ctx != NULL);

    if (NovaHasTimedOut(ctx))
    {
        ReactorNovaHandleTimeout(next_tick);
    }
    else
    {
        ReactorNovaHandleEvents(&ctx->readfds, ctx->all_fds, next_tick);
    }

    /* The signal pipe is always in the watched set so we wake up
    * promptly on a pending signal, but (per its own contract in
    * signals.c) it must be drained or it stays "ready" forever, which
    * would stop select() from ever blocking again. */
    if (FD_ISSET(GetSignalPipe(), &ctx->readfds))
    {
        unsigned char buf;
        while (recv(GetSignalPipe(), &buf, 1, 0) > 0) { /* drain */ }
    }

    EventWatcherHandleEvents(&ctx->readfds);
}

void ReactorContextFinalize(ReactorContext *ctx)
{
    assert(ctx != NULL);

    EventWatcherFinalize();
    ReactorNovaFinalize();
    free(ctx->all_fds);
    ctx->all_fds = NULL;
}
