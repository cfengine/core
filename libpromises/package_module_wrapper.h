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

//TODO: use promise expireafter instead
#define PACKAGE_PROMISE_SCRIPT_TIMEOUT_SEC 4 * 3600 /* 4 hours timeout */
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
} PackageModuleWrapper;

PackageModuleWrapper *NewPackageModuleWrapper(PackageModuleBody *package_module);
void DeletePackageModuleWrapper(PackageModuleWrapper *wrapper);
int PackageWrapperCommunicate(const PackageModuleWrapper *wrapper, const char *args,
                              const char *request, Rlist **response);
char *ParseOptions(Rlist *options);

#endif /* CFENGINE_PACKAGE_MODULE_WRAPPER_H */
