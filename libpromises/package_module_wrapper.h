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

#ifndef CFENGINE_PACKAGE_MODULE_WRAPPER_H
#define CFENGINE_PACKAGE_MODULE_WRAPPER_H

#include <cf3.defs.h>

#define PACKAGE_PROMISE_TERMINATION_CHECK_SEC 1

typedef struct
{
    char *name;
    char *path;
    char *script_path;
    char *script_path_quoted;
    char *script_exec_opts;
    PackageModuleBody *package_module;
    int supported_api_version;
    int timeout_secs; /**< Timeout of each run of the package module */
} PackageModuleWrapper;

/**
 * @brief Creates a wrapper to run a package module.
 *
 * Runs the package module once, to negotiate the API version.
 *
 * @param package_module The package module body, not copied, so it must
 *                       outlive the wrapper
 * @param timeout_secs The timeout of each run of the package module
 * @return The new wrapper, to be destroyed with DeletePackageModuleWrapper(),
 *         or NULL if the package module can't be run or doesn't support API
 *         version 1
 */
PackageModuleWrapper *NewPackageModuleWrapper(PackageModuleBody *package_module,
                                              int timeout_secs);

/**
 * @brief Destroys a package module wrapper.
 *
 * @param wrapper The wrapper, may be NULL
 */
void DeletePackageModuleWrapper(PackageModuleWrapper *wrapper);

/**
 * @brief Runs the package module with a request.
 *
 * @param wrapper The wrapper of the package module
 * @param args The arguments of the package module, e.g. "list-installed"
 * @param request The request, written to the stdin of the package module
 * @param[out] response The lines of the stdout of the package module, only
 *                      set on success
 * @return 0 on success, -1 if the package module couldn't be run or exited
 *         with a non-zero code
 */
int PackageWrapperCommunicate(const PackageModuleWrapper *wrapper, const char *args,
                              const char *request, Rlist **response);

/**
 * @brief Formats the options of a package module as request lines.
 *
 * @param options The options, may be NULL
 * @return One "options=VALUE" line per option, or an empty string if there
 *         are none. To be freed by the caller.
 */
char *PackageModuleFormatOptions(Rlist *options);

/**
 * @brief Destroys a package module body.
 *
 * @param manager The package module body, may be NULL
 */
void PackageModuleBodyDestroy(PackageModuleBody *manager);

#endif /* CFENGINE_PACKAGE_MODULE_WRAPPER_H */
