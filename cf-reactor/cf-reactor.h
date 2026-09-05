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

#ifndef CFENGINE_REACTOR_H
#define CFENGINE_REACTOR_H

#include <platform.h>

/**
 * @brief Shared state for the cf-reactor daemon's single select(2) loop.
 *
 * `all_fds` is a single flat array shared by every event source the daemon
 * watches. Nova's fds always occupy the first `num_nova_fds` slots (Nova
 * owns that sub-range and is the only thing allowed to populate it); any
 * other event source (currently just the watcher subsystem, see watcher.h)
 * appends its own fd(s) after that, and `num_fds` tracks the total number
 * of slots in use. Adding a new event source means:
 *
 *   1. Have it report how many fds it needs, and add that to the capacity
 *      computed in ReactorContextInitialize() (reactor_context.c).
 *   2. Give it an Initialize(fds, max_size, num_fds)/HandleEvents(readfds)/
 *      Finalize(void) triplet shaped like ReactorNova*() or EventWatcher*(),
 *      and wire the three calls into reactor_context.c next to the existing
 *      ones.
 *
 * No other file needs to know how many event sources exist or in what
 * order their fds appear.
 */
typedef struct ReactorContext
{
    int *all_fds;
    size_t all_fds_capacity;
    size_t num_nova_fds;
    size_t num_fds;

    fd_set readfds;
} ReactorContext;

#endif
