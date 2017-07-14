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

#include <verify_packages.h>
#include <verify_new_packages.h>
#include <package_module.h>

#include <actuator.h>
#include <promises.h>
#include <dir.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <file_lib.h>
#include <vars.h>
#include <conversion.h>
#include <expand.h>
#include <scope.h>
#include <vercmp.h>
#include <matching.h>
#include <match_scope.h>
#include <attributes.h>
#include <string_lib.h>
#include <pipes.h>
#include <locks.h>
#include <exec_tools.h>
#include <policy.h>
#include <misc_lib.h>
#include <rlist.h>
#include <ornaments.h>
#include <eval_context.h>
#include <retcode.h>
#include <known_dirs.h>
#include <csv_writer.h>
#include <cf-agent-enterprise-stubs.h>
#include <cf-windows-functions.h>

/* Called structure:

   Top-level: cf-agent calls...

   * CleanScheduledPackages

   * VerifyPackagesPromise -> all the Verify* functions that schedule operation

   * ExecuteScheduledPackages -> all the Execute* functions to run operations

 */

/** Entry points from VerifyPackagesPromise **/

#define REPORT_THIS_PROMISE(__pp) (strncmp(__pp->promiser, "cfe_internal_", 13) != 0)

#define cfPS_HELPER_0ARG(__ctx, __log_level, __result, __pp, __a, __str) \
    if (REPORT_THIS_PROMISE(__pp)) \
    { \
        cfPS(__ctx, __log_level, __result, __pp, __a, __str); \
    }
#define cfPS_HELPER_1ARG(__ctx, __log_level, __result, __pp, __a, __str, __arg1) \
    if (REPORT_THIS_PROMISE(__pp)) \
    { \
        cfPS(__ctx, __log_level, __result, __pp, __a, __str, __arg1); \
    }
#define cfPS_HELPER_2ARG(__ctx, __log_level, __result, __pp, __a, __str, __arg1, __arg2) \
    if (REPORT_THIS_PROMISE(__pp)) \
    { \
        cfPS(__ctx, __log_level, __result, __pp, __a, __str, __arg1, __arg2); \
    }
#define cfPS_HELPER_3ARG(__ctx, __log_level, __result, __pp, __a, __str, __arg1, __arg2, __arg3) \
    if (REPORT_THIS_PROMISE(__pp)) \
    { \
        cfPS(__ctx, __log_level, __result, __pp, __a, __str, __arg1, __arg2, __arg3); \
    }

#define PromiseResultUpdate_HELPER(__pp, __prior, __evidence) \
    REPORT_THIS_PROMISE(__pp) ? PromiseResultUpdate(__prior, __evidence) : __evidence

typedef enum
{
    PACKAGE_PROMISE_TYPE_OLD = 0,
    PACKAGE_PROMISE_TYPE_NEW,
    PACKAGE_PROMISE_TYPE_MIXED,
    PACKAGE_PROMISE_TYPE_OLD_ERROR,
    PACKAGE_PROMISE_TYPE_NEW_ERROR
} PackagePromiseType;

static int PackageSanityCheck(EvalContext *ctx, Attributes a, const Promise *pp);

static int VerifyInstalledPackages(EvalContext *ctx, PackageManager **alllists, const char *default_arch, Attributes a, const Promise *pp, PromiseResult *result);

static PromiseResult VerifyPromisedPackage(EvalContext *ctx, Attributes a, const Promise *pp);
static PromiseResult VerifyPromisedPatch(EvalContext *ctx, Attributes a, const Promise *pp);

/** Utils **/

static char *GetDefaultArch(const char *command);

static bool ExecPackageCommand(EvalContext *ctx, char *command, int verify, int setCmdClasses, Attributes a, const Promise *pp, PromiseResult *result);

static int PrependPatchItem(EvalContext *ctx, PackageItem ** list, char *item, PackageItem * chklist, const char *default_arch, Attributes a, const Promise *pp);
static int PrependMultiLinePackageItem(EvalContext *ctx, PackageItem ** list, char *item, int reset, const char *default_arch, Attributes a, const Promise *pp);
static int PrependListPackageItem(EvalContext *ctx, PackageItem ** list, char *item, const char *default_arch, Attributes a, const Promise *pp);

static PackageManager *GetPackageManager(PackageManager **lists, char *mgr, PackageAction pa, PackageActionPolicy x);
static void DeletePackageManagers(PackageManager *morituri);

static const char *PrefixLocalRepository(const Rlist *repositories, const char *package);

PromiseResult HandleOldPackagePromiseType(EvalContext *ctx, const Promise *pp, Attributes a);

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, ReportPatches, ARG_UNUSED PackageManager *, list)
{
    Log(LOG_LEVEL_VERBOSE, "Patch reporting feature is only available in the enterprise version");
}

/*****************************************************************************/

PackageManager *PACKAGE_SCHEDULE = NULL; /* GLOBAL_X */
PackageManager *INSTALLED_PACKAGE_LISTS = NULL; /* GLOBAL_X */

#define PACKAGE_LIST_COMMAND_WINAPI "/Windows_API"

/*****************************************************************************/

#define PACKAGE_IGNORED_CFE_INTERNAL "cfe_internal_non_existing_package"

/* Returns the old or new package promise type depending on promise 
   constraints. */
static PackagePromiseType GetPackagePromiseVersion(const Packages *packages,
        const NewPackages *new_packages)
{
    /* We have mixed packages promise constraints. */
    if (!packages->is_empty && !new_packages->is_empty)
    {
        return PACKAGE_PROMISE_TYPE_MIXED;
    }
    else if (!new_packages->is_empty) /* new packages promise */
    {
        if (new_packages->package_policy == NEW_PACKAGE_ACTION_NONE)
        {
            return PACKAGE_PROMISE_TYPE_NEW_ERROR;
        }
        return PACKAGE_PROMISE_TYPE_NEW;
    }
    else /* old packages promise */
    {
        //TODO:
        if (!packages->has_package_method)
        {
            return PACKAGE_PROMISE_TYPE_OLD_ERROR;
        }
        return PACKAGE_PROMISE_TYPE_OLD;
    }
}

PromiseResult VerifyPackagesPromise(EvalContext *ctx, const Promise *pp)
{
    PromiseResult result = PROMISE_RESULT_FAIL;
    char *promise_log_message = NULL;
    LogLevel level;
    
    Attributes a = GetPackageAttributes(ctx, pp);
    PackagePromiseType package_promise_type =
            GetPackagePromiseVersion(&a.packages, &a.new_packages);
    
    switch (package_promise_type)
    {
        case PACKAGE_PROMISE_TYPE_NEW:
            Log(LOG_LEVEL_VERBOSE, "Using new package promise.");

            result = HandleNewPackagePromiseType(ctx, pp, a, &promise_log_message,
                    &level);
            
            assert(promise_log_message != NULL);
            
            if (result != PROMISE_RESULT_SKIPPED)
            {
                cfPS(ctx, level, result, pp, a, "%s", promise_log_message);
            }
            free(promise_log_message);
            break;
        case PACKAGE_PROMISE_TYPE_OLD:
            Log(LOG_LEVEL_VERBOSE,
                "Using old package promise. Please note that this old "
                "implementation is being phased out. The old "
                "implementation will continue to work, but forward development "
                "will be directed toward the new implementation.");

            result = HandleOldPackagePromiseType(ctx, pp, a);
        
            /* Update new package promise cache in case we have mixed old and new 
             * package promises in policy. */
            if (result == PROMISE_RESULT_CHANGE || result == PROMISE_RESULT_FAIL)
            {
                UpdatePackagesCache(ctx, false);
            }
            break;
        case PACKAGE_PROMISE_TYPE_NEW_ERROR:
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, 
                         "New package promise failed sanity check.");
            break;
        case PACKAGE_PROMISE_TYPE_OLD_ERROR:
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, 
                         "Old package promise failed sanity check.");
            break;
        case PACKAGE_PROMISE_TYPE_MIXED:
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, 
                         "Mixed old and new package promise attributes inside "
                         "one package promise.");
            break;
        default:
            assert(0); //Shouldn't happen
    }
    return result;
}


/******************************************************************************/

/**
   @brief Executes single packages promise

   Called by cf-agent.

   * checks "name", "version", "arch", "firstrepo" variables from the "this" context
   * gets the package attributes into a
   * on Windows, if the package_list_command is not defined, use the hard-coded PACKAGE_LIST_COMMAND_WINAPI
   * do a package sanity check on the promise
   * print promise banner
   * reset to root directory (Yum bugfix)
   * get the default architecture from a.packages.package_default_arch_command into default_arch
   * call VerifyInstalledPackages with default_arch
   * if the package action is "patch", call VerifyPromisedPatch and return its result through PromiseResultUpdate_HELPER
   * for all other package actions, call VerifyPromisedPackage and return its result through PromiseResultUpdate_HELPER

   @param ctx [in] The evaluation context
   @param pp [in] the Promise for this operation
   @returns the promise result
*/
PromiseResult HandleOldPackagePromiseType(EvalContext *ctx, const Promise *pp, Attributes a)
{
    CfLock thislock;
    char lockname[CF_BUFSIZE];
    PromiseResult result = PROMISE_RESULT_NOOP;
    
    const char *reserved_vars[] = { "name", "version", "arch", "firstrepo", NULL };
    for (int c = 0; reserved_vars[c]; c++)
    {
        const char *reserved = reserved_vars[c];
        VarRef *var_ref = VarRefParseFromScope(reserved, "this");
        if (EvalContextVariableGet(ctx, var_ref, NULL))
        {
            Log(LOG_LEVEL_WARNING, "$(%s) variable has a special meaning in packages promises. "
                "Things may not work as expected if it is already defined.", reserved);
        }
        VarRefDestroy(var_ref);
    }
    
#ifdef __MINGW32__

    if(!a.packages.package_list_command)
    {
        a.packages.package_list_command = PACKAGE_LIST_COMMAND_WINAPI;
    }

#endif

    if (!PackageSanityCheck(ctx, a, pp))
    {
        Log(LOG_LEVEL_VERBOSE, "Package promise %s failed sanity check", pp->promiser);
        result = PROMISE_RESULT_FAIL;
        goto end;
    }

    PromiseBanner(ctx, pp);

// Now verify the package itself
    
    PackagePromiseGlobalLock package_lock = AcquireGlobalPackagePromiseLock(ctx);
    if (package_lock.g_lock.lock == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, 
            "Can not acquire global lock for package promise. Skipping promise "
            "evaluation");
        result = PROMISE_RESULT_SKIPPED;
        goto end;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "package-%s-%s", pp->promiser, a.packages.package_list_command);

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        YieldGlobalPackagePromiseLock(package_lock);
        result = PROMISE_RESULT_SKIPPED;
        goto end;
    }

// Start by reseting the root directory in case yum tries to glob regexs(!)

    if (safe_chdir("/") != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to chdir into '/'");
    }

    char *default_arch = GetDefaultArch(a.packages.package_default_arch_command);

    if (default_arch == NULL)
    {
        cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Unable to obtain default architecture for package manager - aborting");
        YieldCurrentLock(thislock);
        YieldGlobalPackagePromiseLock(package_lock);
        result = PROMISE_RESULT_FAIL;
        goto end;
    }

    Log(LOG_LEVEL_VERBOSE, "Default package architecture for promise %s is '%s'", pp->promiser, default_arch);
    if (!VerifyInstalledPackages(ctx, &INSTALLED_PACKAGE_LISTS, default_arch, a, pp, &result))
    {
        cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Unable to obtain a list of installed packages - aborting");
        free(default_arch);
        YieldCurrentLock(thislock);
        YieldGlobalPackagePromiseLock(package_lock);
        result = PROMISE_RESULT_FAIL;
        goto end;
    }

    free(default_arch);

    switch (a.packages.package_policy)
    {
    case PACKAGE_ACTION_PATCH:
        Log(LOG_LEVEL_VERBOSE, "Verifying patch action for promise %s", pp->promiser);
        result = PromiseResultUpdate_HELPER(pp, result, VerifyPromisedPatch(ctx, a, pp));
        break;

    default:
        Log(LOG_LEVEL_VERBOSE, "Verifying action for promise %s", pp->promiser);
        result = PromiseResultUpdate_HELPER(pp, result, VerifyPromisedPackage(ctx, a, pp));
        break;
    }

    YieldCurrentLock(thislock);
    YieldGlobalPackagePromiseLock(package_lock);

end:
    if (!REPORT_THIS_PROMISE(pp))
    {
        // This will not be reported elsewhere, so give it kept outcome.
        result = PROMISE_RESULT_NOOP;
        cfPS(ctx, LOG_LEVEL_DEBUG, result, pp, a, "Giving dummy package kept outcome");
    }

    return result;
}

/**
   @brief Pre-check of promise contents

   Called by VerifyPackagesPromise.  Does many sanity checks on the
   promise attributes and semantics.

   @param ctx [in] The evaluation context
   @param a [in] the promise Attributes for this operation
   @param pp [in] the Promise for this operation
   @returns the promise result
*/

static int PackageSanityCheck(EvalContext *ctx, Attributes a, const Promise *pp)
{
#ifndef __MINGW32__  // Windows may use Win32 API for listing and parsing

    if (a.packages.package_list_name_regex == NULL)
    {
        cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "You must supply a method for determining the name of existing packages e.g. use the standard library generic package_method");
        return false;
    }

    if (a.packages.package_list_version_regex == NULL)
    {
        cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "You must supply a method for determining the version of existing packages e.g. use the standard library generic package_method");
        return false;
    }

    if ((!a.packages.package_commands_useshell) && (a.packages.package_list_command) && (!IsExecutable(CommandArg0(a.packages.package_list_command))))
    {
        cfPS_HELPER_1ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "The proposed package list command '%s' was not executable",
             a.packages.package_list_command);
        return false;
    }


#endif /* !__MINGW32__ */


    if ((a.packages.package_list_command == NULL) && (a.packages.package_file_repositories == NULL))
    {
        cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "You must supply a method for determining the list of existing packages (a command or repository list) e.g. use the standard library generic package_method");
        return false;
    }

    if (a.packages.package_file_repositories)
    {
        Rlist *rp;

        for (rp = a.packages.package_file_repositories; rp != NULL; rp = rp->next)
        {
            if (strlen(RlistScalarValue(rp)) > CF_MAXVARSIZE - 1)
            {
                cfPS_HELPER_1ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "The repository path '%s' is too long", RlistScalarValue(rp));
                return false;
            }
        }
    }

    if ((a.packages.package_name_regex) || (a.packages.package_version_regex) || (a.packages.package_arch_regex))
    {
        if (a.packages.package_name_regex == NULL)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "You must supply name regex if you supplied version or arch regex for parsing promiser string");
            return false;
        }
        if ((a.packages.package_name_regex) && (a.packages.package_version_regex) && (a.packages.package_arch_regex))
        {
            if ((a.packages.package_version) || (a.packages.package_architectures))
            {
                cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                     "You must either supply all regexs for (name,version,arch) or a separate version number and architecture");
                return false;
            }
        }
        else
        {
            if ((a.packages.package_version) && (a.packages.package_architectures))
            {
                cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                     "You must either supply all regexs for (name,version,arch) or a separate version number and architecture");
                return false;
            }
        }

        if ((a.packages.package_version_regex) && (a.packages.package_version))
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "You must either supply version regex or a separate version number");
            return false;
        }

        if ((a.packages.package_arch_regex) && (a.packages.package_architectures))
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "You must either supply arch regex or a separate architecture");
            return false;
        }
    }

    if (!a.packages.package_installed_regex)
    {
        cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "!! Package installed regex undefined");
        return false;
    }

    if (a.packages.package_policy == PACKAGE_ACTION_VERIFY)
    {
        if (!a.packages.package_verify_command)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Package verify policy is used, but no package_verify_command is defined");
            return false;
        }
        else if ((a.packages.package_noverify_returncode == CF_NOINT) && (a.packages.package_noverify_regex == NULL))
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Package verify policy is used, but no definition of verification failiure is set (package_noverify_returncode or packages.package_noverify_regex)");
            return false;
        }
    }

    if ((a.packages.package_noverify_returncode != CF_NOINT) && (a.packages.package_noverify_regex))
    {
        cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "!! Both package_noverify_returncode and package_noverify_regex are defined, pick one of them");
        return false;
    }

    /* Dependency checks */
    if (!a.packages.package_delete_command)
    {
        if (a.packages.package_delete_convention)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Dependency conflict: package_delete_command is not used, but package_delete_convention is defined.");
            return false;
        }
    }
    if (!a.packages.package_list_command)
    {
        if (a.packages.package_installed_regex)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Dependency conflict: package_list_command is not used, but package_installed_regex is defined.");
            return false;
        }
        if (a.packages.package_list_arch_regex)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Dependency conflict: package_list_command is not used, but package_arch_regex is defined.");
            return false;
        }
        if (a.packages.package_list_name_regex)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Dependency conflict: package_list_command is not used, but package_name_regex is defined.");
            return false;
        }
        if (a.packages.package_list_version_regex)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Dependency conflict: package_list_command is not used, but package_version_regex is defined.");
            return false;
        }
    }
    if (!a.packages.package_patch_command)
    {
        if (a.packages.package_patch_arch_regex)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Dependency conflict: package_patch_command is not used, but package_patch_arch_regex is defined.");
            return false;
        }
        if (a.packages.package_patch_name_regex)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Dependency conflict: package_patch_command is not used, but package_patch_name_regex is defined.");
            return false;
        }
        if (a.packages.package_patch_version_regex)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Dependency conflict: package_patch_command is not used, but package_patch_version_regex is defined.");
            return false;
        }
    }
    if (!a.packages.package_patch_list_command)
    {
        if (a.packages.package_patch_installed_regex)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Dependency conflict: package_patch_list_command is not used, but package_patch_installed_regex is defined.");
            return false;
        }
    }
    if (!a.packages.package_verify_command)
    {
        if (a.packages.package_noverify_regex)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Dependency conflict: package_verify_command is not used, but package_noverify_regex is defined.");
            return false;
        }
        if (a.packages.package_noverify_returncode != CF_NOINT)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "!! Dependency conflict: package_verify_command is not used, but package_noverify_returncode is defined.");
            return false;
        }
    }
    return true;
}

/**
   @brief Generates the list of installed packages

   Called by VerifyInstalledPackages

   * calls a.packages.package_list_update_command if $(sys.statedir)/software_update_timestamp_<manager>
     is older than the interval specified in package_list_update_ifelapsed.
   * assembles the package list from a.packages.package_list_command
   * respects a.packages.package_commands_useshell (boolean)
   * parses with a.packages.package_multiline_start and if successful, calls PrependMultiLinePackageItem
   * else, parses with a.packages.package_installed_regex and if successful, calls PrependListPackageItem

   @param ctx [in] The evaluation context
   @param installed_list [in] a list of PackageItems
   @param default_arch [in] the default architecture
   @param a [in] the promise Attributes for this operation
   @param pp [in] the Promise for this operation
   @param result [inout] the PromiseResult for this operation
   @returns boolean pass/fail of command run
*/
static bool PackageListInstalledFromCommand(EvalContext *ctx,
                                            PackageItem **installed_list,
                                            const char *default_arch,
                                            Attributes a, const Promise *pp,
                                            PromiseResult *result)
{
    if (a.packages.package_list_update_command != NULL)
    {
        if (!a.packages.package_add_command)
        {
            Log(LOG_LEVEL_ERR, "package_add_command missing while trying to "
                               "generate list of installed packages");
            return false;
        }

        time_t horizon = 24 * 60, now = time(NULL);
        bool call_update = true;
        struct stat sb;
        char update_timestamp_file[PATH_MAX];

        snprintf(update_timestamp_file, sizeof(update_timestamp_file), "%s%csoftware_update_timestamp_%s",
                 GetStateDir(), FILE_SEPARATOR,
                 ReadLastNode(RealPackageManager(a.packages.package_add_command)));

        if (stat(update_timestamp_file, &sb) != -1)
        {
            if (a.packages.package_list_update_ifelapsed != CF_NOINT)
            {
                horizon = a.packages.package_list_update_ifelapsed;
            }

            char *rel, *action;
            if (now - sb.st_mtime < horizon * 60)
            {
                rel = "less";
                action = "Not updating";
                call_update = false;
            }
            else
            {
                rel = "more";
                action = "Updating";
            }
            Log(LOG_LEVEL_VERBOSE, "'%s' is %s than %i minutes old. %s package list.",
                update_timestamp_file, rel, a.packages.package_list_update_ifelapsed, action);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "'%s' does not exist. Updating package list.", update_timestamp_file);
        }

        if (call_update)
        {
            Log(LOG_LEVEL_VERBOSE, "Calling package list update command: '%s'", a.packages.package_list_update_command);
            ExecPackageCommand(ctx, a.packages.package_list_update_command, false, false, a, pp, result);

            // Touch timestamp file.
            int err = utime(update_timestamp_file, NULL);
            if (err < 0)
            {
                if (errno == ENOENT)
                {
                    int fd = open(update_timestamp_file, O_WRONLY | O_CREAT, 0600);
                    if (fd >= 0)
                    {
                        close(fd);
                    }
                    else
                    {
                        Log(LOG_LEVEL_ERR, "Could not create timestamp file '%s'. (open: '%s')",
                            update_timestamp_file, GetErrorStr());
                    }
                }
                else
                {
                    Log(LOG_LEVEL_ERR, "Could not update timestamp file '%s'. (utime: '%s')",
                        update_timestamp_file, GetErrorStr());
                }
            }
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Reading package list from '%s'", a.packages.package_list_command);

    FILE *fin;

    if (a.packages.package_commands_useshell)
    {
        if ((fin = cf_popen_sh(a.packages.package_list_command, "r")) == NULL)
        {
            Log(LOG_LEVEL_ERR, "Couldn't open the package list with command '%s'. (cf_popen_sh: %s)",
                  a.packages.package_list_command, GetErrorStr());
            return false;
        }
    }
    else if ((fin = cf_popen(a.packages.package_list_command, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open the package list with command '%s'. (cf_popen: %s)",
            a.packages.package_list_command, GetErrorStr());
        return false;
    }

    const int reset = true, update = false;

    size_t buf_size = CF_BUFSIZE;
    char *buf = xmalloc(buf_size);

    for (;;)
    {
        ssize_t res = CfReadLine(&buf, &buf_size, fin);
        if (res == -1)
        {
            if (!feof(fin))
            {
                Log(LOG_LEVEL_ERR, "Unable to read list of packages from command '%s'. (fread: %s)",
                      a.packages.package_list_command, GetErrorStr());
                cf_pclose(fin);
                free(buf);
                return false;
            }
            else
            {
                break;
            }
        }

        if (a.packages.package_multiline_start)
        {
            if (FullTextMatch(ctx, a.packages.package_multiline_start, buf))
            {
                PrependMultiLinePackageItem(ctx, installed_list, buf, reset, default_arch, a, pp);
            }
            else
            {
                PrependMultiLinePackageItem(ctx, installed_list, buf, update, default_arch, a, pp);
            }
        }
        else
        {
            if (!FullTextMatch(ctx, a.packages.package_installed_regex, buf))
            {
                Log(LOG_LEVEL_VERBOSE, "Package line '%s' did not match the package_installed_regex pattern", buf);
                continue;
            }

            if (!PrependListPackageItem(ctx, installed_list, buf, default_arch, a, pp))
            {
                Log(LOG_LEVEL_VERBOSE, "Package line '%s' did not match one of the package_list_(name|version|arch)_regex patterns", buf);
                continue;
            }

        }
    }

    if (a.packages.package_multiline_start)
    {
        PrependMultiLinePackageItem(ctx, installed_list, buf, reset, default_arch, a, pp);
    }

    free(buf);
    return cf_pclose(fin) == 0;
}

/**
   @brief Writes the software inventory

   Called by VerifyInstalledPackages

   * calls GetSoftwareCacheFilename to get the inventory CSV filename
   * for each PackageManager in the list
   *  * for each PackageItem in the PackageManager's list
   *  * write name, version, architecture, manager name

   @param ctx [in] The evaluation context
   @param list [in] a list of PackageManagers
*/
static void ReportSoftware(PackageManager *list)
{
    FILE *fout;
    PackageManager *mp = NULL;
    PackageItem *pi;
    char name[CF_BUFSIZE];

    GetSoftwareCacheFilename(name);

    if ((fout = fopen(name, "w")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Cannot open the destination file '%s'. (fopen: %s)",
            name, GetErrorStr());
        return;
    }

    Writer *writer_installed = FileWriter(fout);

    CsvWriter *c = CsvWriterOpen(writer_installed);
    if (c)
    {
        for (mp = list; mp != NULL; mp = mp->next)
        {
            for (pi = mp->pack_list; pi != NULL; pi = pi->next)
            {
                CsvWriterField(c, pi->name);
                CsvWriterField(c, pi->version);
                CsvWriterField(c, pi->arch);
                CsvWriterField(c, ReadLastNode(RealPackageManager(mp->manager)));
                CsvWriterNewRecord(c);
            }
        }

        CsvWriterClose(c);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Cannot write CSV to file '%s'", name);
    }

    WriterClose(writer_installed);
}

/**
   @brief Invalidates the software inventory

   Called by ExecuteSchedule and ExecutePatch

   * calls GetSoftwareCacheFilename to get the inventory CSV filename
   * sets atime and mtime on that file to 0
*/
static void InvalidateSoftwareCache(void)
{
    char name[CF_BUFSIZE];
    struct utimbuf epoch = { 0, 0 };

    GetSoftwareCacheFilename(name);

    if (utime(name, &epoch) != 0)
    {
        if (errno != ENOENT)
        {
            Log(LOG_LEVEL_ERR, "Cannot mark software cache as invalid. (utimes: %s)", GetErrorStr());
        }
    }
}

/**
   @brief Gets the cached list of installed packages from file

   Called by VerifyInstalledPackages

   * calls GetSoftwareCacheFilename to get the inventory CSV filename
   * respects a.packages.package_list_update_ifelapsed, returns NULL if file is too old
   * parses the CSV out of the file (name, version, arch, manager) with each limited to 250 chars
   * for each line
   * * if architecture is "default", replace it with default_arch
   * * if the package manager name matches, call PrependPackageItem

   @param ctx [in] The evaluation context
   @param manager [in] the PackageManager we want
   @param default_arch [in] the default architecture
   @param a [in] the promise Attributes for this operation
   @param pp [in] the Promise for this operation
   @returns list of PackageItems
*/
static PackageItem *GetCachedPackageList(EvalContext *ctx, PackageManager *manager, const char *default_arch, Attributes a,
                                         const Promise *pp)
{
    PackageItem *list = NULL;
    char name[CF_MAXVARSIZE], version[CF_MAXVARSIZE], arch[CF_MAXVARSIZE], mgr[CF_MAXVARSIZE], line[CF_BUFSIZE];
    char thismanager[CF_MAXVARSIZE];
    FILE *fin;
    time_t horizon = 24 * 60, now = time(NULL);
    struct stat sb;

    GetSoftwareCacheFilename(name);

    if (stat(name, &sb) == -1)
    {
        return NULL;
    }

    if (a.packages.package_list_update_ifelapsed != CF_NOINT)
    {
        horizon = a.packages.package_list_update_ifelapsed;
    }

    if (now - sb.st_mtime < horizon * 60)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Cache file '%s' exists and is sufficiently fresh according to (package_list_update_ifelapsed)", name);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Cache file '%s' exists, but it is out of date (package_list_update_ifelapsed)", name);
        return NULL;
    }

    if ((fin = fopen(name, "r")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Cannot open the source log '%s' - you need to run a package discovery promise to create it in cf-agent. (fopen: %s)",
              name, GetErrorStr());
        return NULL;
    }

/* Max 2016 entries - at least a week */

    snprintf(thismanager, CF_MAXVARSIZE - 1, "%s", ReadLastNode(RealPackageManager(manager->manager)));

    int linenumber = 0;
    for(;;)
    {
        if (fgets(line, sizeof(line), fin) == NULL)
        {
            if (ferror(fin))
            {
                UnexpectedError("Failed to read line %d from stream '%s'", linenumber+1, name);
                break;
            }
            else /* feof */
            {
                break;
            }
        }
        ++linenumber;
        int scancount = sscanf(line, "%250[^,],%250[^,],%250[^,],%250[^\r\n]", name, version, arch, mgr);
        if (scancount != 4)
        {
            Log(LOG_LEVEL_VERBOSE, "Could only read %d values from line %d in '%s'", scancount, linenumber, name);
        }

        /*
         * Transition to explicit default architecture, if package manager
         * supports it.
         *
         * If old cache contains entries with 'default' architecture, and
         * package method is updated to detect this architecture, on next
         * execution update this architecture to the real one.
         */
        if (!strcmp(arch, "default"))
        {
            strlcpy(arch, default_arch, CF_MAXVARSIZE);
        }

        if (strcmp(thismanager, mgr) == 0)
        {
            PrependPackageItem(ctx, &list, name, version, arch, pp);
        }
    }

    fclose(fin);
    return list;
}

/**
   @brief Verifies installed packages for a single Promise

   Called by VerifyPackagesPromise

   * from all_mgrs, gets the package manager matching a.packages.package_list_command
   * populate manager->pack_list with GetCachedPackageList
   * on Windows, use NovaWin_PackageListInstalledFromAPI if a.packages.package_list_command is set to PACKAGE_LIST_COMMAND_WINAPI
   * on other platforms, use PackageListInstalledFromCommand
   * call ReportSoftware to save the installed packages inventory
   * if a.packages.package_patch_list_command is set, use it and parse each line with a.packages.package_patch_installed_regex; if it matches, call PrependPatchItem
   * call ReportPatches to save the available updates inventory (Enterprise only)

   @param ctx [in] The evaluation context
   @param all_mgrs [in] a list of PackageManagers
   @param default_arch [in] the default architecture
   @param a [in] the promise Attributes for this operation
   @param pp [in] the Promise for this operation
   @param result [inout] the PromiseResult for this operation
   @returns boolean pass/fail of verification
*/
static int VerifyInstalledPackages(EvalContext *ctx, PackageManager **all_mgrs, const char *default_arch,
                                   Attributes a, const Promise *pp, PromiseResult *result)
{
    PackageManager *manager = GetPackageManager(all_mgrs, a.packages.package_list_command, PACKAGE_ACTION_NONE, PACKAGE_ACTION_POLICY_NONE);

    if (manager == NULL)
    {
        Log(LOG_LEVEL_ERR, "Can't create a package manager envelope for '%s'", a.packages.package_list_command);
        return false;
    }

    if (manager->pack_list != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Already have a package list for this manager");
        return true;
    }

    manager->pack_list = GetCachedPackageList(ctx, manager, default_arch, a, pp);

    if (manager->pack_list != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Already have a (cached) package list for this manager ");
        return true;
    }

    if (a.packages.package_list_command == NULL)
    {
        /* skip */
    }
#ifdef __MINGW32__
    else if (strcmp(a.packages.package_list_command, PACKAGE_LIST_COMMAND_WINAPI) == 0)
    {
        if (!NovaWin_PackageListInstalledFromAPI(ctx, &(manager->pack_list), a, pp))
        {
            Log(LOG_LEVEL_ERR, "Could not get list of installed packages");
            return false;
        }
    }
#endif /* !__MINGW32__ */
    else
    {
        if (!PackageListInstalledFromCommand(ctx, &(manager->pack_list), default_arch, a, pp, result))
        {
            Log(LOG_LEVEL_ERR, "Could not get list of installed packages");
            return false;
        }
    }

    ReportSoftware(INSTALLED_PACKAGE_LISTS);

/* Now get available updates */

    if (a.packages.package_patch_list_command != NULL)
    {
            Log(LOG_LEVEL_VERBOSE, "Reading patches from '%s'", CommandArg0(a.packages.package_patch_list_command));

        if ((!a.packages.package_commands_useshell) && (!IsExecutable(CommandArg0(a.packages.package_patch_list_command))))
        {
            Log(LOG_LEVEL_ERR, "The proposed patch list command '%s' was not executable",
                  a.packages.package_patch_list_command);
            return false;
        }

        FILE *fin;

        if (a.packages.package_commands_useshell)
        {
            if ((fin = cf_popen_sh(a.packages.package_patch_list_command, "r")) == NULL)
            {
                Log(LOG_LEVEL_ERR, "Couldn't open the patch list with command '%s'. (cf_popen_sh: %s)",
                      a.packages.package_patch_list_command, GetErrorStr());
                return false;
            }
        }
        else if ((fin = cf_popen(a.packages.package_patch_list_command, "r", true)) == NULL)
        {
            Log(LOG_LEVEL_ERR, "Couldn't open the patch list with command '%s'. (cf_popen: %s)",
                  a.packages.package_patch_list_command, GetErrorStr());
            return false;
        }

        size_t vbuff_size = CF_BUFSIZE;
        char *vbuff = xmalloc(vbuff_size);

        for (;;)
        {
            ssize_t res = CfReadLine(&vbuff, &vbuff_size, fin);
            if (res == -1)
            {
                if (!feof(fin))
                {
                    Log(LOG_LEVEL_ERR, "Unable to read list of patches from command '%s'. (fread: %s)",
                          a.packages.package_patch_list_command, GetErrorStr());
                    cf_pclose(fin);
                    free(vbuff);
                    return false;
                }
                else
                {
                    break;
                }
            }

            // assume patch_list_command lists available patches/updates by default
            if ((a.packages.package_patch_installed_regex == NULL)
                || (!FullTextMatch(ctx, a.packages.package_patch_installed_regex, vbuff)))
            {
                PrependPatchItem(ctx, &(manager->patch_avail), vbuff, manager->patch_list, default_arch, a, pp);
                continue;
            }

            if (!PrependPatchItem(ctx, &(manager->patch_list), vbuff, manager->patch_list, default_arch, a, pp))
            {
                continue;
            }
        }

        cf_pclose(fin);
        free(vbuff);
    }

    if (a.packages.package_patch_list_command != NULL)
    {
        ReportPatches(INSTALLED_PACKAGE_LISTS); // Enterprise only
    }

        Log(LOG_LEVEL_VERBOSE, "Done checking packages and patches");

    return true;
}


/** Evaluate what needs to be done **/

/**
   @brief Finds the largest version of a package available in a file repository

   Called by SchedulePackageOp

   * match = false
   * for each directory in repositories
   * * try to match refAnyVer against each file
   * * if it matches and CompareVersions says it's the biggest found so far, copy the matched version and name into matchName and matchVers and set match to true
   * return match

   @param ctx [in] The evaluation context
   @param matchName [inout] the matched package name (written on match)
   @param matchVers [inout] the matched package version (written on match)
   @param refAnyVer [in] the regex to match against the filename to extract a version
   @param ver [in] the version sought
   @param repositories [in] the list of directories (file repositories)
   @param a [in] the promise Attributes for this operation
   @param pp [in] the Promise for this operation
   @param result [inout] the PromiseResult for this operation
   @returns boolean pass/fail of search
*/
int FindLargestVersionAvail(EvalContext *ctx, char *matchName, char *matchVers, const char *refAnyVer, const char *ver,
                            Rlist *repositories, Attributes a, const Promise *pp, PromiseResult *result)
/* Returns true if a version gt/ge ver is found in local repos, false otherwise */
{
    int match = false;

    // match any version
    if (!ver[0] || strcmp(ver, "*") == 0)
    {
        matchVers[0] = '\0';
    }
    else
    {
        strlcpy(matchVers, ver, CF_MAXVARSIZE);
    }

    for (Rlist *rp = repositories; rp != NULL; rp = rp->next)
    {
        Dir *dirh = DirOpen(RlistScalarValue(rp));
        if (dirh == NULL)
        {
            Log(LOG_LEVEL_ERR, "Can't open local directory '%s'. (opendir: %s)",
                RlistScalarValue(rp), GetErrorStr());
            continue;
        }

        const struct dirent *dirp;
        while (NULL != (dirp = DirRead(dirh)))
        {
            if (FullTextMatch(ctx, refAnyVer, dirp->d_name))
            {
                char *matchVer = ExtractFirstReference(refAnyVer, dirp->d_name);

                // check if match is largest so far
                if (CompareVersions(ctx, matchVer, matchVers, a, pp, result) == VERCMP_MATCH)
                {
                    strlcpy(matchVers, matchVer, CF_MAXVARSIZE);
                    strlcpy(matchName, dirp->d_name, CF_MAXVARSIZE);
                    match = true;
                }
            }
        }

        DirClose(dirh);
    }

    Log(LOG_LEVEL_DEBUG, "FindLargestVersionAvail: largest version of '%s' is '%s' (match=%d)",
        matchName, matchVers, match);

    return match;
}

/**
   @brief Returns true if a package (n,v,a) is installed and v is larger than the installed version

   Called by SchedulePackageOp

   * for each known PackageManager, compare to attr.packages.package_list_command
   * bail out if no manager was found
   * for each PackageItem pi in the manager's package list
   * * if pi->name equals n and (a is "*" or a equals pi->arch)
   * * * record instV and instA
   * * * copy attr into attr2 and override the attr2.packages.package_select to PACKAGE_VERSION_COMPARATOR_LT
   * * * return CompareVersions of the new monster
   * return false if the above found no matches

   @param ctx [in] The evaluation context
   @param n [in] the specific name
   @param v [in] the specific version
   @param a [in] the specific architecture
   @param instV [inout] the matched package version (written on match)
   @param instA [inout] the matched package architecture (written on match)
   @param attr [in] the promise Attributes for this operation
   @param pp [in] the Promise for this operation
   @param result [inout] the PromiseResult for this operation
   @returns boolean if given (n,v,a) is newer than known packages
*/
static int IsNewerThanInstalled(EvalContext *ctx, const char *n, const char *v, const char *a, char *instV, char *instA, Attributes attr,
                                const Promise *pp, PromiseResult *result)
{
    PackageManager *mp = INSTALLED_PACKAGE_LISTS;
    while (mp != NULL)
    {
        if (strcmp(mp->manager, attr.packages.package_list_command) == 0)
        {
            break;
        }
        mp = mp->next;
    }

    if (NULL == mp)
    {
        Log(LOG_LEVEL_VERBOSE, "Found no package manager matching attr.packages.package_list_command '%s'",
            attr.packages.package_list_command == NULL ? "[empty]" : attr.packages.package_list_command);
        return false;
    }

    Log(LOG_LEVEL_VERBOSE, "Looking for an installed package older than (%s,%s,%s) [name,version,arch]", n, v, a);

    for (PackageItem *pi = mp->pack_list; pi != NULL; pi = pi->next)
    {
        if (strcmp(n, pi->name) == 0 &&
            (strcmp(a, "*") == 0 || strcmp(a, pi->arch) == 0))
        {
            Log(LOG_LEVEL_VERBOSE,
                "Found installed package (%s,%s,%s) [name,version,arch]",
                pi->name, pi->version, pi->arch);

            strlcpy(instV, pi->version, CF_MAXVARSIZE);
            strlcpy(instA, pi->arch, CF_MAXVARSIZE);

            /* Horrible */
            Attributes attr2 = attr;
            attr2.packages.package_select = PACKAGE_VERSION_COMPARATOR_LT;

            return CompareVersions(ctx, pi->version, v, attr2, pp, result) == VERCMP_MATCH;
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Package (%s,%s) [name,arch] is not installed", n, a);
    return false;
}

/**
   @brief Returns string version of a PackageAction

   @param pa [in] The PackageAction
   @returns string representation of pa or a ProgrammingError
*/
static const char *PackageAction2String(PackageAction pa)
{
    switch (pa)
    {
    case PACKAGE_ACTION_ADD:
        return "installing";
    case PACKAGE_ACTION_DELETE:
        return "uninstalling";
    case PACKAGE_ACTION_REINSTALL:
        return "reinstalling";
    case PACKAGE_ACTION_UPDATE:
        return "updating";
    case PACKAGE_ACTION_ADDUPDATE:
        return "installing/updating";
    case PACKAGE_ACTION_PATCH:
        return "patching";
    case PACKAGE_ACTION_VERIFY:
        return "verifying";
    default:
        ProgrammingError("CFEngine: internal error: illegal package action");
    }
}

/**
   @brief Adds a specific package (name,version,arch) as specified by Attributes a to the scheduled operations

   Called by SchedulePackageOp.

   Either warn or fix, based on a->transaction.action.

   To fix, calls GetPackageManager and enqueues the desired operation and package with the returned manager

   @param ctx [in] The evaluation context
   @param a [in] the Attributes specifying how to compare
   @param mgr [in] the specific manager name
   @param pa [in] the PackageAction to enqueue
   @param name [in] the specific name
   @param version [in] the specific version
   @param arch [in] the specific architecture
   @param pp [in] the Promise for this operation
   @returns the promise result
*/
static PromiseResult AddPackageToSchedule(EvalContext *ctx, const Attributes *a, char *mgr, PackageAction pa,
                                          const char *name, const char *version, const char *arch,
                                          const Promise *pp)
{
    switch (a->transaction.action)
    {
    case cfa_warn:

        cfPS_HELPER_3ARG(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, *a, "Need to repair promise '%s' by '%s' package '%s'",
             pp->promiser, PackageAction2String(pa), name);
        return PROMISE_RESULT_WARN;

    case cfa_fix:
    {
        PackageManager *manager = GetPackageManager(&PACKAGE_SCHEDULE, mgr, pa, a->packages.package_changes);

        if (NULL == manager)
        {
            ProgrammingError("AddPackageToSchedule: Null package manager found!!!");
        }

        PrependPackageItem(ctx, &(manager->pack_list), name, version, arch, pp);
        return PROMISE_RESULT_CHANGE;
    }
    default:
        ProgrammingError("CFEngine: internal error: illegal file action");
    }
}

/**
   @brief Adds a specific patch (name,version,arch) as specified by Attributes a to the scheduled operations

   Called by SchedulePackageOp.

   Either warn or fix, based on a->transaction.action.

   To fix, calls GetPackageManager and enqueues the desired operation and package with the returned manager

   @param ctx [in] The evaluation context
   @param a [in] the Attributes specifying how to compare
   @param mgr [in] the specific manager name
   @param pa [in] the PackageAction to enqueue
   @param name [in] the specific name
   @param version [in] the specific version
   @param arch [in] the specific architecture
   @param pp [in] the Promise for this operation
   @returns the promise result
*/
static PromiseResult AddPatchToSchedule(EvalContext *ctx, const Attributes *a, char *mgr, PackageAction pa,
                                        const char *name, const char *version, const char *arch,
                                        const Promise *pp)
{
    switch (a->transaction.action)
    {
    case cfa_warn:

        cfPS_HELPER_3ARG(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, *a, "Need to repair promise '%s' by '%s' package '%s'",
             pp->promiser, PackageAction2String(pa), name);
        return PROMISE_RESULT_WARN;

    case cfa_fix:
    {
        PackageManager *manager = GetPackageManager(&PACKAGE_SCHEDULE, mgr, pa, a->packages.package_changes);

        if (NULL == manager)
        {
            ProgrammingError("AddPatchToSchedule: Null package manager found!!!");
        }

        PrependPackageItem(ctx, &(manager->patch_list), name, version, arch, pp);
        return PROMISE_RESULT_CHANGE;
    }
    default:
        ProgrammingError("Illegal file action");
    }
}

/**
   @brief Schedules a package operation based on the action, package state, and everything else.

   Called by VerifyPromisedPatch and CheckPackageState.

   This function has a complexity metric of 3 Googols.

   * if package_delete_convention or package_name_convention are given and apply to the operation, construct the package name from them (from PACKAGES_CONTEXT)
   * else, just use the given package name
   * warn about "*" in the package name
   * set package_select_in_range with magic
   * create PackageAction policy from the package_policy and then split ADDUPDATE into ADD or UPDATE based on "installed"
   * result starts as NOOP
   * switch(policy)

   * * case ADD and "installed":
   * * * if we have package_file_repositories
   * * * * use the package_name_convention to build the package name (from PACKAGES_CONTEXT_ANYVER, setting version to "*")
   * * * * if FindLargestVersionAvail finds the latest package version in the file repos, use that as the package name
   * * * AddPackageToSchedule package_add_command, ADD, package name, etc.

   * * case DELETE and (matched AND package_select_in_range) OR (installed AND no_version_specified):
   * * * fail promise unless package_delete_command
   * * * if we have package_file_repositories
   * * * * clean up the name string from any "repo" references and add the right file repo
   * * * AddPackageToSchedule package_delete_command, DELETE, package name, etc.

   * * case REINSTALL:
   * * * fail promise unless package_delete_command
   * * * fail promise if no_version_specified
   * * * if (matched AND package_select_in_range) OR (installed AND no_version_specified) do AddPackageToSchedule package_delete_command, DELETE, package name, etc.
   * * * AddPackageToSchedule package_add_command, ADD, package name, etc.

   * * case UPDATE:
   * * * if we have package_file_repositories
   * * * * use the package_name_convention to build the package name (from PACKAGES_CONTEXT_ANYVER, setting version to "*")
   * * * * if FindLargestVersionAvail finds the latest package version in the file repos, use that as the package name
   * * * if installed, IsNewerThanInstalled is checked, and if it returns false we don't update an up-to-date package
   * * * if installed or (matched AND package_select_in_range AND !no_version_specified) (this is the main update condition)
   * * * * if package_update_command is not given
   * * * * * if package_delete_convention is given, use it to build id_del (from PACKAGES_CONTEXT)
   * * * * * fail promise if package_update_command and package_add_command are not given
   * * * * * AddPackageToSchedule with package_delete_command, DELETE, id_del, etc
   * * * * * AddPackageToSchedule with package_add_command, ADD, package name, etc
   * * * * else we have package_update_command, so AddPackageToSchedule with package_update_command, UPDATE, package name, etc
   * * * else the package is not updateable: no match or not installed, fail promise

   * * case PATCH:
   * * * if matched and not installed, AddPatchToSchedule with package_patch_command, PATCH, package name, etc.

   * * case VERIFY:
   * * * if (matched and package_select_in_range) OR (installed AND no_version_specified), AddPatchToSchedule with package_verify_command, VERIFY, package name, etc.

   @param ctx [in] The evaluation context
   @param name [in] the specific name
   @param version [in] the specific version
   @param arch [in] the specific architecture
   @param installed [in] is the package installed?
   @param matched [in] is the package matched in the available list?
   @param no_version_specified [in] no version was specified in the promise
   @param a [in] the Attributes specifying how to compare
   @param pp [in] the Promise for this operation
   @returns the promise result
*/
static PromiseResult SchedulePackageOp(EvalContext *ctx, const char *name, const char *version, const char *arch, int installed, int matched,
                                       int no_version_specified, Attributes a, const Promise *pp)
{
    char refAnyVerEsc[CF_EXPANDSIZE];
    char largestVerAvail[CF_MAXVARSIZE];
    char largestPackAvail[CF_MAXVARSIZE];
    char id[CF_EXPANDSIZE];

    Log(LOG_LEVEL_VERBOSE,
        "Checking if package (%s,%s,%s) [name,version,arch] "
        "is at the desired state (installed=%d,matched=%d)",
        name, version, arch, installed, matched);

/* Now we need to know the name-convention expected by the package manager */

    Buffer *expanded = BufferNew();
    if ((a.packages.package_name_convention) || (a.packages.package_delete_convention))
    {
        VarRef *ref_name = VarRefParseFromScope("name", PACKAGES_CONTEXT);
        EvalContextVariablePut(ctx, ref_name, name, CF_DATA_TYPE_STRING, "source=promise");

        VarRef *ref_version = VarRefParseFromScope("version", PACKAGES_CONTEXT);
        EvalContextVariablePut(ctx, ref_version, version, CF_DATA_TYPE_STRING, "source=promise");

        VarRef *ref_arch = VarRefParseFromScope("arch", PACKAGES_CONTEXT);
        EvalContextVariablePut(ctx, ref_arch, arch, CF_DATA_TYPE_STRING, "source=promise");

        if ((a.packages.package_delete_convention) && (a.packages.package_policy == PACKAGE_ACTION_DELETE))
        {
            ExpandScalar(ctx, NULL, PACKAGES_CONTEXT, a.packages.package_delete_convention, expanded);
            strlcpy(id, BufferData(expanded), CF_EXPANDSIZE);
        }
        else if (a.packages.package_name_convention)
        {
            ExpandScalar(ctx, NULL, PACKAGES_CONTEXT, a.packages.package_name_convention, expanded);
            strlcpy(id, BufferData(expanded), CF_EXPANDSIZE);
        }
        else
        {
            strlcpy(id, name, CF_EXPANDSIZE);
        }

        EvalContextVariableRemove(ctx, ref_name);
        VarRefDestroy(ref_name);

        EvalContextVariableRemove(ctx, ref_version);
        VarRefDestroy(ref_version);

        EvalContextVariableRemove(ctx, ref_arch);
        VarRefDestroy(ref_arch);
    }
    else
    {
        strlcpy(id, name, CF_EXPANDSIZE);
    }

    Log(LOG_LEVEL_VERBOSE, "Package promises to refer to itself as '%s' to the manager", id);

    if (strchr(id, '*'))
    {
        Log(LOG_LEVEL_VERBOSE, "Package name contains '*' -- perhaps "
            "a missing attribute (name/version/arch) should be specified");
    }

    // This is very confusing
    int package_select_in_range;
    switch (a.packages.package_select)
    {
    case PACKAGE_VERSION_COMPARATOR_EQ:
    case PACKAGE_VERSION_COMPARATOR_GE:
    case PACKAGE_VERSION_COMPARATOR_LE:
    case PACKAGE_VERSION_COMPARATOR_NONE:
        Log(LOG_LEVEL_VERBOSE, "Package version seems to match criteria");
        package_select_in_range = true;
        break;

    default:
        package_select_in_range = false;
        break;
    }

    PackageAction policy = a.packages.package_policy;
    if (policy == PACKAGE_ACTION_ADDUPDATE) /* Work out which: */
    {
        if (installed)
        {
            policy = PACKAGE_ACTION_UPDATE;
        }
        else
        {
            policy = PACKAGE_ACTION_ADD;
        }
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    switch (policy)
    {
    case PACKAGE_ACTION_ADD:

        if (installed == 0)
        {
            if ((a.packages.package_file_repositories != NULL))
            {
                Log(LOG_LEVEL_VERBOSE, "Package method specifies a file repository");

                {
                    VarRef *ref_name = VarRefParseFromScope("name", PACKAGES_CONTEXT_ANYVER);
                    EvalContextVariablePut(ctx, ref_name, name, CF_DATA_TYPE_STRING, "source=promise");

                    VarRef *ref_version = VarRefParseFromScope("version", PACKAGES_CONTEXT_ANYVER);
                    EvalContextVariablePut(ctx, ref_version, "(.*)", CF_DATA_TYPE_STRING, "source=promise");

                    VarRef *ref_arch = VarRefParseFromScope("arch", PACKAGES_CONTEXT_ANYVER);
                    EvalContextVariablePut(ctx, ref_arch, arch, CF_DATA_TYPE_STRING, "source=promise");

                    BufferClear(expanded);
                    if (a.packages.package_name_convention)
                    {
                        ExpandScalar(ctx, NULL, PACKAGES_CONTEXT_ANYVER, a.packages.package_name_convention, expanded);
                    }

                    EvalContextVariableRemove(ctx, ref_name);
                    VarRefDestroy(ref_name);

                    EvalContextVariableRemove(ctx, ref_version);
                    VarRefDestroy(ref_version);

                    EvalContextVariableRemove(ctx, ref_arch);
                    VarRefDestroy(ref_arch);
                }

                EscapeSpecialChars(BufferData(expanded), refAnyVerEsc, sizeof(refAnyVerEsc), "(.*)","");

                if (FindLargestVersionAvail(ctx, largestPackAvail, largestVerAvail, refAnyVerEsc, version,
                                            a.packages.package_file_repositories, a, pp, &result))
                {
                    Log(LOG_LEVEL_VERBOSE, "Using latest version in file repositories; '%s'", largestPackAvail);
                    strlcpy(id, largestPackAvail, CF_EXPANDSIZE);
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE, "No package in file repositories satisfy version constraint");
                    break;
                }
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Package method does NOT specify a file repository");
            }

            Log(LOG_LEVEL_VERBOSE, "Schedule package for addition");

            if (a.packages.package_add_command == NULL)
            {
                cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Package add command undefined");
                BufferDestroy(expanded);
                return PROMISE_RESULT_FAIL;
            }
            result = PromiseResultUpdate_HELPER(pp, result,
                                         AddPackageToSchedule(ctx, &a, a.packages.package_add_command,
                                                              PACKAGE_ACTION_ADD, id, "any", "any", pp));
        }
        else
        {
            cfPS_HELPER_1ARG(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Package '%s' already installed, so we never add it again",
                 pp->promiser);
        }
        break;

    case PACKAGE_ACTION_DELETE:

        // we're deleting a matched package found in a range OR an installed package with no version
        if ((matched && package_select_in_range) ||
            (installed && no_version_specified))
        {
            Log(LOG_LEVEL_VERBOSE, "Schedule package for deletion");

            if (a.packages.package_delete_command == NULL)
            {
                cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Package delete command undefined");
                BufferDestroy(expanded);
                return PROMISE_RESULT_FAIL;
            }
            // expand local repository in the name convention, if present
            if (a.packages.package_file_repositories)
            {
                Log(LOG_LEVEL_VERBOSE, "Package method specifies a file repository");

                // remove any "$(repo)" from the name convention string

                if (strncmp(id, "$(firstrepo)", 12) == 0)
                {
                    const char *idBuf = id + 12;

                    // and add the correct repo
                    const char *pathName = PrefixLocalRepository(a.packages.package_file_repositories, idBuf);

                    if (pathName)
                    {
                        strlcpy(id, pathName, CF_EXPANDSIZE);
                        Log(LOG_LEVEL_VERBOSE,
                            "Expanded the package repository to '%s'", id);
                    }
                    else
                    {
                        Log(LOG_LEVEL_ERR, "Package '%s' can't be found "
                            "in any of the listed repositories", idBuf);
                    }
                }
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Package method does NOT specify a file repository");
            }

            result = PromiseResultUpdate_HELPER(pp, result,
                                         AddPackageToSchedule(ctx, &a, a.packages.package_delete_command,
                                                              PACKAGE_ACTION_DELETE, id, "any", "any", pp));
        }
        else
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Package deletion is as promised -- no match");
        }
        break;

    case PACKAGE_ACTION_REINSTALL:
        if (a.packages.package_delete_command == NULL)
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Package delete command undefined");
            BufferDestroy(expanded);
            return PROMISE_RESULT_FAIL;
        }

        if (!no_version_specified)
        {
            Log(LOG_LEVEL_VERBOSE, "Schedule package for reinstallation");
            if (a.packages.package_add_command == NULL)
            {
                cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Package add command undefined");
                BufferDestroy(expanded);
                return PROMISE_RESULT_FAIL;
            }

            // we're deleting a matched package found in a range OR an installed package with no version
            if ((matched && package_select_in_range) ||
                (installed && no_version_specified))
            {
                result = PromiseResultUpdate_HELPER(pp, result,
                                             AddPackageToSchedule(ctx, &a, a.packages.package_delete_command,
                                                                  PACKAGE_ACTION_DELETE, id, "any", "any", pp));
            }

            result = PromiseResultUpdate_HELPER(pp, result,
                                         AddPackageToSchedule(ctx, &a, a.packages.package_add_command,
                                                              PACKAGE_ACTION_ADD, id, "any", "any", pp));
        }
        else
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "Package reinstallation cannot be promised -- insufficient version info or no match");
            BufferDestroy(expanded);
            return PROMISE_RESULT_FAIL;
        }
        break;

    case PACKAGE_ACTION_UPDATE:
    {
        char inst_arch[CF_MAXVARSIZE];
        char inst_ver[CF_MAXVARSIZE];
        *inst_ver = '\0';
        *inst_arch = '\0';

        if ((a.packages.package_file_repositories != NULL))
        {
            Log(LOG_LEVEL_VERBOSE, "Package method specifies a file repository");

            {
                VarRef *ref_name = VarRefParseFromScope("name", PACKAGES_CONTEXT_ANYVER);
                EvalContextVariablePut(ctx, ref_name, name, CF_DATA_TYPE_STRING, "source=promise");

                VarRef *ref_version = VarRefParseFromScope("version", PACKAGES_CONTEXT_ANYVER);
                EvalContextVariablePut(ctx, ref_version, "(.*)", CF_DATA_TYPE_STRING, "source=promise");

                VarRef *ref_arch = VarRefParseFromScope("arch", PACKAGES_CONTEXT_ANYVER);
                EvalContextVariablePut(ctx, ref_arch, arch, CF_DATA_TYPE_STRING, "source=promise");

                BufferClear(expanded);
                ExpandScalar(ctx, NULL, PACKAGES_CONTEXT_ANYVER, a.packages.package_name_convention, expanded);

                EvalContextVariableRemove(ctx, ref_name);
                VarRefDestroy(ref_name);

                EvalContextVariableRemove(ctx, ref_version);
                VarRefDestroy(ref_version);

                EvalContextVariableRemove(ctx, ref_arch);
                VarRefDestroy(ref_arch);
            }


            EscapeSpecialChars(BufferData(expanded), refAnyVerEsc, sizeof(refAnyVerEsc), "(.*)","");

            if (FindLargestVersionAvail(ctx, largestPackAvail, largestVerAvail, refAnyVerEsc, version,
                                        a.packages.package_file_repositories, a, pp, &result))
            {
                Log(LOG_LEVEL_VERBOSE, "Using latest version in file repositories; '%s'", largestPackAvail);
                strlcpy(id, largestPackAvail, CF_EXPANDSIZE);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "No package in file repositories satisfy version constraint");
                break;
            }
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Package method does NOT specify a file repository");
            strlcpy(largestVerAvail, version, sizeof(largestVerAvail));  // user-supplied version
        }

        if (installed)
        {
            Log(LOG_LEVEL_VERBOSE, "Checking if latest available version is newer than installed...");
            if (IsNewerThanInstalled(ctx, name, largestVerAvail, arch, inst_ver, inst_arch, a, pp, &result))
            {
                Log(LOG_LEVEL_VERBOSE,
                      "Installed package (%s,%s,%s) [name,version,arch] is older than latest available (%s,%s,%s) [name,version,arch] - updating", name,
                      inst_ver, inst_arch, name, largestVerAvail, arch);
            }
            else
            {
                cfPS_HELPER_1ARG(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
                    "Installed packaged '%s' is up to date, not updating", pp->promiser);
                break;
            }
        }

        if (installed ||
            (matched && package_select_in_range && !no_version_specified))
        {
            if (a.packages.package_update_command == NULL)
            {
                Log(LOG_LEVEL_VERBOSE, "Package update command undefined - failing over to delete then add");

                // we need to have the version of installed package
                const char *id_del = id;
                if (a.packages.package_delete_convention)
                {
                    if (*inst_ver == '\0')
                    {
                        inst_ver[0] = '*';
                        inst_ver[1] = '\0';
                    }

                    if (*inst_arch == '\0')
                    {
                        inst_arch[0] = '*';
                        inst_arch[1] = '\0';
                    }

                    VarRef *ref_name = VarRefParseFromScope("name", PACKAGES_CONTEXT);
                    EvalContextVariablePut(ctx, ref_name, name, CF_DATA_TYPE_STRING, "source=promise");

                    VarRef *ref_version = VarRefParseFromScope("version", PACKAGES_CONTEXT);
                    EvalContextVariablePut(ctx, ref_version, inst_ver, CF_DATA_TYPE_STRING, "source=promise");

                    VarRef *ref_arch = VarRefParseFromScope("arch", PACKAGES_CONTEXT);
                    EvalContextVariablePut(ctx, ref_arch, inst_arch, CF_DATA_TYPE_STRING, "source=promise");

                    BufferClear(expanded);
                    ExpandScalar(ctx, NULL, PACKAGES_CONTEXT, a.packages.package_delete_convention, expanded);
                    id_del = BufferData(expanded);

                    EvalContextVariableRemove(ctx, ref_name);
                    VarRefDestroy(ref_name);

                    EvalContextVariableRemove(ctx, ref_version);
                    VarRefDestroy(ref_version);

                    EvalContextVariableRemove(ctx, ref_arch);
                    VarRefDestroy(ref_arch);
                }

                Log(LOG_LEVEL_VERBOSE, "Scheduling package with id '%s' for deletion", id_del);

                if (a.packages.package_add_command == NULL)
                {
                    cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Package add command undefined");
                    BufferDestroy(expanded);
                    return PROMISE_RESULT_FAIL;
                }
                if (a.packages.package_delete_command == NULL)
                {
                    cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Package delete command undefined");
                    BufferDestroy(expanded);
                    return PROMISE_RESULT_FAIL;
                }
                result = PromiseResultUpdate_HELPER(pp, result,
                                             AddPackageToSchedule(ctx, &a, a.packages.package_delete_command,
                                                                  PACKAGE_ACTION_DELETE, id_del, "any", "any", pp));

                result = PromiseResultUpdate_HELPER(pp, result,
                                             AddPackageToSchedule(ctx, &a, a.packages.package_add_command,
                                                                  PACKAGE_ACTION_ADD, id, "any", "any", pp));
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Schedule package for update");
                result = PromiseResultUpdate_HELPER(pp, result,
                                             AddPackageToSchedule(ctx, &a, a.packages.package_update_command,
                                                                  PACKAGE_ACTION_UPDATE, id, "any", "any", pp));
            }
        }
        else
        {
            cfPS_HELPER_1ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Package '%s' cannot be updated -- no match or not installed",
                 pp->promiser);
            result = PromiseResultUpdate_HELPER(pp, result, PROMISE_RESULT_FAIL);
        }
        break;
    }
    case PACKAGE_ACTION_PATCH:

        if (matched && (!installed))
        {
            Log(LOG_LEVEL_VERBOSE, "Schedule package for patching");
            result = PromiseResultUpdate_HELPER(pp, result,
                                         AddPatchToSchedule(ctx, &a, a.packages.package_patch_command,
                                                            PACKAGE_ACTION_PATCH, id, "any", "any", pp));
        }
        else
        {
            cfPS_HELPER_1ARG(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
                 "Package patch state of '%s' is as promised -- already installed", pp->promiser);
        }
        break;

    case PACKAGE_ACTION_VERIFY:

        if ((matched && package_select_in_range) ||
            (installed && no_version_specified))
        {
            Log(LOG_LEVEL_VERBOSE, "Schedule package for verification");
            result = PromiseResultUpdate_HELPER(pp, result,
                                         AddPackageToSchedule(ctx, &a, a.packages.package_verify_command,
                                                              PACKAGE_ACTION_VERIFY, id, "any", "any", pp));
        }
        else
        {
            cfPS_HELPER_1ARG(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a, "Package '%s' cannot be verified -- no match", pp->promiser);
            BufferDestroy(expanded);
            return PROMISE_RESULT_FAIL;
        }

        break;

    default:
        break;
    }

    BufferDestroy(expanded);
    return result;
}

/**
   @brief Compare a PackageItem to a specific package (n,v,arch) as specified by Attributes a

   Called by PatchMatch and PackageMatch.

   First, checks the package names are the same (according to CompareCSVName).
   Second, checks the architectures are the same or arch is "*"
   Third, checks the versions with CompareVersions or version is "*"

   @param ctx [in] The evaluation context
   @param n [in] the specific name
   @param v [in] the specific version
   @param arch [in] the specific architecture
   @param pi [in] the PackageItem to check
   @param a [in] the Attributes specifying how to compare
   @param pp [in] the Promise for this operation
   @param mode [in] the operating mode, informational for logging
   @returns the version comparison result
*/
VersionCmpResult ComparePackages(EvalContext *ctx,
                                 const char *n, const char *v, const char *arch,
                                 PackageItem *pi, Attributes a,
                                 const Promise *pp,
                                 const char *mode,
                                 PromiseResult *result)
{
    Log(LOG_LEVEL_VERBOSE, "Comparing %s package (%s,%s,%s) "
        "to [%s] with given (%s,%s,%s) [name,version,arch]",
        mode, pi->name, pi->version, pi->arch, PackageVersionComparatorToString(a.packages.package_select), n, v, arch);

    if (CompareCSVName(n, pi->name) != 0)
    {
        return VERCMP_NO_MATCH;
    }

    Log(LOG_LEVEL_VERBOSE, "Matched %s name '%s'", mode, n);

    if (strcmp(arch, "*") != 0)
    {
        if (strcmp(arch, pi->arch) != 0)
        {
            return VERCMP_NO_MATCH;
        }

        Log(LOG_LEVEL_VERBOSE, "Matched %s arch '%s'", mode, arch);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Matched %s wildcard arch '%s'", mode, arch);
    }

    if (strcmp(v, "*") == 0)
    {
        Log(LOG_LEVEL_VERBOSE, "Matched %s wildcard version '%s'", mode, v);
        return VERCMP_MATCH;
    }

    VersionCmpResult vc = CompareVersions(ctx, pi->version, v, a, pp, result);
    Log(LOG_LEVEL_VERBOSE,
        "Version comparison returned %s for %s package (%s,%s,%s) "
        "to [%s] with given (%s,%s,%s) [name,version,arch]",
        vc == VERCMP_MATCH ? "MATCH" : vc == VERCMP_NO_MATCH ? "NO_MATCH" : "ERROR",
        mode,
        pi->name, pi->version, pi->arch,
        PackageVersionComparatorToString(a.packages.package_select),
        n, v, arch);

    return vc;

}

/**
   @brief Finds a specific package (n,v,a) [name, version, architecture] as specified by Attributes attr

   Called by VerifyPromisedPatch.

   Goes through all the installed packages to find matches for the given attributes.

   The package manager is checked against attr.packages.package_list_command.

   The package name is checked as a regular expression. then (n,v,a) with ComparePackages.

   @param ctx [in] The evaluation context
   @param n [in] the specific name
   @param v [in] the specific version
   @param a [in] the specific architecture
   @param attr [in] the Attributes specifying how to compare
   @param pp [in] the Promise for this operation
   @param mode [in] the operating mode, informational for logging
   @returns the version comparison result
*/
static VersionCmpResult PatchMatch(EvalContext *ctx,
                                   const char *n, const char *v, const char *a,
                                   Attributes attr, const Promise *pp,
                                   const char* mode,
                                   PromiseResult *result)
{
    PackageManager *mp;

    // This REALLY needs some commenting
    for (mp = INSTALLED_PACKAGE_LISTS; mp != NULL; mp = mp->next)
    {
        if (strcmp(mp->manager, attr.packages.package_list_command) == 0)
        {
            break;
        }
    }

    Log(LOG_LEVEL_VERBOSE, "PatchMatch: looking for %s to [%s] with given (%s,%s,%s) [name,version,arch] in package manager %s",
        mode, PackageVersionComparatorToString(attr.packages.package_select), n, v, a, mp->manager);

    for (PackageItem *pi = mp->patch_list; pi != NULL; pi = pi->next)
    {
        if (FullTextMatch(ctx, n, pi->name)) /* Check regexes */
        {
            Log(LOG_LEVEL_VERBOSE, "PatchMatch: regular expression match succeeded for %s against %s", n, pi->name);
            return VERCMP_MATCH;
        }
        else
        {
            VersionCmpResult res = ComparePackages(ctx, n, v, a, pi, attr, pp, mode, result);
            if (res != VERCMP_NO_MATCH)
            {
                Log(LOG_LEVEL_VERBOSE, "PatchMatch: patch comparison for %s was decisive: %s", pi->name, res == VERCMP_MATCH ? "MATCH" : "ERROR");
                return res;
            }
        }
    }

    Log(LOG_LEVEL_VERBOSE, "PatchMatch did not match the constraints of promise (%s,%s,%s) [name,version,arch]", n, v, a);
    return VERCMP_NO_MATCH;
}

/**
   @brief Finds a specific package (n,v,a) [name, version, architecture] as specified by Attributes attr

   Called by CheckPackageState.

   Goes through all the installed packages to find matches for the given attributes.

   The package manager is checked against attr.packages.package_list_command.

   The (n,v,a) search is done with ComparePackages.

   @param ctx [in] The evaluation context
   @param n [in] the specific name
   @param v [in] the specific version
   @param a [in] the specific architecture
   @param attr [in] the Attributes specifying how to compare
   @param pp [in] the Promise for this operation
   @param mode [in] the operating mode, informational for logging
   @returns the version comparison result
*/
static VersionCmpResult PackageMatch(EvalContext *ctx,
                                     const char *n, const char *v, const char *a,
                                     Attributes attr,
                                     const Promise *pp,
                                     const char* mode,
                                     PromiseResult *result)
/*
 * Returns VERCMP_MATCH if any installed packages match (n,v,a), VERCMP_NO_MATCH otherwise, VERCMP_ERROR on error.
 * The mode is informational
 */
{
    PackageManager *mp = NULL;

    // This REALLY needs some commenting
    for (mp = INSTALLED_PACKAGE_LISTS; mp != NULL; mp = mp->next)
    {
        if (strcmp(mp->manager, attr.packages.package_list_command) == 0)
        {
            break;
        }
    }

    Log(LOG_LEVEL_VERBOSE, "PackageMatch: looking for %s (%s,%s,%s) [name,version,arch] in package manager %s", mode, n, v, a, mp->manager);

    for (PackageItem *pi = mp->pack_list; pi != NULL; pi = pi->next)
    {
        VersionCmpResult res = ComparePackages(ctx, n, v, a, pi, attr, pp, mode, result);

        if (res != VERCMP_NO_MATCH)
        {
            Log(LOG_LEVEL_VERBOSE, "PackageMatch: package comparison for %s %s was decisive: %s", mode, pi->name, res == VERCMP_MATCH ? "MATCH" : "ERROR");
            return res;
        }
    }

    Log(LOG_LEVEL_VERBOSE, "PackageMatch did not find %s packages to match the constraints of promise (%s,%s,%s) [name,version,arch]", mode, n, v, a);
    return VERCMP_NO_MATCH;
}

/**
   @brief Check if the operation should be scheduled based on the package policy, if the package matches, and if it's installed

   Called by CheckPackageState.

   Uses a.packages.package_policy to determine operating mode.

   The use of matches and installed depends on the package_policy:
   * PACKAGE_ACTION_DELETE: schedule if (matches AND installed)
   * PACKAGE_ACTION_REINSTALL: schedule if (matches AND installed)
   * all other policies: schedule if (not matches OR not installed)

   @param ctx [in] The evaluation context
   @param a [in] the Attributes specifying the package policy
   @param pp [in] the Promise for this operation
   @param matches [in] whether the package matches
   @param installed [in] whether the package is installed
   @returns whether the package operation should be scheduled
*/
static int WillSchedulePackageOperation(EvalContext *ctx, Attributes a, const Promise *pp, int matches, int installed)
{
    PackageAction policy = a.packages.package_policy;

    Log(LOG_LEVEL_DEBUG, "WillSchedulePackageOperation: on entry, action %s: package %s matches = %s, installed = %s.",
        PackageAction2String(policy), pp->promiser, matches ? "yes" : "no", installed ? "yes" : "no");

    switch (policy)
    {
    case PACKAGE_ACTION_DELETE:
        if (matches && installed)
        {
            Log(LOG_LEVEL_VERBOSE, "WillSchedulePackageOperation: Package %s to be deleted is installed.", pp->promiser);
            return true;
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "WillSchedulePackageOperation: Package %s can't be deleted if it's not installed, NOOP.", pp->promiser);
            cfPS_HELPER_1ARG(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Package %s to be deleted does not exist anywhere",
                 pp->promiser);
        }
        break;

    case PACKAGE_ACTION_REINSTALL:
        if (matches && installed)
        {
            Log(LOG_LEVEL_VERBOSE, "WillSchedulePackageOperation: Package %s to be reinstalled is already installed.", pp->promiser);
            return true;
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "WillSchedulePackageOperation: Package %s already installed, NOOP.", pp->promiser);
            cfPS_HELPER_1ARG(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Package '%s' already installed and matches criteria",
                 pp->promiser);
        }
        break;

    default:
        if (!matches) // why do we schedule a 'not matched' operation?
        {
            return true;
        }
        else if (!installed) // matches and not installed
        {
            return true;
        }
        else // matches and installed
        {
            Log(LOG_LEVEL_DEBUG, "WillSchedulePackageOperation: Package %s already installed, NOOP.", pp->promiser);
            cfPS_HELPER_1ARG(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Package '%s' already installed and matches criteria",
                 pp->promiser);
        }
        break;
    }

    return false;
}

/**
   @brief Checks the state of a specific package (name,version,arch) as specified by Attributes a

   Called by VerifyPromisedPackage.

   * copies a into a2, overrides a2.packages.package_select to PACKAGE_VERSION_COMPARATOR_EQ
   * VersionCmpResult installed = check if (name,*,arch) is installed with PackageMatch (note version override!)
   * if PackageMatch returned an error, fail the promise
   * VersionCmpResult matches = check if (name,version,arch) is installed with PackageMatch
   * if PackageMatch returned an error, fail the promise
   * if WillSchedulePackageOperation with "matches" and "installed" passes, call SchedulePackageOp on the package

   @param ctx [in] The evaluation context
   @param a [in] the Attributes specifying how to compare
   @param pp [in] the Promise for this operation
   @param name [in] the specific name
   @param version [in] the specific version
   @param arch [in] the specific architecture
   @param no_version [in] ignore the version, be cool
   @returns the promise result
*/
static PromiseResult CheckPackageState(EvalContext *ctx, Attributes a, const Promise *pp, const char *name, const char *version,
                                       const char *arch, bool no_version)
{
    PromiseResult result = PROMISE_RESULT_NOOP;

    /* Horrible */
    Attributes a2 = a;
    a2.packages.package_select = PACKAGE_VERSION_COMPARATOR_EQ;

    VersionCmpResult installed = PackageMatch(ctx, name, "*", arch, a2, pp, "[installed]", &result);
    Log(LOG_LEVEL_VERBOSE, "CheckPackageState: Installed package match for (%s,%s,%s) [name,version,arch] was decisive: %s",
        name, "*", arch, installed == VERCMP_MATCH ? "MATCH" : "ERROR-OR-NOMATCH");

    if (installed == VERCMP_ERROR)
    {
        cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a2, "Failure trying to compare installed package versions");
        result = PromiseResultUpdate_HELPER(pp, result, PROMISE_RESULT_FAIL);
        return result;
    }

    VersionCmpResult matches = PackageMatch(ctx, name, version, arch, a2, pp, "[available]", &result);
    Log(LOG_LEVEL_VERBOSE, "CheckPackageState: Available package match for (%s,%s,%s) [name,version,arch] was decisive: %s",
        name, version, arch, matches == VERCMP_MATCH ? "MATCH" : "ERROR-OR-NOMATCH");

    if (matches == VERCMP_ERROR)
    {
        cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a2, "Failure trying to compare available package versions");
        result = PromiseResultUpdate_HELPER(pp, result, PROMISE_RESULT_FAIL);
        return result;
    }

    if (WillSchedulePackageOperation(ctx, a2, pp, matches, installed))
    {
        Log(LOG_LEVEL_VERBOSE, "CheckPackageState: matched package (%s,%s,%s) [name,version,arch]; scheduling operation", name, version, arch);
        return SchedulePackageOp(ctx, name, version, arch, installed, matches, no_version, a, pp);
    }

    return result;
}

/**
   @brief Verifies a promised patch operation as defined by a and pp

   Called by VerifyPackagesPromise for the patch operation.

   * package name is pp->promiser
   * installed and matches counts = 0
   * copies a into a2 and overrides a2.packages.package_select to PACKAGE_VERSION_COMPARATOR_EQ
   * promise result starts as NOOP
   * if package version is given
   * * for arch = each architecture requested in a2, or (if none given) any architecture "*"
   * * * installed1 = PatchMatch(a2, name, any version "*", any architecture "*")
   * * * matches1 = PatchMatch(a2, name, requested version, arch)
   * * * if either installed1 or matches1 failed, return promise error
   * * * else, installed += installed1; matches += matches1
   * else if package_version_regex is given
   * * assume that package_name_regex and package_arch_regex are also given and use the 3 regexes to extract name, version, arch
   * * * installed = PatchMatch(a2, matched name, any version "*", any architecture "*")
   * * * matches = PatchMatch(a2, matched name, matched version, matched architecture)
   * * * if either installed or matches failed, return promise error
   * else (no explicit version is given) (SAME LOOP AS EXPLICIT VERSION LOOP ABOVE)
   * * no_version = true
   * * for arch = each architecture requested in a2, or (if none given) any architecture "*"
   * * * requested version = any version '*'
   * * * installed1 = PatchMatch(a2, name, any version "*", any architecture "*")
   * * * matches1 = PatchMatch(a2, name, requested version '*', arch)
   * * * if either installed1 or matches1 failed, return promise error
   * * * else, installed += installed1; matches += matches1
   * finally, call SchedulePackageOp with the found name, version, arch, installed, matches, no_version

   @param ctx [in] The evaluation context
   @param a [in] the Attributes specifying how to compare
   @param pp [in] the Promise for this operation
   @returns the promise result (failure or NOOP)
*/
static PromiseResult VerifyPromisedPatch(EvalContext *ctx, Attributes a, const Promise *pp)
{
    char version[CF_MAXVARSIZE];
    char name[CF_MAXVARSIZE];
    char arch[CF_MAXVARSIZE];
    char *package = pp->promiser;
    int matches = 0, installed = 0, no_version = false;
    Rlist *rp;

    /* Horrible */
    Attributes a2 = a;
    a2.packages.package_select = PACKAGE_VERSION_COMPARATOR_EQ;

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (a2.packages.package_version) /* The version is specified explicitly */
    {
        // Note this loop will run if rp is NULL
        for (rp = a2.packages.package_architectures; ; rp = rp->next)
        {
            strlcpy(name, pp->promiser, CF_MAXVARSIZE);
            strlcpy(version, a2.packages.package_version, CF_MAXVARSIZE);
            strlcpy(arch, NULL == rp ? "*" : RlistScalarValue(rp), CF_MAXVARSIZE);
            VersionCmpResult installed1 = PatchMatch(ctx, name, "*", "*", a2, pp, "[installed1]", &result);
            VersionCmpResult matches1 = PatchMatch(ctx, name, version, arch, a2, pp, "[available1]", &result);

            if ((installed1 == VERCMP_ERROR) || (matches1 == VERCMP_ERROR))
            {
                cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a2, "Failure trying to compare package versions");
                result = PromiseResultUpdate_HELPER(pp, result, PROMISE_RESULT_FAIL);
                return result;
            }

            installed += installed1;
            matches += matches1;

            if (NULL == rp) break; // Note we exit the loop explicitly here
        }
    }
    else if (a2.packages.package_version_regex) // version is not given, but a version regex is
    {
        /* The name, version and arch are to be extracted from the promiser */
        strlcpy(version, ExtractFirstReference(a2.packages.package_version_regex, package), CF_MAXVARSIZE);
        strlcpy(name, ExtractFirstReference(a2.packages.package_name_regex, package), CF_MAXVARSIZE);
        strlcpy(arch, ExtractFirstReference(a2.packages.package_arch_regex, package), CF_MAXVARSIZE);
        installed = PatchMatch(ctx, name, "*", "*", a2, pp, "[installed]", &result);
        matches = PatchMatch(ctx, name, version, arch, a2, pp, "[available]", &result);

        if ((installed == VERCMP_ERROR) || (matches == VERCMP_ERROR))
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a2, "Failure trying to compare package versions");
            result = PromiseResultUpdate_HELPER(pp, result, PROMISE_RESULT_FAIL);
            return result;
        }
    }
    else // the desired package version was not specified
    {
        no_version = true;

        // Note this loop will run if rp is NULL
        for (rp = a2.packages.package_architectures; ; rp = rp->next)
        {
            strlcpy(name, pp->promiser, CF_MAXVARSIZE);
            strlcpy(version, "*", CF_MAXVARSIZE);
            strlcpy(arch, NULL == rp ? "*" : RlistScalarValue(rp), CF_MAXVARSIZE);
            VersionCmpResult installed1 = PatchMatch(ctx, name, "*", "*", a2, pp, "[installed1]", &result);
            VersionCmpResult matches1 = PatchMatch(ctx, name, version, arch, a2, pp, "[available1]", &result);

            if ((installed1 == VERCMP_ERROR) || (matches1 == VERCMP_ERROR))
            {
                cfPS_HELPER_0ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a2, "Failure trying to compare package versions");
                result = PromiseResultUpdate_HELPER(pp, result, PROMISE_RESULT_FAIL);
                return result;
            }

            installed += installed1;
            matches += matches1;

            if (NULL == rp) break; // Note we exit the loop explicitly here
        }
    }

    Log(LOG_LEVEL_VERBOSE, "%d patch(es) matching the name '%s' already installed", installed, name);
    Log(LOG_LEVEL_VERBOSE, "%d patch(es) match the promise body's criteria fully", matches);

    SchedulePackageOp(ctx, name, version, arch, installed, matches, no_version, a, pp);

    return PROMISE_RESULT_NOOP;
}

/**
   @brief Verifies a promised package operation as defined by a and pp

   Called by VerifyPackagesPromise for any non-patch operation.

   * package name is pp->promiser
   * promise result starts as NOOP
   * if package version is given
   * * if no architecture given, the promise result comes from CheckPackageState with name, version, any architecture '*', no_version=false
   * * else if architectures were given, the promise result comes from CheckPackageState with name, version, arch, no_version=false FOR EACH ARCHITECTURE
   * else if package_version_regex is given
   * * assume that package_name_regex and package_arch_regex are also given and use the 3 regexes to extract name, version, arch
   * * if the arch extraction failed, use any architecture '*'
   * * the promise result comes from CheckPackageState with name, version, arch, no_version=false)
   * else (no explicit version is given) (SAME LOOP AS EXPLICIT VERSION LOOP ABOVE)
   * * if no architecture given, the promise result comes from CheckPackageState with name, any version "*", any architecture '*', no_version=true
   * * else if architectures were given, the promise result comes from CheckPackageState with name, any version "*", arch, no_version=true FOR EACH ARCHITECTURE

   @param ctx [in] The evaluation context
   @param a [in] the Attributes specifying how to compare
   @param pp [in] the Promise for this operation
   @returns the promise result as set by CheckPackageState
*/
static PromiseResult VerifyPromisedPackage(EvalContext *ctx, Attributes a, const Promise *pp)
{
    const char *package = pp->promiser;

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (a.packages.package_version)
    {
        /* The version is specified separately */
        Log(LOG_LEVEL_VERBOSE, "Package version %s specified explicitly in promise body", a.packages.package_version);

        if (a.packages.package_architectures == NULL)
        {
            Log(LOG_LEVEL_VERBOSE, " ... trying any arch '*'");
            result = PromiseResultUpdate_HELPER(pp, result, CheckPackageState(ctx, a, pp, package, a.packages.package_version, "*", false));
        }
        else
        {
            for (Rlist *rp = a.packages.package_architectures; rp != NULL; rp = rp->next)
            {
                Log(LOG_LEVEL_VERBOSE, " ... trying listed arch '%s'", RlistScalarValue(rp));
                result = PromiseResultUpdate_HELPER(pp, result,
                                             CheckPackageState(ctx, a, pp, package, a.packages.package_version,
                                                               RlistScalarValue(rp), false));
            }
        }
    }
    else if (a.packages.package_version_regex)
    {
        /* The name, version and arch are to be extracted from the promiser */
        Log(LOG_LEVEL_VERBOSE, "Package version %s specified implicitly in promiser's name", a.packages.package_version_regex);

        char version[CF_MAXVARSIZE];
        char name[CF_MAXVARSIZE];
        char arch[CF_MAXVARSIZE];
        strlcpy(version, ExtractFirstReference(a.packages.package_version_regex, package), CF_MAXVARSIZE);
        strlcpy(name, ExtractFirstReference(a.packages.package_name_regex, package), CF_MAXVARSIZE);
        strlcpy(arch, ExtractFirstReference(a.packages.package_arch_regex, package), CF_MAXVARSIZE);

        if (!arch[0])
        {
            strlcpy(arch, "*", CF_MAXVARSIZE);
        }

        if (strcmp(arch, "CF_NOMATCH") == 0)    // no match on arch regex, use any arch
        {
            strlcpy(arch, "*", CF_MAXVARSIZE);
        }

        Log(LOG_LEVEL_VERBOSE, " ... trying arch '%s' and version '%s'", arch, version);
        result = PromiseResultUpdate_HELPER(pp, result, CheckPackageState(ctx, a, pp, name, version, arch, false));
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Package version was not specified");

        if (a.packages.package_architectures == NULL)
        {
            Log(LOG_LEVEL_VERBOSE, " ... trying any arch '*' and any version '*'");
            result = PromiseResultUpdate_HELPER(pp, result, CheckPackageState(ctx, a, pp, package, "*", "*", true));
        }
        else
        {
            for (Rlist *rp = a.packages.package_architectures; rp != NULL; rp = rp->next)
            {
                Log(LOG_LEVEL_VERBOSE, " ... trying listed arch '%s' and any version '*'", RlistScalarValue(rp));
                result = PromiseResultUpdate_HELPER(pp, result, CheckPackageState(ctx, a, pp, package, "*", RlistScalarValue(rp), true));
            }
        }
    }

    return result;
}

/** Execute scheduled operations **/

/**
   @brief Central dispatcher for scheduled operations

   Called by ExecutePackageSchedule.

   Almost identical to ExecutePatch.

   * verify = false
   * for each PackageManager pm in the schedule
   * * if pm->pack_list is empty or the scheduled pm->action doesn't match the given action, skip this pm
   * * estimate the size of the command string from pm->pack_list and pm->policy (SHOULD USE Buffer)
   * * from the first PackageItem in pm->pack_list, get the Promise pp and its Attributes a
   * * switch(action)
   * * * case ADD:
   * * * * command_string = a.packages.package_add_command + estimated_size room for package names
   * * * case DELETE:
   * * * * command_string = a.packages.package_delete_command + estimated_size room for package names
   * * * case UPDATE:
   * * * * command_string = a.packages.package_update_command + estimated_size room for package names
   * * * case VERIFY:
   * * * * command_string = a.packages.package_verify_command + estimated_size room for package names
   * * * * verify = true

   * * if the command string ends with $, run it with ExecPackageCommand(command, verify) and magic the promise evaluation
   * * else, switch(pm->policy)
   * * * case INDIVIDUAL:
   * * * * for each PackageItem in the pack_list, build the command and run it with ExecPackageCommand(command, verify) and magic the promise evaluation
   * * * * NOTE with file repositories and ADD/UPDATE operations, the package name gets the repo path too
   * * * * NOTE special treatment of PACKAGE_IGNORED_CFE_INTERNAL
   * * * case BULK:
   * * * * for all PackageItems in the pack_list, build the command and run it with ExecPackageCommand(command, verify) and magic the promise evaluation
   * * * * NOTE with file repositories and ADD/UPDATE operations, the package name gets the repo path too
   * * * * NOTE special treatment of PACKAGE_IGNORED_CFE_INTERNAL
   * * clean up command_string
   * if the operation was not a verification, InvalidateSoftwareCache

   @param ctx [in] The evaluation context
   @param schedule [in] the PackageManager list with the operations schedule
   @param action [in] the PackageAction desired
   @returns boolean success/fail (fail only on ProgrammingError, should never happen)
*/
static bool ExecuteSchedule(EvalContext *ctx, const PackageManager *schedule, PackageAction action)
{
    bool verify = false;

    for (const PackageManager *pm = schedule; pm != NULL; pm = pm->next)
    {
        if (pm->action != action)
        {
            continue;
        }

        if (pm->pack_list == NULL)
        {
            continue;
        }

        size_t estimated_size = 0;

        for (const PackageItem *pi = pm->pack_list; pi != NULL; pi = pi->next)
        {
            size_t size = strlen(pi->name) + strlen("  ");

            switch (pm->policy)
            {
            case PACKAGE_ACTION_POLICY_INDIVIDUAL:

                if (size > estimated_size)
                {
                    estimated_size = size + CF_MAXVARSIZE;
                }
                break;

            case PACKAGE_ACTION_POLICY_BULK:

                estimated_size += size + CF_MAXVARSIZE;
                break;

            default:
                break;
            }
        }

        const Promise *const pp = pm->pack_list->pp;
        Attributes a = GetPackageAttributes(ctx, pp);
        char *command_string = NULL;

        switch (action)
        {
        case PACKAGE_ACTION_ADD:

            Log(LOG_LEVEL_VERBOSE, "Execute scheduled package addition");

            if (a.packages.package_add_command == NULL)
            {
                ProgrammingError("Package add command undefined");
                return false;
            }

            Log(LOG_LEVEL_INFO, "Installing %-.39s...", pp->promiser);

            command_string = xmalloc(estimated_size + strlen(a.packages.package_add_command) + 2);
            strcpy(command_string, a.packages.package_add_command);
            break;

        case PACKAGE_ACTION_DELETE:

            Log(LOG_LEVEL_VERBOSE, "Execute scheduled package deletion");

            if (a.packages.package_delete_command == NULL)
            {
                ProgrammingError("Package delete command undefined");
                return false;
            }

            Log(LOG_LEVEL_INFO, "Deleting %-.39s...", pp->promiser);

            command_string = xmalloc(estimated_size + strlen(a.packages.package_delete_command) + 2);
            strcpy(command_string, a.packages.package_delete_command);
            break;

        case PACKAGE_ACTION_UPDATE:

            Log(LOG_LEVEL_VERBOSE, "Execute scheduled package update");

            if (a.packages.package_update_command == NULL)
            {
                ProgrammingError("Package update command undefined");
                return false;
            }

            Log(LOG_LEVEL_INFO, "Updating %-.39s...", pp->promiser);

            command_string = xcalloc(1, estimated_size + strlen(a.packages.package_update_command) + 2);
            strcpy(command_string, a.packages.package_update_command);

            break;

        case PACKAGE_ACTION_VERIFY:

            Log(LOG_LEVEL_VERBOSE, "Execute scheduled package verification");

            if (a.packages.package_verify_command == NULL)
            {
                ProgrammingError("Package verify command undefined");
                return false;
            }

            command_string = xmalloc(estimated_size + strlen(a.packages.package_verify_command) + 2);
            strcpy(command_string, a.packages.package_verify_command);

            verify = true;
            break;

        default:
            ProgrammingError("Unknown action attempted");
            return false;
        }

        /* if the command ends with $ then we assume the package manager does not accept package names */

        if (*(command_string + strlen(command_string) - 1) == '$')
        {
            *(command_string + strlen(command_string) - 1) = '\0';
            Log(LOG_LEVEL_VERBOSE, "Command does not allow arguments");
            PromiseResult result = PROMISE_RESULT_NOOP;

            EvalContextStackPushPromiseFrame(ctx, pp, false);
            if (EvalContextStackPushPromiseIterationFrame(ctx, 0, NULL))
            {
                if (ExecPackageCommand(ctx, command_string, verify, true, a, pp, &result))
                {
                    Log(LOG_LEVEL_VERBOSE, "Package schedule execution ok (outcome cannot be promised by cf-agent)");
                }
                else
                {
                    Log(LOG_LEVEL_ERR, "Package schedule execution failed");
                }

                EvalContextStackPopFrame(ctx);
            }
            EvalContextStackPopFrame(ctx);

            EvalContextLogPromiseIterationOutcome(ctx, pp, result);
        }
        else
        {
            strcat(command_string, " ");

            Log(LOG_LEVEL_VERBOSE, "Command prefix '%s'", command_string);

            switch (pm->policy)
            {
            case PACKAGE_ACTION_POLICY_INDIVIDUAL:

                for (const PackageItem *pi = pm->pack_list; pi != NULL; pi = pi->next)
                {
                    const Promise *const ppi = pi->pp;
                    Attributes a = GetPackageAttributes(ctx, ppi);

                    char *offset = command_string + strlen(command_string);

                    if ((a.packages.package_file_repositories) && ((action == PACKAGE_ACTION_ADD) || (action == PACKAGE_ACTION_UPDATE)))
                    {
                        const char *sp = PrefixLocalRepository(a.packages.package_file_repositories, pi->name);
                        if (sp != NULL)
                        {
                            strcat(offset, sp);
                        }
                        else
                        {
                            continue;
                        }
                    }
                    else
                    {
                        strcat(offset, pi->name);
                    }

                    PromiseResult result = PROMISE_RESULT_NOOP;
                    EvalContextStackPushPromiseFrame(ctx, ppi, false);
                    if (EvalContextStackPushPromiseIterationFrame(ctx, 0, NULL))
                    {
                        if (ExecPackageCommand(ctx, command_string, verify, true, a, ppi, &result))
                        {
                            Log(LOG_LEVEL_VERBOSE,
                                "Package schedule execution ok for '%s' (outcome cannot be promised by cf-agent)",
                                  pi->name);
                        }
                        else if (0 == strncmp(pi->name, PACKAGE_IGNORED_CFE_INTERNAL, strlen(PACKAGE_IGNORED_CFE_INTERNAL)))
                        {
                            Log(LOG_LEVEL_DEBUG, "ExecuteSchedule: Ignoring outcome for special package '%s'", pi->name);
                        }
                        else
                        {
                            Log(LOG_LEVEL_ERR, "Package schedule execution failed for '%s'", pi->name);
                        }

                        EvalContextStackPopFrame(ctx);
                    }
                    EvalContextStackPopFrame(ctx);

                    EvalContextLogPromiseIterationOutcome(ctx, ppi, result);

                    *offset = '\0';
                }

                break;

            case PACKAGE_ACTION_POLICY_BULK:
                {
                    for (const PackageItem *pi = pm->pack_list; pi != NULL; pi = pi->next)
                    {
                        if (pi->name)
                        {
                            char *offset = command_string + strlen(command_string);

                            if (a.packages.package_file_repositories &&
                                (action == PACKAGE_ACTION_ADD ||
                                 action == PACKAGE_ACTION_UPDATE))
                            {
                                const char *sp = PrefixLocalRepository(a.packages.package_file_repositories, pi->name);
                                if (sp != NULL)
                                {
                                    strcpy(offset, sp);
                                }
                                else
                                {
                                    break;
                                }
                            }
                            else
                            {
                                strcpy(offset, pi->name);
                            }

                            strcat(command_string, " ");
                        }
                    }

                    PromiseResult result = PROMISE_RESULT_NOOP;
                    EvalContextStackPushPromiseFrame(ctx, pp, false);
                    if (EvalContextStackPushPromiseIterationFrame(ctx, 0, NULL))
                    {
                        bool ok = ExecPackageCommand(ctx, command_string, verify, true, a, pp, &result);

                        for (const PackageItem *pi = pm->pack_list; pi != NULL; pi = pi->next)
                        {
                            if (ok)
                            {
                                Log(LOG_LEVEL_VERBOSE,
                                    "Bulk package schedule execution ok for '%s' (outcome cannot be promised by cf-agent)",
                                      pi->name);
                            }
                            else if (0 == strncmp(pi->name, PACKAGE_IGNORED_CFE_INTERNAL, strlen(PACKAGE_IGNORED_CFE_INTERNAL)))
                            {
                                Log(LOG_LEVEL_DEBUG, "ExecuteSchedule: Ignoring outcome for special package '%s'", pi->name);
                            }
                            else
                            {
                                Log(LOG_LEVEL_ERR, "Bulk package schedule execution failed somewhere - unknown outcome for '%s'",
                                      pi->name);
                            }
                        }

                        EvalContextStackPopFrame(ctx);
                    }
                    EvalContextStackPopFrame(ctx);
                    EvalContextLogPromiseIterationOutcome(ctx, pp, result);
                }

                break;

            default:
                break;
            }
        }

        if (command_string)
        {
            free(command_string);
        }
    }

/* We have performed some modification operation on packages, our cache is invalid */
    if (!verify)
    {
        InvalidateSoftwareCache();
    }

    return true;
}

/**
   @brief Central dispatcher for scheduled patch operations

   Called by ExecutePackageSchedule.

   Almost identical to ExecuteSchedule except it only accepts the
   PATCH PackageAction and operates on the PackageManagers' patch_list.

   * for each PackageManager pm in the schedule
   * * if pm->patch_list is empty or the scheduled pm->action doesn't match the given action, skip this pm
   * * estimate the size of the command string from pm->patch_list and pm->policy (SHOULD USE Buffer)
   * * from the first PackageItem in pm->patch_list, get the Promise pp and its Attributes a
   * * switch(action)
   * * * case PATCH:
   * * * * command_string = a.packages.package_patch_command + estimated_size room for package names

   * * if the command string ends with $, run it with ExecPackageCommand(command, verify) and magic the promise evaluation
   * * else, switch(pm->policy)
   * * * case INDIVIDUAL:
   * * * * for each PackageItem in the patch_list, build the command and run it with ExecPackageCommand(command, verify) and magic the promise evaluation
   * * * * NOTE with file repositories and ADD/UPDATE operations, the package name gets the repo path too
   * * * * NOTE special treatment of PACKAGE_IGNORED_CFE_INTERNAL
   * * * case BULK:
   * * * * for all PackageItems in the patch_list, build the command and run it with ExecPackageCommand(command, verify) and magic the promise evaluation
   * * * * NOTE with file repositories and ADD/UPDATE operations, the package name gets the repo path too
   * * * * NOTE special treatment of PACKAGE_IGNORED_CFE_INTERNAL
   * * clean up command_string
   * InvalidateSoftwareCache

   @param ctx [in] The evaluation context
   @param schedule [in] the PackageManager list with the operations schedule
   @param action [in] the PackageAction desired
   @returns boolean success/fail (fail only on ProgrammingError, should never happen)
*/
static bool ExecutePatch(EvalContext *ctx, const PackageManager *schedule, PackageAction action)
{
    for (const PackageManager *pm = schedule; pm != NULL; pm = pm->next)
    {
        if (pm->action != action)
        {
            continue;
        }

        if (pm->patch_list == NULL)
        {
            continue;
        }

        size_t estimated_size = 0;

        for (const PackageItem *pi = pm->patch_list; pi != NULL; pi = pi->next)
        {
            size_t size = strlen(pi->name) + strlen("  ");

            switch (pm->policy)
            {
            case PACKAGE_ACTION_POLICY_INDIVIDUAL:
                if (size > estimated_size)
                {
                    estimated_size = size;
                }
                break;

            case PACKAGE_ACTION_POLICY_BULK:
                estimated_size += size;
                break;

            default:
                break;
            }
        }

        char *command_string = NULL;
        const Promise *const pp = pm->patch_list->pp;
        Attributes a = GetPackageAttributes(ctx, pp);

        switch (action)
        {
        case PACKAGE_ACTION_PATCH:

            Log(LOG_LEVEL_VERBOSE, "Execute scheduled package patch");

            if (a.packages.package_patch_command == NULL)
            {
                ProgrammingError("Package patch command undefined");
                return false;
            }

            command_string = xmalloc(estimated_size + strlen(a.packages.package_patch_command) + 2);
            strcpy(command_string, a.packages.package_patch_command);
            break;

        default:
            ProgrammingError("Unknown action attempted");
            return false;
        }

        /* if the command ends with $ then we assume the package manager does not accept package names */

        if (command_string[strlen(command_string) - 1] == '$')
        {
            command_string[strlen(command_string) - 1] = '\0';
            Log(LOG_LEVEL_VERBOSE, "Command does not allow arguments");

            PromiseResult result = PROMISE_RESULT_NOOP;
            EvalContextStackPushPromiseFrame(ctx, pp, false);
            if (EvalContextStackPushPromiseIterationFrame(ctx, 0, NULL))
            {
                if (ExecPackageCommand(ctx, command_string, false, true, a, pp, &result))
                {
                    Log(LOG_LEVEL_VERBOSE, "Package patching seemed to succeed (outcome cannot be promised by cf-agent)");
                }
                else
                {
                    Log(LOG_LEVEL_ERR, "Package patching failed");
                }

                EvalContextStackPopFrame(ctx);
            }
            EvalContextStackPopFrame(ctx);
            EvalContextLogPromiseIterationOutcome(ctx, pp, result);
        }
        else
        {
            strcat(command_string, " ");

            Log(LOG_LEVEL_VERBOSE, "Command prefix '%s'", command_string);

            switch (pm->policy)
            {
            case PACKAGE_ACTION_POLICY_INDIVIDUAL:
                for (const PackageItem *pi = pm->patch_list; pi != NULL; pi = pi->next)
                {
                    char *offset = command_string + strlen(command_string);

                    strcat(offset, pi->name);

                    PromiseResult result = PROMISE_RESULT_NOOP;
                    EvalContextStackPushPromiseFrame(ctx, pp, false);
                    if (EvalContextStackPushPromiseIterationFrame(ctx, 0, NULL))
                    {
                        if (ExecPackageCommand(ctx, command_string, false, true, a, pp, &result))
                        {
                            Log(LOG_LEVEL_VERBOSE,
                                "Package schedule execution ok for '%s' (outcome cannot be promised by cf-agent)",
                                  pi->name);
                        }
                        else if (0 == strncmp(pi->name, PACKAGE_IGNORED_CFE_INTERNAL, strlen(PACKAGE_IGNORED_CFE_INTERNAL)))
                        {
                            Log(LOG_LEVEL_DEBUG, "ExecutePatch: Ignoring outcome for special package '%s'", pi->name);
                        }
                        else
                        {
                            Log(LOG_LEVEL_ERR, "Package schedule execution failed for '%s'", pi->name);
                        }

                        EvalContextStackPopFrame(ctx);
                    }
                    EvalContextStackPopFrame(ctx);
                    EvalContextLogPromiseIterationOutcome(ctx, pp, result);

                    *offset = '\0';
                }

                break;

            case PACKAGE_ACTION_POLICY_BULK:
                for (const PackageItem *pi = pm->patch_list; pi != NULL; pi = pi->next)
                {
                    if (pi->name)
                    {
                        strcat(command_string, pi->name);
                        strcat(command_string, " ");
                    }
                }

                PromiseResult result = PROMISE_RESULT_NOOP;
                EvalContextStackPushPromiseFrame(ctx, pp, false);
                if (EvalContextStackPushPromiseIterationFrame(ctx, 0, NULL))
                {
                    bool ok = ExecPackageCommand(ctx, command_string, false, true, a, pp, &result);

                    for (const PackageItem *pi = pm->patch_list; pi != NULL; pi = pi->next)
                    {
                        if (ok)
                        {
                            Log(LOG_LEVEL_VERBOSE,
                                "Bulk package schedule execution ok for '%s' (outcome cannot be promised by cf-agent)",
                                  pi->name);
                        }
                        else if (0 == strncmp(pi->name, PACKAGE_IGNORED_CFE_INTERNAL, strlen(PACKAGE_IGNORED_CFE_INTERNAL)))
                        {
                            Log(LOG_LEVEL_DEBUG, "ExecutePatch: Ignoring outcome for special package '%s'", pi->name);
                        }
                        else
                        {
                            Log(LOG_LEVEL_ERR, "Bulk package schedule execution failed somewhere - unknown outcome for '%s'",
                                  pi->name);
                        }
                    }

                    EvalContextStackPopFrame(ctx);
                }
                EvalContextStackPopFrame(ctx);
                EvalContextLogPromiseIterationOutcome(ctx, pp, result);
                break;

            default:
                break;
            }

        }

        if (command_string)
        {
            free(command_string);
        }
    }

/* We have performed some operation on packages, our cache is invalid */
    InvalidateSoftwareCache();

    return true;
}

/**
   @brief Ordering manager for scheduled package operations

   Called by ExecuteScheduledPackages.

   * ExecuteSchedule(schedule, DELETE)
   * ExecuteSchedule(schedule, ADD)
   * ExecuteSchedule(schedule, UPDATE)
   * ExecutePatch(schedule, PATCH)
   * ExecuteSchedule(schedule, VERIFY)

   @param ctx [in] The evaluation context
   @param schedule [in] the PackageManager list with the operations schedule
*/
static void ExecutePackageSchedule(EvalContext *ctx, PackageManager *schedule)
{
        Log(LOG_LEVEL_VERBOSE, "Offering the following package-promise suggestions to the managers");

    /* Normal ordering */

    Log(LOG_LEVEL_VERBOSE, "Deletion schedule...");
    if (!ExecuteSchedule(ctx, schedule, PACKAGE_ACTION_DELETE))
    {
        Log(LOG_LEVEL_ERR, "Aborting package schedule");
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Addition schedule...");
    if (!ExecuteSchedule(ctx, schedule, PACKAGE_ACTION_ADD))
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Update schedule...");
    if (!ExecuteSchedule(ctx, schedule, PACKAGE_ACTION_UPDATE))
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Patch schedule...");
    if (!ExecutePatch(ctx, schedule, PACKAGE_ACTION_PATCH))
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Verify schedule...");
    if (!ExecuteSchedule(ctx, schedule, PACKAGE_ACTION_VERIFY))
    {
        return;
    }
}

/**
 * @brief Execute the full package schedule.
 *
 * Called by cf-agent only.
 *
 */
void ExecuteScheduledPackages(EvalContext *ctx)
{
    if (PACKAGE_SCHEDULE)
    {
        ExecutePackageSchedule(ctx, PACKAGE_SCHEDULE);
    }
}

/** Cleanup **/

/**
 * @brief Clean the package schedule and installed lists.
 *
 * Called by cf-agent only.  Cleans bookkeeping data.
 *
 */
void CleanScheduledPackages(void)
{
    DeletePackageManagers(PACKAGE_SCHEDULE);
    PACKAGE_SCHEDULE = NULL;
    DeletePackageManagers(INSTALLED_PACKAGE_LISTS);
    INSTALLED_PACKAGE_LISTS = NULL;
}

/** Utils **/

static PackageManager *GetPackageManager(PackageManager **lists, char *mgr,
                                         PackageAction pa,
                                         PackageActionPolicy policy)
{
    PackageManager *np;

    Log(LOG_LEVEL_VERBOSE, "Looking for a package manager called '%s'", mgr);

    if (mgr == NULL || mgr[0] == '\0')
    {
        Log(LOG_LEVEL_ERR, "Attempted to create a package manager with no name");
        return NULL;
    }

    for (np = *lists; np != NULL; np = np->next)
    {
        if ((strcmp(np->manager, mgr) == 0) && (policy == np->policy))
        {
            return np;
        }
    }

    np = xcalloc(1, sizeof(PackageManager));

    np->manager = xstrdup(mgr);
    np->action = pa;
    np->policy = policy;
    np->next = *lists;
    *lists = np;
    return np;
}

static void DeletePackageItems(PackageItem * pi)
{
    while (pi)
    {
        PackageItem *next = pi->next;
        free(pi->name);
        free(pi->version);
        free(pi->arch);
        PromiseDestroy(pi->pp);
        free(pi);
        pi = next;
    }
}

static void DeletePackageManagers(PackageManager *np)
{
    while (np)
    {
        PackageManager *next = np->next;
        DeletePackageItems(np->pack_list);
        DeletePackageItems(np->patch_list);
        DeletePackageItems(np->patch_avail);
        free(np->manager);
        free(np);
        np = next;
    }
}

const char *PrefixLocalRepository(const Rlist *repositories, const char *package)
{
    static char quotedPath[CF_MAXVARSIZE]; /* GLOBAL_R, no need to initialize */
    struct stat sb;
    char path[CF_BUFSIZE];

    for (const Rlist *rp = repositories; rp != NULL; rp = rp->next)
    {
        if (strlcpy(path, RlistScalarValue(rp), sizeof(path)) < sizeof(path))
        {
            AddSlash(path);

            if (strlcat(path, package, sizeof(path)) < sizeof(path) &&
                stat(path, &sb) != -1)
            {
                snprintf(quotedPath, sizeof(quotedPath), "\"%s\"", path);
                return quotedPath;
            }
        }
    }

    return NULL;
}

bool ExecPackageCommand(EvalContext *ctx, char *command, int verify, int setCmdClasses, Attributes a,
                        const Promise *pp, PromiseResult *result)
{
    bool retval = true;
    char lineSafe[CF_BUFSIZE], *cmd;
    FILE *pfp;
    int packmanRetval = 0;

    if ((!a.packages.package_commands_useshell) && (!IsExecutable(CommandArg0(command))))
    {
        cfPS_HELPER_1ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "The proposed package schedule command '%s' was not executable", command);
        *result = PromiseResultUpdate_HELPER(pp, *result, PROMISE_RESULT_FAIL);
        return false;
    }

    if (DONTDO)
    {
        return true;
    }

/* Use this form to avoid limited, intermediate argument processing - long lines */

    if (a.packages.package_commands_useshell)
    {
        Log(LOG_LEVEL_VERBOSE, "Running %s in shell", command);
        if ((pfp = cf_popen_sh(command, "r")) == NULL)
        {
            cfPS_HELPER_2ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Couldn't start command '%20s'. (cf_popen_sh: %s)",
                 command, GetErrorStr());
            *result = PromiseResultUpdate_HELPER(pp, *result, PROMISE_RESULT_FAIL);
            return false;
        }
    }
    else
    {
        if ((pfp = cf_popen(command, "r", true)) == NULL)
        {
            cfPS_HELPER_2ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Couldn't start command '%20s'. (cf_popen: %s)",
                 command, GetErrorStr());
            *result = PromiseResultUpdate_HELPER(pp, *result, PROMISE_RESULT_FAIL);
            return false;
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Executing %-.60s...", command);

/* Look for short command summary */
    for (cmd = command; (*cmd != '\0') && (*cmd != ' '); cmd++)
    {
    }

    while (cmd > command && cmd[-1] != FILE_SEPARATOR)
    {
        cmd--;
    }

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);

    for (;;)
    {
        ssize_t res = CfReadLine(&line, &line_size, pfp);
        if (res == -1)
        {
            if (!feof(pfp))
            {
                cfPS_HELPER_2ARG(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Unable to read output from command '%20s'. (fread: %s)",
                     command, GetErrorStr());
                *result = PromiseResultUpdate_HELPER(pp, *result, PROMISE_RESULT_FAIL);
                cf_pclose(pfp);
                free(line);
                return false;
            }
            else
            {
                break;
            }
        }

        ReplaceStr(line, lineSafe, sizeof(lineSafe), "%", "%%");
        Log(LOG_LEVEL_INFO, "Q:%20.20s ...:%s", cmd, lineSafe);

        if (verify && (line[0] != '\0'))
        {
            if (a.packages.package_noverify_regex)
            {
                if (FullTextMatch(ctx, a.packages.package_noverify_regex, line))
                {
                    cfPS_HELPER_2ARG(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a, "Package verification error in %-.40s ... :%s", cmd, lineSafe);
                    *result = PromiseResultUpdate_HELPER(pp, *result, PROMISE_RESULT_FAIL);
                    retval = false;
                }
            }
        }
    }

    free(line);

    packmanRetval = cf_pclose(pfp);

    if (verify && (a.packages.package_noverify_returncode != CF_NOINT))
    {
        if (a.packages.package_noverify_returncode == packmanRetval)
        {
            cfPS_HELPER_1ARG(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a, "Package verification error (returned %d)", packmanRetval);
            *result = PromiseResultUpdate_HELPER(pp, *result, PROMISE_RESULT_FAIL);
            retval = false;
        }
        else
        {
            cfPS_HELPER_1ARG(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, a, "Package verification succeeded (returned %d)", packmanRetval);
            *result = PromiseResultUpdate_HELPER(pp, *result, PROMISE_RESULT_FAIL);
        }
    }
    else if (verify && (a.packages.package_noverify_regex))
    {
        if (retval)             // set status if we succeeded above
        {
            cfPS_HELPER_0ARG(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, a,
                 "Package verification succeeded (no match with package_noverify_regex)");
        }
    }
    else if (setCmdClasses)     // generic return code check
    {
        if (REPORT_THIS_PROMISE(pp))
        {
            retval = VerifyCommandRetcode(ctx, packmanRetval, a, pp, result);
        }
    }

    return retval;
}

int PrependPackageItem(EvalContext *ctx, PackageItem ** list,
                       const char *name, const char *version, const char *arch,
                       const Promise *pp)
{
    if (!list || !name[0] || !version[0] || !arch[0])
    {
        return false;
    }

    Log(LOG_LEVEL_VERBOSE,
        "Package (%s,%s,%s) [name,version,arch] found",
        name, version, arch);

    PackageItem *pi = xmalloc(sizeof(PackageItem));

    pi->next = *list;
    pi->name = xstrdup(name);
    pi->version = xstrdup(version);
    pi->arch = xstrdup(arch);
    *list = pi;

/* Finally we need these for later schedule exec, once this iteration context has gone */

    pi->pp = DeRefCopyPromise(ctx, pp);
    return true;
}

static int PackageInItemList(PackageItem * list, char *name, char *version, char *arch)
{
    if (!name[0] || !version[0] || !arch[0])
    {
        return false;
    }

    for (PackageItem *pi = list; pi != NULL; pi = pi->next)
    {
        if (strcmp(pi->name, name) == 0 &&
            strcmp(pi->version, version) == 0 &&
            strcmp(pi->arch, arch) == 0)
        {
            return true;
        }
    }

    return false;
}

static int PrependPatchItem(EvalContext *ctx, PackageItem ** list, char *item, PackageItem * chklist, const char *default_arch,
                            Attributes a, const Promise *pp)
{
    char name[CF_MAXVARSIZE];
    char arch[CF_MAXVARSIZE];
    char version[CF_MAXVARSIZE];
    char vbuff[CF_MAXVARSIZE];

    strlcpy(vbuff, ExtractFirstReference(a.packages.package_patch_name_regex, item), CF_MAXVARSIZE);
    sscanf(vbuff, "%s", name);  /* trim */
    strlcpy(vbuff, ExtractFirstReference(a.packages.package_patch_version_regex, item), CF_MAXVARSIZE);
    sscanf(vbuff, "%s", version);       /* trim */

    if (a.packages.package_patch_arch_regex)
    {
        strlcpy(vbuff, ExtractFirstReference(a.packages.package_patch_arch_regex, item), CF_MAXVARSIZE );
        sscanf(vbuff, "%s", arch);      /* trim */
    }
    else
    {
        strlcpy(arch, default_arch, CF_MAXVARSIZE );
    }

    if ((strcmp(name, "CF_NOMATCH") == 0) || (strcmp(version, "CF_NOMATCH") == 0) || (strcmp(arch, "CF_NOMATCH") == 0))
    {
        return false;
    }

    Log(LOG_LEVEL_DEBUG, "PrependPatchItem: Patch line '%s', with name '%s', version '%s', arch '%s'", item, name, version, arch);

    if (PackageInItemList(chklist, name, version, arch))
    {
        Log(LOG_LEVEL_VERBOSE, "Patch for (%s,%s,%s) [name,version,arch] found, but it appears to be installed already", name, version,
              arch);
        return false;
    }

    return PrependPackageItem(ctx, list, name, version, arch, pp);
}

static int PrependMultiLinePackageItem(EvalContext *ctx, PackageItem ** list, char *item, int reset, const char *default_arch,
                                       Attributes a, const Promise *pp)
{
    static char name[CF_MAXVARSIZE] = ""; /* GLOBAL_X */
    static char arch[CF_MAXVARSIZE] = ""; /* GLOBAL_X */
    static char version[CF_MAXVARSIZE] = ""; /* GLOBAL_X */
    static char vbuff[CF_MAXVARSIZE] = ""; /* GLOBAL_X */

    if (reset)
    {
        if ((strcmp(name, "CF_NOMATCH") == 0) || (strcmp(version, "CF_NOMATCH") == 0))
        {
            return false;
        }

        if ((strcmp(name, "") != 0) || (strcmp(version, "") != 0))
        {
            Log(LOG_LEVEL_DEBUG, "PrependMultiLinePackageItem: Extracted package name '%s', version '%s', arch '%s'", name, version, arch);
            PrependPackageItem(ctx, list, name, version, arch, pp);
        }

        strcpy(name, "CF_NOMATCH");
        strcpy(version, "CF_NOMATCH");
        strcpy(arch, default_arch);
    }

    if (FullTextMatch(ctx, a.packages.package_list_name_regex, item))
    {
        strlcpy(vbuff, ExtractFirstReference(a.packages.package_list_name_regex, item), CF_MAXVARSIZE);
        sscanf(vbuff, "%s", name);      /* trim */
    }

    if (FullTextMatch(ctx, a.packages.package_list_version_regex, item))
    {
        strlcpy(vbuff, ExtractFirstReference(a.packages.package_list_version_regex, item), CF_MAXVARSIZE );
        sscanf(vbuff, "%s", version);   /* trim */
    }

    if ((a.packages.package_list_arch_regex) && (FullTextMatch(ctx, a.packages.package_list_arch_regex, item)))
    {
        if (a.packages.package_list_arch_regex)
        {
            strlcpy(vbuff, ExtractFirstReference(a.packages.package_list_arch_regex, item), CF_MAXVARSIZE);
            sscanf(vbuff, "%s", arch);  /* trim */
        }
    }

    return false;
}

static int PrependListPackageItem(EvalContext *ctx, PackageItem ** list,
                                  char *item, const char *default_arch, Attributes a,
                                  const Promise *pp)
{
    char name[CF_MAXVARSIZE];
    char arch[CF_MAXVARSIZE];
    char version[CF_MAXVARSIZE];
    char vbuff[CF_MAXVARSIZE];

    strlcpy(vbuff, ExtractFirstReference(a.packages.package_list_name_regex, item), CF_MAXVARSIZE);
    sscanf(vbuff, "%s", name);  /* trim */

    strlcpy(vbuff, ExtractFirstReference(a.packages.package_list_version_regex, item), CF_MAXVARSIZE);
    sscanf(vbuff, "%s", version);       /* trim */

    if (a.packages.package_list_arch_regex)
    {
        strlcpy(vbuff, ExtractFirstReference(a.packages.package_list_arch_regex, item), CF_MAXVARSIZE);
        sscanf(vbuff, "%s", arch);      /* trim */
    }
    else
    {
        strlcpy(arch, default_arch, CF_MAXVARSIZE);
    }

    if ((strcmp(name, "CF_NOMATCH") == 0) || (strcmp(version, "CF_NOMATCH") == 0) || (strcmp(arch, "CF_NOMATCH") == 0))
    {
        return false;
    }

    Log(LOG_LEVEL_DEBUG, "PrependListPackageItem: Package line '%s', name '%s', version '%s', arch '%s'", item, name, version, arch);

    return PrependPackageItem(ctx, list, name, version, arch, pp);
}

static char *GetDefaultArch(const char *command)
{
    if (command == NULL)
    {
        return xstrdup("default");
    }

    Log(LOG_LEVEL_VERBOSE, "Obtaining default architecture for package manager '%s'", command);

    FILE *fp = cf_popen_sh(command, "r");
    if (fp == NULL)
    {
        return NULL;
    }

    size_t arch_size = CF_SMALLBUF;
    char *arch = xmalloc(arch_size);

    ssize_t res = CfReadLine(&arch, &arch_size, fp);
    if (res == -1)
    {
        cf_pclose(fp);
        free(arch);
        return NULL;
    }

    Log(LOG_LEVEL_VERBOSE, "Default architecture for package manager is '%s'", arch);

    cf_pclose(fp);
    return arch;
}
