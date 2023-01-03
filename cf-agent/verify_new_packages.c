/*
  Copyright 2023 Northern.tech AS

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
#include <promises.h>           /* PromiseRef */

static bool NewPackagePromiseSanityCheck(const Attributes *a)
{
    assert(a != NULL);
    if (!a->new_packages.module_body || !a->new_packages.module_body->name)
    {
        Log(LOG_LEVEL_ERR, "Can not find package module body in policy.");
        return false;
    }

    if (a->new_packages.module_body->updates_ifelapsed == CF_NOINT ||
        a->new_packages.module_body->installed_ifelapsed == CF_NOINT)
    {
        Log(LOG_LEVEL_ERR,
                "Invalid or missing arguments in package_module body '%s':  "
                "query_installed_ifelapsed = %d query_updates_ifelapsed = %d",
                a->new_packages.module_body->name,
                a->new_packages.module_body->installed_ifelapsed,
                a->new_packages.module_body->updates_ifelapsed);
            return false;
        return false;
    }

    if (a->new_packages.package_policy == NEW_PACKAGE_ACTION_NONE)
    {
        Log(LOG_LEVEL_ERR, "Unsupported package policy in package promise.");
        return false;
    }
    return true;
}

PromiseResult HandleNewPackagePromiseType(EvalContext *ctx, const Promise *pp, const Attributes *a)
{
    assert(a != NULL);
    Log(LOG_LEVEL_DEBUG, "New package promise handler");


    if (!NewPackagePromiseSanityCheck(a))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "New package promise failed sanity check.");
        return PROMISE_RESULT_FAIL;
    }

    PromiseBanner(ctx, pp);

    PackagePromiseGlobalLock global_lock = AcquireGlobalPackagePromiseLock(ctx);

    CfLock package_promise_lock;
    char promise_lock[CF_BUFSIZE];
    snprintf(promise_lock, sizeof(promise_lock), "new-package-%s-%s",
             pp->promiser, a->new_packages.module_body->name);

    if (global_lock.g_lock.lock == NULL)
    {
        Log(LOG_LEVEL_DEBUG, "Skipping promise execution due to global packaging locking.");
        return PROMISE_RESULT_SKIPPED;
    }

    package_promise_lock =
            AcquireLock(ctx, promise_lock, VUQNAME, CFSTARTTIME,
            a->transaction.ifelapsed, a->transaction.expireafter, pp, false);
    if (package_promise_lock.lock == NULL)
    {
        YieldGlobalPackagePromiseLock(global_lock);

        Log(LOG_LEVEL_DEBUG, "Skipping promise execution due to promise-specific package locking.");
        return PROMISE_RESULT_SKIPPED;
    }

    PackageModuleWrapper *package_module =
        NewPackageModuleWrapper(a->new_packages.module_body);

    if (package_module == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "Some error occurred while contacting package module for promise '%s'",
             pp->promiser);

        YieldCurrentLock(package_promise_lock);
        YieldGlobalPackagePromiseLock(global_lock);

        return PROMISE_RESULT_FAIL;
    }

    PromiseResult result = PROMISE_RESULT_FAIL;

    switch (a->new_packages.package_policy)
    {
        case NEW_PACKAGE_ACTION_ABSENT:
            result = HandleAbsentPromiseAction(ctx, pp, a, package_module);

            switch (result)
            {
                case PROMISE_RESULT_FAIL:
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                         "Error removing package '%s'", pp->promiser);
                    break;
                case PROMISE_RESULT_CHANGE:
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a,
                         "Successfully removed package '%s'", pp->promiser);
                    break;
                case PROMISE_RESULT_NOOP:
                    /* Properly logged in HandleAbsentPromiseAction() */
                    cfPS(ctx, LOG_LEVEL_NOTHING, PROMISE_RESULT_NOOP, pp, a, NULL);
                    break;
                case PROMISE_RESULT_WARN:
                    /* Properly logged in HandleAbsentPromiseAction() */
                    cfPS(ctx, LOG_LEVEL_NOTHING, PROMISE_RESULT_WARN, pp, a, NULL);
                    break;
                default:
                    ProgrammingError("Absent promise action evaluation returned"
                                     " unsupported result: %d", result);
                    break;
            }
            break;
        case NEW_PACKAGE_ACTION_PRESENT:
            result = HandlePresentPromiseAction(ctx, pp, a, package_module);

            switch (result)
            {
                case PROMISE_RESULT_FAIL:
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                         "Error installing package '%s'", pp->promiser);
                    break;
                case PROMISE_RESULT_CHANGE:
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a,
                         "Successfully installed package '%s'", pp->promiser);
                    break;
                case PROMISE_RESULT_NOOP:
                    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
                         "Package '%s' already installed", pp->promiser);
                    break;
                case PROMISE_RESULT_WARN:
                    /* Properly logged in HandlePresentPromiseAction() */
                    cfPS(ctx, LOG_LEVEL_NOTHING, PROMISE_RESULT_WARN, pp, a, NULL);
                    break;
                default:
                    ProgrammingError("Present promise action evaluation returned"
                                     " unsupported result: %d", result);
                    break;
            }

            break;
        case NEW_PACKAGE_ACTION_NONE:
        default:
            ProgrammingError("Unsupported package action: %d", a->new_packages.package_policy);
            break;
    }

    DeletePackageModuleWrapper(package_module);

    YieldCurrentLock(package_promise_lock);
    YieldGlobalPackagePromiseLock(global_lock);

    return result;
}
