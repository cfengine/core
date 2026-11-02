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

#ifndef CFENGINE_PACKAGE_WATCHER_H
#define CFENGINE_PACKAGE_WATCHER_H

#include <platform.h>
#include <watcher.h>

/**
 * @brief Creates the state of a package_installed or package_uninstalled
 *        watcher.
 *
 * @param ctx The context, to look up the package module
 * @param args The name of the package to watch, optionally followed by the
 *             name of the package module to list the packages with
 * @return The new state, to be destroyed with PackageWatcherStateDestroy(),
 *         or NULL if there is no such package module
 */
void *PackageWatcherStateNew(const EvalContext *ctx, const Rlist *args);

/**
 * @brief Destroys the state of a package watcher.
 *
 * @param state The watcher state, may be NULL
 */
void PackageWatcherStateDestroy(void *state);

/**
 * @brief Checks for the package_installed event.
 *
 * @param state The watcher state
 * @return WATCHER_CHECK_EVENT if the package was installed since the last
 *         check, WATCHER_CHECK_ERROR if the last listing failed,
 *         WATCHER_CHECK_NO_EVENT otherwise
 */
WatcherCheckResult PackageWatcherCheckPackageInstalled(void *state);

/**
 * @brief Checks for the package_uninstalled event.
 *
 * @param state The watcher state
 * @return WATCHER_CHECK_EVENT if the package was uninstalled since the last
 *         check, WATCHER_CHECK_ERROR if the last listing failed,
 *         WATCHER_CHECK_NO_EVENT otherwise
 */
WatcherCheckResult PackageWatcherCheckPackageUninstalled(void *state);

#endif /* CFENGINE_PACKAGE_WATCHER_H */
