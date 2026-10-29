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

#ifndef CFENGINE_WATCH_BACKENDS_H
#define CFENGINE_WATCH_BACKENDS_H

#include <reactor_watch_backend.h>
#include <rlist.h>

/* Environment variable replacing the default 'watch_methods' (of watchers
 * that don't set it), a comma-separated list, so that the reactor tests can
 * be run with each method in turn */
#define WATCH_METHODS_TEST_OVERRIDE_ENV "CFENGINE_TEST_OVERRIDE_WATCH_METHODS"

/**
 * @brief Collect the watch methods of this binary (cf-reactor's own and the
 *        Enterprise extension's). They are started when first used.
 * @param host what the methods call back into, see ReactorWatchHost
 */
void WatchBackendsInitialize(const ReactorWatchHost *host);

/** @brief Stop the started methods. */
void WatchBackendsFinalize(void);

/**
 * @brief Watch target with the first available method of methods.
 *
 * Methods for another platform, or that this binary doesn't have, are
 * skipped. "poll" always succeeds, as the watcher is polled at its interval
 * anyway: it means polling alone is acceptable.
 *
 * @param watcher_id the id passed back to ReactorWatchHost.mark_dirty()
 * @param methods method names in order of preference, NULL for the default
 * @return the name of the method used (static), or NULL if none of methods
 *         is available and they don't include "poll"
 */
const char *WatchBackendsAdd(uint64_t watcher_id, const ReactorWatchTarget *target,
                             const Rlist *methods, const char *promiser);

/** @brief Stop watching for watcher_id with method, as returned by WatchBackendsAdd(). */
void WatchBackendsRemove(uint64_t watcher_id, const char *method);

#endif
