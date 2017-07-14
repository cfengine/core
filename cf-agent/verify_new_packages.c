/*
   Copyright 2017 Northern.tech AS

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

#include <verify_new_packages.h>
#include <package_module.h>
#include <logging.h>
#include <string_lib.h>
#include <locks.h>
#include <ornaments.h>

static bool NewPackagePromiseSanityCheck(Attributes a)
{
    if (!a.new_packages.module_body || !a.new_packages.module_body->name)
    {
        Log(LOG_LEVEL_ERR, "Can not find package module body in policy.");
        return false;
    }
    
    if (a.new_packages.module_body->updates_ifelapsed == CF_NOINT ||
        a.new_packages.module_body->installed_ifelapsed == CF_NOINT)
    {
        Log(LOG_LEVEL_ERR,
                "Invalid or missing arguments in package_module body '%s':  "
                "query_installed_ifelapsed = %d query_updates_ifelapsed = %d",
                a.new_packages.module_body->name, 
                a.new_packages.module_body->installed_ifelapsed,
                a.new_packages.module_body->updates_ifelapsed);
            return false;
        return false;
    }
    
    if (a.new_packages.package_policy == NEW_PACKAGE_ACTION_NONE)
    {
        Log(LOG_LEVEL_ERR, "Unsupported package policy in package promise.");
        return false;
    }
    return true;
}

PromiseResult HandleNewPackagePromiseType(EvalContext *ctx, const Promise *pp,
                                          Attributes a, char **promise_log_msg,
                                          LogLevel *log_lvl)
{
    Log(LOG_LEVEL_DEBUG, "New package promise handler");
    
    
    if (!NewPackagePromiseSanityCheck(a))
    {
        *promise_log_msg =
                SafeStringDuplicate("New package promise failed sanity check.");
        *log_lvl = LOG_LEVEL_ERR;
        return PROMISE_RESULT_FAIL;
    }
    
    PromiseBanner(ctx, pp);
    
    PackagePromiseGlobalLock global_lock = AcquireGlobalPackagePromiseLock(ctx);
    
    CfLock package_promise_lock;
    char promise_lock[CF_BUFSIZE];
    snprintf(promise_lock, sizeof(promise_lock), "new-package-%s-%s",
             pp->promiser, a.new_packages.module_body->name);

    if (global_lock.g_lock.lock == NULL)
    {
        *promise_log_msg =
                SafeStringDuplicate("Can not acquire global lock for package "
                                    "promise. Skipping promise evaluation");
        *log_lvl = LOG_LEVEL_INFO;
        
        return PROMISE_RESULT_SKIPPED;
    }
    
    package_promise_lock =
            AcquireLock(ctx, promise_lock, VUQNAME, CFSTARTTIME,
            a.transaction, pp, false);
    if (package_promise_lock.lock == NULL)
    {
        Log(LOG_LEVEL_DEBUG, "Skipping promise execution due to locking.");
        YieldGlobalPackagePromiseLock(global_lock);
        
        *promise_log_msg =
                StringFormat("Can not acquire lock for '%s' package promise. "
                             "Skipping promise evaluation",  pp->promiser);
        *log_lvl = LOG_LEVEL_VERBOSE;
        
        return PROMISE_RESULT_SKIPPED;
    }
    
    PackageModuleWrapper *package_module =
            NewPackageModuleWrapper(a.new_packages.module_body);
    
    if (!package_module)
    {
        *promise_log_msg =
                StringFormat("Some error occurred while contacting package "
                             "module - promise: %s", pp->promiser);
        *log_lvl = LOG_LEVEL_ERR;
        
        YieldCurrentLock(package_promise_lock);
        YieldGlobalPackagePromiseLock(global_lock);
    
        return PROMISE_RESULT_FAIL;
    }
    
    PromiseResult result = PROMISE_RESULT_FAIL;
    
    switch (a.new_packages.package_policy)
    {
        case NEW_PACKAGE_ACTION_ABSENT:
            result = HandleAbsentPromiseAction(ctx, pp->promiser, 
                                               &a.new_packages,
                                               package_module,
                                               a.transaction.action);

            switch (result)
            {
                case PROMISE_RESULT_FAIL:
                    *log_lvl = LOG_LEVEL_ERR;
                    *promise_log_msg =
                        StringFormat("Error removing package '%s'",
                                     pp->promiser);
                    break;
                case PROMISE_RESULT_CHANGE:
                    *log_lvl = LOG_LEVEL_INFO;
                    *promise_log_msg =
                        StringFormat("Successfully removed package '%s'",
                                     pp->promiser);
                    break;
                case PROMISE_RESULT_NOOP:
                    *log_lvl = LOG_LEVEL_VERBOSE;
                    *promise_log_msg =
                        StringFormat("Package '%s' was not installed",
                                     pp->promiser);
                    break;
                case PROMISE_RESULT_WARN:
                    *log_lvl = LOG_LEVEL_WARNING;
                    *promise_log_msg =
                        StringFormat("Package '%s' needs to be removed,"
                                     "but only warning was promised",
                                     pp->promiser);
                    break;
                default:
                    ProgrammingError(
                            "Absent promise action evaluation returned "
                             "unsupported result: %d",
                            result);
                    break;
            }
            break;
        case NEW_PACKAGE_ACTION_PRESENT:
            result = HandlePresentPromiseAction(ctx, pp->promiser, 
                                                &a.new_packages,
                                                package_module,
                                                a.transaction.action);

            switch (result)
            {
                case PROMISE_RESULT_FAIL:
                    *log_lvl = LOG_LEVEL_ERR;
                    *promise_log_msg =
                        StringFormat("Error installing package '%s'",
                                     pp->promiser);
                    break;
                case PROMISE_RESULT_CHANGE:
                    *log_lvl = LOG_LEVEL_INFO;
                    *promise_log_msg =
                        StringFormat("Successfully installed package '%s'",
                                     pp->promiser);
                    break;
                case PROMISE_RESULT_NOOP:
                    *log_lvl = LOG_LEVEL_VERBOSE;
                    *promise_log_msg =
                        StringFormat("Package '%s' already installed",
                                     pp->promiser);
                    break;
                case PROMISE_RESULT_WARN:
                    *log_lvl = LOG_LEVEL_WARNING;
                    *promise_log_msg =
                        StringFormat("Package '%s' needs to be installed,"
                                     "but only warning was promised",
                                     pp->promiser);
                    break;
                default:
                    ProgrammingError(
                            "Present promise action evaluation returned "
                             "unsupported result: %d",
                            result);
                    break;
            }

            break;
        case NEW_PACKAGE_ACTION_NONE:
        default:
            ProgrammingError("Unsupported package action: %d",
                             a.new_packages.package_policy);
            break;
    }
    
    DeletePackageModuleWrapper(package_module);
    
    YieldCurrentLock(package_promise_lock);
    YieldGlobalPackagePromiseLock(global_lock);
    
    return result;
}
