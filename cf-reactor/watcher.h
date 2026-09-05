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

#ifndef CFENGINE_WATCHER_H
#define CFENGINE_WATCHER_H

#include <cf-reactor.h>
#include <cf3.defs.h>   /* Bundle */

typedef enum
{
  EVENT_FILE_DELETED,
} EventType;

typedef bool (*WatcherCheckFn)(void *payload);
typedef void (*WatcherPayloadDestroyFn)(void *payload);

void WatcherRegistryInitialize(void);
void WatcherRegistryFinalize(void);

/**
 * @brief Register a specific watcher instance.
 * 
 * @param key the events promise identifier
 * @param type the type of watcher, defined in when bodies
 * @param payload the data used for by the watcher, depending on the type
 * @param bundle the bundle to run on event
 * @param interval interval between runs
 */
void WatcherRegister(const char *key, EventType type, void *payload, Bundle *bundle, time_t interval);
bool EventWatcherInitialize(int *fds, size_t max_size, size_t *num_fds);
void EventWatcherHandleEvents(fd_set *readfds);
void EventWatcherFinalize(void);

#endif
