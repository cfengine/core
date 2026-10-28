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

#ifndef CFENGINE_REACTOR_WATCH_BACKEND_H
#define CFENGINE_REACTOR_WATCH_BACKEND_H

/* Interface between cf-reactor and the methods it can watch for events with
 * (the 'watch_methods' attribute of 'when' bodies), shared with the
 * Enterprise extension, which provides some of the methods.
 *
 * A method doesn't detect events itself. It only tells cf-reactor that
 * something touched what a watcher is about, by marking the watcher dirty,
 * and the watcher's own check then decides whether its event happened. So
 * every event type behaves the same with every method, and polling at the
 * watcher's interval keeps running underneath as the reference. */

#include <platform.h>

/* JSON array written to the state directory by cf-reactor whenever its
 * watchers are (re-)registered, one object per watcher: "promiser",
 * "bundle", "method" (the watch method it resolved to) and "interval".
 * Read by Enterprise reporting. */
#define REACTOR_WATCHERS_REPORT_FILE "cf_reactor_watchers.json"

/* Bumped on any incompatible change of the structs below */
#define REACTOR_WATCH_BACKEND_ABI_VERSION 1

typedef enum
{
    REACTOR_WATCH_TARGET_FILE,  /* the file at `path`, which may not exist */
    REACTOR_WATCH_TARGET_USERS, /* the local user accounts, `path` is NULL */
} ReactorWatchTargetType;

typedef struct
{
    ReactorWatchTargetType type;
    const char *path;
} ReactorWatchTarget;

/* What cf-reactor provides to the methods. Both functions are thread-safe,
 * and may be called with a watcher_id that is no longer registered, which is
 * ignored. */
typedef struct
{
    int abi_version;                              /* REACTOR_WATCH_BACKEND_ABI_VERSION */
    void (*mark_dirty)(uint64_t watcher_id);      /* check this watcher now */
    void (*mark_all_dirty)(void);                 /* events were lost, check all now */
} ReactorWatchHost;

/* A method to watch for events with. Each one runs its own thread (or uses
 * the OS's own callbacks) to read its notifications, and calls the host's
 * mark_dirty() for the watchers they concern. add() and remove() are called
 * from cf-reactor's main thread. */
typedef struct
{
    const char *name;                                       /* as in 'watch_methods' */
    bool (*available)(void);                                /* this host supports it */
    bool (*start)(const ReactorWatchHost *host);
    void (*stop)(void);
    bool (*add)(uint64_t watcher_id, const ReactorWatchTarget *target);
    void (*remove)(uint64_t watcher_id);
} ReactorWatchBackend;

#endif
