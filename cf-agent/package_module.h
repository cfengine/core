/*
   Copyright 2018 Northern.tech AS

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

#ifndef PACKAGE_MODULE_H
#define PACKAGE_MODULE_H

#include <cf3.defs.h>

//TODO: use promise expireafter instead
#define PACKAGE_PROMISE_SCRIPT_TIMEOUT_SEC 4 * 3600 /* 4 hours timeout */
#define PACKAGE_PROMISE_TERMINATION_CHECK_SEC 1

#define GLOBAL_PACKAGE_PROMISE_LOCK_NAME "new_packages_promise_lock"

typedef enum
{
    PACKAGE_TYPE_NONE,
    PACKAGE_TYPE_REPO,
    PACKAGE_TYPE_FILE
} PackageType;

typedef struct 
{
    char *name;
    char *version;
    char *arch;
    PackageType type;
} PackageInfo;

typedef struct
{
    char *type;
    char *message;
} PackageError;

typedef struct
{
    char *name;
    char *path;
    char *script_path;
    char *script_exec_opts;
    PackageModuleBody *package_module;
    int supported_api_version;
} PackageModuleWrapper;

typedef struct
{
    CfLock g_lock;
    EvalContext *lock_ctx;
} PackagePromiseGlobalLock;

typedef enum {
    UPDATE_TYPE_INSTALLED,
    UPDATE_TYPE_UPDATES,
    UPDATE_TYPE_LOCAL_UPDATES,
} UpdateType;

PromiseResult HandlePresentPromiseAction(EvalContext *ctx, 
                                         const char *package_name,
                                         const NewPackages *policy_data,
                                         const PackageModuleWrapper *wrapper,
                                         enum cfopaction action);
PromiseResult HandleAbsentPromiseAction(EvalContext *ctx,
                                        char *package_name,
                                        const NewPackages *policy_data, 
                                        const PackageModuleWrapper *wrapper,
                                        enum cfopaction action);

void UpdatePackagesCache(EvalContext *ctx, bool force_update);

PackageModuleWrapper *NewPackageModuleWrapper(PackageModuleBody *package_module);
void DeletePackageModuleWrapper(PackageModuleWrapper *wrapper);

PackagePromiseGlobalLock AcquireGlobalPackagePromiseLock(EvalContext *ctx);
void YieldGlobalPackagePromiseLock(PackagePromiseGlobalLock lock);

#endif
