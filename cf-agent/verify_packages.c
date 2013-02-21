/* 
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.

*/

#include "verify_packages.h"

#include "promises.h"
#include "dir.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "vars.h"
#include "conversion.h"
#include "expand.h"
#include "scope.h"
#include "vercmp.h"
#include "matching.h"
#include "attributes.h"
#include "cfstream.h"
#include "string_lib.h"
#include "pipes.h"
#include "transaction.h"
#include "logging.h"
#include "exec_tools.h"
#include "policy.h"
#include "misc_lib.h"
#include "rlist.h"

#ifdef HAVE_NOVA
#include "agent_reports.h"
#endif

/** Entry points from VerifyPackagesPromise **/

static int PackageSanityCheck(Attributes a, Promise *pp);

static int VerifyInstalledPackages(PackageManager **alllists, const char *default_arch, Attributes a, Promise *pp);

static void VerifyPromisedPackage(Attributes a, Promise *pp);
static void VerifyPromisedPatch(Attributes a, Promise *pp);

/** Utils **/

static char *GetDefaultArch(const char *command);

static int ExecPackageCommand(char *command, int verify, int setCmdClasses, Attributes a, Promise *pp);

static int PrependPatchItem(PackageItem ** list, char *item, PackageItem * chklist, const char *default_arch, Attributes a, Promise *pp);
static int PrependMultiLinePackageItem(PackageItem ** list, char *item, int reset, const char *default_arch, Attributes a, Promise *pp);
static int PrependListPackageItem(PackageItem ** list, char *item, const char *default_arch, Attributes a, Promise *pp);

static PackageManager *NewPackageManager(PackageManager **lists, char *mgr, PackageAction pa, PackageActionPolicy x);
static void DeletePackageManagers(PackageManager *newlist);

static char *PrefixLocalRepository(Rlist *repositories, char *package);

#ifndef HAVE_NOVA
void ReportPatches(PackageManager *list)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "# Patch reporting feature is only available in version Nova and above\n");
}
#endif

/*****************************************************************************/

PackageManager *PACKAGE_SCHEDULE = NULL;
PackageManager *INSTALLED_PACKAGE_LISTS = NULL;

#define PACKAGE_LIST_COMMAND_WINAPI "/Windows_API"

/*****************************************************************************/

void VerifyPackagesPromise(Promise *pp)
{
    Attributes a = { {0} };
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetPackageAttributes(pp);

#ifdef __MINGW32__

    if(!a.packages.package_list_command)
    {
        a.packages.package_list_command = PACKAGE_LIST_COMMAND_WINAPI;
    }

#endif

    if (!PackageSanityCheck(a, pp))
    {
        return;
    }

    PromiseBanner(pp);

// Now verify the package itself

    snprintf(lockname, CF_BUFSIZE - 1, "package-%s-%s", pp->promiser, a.packages.package_list_command);

    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, false);

    if (thislock.lock == NULL)
    {
        return;
    }

// Start by reseting the root directory in case yum tries to glob regexs(!)

    if (chdir("/") != 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Failed to chdir into '/'");
    }

    char *default_arch = GetDefaultArch(a.packages.package_default_arch_command);

    if (default_arch == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, " !! Unable to obtain default architecture for package manager - aborting");
        return;
    }

    if (!VerifyInstalledPackages(&INSTALLED_PACKAGE_LISTS, default_arch, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, " !! Unable to obtain a list of installed packages - aborting");
        free(default_arch);
        return;
    }

    free(default_arch);

    switch (a.packages.package_policy)
    {
    case PACKAGE_ACTION_PATCH:
        VerifyPromisedPatch(a, pp);
        break;

    default:
        VerifyPromisedPackage(a, pp);
        break;
    }

    YieldCurrentLock(thislock);
}

/** Pre-check of promise contents **/

static int PackageSanityCheck(Attributes a, Promise *pp)
{
#ifndef __MINGW32__  // Windows may use Win32 API for listing and parsing

    if (a.packages.package_list_name_regex == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
             " !! You must supply a method for determining the name of existing packages e.g. use the standard library generic package_method");
        return false;
    }
    
    if (a.packages.package_list_version_regex == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
             " !! You must supply a method for determining the version of existing packages e.g. use the standard library generic package_method");
        return false;
    }

    if ((!a.packages.package_commands_useshell) && (a.packages.package_list_command) && (!IsExecutable(CommandArg0(a.packages.package_list_command))))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
             "The proposed package list command \"%s\" was not executable",
             a.packages.package_list_command);
        return false;
    }


#endif /* !__MINGW32__ */


    if ((a.packages.package_list_command == NULL) && (a.packages.package_file_repositories == NULL))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
             " !! You must supply a method for determining the list of existing packages (a command or repository list) e.g. use the standard library generic package_method");
        return false;
    }

    if (a.packages.package_file_repositories)
    {
        Rlist *rp;

        for (rp = a.packages.package_file_repositories; rp != NULL; rp = rp->next)
        {
            if (strlen(rp->item) > CF_MAXVARSIZE - 1)
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, " !! The repository path \"%s\" is too long", RlistScalarValue(rp));
                return false;
            }
        }
    }

    if ((a.packages.package_name_regex) || (a.packages.package_version_regex) || (a.packages.package_arch_regex))
    {
        if (a.packages.package_name_regex == NULL)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, " !! You must supply name regex if you supplied version or arch regex for parsing promiser string");
            return false;
        }
        if ((a.packages.package_name_regex) && (a.packages.package_version_regex) && (a.packages.package_arch_regex))
        {
            if ((a.packages.package_version) || (a.packages.package_architectures))
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                     " !! You must either supply all regexs for (name,version,arch) or a separate version number and architecture");
                return false;
            }
        }
        else
        {
            if ((a.packages.package_version) && (a.packages.package_architectures))
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                     " !! You must either supply all regexs for (name,version,arch) or a separate version number and architecture");
                return false;
            }
        }

        if ((a.packages.package_version_regex) && (a.packages.package_version))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                 " !! You must either supply version regex or a separate version number");
            return false;
        }

        if ((a.packages.package_arch_regex) && (a.packages.package_architectures))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                 " !! You must either supply arch regex or a separate architecture");
            return false;
        }
    }

    if (!a.packages.package_installed_regex)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "!! Package installed regex undefined");
        return false;
    }

    if (a.packages.package_policy == PACKAGE_ACTION_VERIFY)
    {
        if (!a.packages.package_verify_command)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Package verify policy is used, but no package_verify_command is defined");
            return false;
        }
        else if ((a.packages.package_noverify_returncode == CF_NOINT) && (a.packages.package_noverify_regex == NULL))
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Package verify policy is used, but no definition of verification failiure is set (package_noverify_returncode or packages.package_noverify_regex)");
            return false;
        }
    }

    if ((a.packages.package_noverify_returncode != CF_NOINT) && (a.packages.package_noverify_regex))
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
             "!! Both package_noverify_returncode and package_noverify_regex are defined, pick one of them");
        return false;
    }

    /* Dependency checks */
    if (!a.packages.package_delete_command)
    {
        if (a.packages.package_delete_convention)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Dependency conflict: package_delete_command is not used, but package_delete_convention is defined.");
            return false;
        }
    }
    if (!a.packages.package_list_command)
    {
        if (a.packages.package_installed_regex)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Dependency conflict: package_list_command is not used, but package_installed_regex is defined.");
            return false;
        }
        if (a.packages.package_list_arch_regex)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Dependency conflict: package_list_command is not used, but package_arch_regex is defined.");
            return false;
        }
        if (a.packages.package_list_name_regex)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Dependency conflict: package_list_command is not used, but package_name_regex is defined.");
            return false;
        }
        if (a.packages.package_list_version_regex)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Dependency conflict: package_list_command is not used, but package_version_regex is defined.");
            return false;
        }
    }
    if (!a.packages.package_patch_command)
    {
        if (a.packages.package_patch_arch_regex)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Dependency conflict: package_patch_command is not used, but package_patch_arch_regex is defined.");
            return false;
        }
        if (a.packages.package_patch_name_regex)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Dependency conflict: package_patch_command is not used, but package_patch_name_regex is defined.");
            return false;
        }
        if (a.packages.package_patch_version_regex)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Dependency conflict: package_patch_command is not used, but package_patch_version_regex is defined.");
            return false;
        }
    }
    if (!a.packages.package_patch_list_command)
    {
        if (a.packages.package_patch_installed_regex)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Dependency conflict: package_patch_list_command is not used, but package_patch_installed_regex is defined.");
            return false;
        }
    }
    if (!a.packages.package_verify_command)
    {
        if (a.packages.package_noverify_regex)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Dependency conflict: package_verify_command is not used, but package_noverify_regex is defined.");
            return false;
        }
        if (a.packages.package_noverify_returncode != CF_NOINT)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                 "!! Dependency conflict: package_verify_command is not used, but package_noverify_returncode is defined.");
            return false;
        }
    }
    return true;
}

/** Get the list of installed packages **/

static bool PackageListInstalledFromCommand(PackageItem **installed_list, const char *default_arch, Attributes a, Promise *pp)
{
    if (a.packages.package_list_update_command != NULL)
    {
        ExecPackageCommand(a.packages.package_list_update_command, false, false, a, pp);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " ???????????????????????????????????????????????????????????????\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "   Reading package list from %s\n", CommandArg0(a.packages.package_list_command));
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " ???????????????????????????????????????????????????????????????\n");

    FILE *fin;
    
    if (a.packages.package_commands_useshell)
    {
        if ((fin = cf_popen_sh(a.packages.package_list_command, "r")) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "cf_popen_sh", "Couldn't open the package list with command %s",
                  a.packages.package_list_command);
            return false;
        }
    }
    else if ((fin = cf_popen(a.packages.package_list_command, "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "cf_popen", "Couldn't open the package list with command %s",
              a.packages.package_list_command);
        return false;
    }

    const int reset = true, update = false;
    char buf[CF_BUFSIZE];
    
    while (!feof(fin))
    {
        memset(buf, 0, CF_BUFSIZE);
        if (CfReadLine(buf, CF_BUFSIZE, fin) == -1)
        {
            FatalError("Error in CfReadLine");
        }
        CF_OCCUR++;

        if (a.packages.package_multiline_start)
        {
            if (FullTextMatch(a.packages.package_multiline_start, buf))
            {
                PrependMultiLinePackageItem(installed_list, buf, reset, default_arch, a, pp);
            }
            else
            {
                PrependMultiLinePackageItem(installed_list, buf, update, default_arch, a, pp);
            }
        }
        else
        {
            if (!FullTextMatch(a.packages.package_installed_regex, buf))
            {
                continue;
            }
            
            if (!PrependListPackageItem(installed_list, buf, default_arch, a, pp))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Package line %s did not match one of the package_list_(name|version|arch)_regex patterns", buf);
                continue;
            }

        }
    }
    
    if (a.packages.package_multiline_start)
    {
        PrependMultiLinePackageItem(installed_list, buf, reset, default_arch, a, pp);
    }
    
    return cf_pclose(fin) == 0;
}

static void ReportSoftware(PackageManager *list)
{
    FILE *fout;
    PackageManager *mp = NULL;
    PackageItem *pi;
    char name[CF_BUFSIZE];

    GetSoftwareCacheFilename(name);

    if ((fout = fopen(name, "w")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Cannot open the destination file %s", name);
        return;
    }

    for (mp = list; mp != NULL; mp = mp->next)
    {
        for (pi = mp->pack_list; pi != NULL; pi = pi->next)
        {
            fprintf(fout, "%s,", CanonifyChar(pi->name, ','));
            fprintf(fout, "%s,", CanonifyChar(pi->version, ','));
            fprintf(fout, "%s,%s\n", pi->arch, ReadLastNode(CommandArg0(mp->manager)));
        }
    }

    fclose(fout);
}

static PackageItem *GetCachedPackageList(PackageManager *manager, const char *default_arch, Attributes a, Promise *pp)
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "",
              " -> Cache file \"%s\" exists and is sufficiently fresh according to (package_list_update_ifelapsed)", name);
    }
    else
    {
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Cache file \"%s\" exists, but it is out of date (package_list_update_ifelapsed)", name);
        return NULL;
    }

    if ((fin = fopen(name, "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "fopen",
              "Cannot open the source log %s - you need to run a package discovery promise to create it in cf-agent",
              name);
        return NULL;
    }

/* Max 2016 entries - at least a week */

    snprintf(thismanager, CF_MAXVARSIZE - 1, "%s", ReadLastNode(CommandArg0(manager->manager)));

    int linenumber = 0;
    while (!feof(fin))
    {
        line[0] = '\0';
        if (fgets(line, CF_BUFSIZE, fin) == NULL)
        {
            if (strlen(line))
            {
                UnexpectedError("Failed to read line %d from stream '%s'", linenumber+1, name);
            }
        }
        ++linenumber;
        int scancount = sscanf(line, "%250[^,],%250[^,],%250[^,],%250[^\n]", name, version, arch, mgr);
        if (scancount != 4)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could only read %d values from line %d in '%s'", scancount, linenumber, name);
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
            CfDebug("READPKG: %s\n", line);
            PrependPackageItem(&list, name, version, arch, a, pp);
        }
    }

    fclose(fin);
    return list;
}

static int VerifyInstalledPackages(PackageManager **all_mgrs, const char *default_arch, Attributes a, Promise *pp)
{
    PackageManager *manager = NewPackageManager(all_mgrs, a.packages.package_list_command, PACKAGE_ACTION_NONE, PACKAGE_ACTION_POLICY_NONE);
    char vbuff[CF_BUFSIZE];

    if (manager == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Can't create a package manager envelope for \"%s\"", a.packages.package_list_command);
        return false;
    }

    if (manager->pack_list != NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?? Already have a package list for this manager ");
        return true;
    }

    manager->pack_list = GetCachedPackageList(manager, default_arch, a, pp);

    if (manager->pack_list != NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?? Already have a (cached) package list for this manager ");
        return true;
    }

#ifdef __MINGW32__

    if (strcmp(a.packages.package_list_command, PACKAGE_LIST_COMMAND_WINAPI) == 0)
    {
        if (!NovaWin_PackageListInstalledFromAPI(&(manager->pack_list), a, pp))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not get list of installed packages");
            return false;
        }
    }
    else
    {
        if(!PackageListInstalledFromCommand(&(manager->pack_list), default_arch, a, pp))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not get list of installed packages");
            return false;
        }
    }

#else

    if (a.packages.package_list_command)
    {
        if(!PackageListInstalledFromCommand(&(manager->pack_list), default_arch, a, pp))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not get list of installed packages");
            return false;
        }
    }
    
#endif /* !__MINGW32__ */

    ReportSoftware(INSTALLED_PACKAGE_LISTS);

/* Now get available updates */

    if (a.packages.package_patch_list_command != NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " ???????????????????????????????????????????????????????????????\n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "   Reading patches from %s\n", CommandArg0(a.packages.package_patch_list_command));
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " ???????????????????????????????????????????????????????????????\n");

        if ((!a.packages.package_commands_useshell) && (!IsExecutable(CommandArg0(a.packages.package_patch_list_command))))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "The proposed patch list command \"%s\" was not executable",
                  a.packages.package_patch_list_command);
            return false;
        }

        FILE *fin;

        if (a.packages.package_commands_useshell)
        {
            if ((fin = cf_popen_sh(a.packages.package_patch_list_command, "r")) == NULL)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "cf_popen_sh", "Couldn't open the patch list with command %s\n",
                      a.packages.package_patch_list_command);
                return false;
            }
        }
        else if ((fin = cf_popen(a.packages.package_patch_list_command, "r")) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "cf_popen", "Couldn't open the patch list with command %s\n",
                  a.packages.package_patch_list_command);
            return false;
        }

        while (!feof(fin))
        {
            memset(vbuff, 0, CF_BUFSIZE);
            if (CfReadLine(vbuff, CF_BUFSIZE, fin) == -1)
            {
                FatalError("Error in CfReadLine");
            }

            // assume patch_list_command lists available patches/updates by default
            if ((a.packages.package_patch_installed_regex == NULL)
                || (!FullTextMatch(a.packages.package_patch_installed_regex, vbuff)))
            {
                PrependPatchItem(&(manager->patch_avail), vbuff, manager->patch_list, default_arch, a, pp);
                continue;
            }

            if (!PrependPatchItem(&(manager->patch_list), vbuff, manager->patch_list, default_arch, a, pp))
            {
                continue;
            }
        }

        cf_pclose(fin);
    }

    ReportPatches(INSTALLED_PACKAGE_LISTS);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " ???????????????????????????????????????????????????????????????\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "  Done checking packages and patches \n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " ???????????????????????????????????????????????????????????????\n");

    return true;
}


/** Evaluate what needs to be done **/

int FindLargestVersionAvail(char *matchName, char *matchVers, const char *refAnyVer, const char *ver,
                            PackageVersionComparator package_select, Rlist *repositories, Attributes a, Promise *pp)
/* Returns true if a version gt/ge ver is found in local repos, false otherwise */
{
    Rlist *rp;
    const struct dirent *dirp;
    char largestVer[CF_MAXVARSIZE];
    char largestVerName[CF_MAXVARSIZE];
    char *matchVer;
    int match;
    Dir *dirh;

    CfDebug("FindLargestVersionAvail()\n");

    match = false;

    // match any version
    if ((strlen(ver) == 0) || (strcmp(ver, "*") == 0))
    {
        memset(largestVer, 0, sizeof(largestVer));
    }
    else
    {
        snprintf(largestVer, sizeof(largestVer), "%s", ver);

        if (package_select == PACKAGE_VERSION_COMPARATOR_GT)   // either gt or ge
        {
            largestVer[strlen(largestVer) - 1]++;
        }
    }

    for (rp = repositories; rp != NULL; rp = rp->next)
    {

        if ((dirh = OpenDirLocal(RlistScalarValue(rp))) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "opendir", "!! Can't open local directory \"%s\"\n", RlistScalarValue(rp));
            continue;
        }

        for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
        {
            if (FullTextMatch(refAnyVer, dirp->d_name))
            {
                matchVer = ExtractFirstReference(refAnyVer, dirp->d_name);

                /* Horrible */
                Attributes a2 = a;
                a2.packages.package_select = PACKAGE_VERSION_COMPARATOR_GT;

                // check if match is largest so far
                if (CompareVersions(matchVer, largestVer, a2, pp) == VERCMP_MATCH)
                {
                    snprintf(largestVer, sizeof(largestVer), "%s", matchVer);
                    snprintf(largestVerName, sizeof(largestVerName), "%s", dirp->d_name);
                    match = true;
                }
            }

        }

        CloseDir(dirh);
    }

    CfDebug("largest ver is \"%s\", name is \"%s\"\n", largestVer, largestVerName);
    CfDebug("match=%d\n", match);

    if (match)
    {
        snprintf(matchName, CF_MAXVARSIZE, "%s", largestVerName);
        snprintf(matchVers, CF_MAXVARSIZE, "%s", largestVer);
    }

    return match;
}

static int IsNewerThanInstalled(const char *n, const char *v, const char *a, char *instV, char *instA, Attributes attr, Promise *pp)
/* Returns true if a package (n, a) is installed and v is larger than
 * the installed version. instV and instA are the version and arch installed. */
{
    PackageManager *mp = NULL;
    PackageItem *pi;

    for (mp = INSTALLED_PACKAGE_LISTS; mp != NULL; mp = mp->next)
    {
        if (strcmp(mp->manager, attr.packages.package_list_command) == 0)
        {
            break;
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Looking for an installed package older than (%s,%s,%s)", n, v, a);

    for (pi = mp->pack_list; pi != NULL; pi = pi->next)
    {
        if ((strcmp(n, pi->name) == 0) && (((strcmp(a, "*") == 0)) || (strcmp(a, pi->arch) == 0)))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Found installed package (%s,%s,%s)", pi->name, pi->version, pi->arch);

            snprintf(instV, CF_MAXVARSIZE, "%s", pi->version);
            snprintf(instA, CF_MAXVARSIZE, "%s", pi->arch);

            /* Horrible */
            Attributes attr2 = attr;
            attr2.packages.package_select = PACKAGE_VERSION_COMPARATOR_LT;

            if (CompareVersions(pi->version, v, attr2, pp) == VERCMP_MATCH)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Package (%s,%s) is not installed\n", n, a);
    return false;
}

static void SchedulePackageOp(const char *name, const char *version, const char *arch, int installed, int matched,
                              int no_version_specified, Attributes a, Promise *pp)
{
    PackageManager *manager;
    char reference[CF_EXPANDSIZE], reference2[CF_EXPANDSIZE];
    char refAnyVer[CF_EXPANDSIZE];
    char refAnyVerEsc[CF_EXPANDSIZE];
    char largestVerAvail[CF_MAXVARSIZE];
    char largestPackAvail[CF_MAXVARSIZE];
    char instVer[CF_MAXVARSIZE];
    char instArch[CF_MAXVARSIZE];
    char idBuf[CF_MAXVARSIZE];
    char *id_del;
    char id[CF_EXPANDSIZE];
    char *pathName = NULL;
    int package_select_in_range = false;
    PackageAction policy;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking if package (%s,%s,%s) is at the desired state (installed=%d,matched=%d)",
          name, version, arch, installed, matched);

/* Now we need to know the name-convention expected by the package manager */

    if ((a.packages.package_name_convention) || (a.packages.package_delete_convention))
    {
        SetNewScope("cf_pack_context");
        NewScalar("cf_pack_context", "name", name, DATA_TYPE_STRING);
        NewScalar("cf_pack_context", "version", version, DATA_TYPE_STRING);
        NewScalar("cf_pack_context", "arch", arch, DATA_TYPE_STRING);

        if ((a.packages.package_delete_convention) && (a.packages.package_policy == PACKAGE_ACTION_DELETE))
        {
            ExpandScalar(a.packages.package_delete_convention, reference);
            strlcpy(id, reference, CF_EXPANDSIZE);
        }
        else if (a.packages.package_name_convention)
        {
            ExpandScalar(a.packages.package_name_convention, reference);
            strlcpy(id, reference, CF_EXPANDSIZE);
        }
        else
        {
            strlcpy(id, name, CF_EXPANDSIZE);
        }

        DeleteScope("cf_pack_context");
    }
    else
    {
        strlcpy(id, name, CF_EXPANDSIZE);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Package promises to refer to itself as \"%s\" to the manager\n", id);

    if (strchr(id, '*'))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "",
              "!! Package name contians '*' -- perhaps a missing attribute (name/version/arch) should be specified");
    }

    if ((a.packages.package_select == PACKAGE_VERSION_COMPARATOR_EQ) || (a.packages.package_select == PACKAGE_VERSION_COMPARATOR_GE) ||
        (a.packages.package_select == PACKAGE_VERSION_COMPARATOR_LE) || (a.packages.package_select == PACKAGE_VERSION_COMPARATOR_NONE))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Package version seems to match criteria");
        package_select_in_range = true;
    }

    policy = a.packages.package_policy;

    if (policy == PACKAGE_ACTION_ADDUPDATE)
    {
        if (!installed)
        {
            policy = PACKAGE_ACTION_ADD;
        }
        else
        {
            policy = PACKAGE_ACTION_UPDATE;
        }
    }

    switch (policy)
    {
    case PACKAGE_ACTION_ADD:

        if (installed == 0)
        {
            if ((a.packages.package_file_repositories != NULL) &&
                ((a.packages.package_select == PACKAGE_VERSION_COMPARATOR_GT) || (a.packages.package_select == PACKAGE_VERSION_COMPARATOR_GE)))
            {
                SetNewScope("cf_pack_context_anyver");
                NewScalar("cf_pack_context_anyver", "name", name, DATA_TYPE_STRING);
                NewScalar("cf_pack_context_anyver", "version", "(.*)", DATA_TYPE_STRING);
                NewScalar("cf_pack_context_anyver", "arch", arch, DATA_TYPE_STRING);
                ExpandScalar(a.packages.package_name_convention, refAnyVer);
                DeleteScope("cf_pack_context_anyver");

                EscapeSpecialChars(refAnyVer, refAnyVerEsc, sizeof(refAnyVerEsc), "(.*)","");

                if (FindLargestVersionAvail
                    (largestPackAvail, largestVerAvail, refAnyVerEsc, version, a.packages.package_select,
                     a.packages.package_file_repositories, a, pp))
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Using latest version in file repositories; \"%s\"", largestPackAvail);
                    strlcpy(id, largestPackAvail, CF_EXPANDSIZE);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "No package in file repositories satisfy version constraint");
                    break;
                }
            }

            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Schedule package for addition\n");

            if (a.packages.package_add_command == NULL)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Package add command undefined");
                return;
            }
            manager =
                NewPackageManager(&PACKAGE_SCHEDULE, a.packages.package_add_command, PACKAGE_ACTION_ADD,
                                  a.packages.package_changes);
            PrependPackageItem(&(manager->pack_list), id, "any", "any", a, pp);
        }
        else
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Package \"%s\" already installed, so we never add it again\n",
                 pp->promiser);
        }
        break;

    case PACKAGE_ACTION_DELETE:

        if ((matched && package_select_in_range) || (installed && no_version_specified))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Schedule package for deletion\n");

            if (a.packages.package_delete_command == NULL)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Package delete command undefined");
                return;
            }
            // expand local repository in the name convetion, if present
            if (a.packages.package_file_repositories)
            {
                // remove any "$(repo)" from the name convention string

                if (strncmp(id, "$(firstrepo)", 12) == 0)
                {
                    snprintf(idBuf, sizeof(idBuf), "%s", id + 12);

                    // and add the correct repo
                    pathName = PrefixLocalRepository(a.packages.package_file_repositories, idBuf);

                    if (pathName)
                    {
                        strlcpy(id, pathName, CF_EXPANDSIZE);
                        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Expanded the package repository to %s", id);
                    }
                    else
                    {
                        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Package \"%s\" can't be found in any of the listed repositories",
                              idBuf);
                    }
                }
            }

            manager =
                NewPackageManager(&PACKAGE_SCHEDULE, a.packages.package_delete_command, PACKAGE_ACTION_DELETE,
                                  a.packages.package_changes);
            PrependPackageItem(&(manager->pack_list), id, "any", "any", a, pp);
        }
        else
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Package deletion is as promised -- no match\n");
        }
        break;

    case PACKAGE_ACTION_REINSTALL:
        if (a.packages.package_delete_command == NULL)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Package delete command undefined");
            return;
        }

        if (!no_version_specified)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Schedule package for reinstallation\n");
            if (a.packages.package_add_command == NULL)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Package add command undefined");
                return;
            }
            if ((matched && package_select_in_range) || (installed && no_version_specified))
            {
                manager =
                    NewPackageManager(&PACKAGE_SCHEDULE, a.packages.package_delete_command, PACKAGE_ACTION_DELETE,
                                      a.packages.package_changes);
                PrependPackageItem(&(manager->pack_list), id, "any", "any", a, pp);
            }
            manager =
                NewPackageManager(&PACKAGE_SCHEDULE, a.packages.package_add_command, PACKAGE_ACTION_ADD,
                                  a.packages.package_changes);
            PrependPackageItem(&(manager->pack_list), id, "any", "any", a, pp);
        }
        else
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                 "!! Package reinstallation cannot be promised -- insufficient version info or no match\n");
        }

        break;

    case PACKAGE_ACTION_UPDATE:

        *instVer = '\0';
        *instArch = '\0';

        if ((a.packages.package_file_repositories != NULL) &&
            ((a.packages.package_select == PACKAGE_VERSION_COMPARATOR_GT) || (a.packages.package_select == PACKAGE_VERSION_COMPARATOR_GE)))
        {
            SetNewScope("cf_pack_context_anyver");
            NewScalar("cf_pack_context_anyver", "name", name, DATA_TYPE_STRING);
            NewScalar("cf_pack_context_anyver", "version", "(.*)", DATA_TYPE_STRING);
            NewScalar("cf_pack_context_anyver", "arch", arch, DATA_TYPE_STRING);
            ExpandScalar(a.packages.package_name_convention, refAnyVer);
            DeleteScope("cf_pack_context_anyver");

            EscapeSpecialChars(refAnyVer, refAnyVerEsc, sizeof(refAnyVerEsc), "(.*)","");

            if (FindLargestVersionAvail
                (largestPackAvail, largestVerAvail, refAnyVerEsc, version, a.packages.package_select,
                 a.packages.package_file_repositories, a, pp))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Using latest version in file repositories; \"%s\"", largestPackAvail);
                strlcpy(id, largestPackAvail, CF_EXPANDSIZE);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "No package in file repositories satisfy version constraint");
                break;
            }
        }
        else
        {
            snprintf(largestVerAvail, sizeof(largestVerAvail), "%s", version);  // user-supplied version
        }

        if (installed)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking if latest available version is newer than installed...");
            if (IsNewerThanInstalled(name, largestVerAvail, arch, instVer, instArch, a, pp))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "",
                      "Installed package (%s,%s,%s) is older than latest available (%s,%s,%s) - updating", name,
                      instVer, instArch, name, largestVerAvail, arch);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Installed package is up to date, not updating");
                break;
            }
        }

        if ((matched && package_select_in_range && (!no_version_specified)) || installed)
        {
            if (a.packages.package_update_command == NULL)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Package update command undefined - failing over to delete then add");

                // we need to have the version of installed package
                if (a.packages.package_delete_convention)
                {
                    if (*instVer == '\0')
                    {
                        instVer[0] = '*';
                        instVer[1] = '\0';
                    }

                    if (*instArch == '\0')
                    {
                        instArch[0] = '*';
                        instArch[1] = '\0';
                    }

                    SetNewScope("cf_pack_context");
                    NewScalar("cf_pack_context", "name", name, DATA_TYPE_STRING);
                    NewScalar("cf_pack_context", "version", instVer, DATA_TYPE_STRING);
                    NewScalar("cf_pack_context", "arch", instArch, DATA_TYPE_STRING);
                    ExpandScalar(a.packages.package_delete_convention, reference2);
                    id_del = reference2;
                    DeleteScope("cf_pack_context");
                }
                else
                {
                    id_del = id;        // defaults to the package_name_convention
                }

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Scheduling package with id \"%s\" for deletion", id_del);

                if (a.packages.package_add_command == NULL)
                {
                    cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Package add command undefined");
                    return;
                }
                if (a.packages.package_delete_command == NULL)
                {
                    cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Package delete command undefined");
                    return;
                }
                manager =
                    NewPackageManager(&PACKAGE_SCHEDULE, a.packages.package_delete_command, PACKAGE_ACTION_DELETE,
                                      a.packages.package_changes);
                PrependPackageItem(&(manager->pack_list), id_del, "any", "any", a, pp);

                manager =
                    NewPackageManager(&PACKAGE_SCHEDULE, a.packages.package_add_command, PACKAGE_ACTION_ADD,
                                      a.packages.package_changes);
                PrependPackageItem(&(manager->pack_list), id, "any", "any", a, pp);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Schedule package for update\n");
                manager =
                    NewPackageManager(&PACKAGE_SCHEDULE, a.packages.package_update_command, PACKAGE_ACTION_UPDATE,
                                      a.packages.package_changes);
                PrependPackageItem(&(manager->pack_list), id, "any", "any", a, pp);
            }
        }
        else
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "!! Package \"%s\" cannot be updated -- no match or not installed",
                 pp->promiser);
        }
        break;

    case PACKAGE_ACTION_PATCH:

        if (matched && (!installed))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Schedule package for patching\n");
            manager =
                NewPackageManager(&PACKAGE_SCHEDULE, a.packages.package_patch_command, PACKAGE_ACTION_PATCH,
                                  a.packages.package_changes);
            PrependPackageItem(&(manager->patch_list), id, "any", "any", a, pp);
        }
        else
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a,
                 " -> Package patch state of \"%s\" is as promised -- already installed\n", pp->promiser);
        }
        break;

    case PACKAGE_ACTION_VERIFY:

        if ((matched && package_select_in_range) || (installed && no_version_specified))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Schedule package for verification\n");
            manager =
                NewPackageManager(&PACKAGE_SCHEDULE, a.packages.package_verify_command, PACKAGE_ACTION_VERIFY,
                                  a.packages.package_changes);
            PrependPackageItem(&(manager->pack_list), id, "any", "any", a, pp);
        }
        else
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, "!! Package \"%s\" cannot be verified -- no match\n", pp->promiser);
        }

        break;

    default:
        break;
    }
}

VersionCmpResult ComparePackages(const char *n, const char *v, const char *arch, PackageItem * pi, Attributes a, Promise *pp)
{
    CfDebug("Compare (%s,%s,%s) and (%s,%s,%s)\n", n, v, arch, pi->name, pi->version, pi->arch);

    if (CompareCSVName(n, pi->name) != 0)
    {
        return VERCMP_NO_MATCH;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Matched name %s\n", n);

    if (strcmp(arch, "*") != 0)
    {
        if (strcmp(arch, pi->arch) != 0)
        {
            return VERCMP_NO_MATCH;
        }

        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Matched arch %s\n", arch);
    }

    if (strcmp(v, "*") == 0)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Matched version *\n");
        return VERCMP_MATCH;
    }

    return CompareVersions(pi->version, v, a, pp);

}

static VersionCmpResult PatchMatch(const char *n, const char *v, const char *a, Attributes attr, Promise *pp)
{
    PackageManager *mp = NULL;
    PackageItem *pi;

    for (mp = INSTALLED_PACKAGE_LISTS; mp != NULL; mp = mp->next)
    {
        if (strcmp(mp->manager, attr.packages.package_list_command) == 0)
        {
            break;
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Looking for (%s,%s,%s)\n", n, v, a);

    for (pi = mp->patch_list; pi != NULL; pi = pi->next)
    {
        if (FullTextMatch(n, pi->name)) /* Check regexes */
        {
            return VERCMP_MATCH;
        }
        else
        {
            VersionCmpResult res = ComparePackages(n, v, a, pi, attr, pp);
            if (res != VERCMP_NO_MATCH)
            {
                return res;
            }
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Unsatisfied constraints in promise (%s,%s,%s)\n", n, v, a);
    return VERCMP_NO_MATCH;
}

static VersionCmpResult PackageMatch(const char *n, const char *v, const char *a, Attributes attr, Promise *pp)
/*
 * Returns VERCMP_MATCH if any installed packages match (n,v,a), VERCMP_NO_MATCH otherwise, VERCMP_ERROR on error.
 */
{
    PackageManager *mp = NULL;
    PackageItem *pi;

    for (mp = INSTALLED_PACKAGE_LISTS; mp != NULL; mp = mp->next)
    {
        if (strcmp(mp->manager, attr.packages.package_list_command) == 0)
        {
            break;
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Looking for (%s,%s,%s)\n", n, v, a);

    for (pi = mp->pack_list; pi != NULL; pi = pi->next)
    {
        VersionCmpResult res = ComparePackages(n, v, a, pi, attr, pp);

        if (res != VERCMP_NO_MATCH)
        {
            return res;
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "No installed packages matched (%s,%s,%s)\n", n, v, a);
    return VERCMP_NO_MATCH;
}

static int VersionCheckSchedulePackage(Attributes a, Promise *pp, int matches, int installed)
{
/* The meaning of matches and installed depends on the package policy */
    PackageAction policy = a.packages.package_policy;

    switch (policy)
    {
    case PACKAGE_ACTION_DELETE:
        if (matches && installed)
        {
            return true;
        }
        break;

    case PACKAGE_ACTION_REINSTALL:
        if (matches && installed)
        {
            return true;
        }
        else
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Package (%s) already installed and matches criteria\n",
                 pp->promiser);
        }
        break;

    default:
        if ((!installed) || (!matches))
        {
            return true;
        }
        else
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Package (%s) already installed and matches criteria\n",
                 pp->promiser);
        }
        break;
    }

    return false;
}

static void CheckPackageState(Attributes a, Promise *pp, const char *name, const char *version, const char *arch, bool no_version)
{
    VersionCmpResult installed = PackageMatch(name, "*", arch, a, pp);
    VersionCmpResult matches = PackageMatch(name, version, arch, a, pp);

    if ((installed == VERCMP_ERROR) || (matches == VERCMP_ERROR))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "Failure trying to compare package versions");
        return;
    }

    if (VersionCheckSchedulePackage(a, pp, matches, installed))
    {
        SchedulePackageOp(name, version, arch, installed, matches, no_version, a, pp);
    }
}

static void VerifyPromisedPatch(Attributes a, Promise *pp)
{
    char version[CF_MAXVARSIZE];
    char name[CF_MAXVARSIZE];
    char arch[CF_MAXVARSIZE];
    char *package = pp->promiser;
    int matches = 0, installed = 0, no_version = false;
    Rlist *rp;

    if (a.packages.package_version)
    {
        /* The version is specified separately */

        for (rp = a.packages.package_architectures; rp != NULL; rp = rp->next)
        {
            strncpy(name, pp->promiser, CF_MAXVARSIZE - 1);
            strncpy(version, a.packages.package_version, CF_MAXVARSIZE - 1);
            strncpy(arch, rp->item, CF_MAXVARSIZE - 1);
            VersionCmpResult installed1 = PatchMatch(name, "*", "*", a, pp);
            VersionCmpResult matches1 = PatchMatch(name, version, arch, a, pp);

            if ((installed1 == VERCMP_ERROR) || (matches1 == VERCMP_ERROR))
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "Failure trying to compare package versions");
                return;
            }

            installed += installed1;
            matches += matches1;
        }

        if (a.packages.package_architectures == NULL)
        {
            strncpy(name, pp->promiser, CF_MAXVARSIZE - 1);
            strncpy(version, a.packages.package_version, CF_MAXVARSIZE - 1);
            strncpy(arch, "*", CF_MAXVARSIZE - 1);
            installed = PatchMatch(name, "*", "*", a, pp);
            matches = PatchMatch(name, version, arch, a, pp);

            if ((installed == VERCMP_ERROR) || (matches == VERCMP_ERROR))
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "Failure trying to compare package versions");
                return;
            }
        }
    }
    else if (a.packages.package_version_regex)
    {
        /* The name, version and arch are to be extracted from the promiser */
        strncpy(version, ExtractFirstReference(a.packages.package_version_regex, package), CF_MAXVARSIZE - 1);
        strncpy(name, ExtractFirstReference(a.packages.package_name_regex, package), CF_MAXVARSIZE - 1);
        strncpy(arch, ExtractFirstReference(a.packages.package_arch_regex, package), CF_MAXVARSIZE - 1);
        installed = PatchMatch(name, "*", "*", a, pp);
        matches = PatchMatch(name, version, arch, a, pp);

        if ((installed == VERCMP_ERROR) || (matches == VERCMP_ERROR))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "Failure trying to compare package versions");
            return;
        }
    }
    else
    {
        no_version = true;

        for (rp = a.packages.package_architectures; rp != NULL; rp = rp->next)
        {
            strncpy(name, pp->promiser, CF_MAXVARSIZE - 1);
            strncpy(version, "*", CF_MAXVARSIZE - 1);
            strncpy(arch, rp->item, CF_MAXVARSIZE - 1);
            VersionCmpResult installed1 = PatchMatch(name, "*", "*", a, pp);
            VersionCmpResult matches1 = PatchMatch(name, version, arch, a, pp);

            if ((installed1 == VERCMP_ERROR) || (matches1 == VERCMP_ERROR))
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "Failure trying to compare package versions");
                return;
            }

            installed += installed1;
            matches += matches1;
        }

        if (a.packages.package_architectures == NULL)
        {
            strncpy(name, pp->promiser, CF_MAXVARSIZE - 1);
            strncpy(version, "*", CF_MAXVARSIZE - 1);
            strncpy(arch, "*", CF_MAXVARSIZE - 1);
            installed = PatchMatch(name, "*", "*", a, pp);
            matches = PatchMatch(name, version, arch, a, pp);

            if ((installed == VERCMP_ERROR) || (matches == VERCMP_ERROR))
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "Failure trying to compare package versions");
                return;
            }
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> %d patch(es) matching the name \"%s\" already installed\n", installed, name);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> %d patch(es) match the promise body's criteria fully\n", matches);

    SchedulePackageOp(name, version, arch, installed, matches, no_version, a, pp);
}

static void VerifyPromisedPackage(Attributes a, Promise *pp)
{
    const char *package = pp->promiser;

    if (a.packages.package_version)
    {
        /* The version is specified separately */
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Package version specified explicitly in promise body");

        if (a.packages.package_architectures == NULL)
        {
            CheckPackageState(a, pp, package, a.packages.package_version, "*", false);
        }
        else
        {
            for (Rlist *rp = a.packages.package_architectures; rp != NULL; rp = rp->next)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " ... trying listed arch %s\n", RlistScalarValue(rp));
                CheckPackageState(a, pp, package, a.packages.package_version, RlistScalarValue(rp), false);
            }
        }
    }
    else if (a.packages.package_version_regex)
    {
        /* The name, version and arch are to be extracted from the promiser */
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Package version specified implicitly in promiser's name");

        char version[CF_MAXVARSIZE];
        char name[CF_MAXVARSIZE];
        char arch[CF_MAXVARSIZE];
        strlcpy(version, ExtractFirstReference(a.packages.package_version_regex, package), CF_MAXVARSIZE);
        strlcpy(name, ExtractFirstReference(a.packages.package_name_regex, package), CF_MAXVARSIZE);
        strlcpy(arch, ExtractFirstReference(a.packages.package_arch_regex, package), CF_MAXVARSIZE);

        if (strlen(arch) == 0)
        {
            strncpy(arch, "*", CF_MAXVARSIZE - 1);
        }

        if (strcmp(arch, "CF_NOMATCH") == 0)    // no match on arch regex, use any arch
        {
            strcpy(arch, "*");
        }

        CheckPackageState(a, pp, name, version, arch, false);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Package version was not specified");

        if (a.packages.package_architectures == NULL)
        {
            CheckPackageState(a, pp, package, "*", "*", true);
        }
        else
        {
            for (Rlist *rp = a.packages.package_architectures; rp != NULL; rp = rp->next)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " ... trying listed arch %s\n", RlistScalarValue(rp));
                CheckPackageState(a, pp, package, "*", rp->item, true);
            }
        }
    }
}

/** Execute scheduled operations **/

static void InvalidateSoftwareCache(void)
{
    char name[CF_BUFSIZE];
    struct utimbuf epoch = { 0, 0 };

    GetSoftwareCacheFilename(name);

    if (utime(name, &epoch) != 0)
    {
        if (errno != ENOENT)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "utimes", "Cannot mark software cache as invalid");
        }
    }
}

static int ExecuteSchedule(PackageManager *schedule, PackageAction action)
{
    PackageItem *pi;
    PackageManager *pm;
    int size, estimated_size, retval = true, verify = false;
    char *command_string = NULL;
    Attributes a = { {0} };
    Promise *pp;
    int ok;

    for (pm = schedule; pm != NULL; pm = pm->next)
    {
        if (pm->action != action)
        {
            continue;
        }

        if (pm->pack_list == NULL)
        {
            continue;
        }

        estimated_size = 0;

        for (pi = pm->pack_list; pi != NULL; pi = pi->next)
        {
            size = strlen(pi->name) + strlen("  ");

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

        pp = pm->pack_list->pp;
        a = GetPackageAttributes(pp);

        switch (action)
        {
        case PACKAGE_ACTION_ADD:

            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Execute scheduled package addition");

            if (a.packages.package_add_command == NULL)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Package add command undefined");
                return false;
            }

            CfOut(OUTPUT_LEVEL_INFORM, "", "Installing %-.39s...\n", pp->promiser);

            command_string = xmalloc(estimated_size + strlen(a.packages.package_add_command) + 2);
            strcpy(command_string, a.packages.package_add_command);
            break;

        case PACKAGE_ACTION_DELETE:

            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Execute scheduled package deletion");

            if (a.packages.package_delete_command == NULL)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Package delete command undefined");
                return false;
            }

            CfOut(OUTPUT_LEVEL_INFORM, "", "Deleting %-.39s...\n", pp->promiser);

            command_string = xmalloc(estimated_size + strlen(a.packages.package_delete_command) + 2);
            strcpy(command_string, a.packages.package_delete_command);
            break;

        case PACKAGE_ACTION_UPDATE:

            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Execute scheduled package update");

            if (a.packages.package_update_command == NULL)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Package update command undefined");
                return false;
            }

            CfOut(OUTPUT_LEVEL_INFORM, "", "Updating %-.39s...\n", pp->promiser);

            command_string = xcalloc(1, estimated_size + strlen(a.packages.package_update_command) + 2);
            strcpy(command_string, a.packages.package_update_command);

            break;

        case PACKAGE_ACTION_VERIFY:

            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Execute scheduled package verification");

            if (a.packages.package_verify_command == NULL)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Package verify command undefined");
                return false;
            }

            command_string = xmalloc(estimated_size + strlen(a.packages.package_verify_command) + 2);
            strcpy(command_string, a.packages.package_verify_command);

            verify = true;
            break;

        default:
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Unknown action attempted");
            return false;
        }

        /* if the command ends with $ then we assume the package manager does not accept package names */

        if (*(command_string + strlen(command_string) - 1) == '$')
        {
            *(command_string + strlen(command_string) - 1) = '\0';
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Command does not allow arguments");
            if (ExecPackageCommand(command_string, verify, true, a, pp))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Package schedule execution ok (outcome cannot be promised by cf-agent)");
            }
            else
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "!! Package schedule execution failed");
            }
        }
        else
        {
            strcat(command_string, " ");

            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Command prefix: %s\n", command_string);

            switch (pm->policy)
            {
            case PACKAGE_ACTION_POLICY_INDIVIDUAL:

                for (pi = pm->pack_list; pi != NULL; pi = pi->next)
                {
                    pp = pi->pp;
                    a = GetPackageAttributes(pp);

                    char *sp, *offset = command_string + strlen(command_string);

                    if ((a.packages.package_file_repositories) && ((action == PACKAGE_ACTION_ADD) || (action == PACKAGE_ACTION_UPDATE)))
                    {
                        if ((sp = PrefixLocalRepository(a.packages.package_file_repositories, pi->name)) != NULL)
                        {
                            strcat(offset, sp);
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        strcat(offset, pi->name);
                    }

                    if (ExecPackageCommand(command_string, verify, true, a, pp))
                    {
                        CfOut(OUTPUT_LEVEL_VERBOSE, "",
                              "Package schedule execution ok for %s (outcome cannot be promised by cf-agent)",
                              pi->name);
                    }
                    else
                    {
                        CfOut(OUTPUT_LEVEL_ERROR, "", "Package schedule execution failed for %s", pi->name);
                    }

                    *offset = '\0';
                }

                break;

            case PACKAGE_ACTION_POLICY_BULK:

                for (pi = pm->pack_list; pi != NULL; pi = pi->next)
                {
                    if (pi->name)
                    {
                        char *sp, *offset = command_string + strlen(command_string);

                        if ((a.packages.package_file_repositories) && ((action == PACKAGE_ACTION_ADD) || (action == PACKAGE_ACTION_UPDATE)))
                        {
                            if ((sp = PrefixLocalRepository(a.packages.package_file_repositories, pi->name)) != NULL)
                            {
                                strcat(offset, sp);
                            }
                            else
                            {
                                break;
                            }
                        }
                        else
                        {
                            strcat(offset, pi->name);
                        }

                        strcat(command_string, " ");
                    }
                }

                ok = ExecPackageCommand(command_string, verify, true, a, pp);

                for (pi = pm->pack_list; pi != NULL; pi = pi->next)
                {
                    if (ok)
                    {
                        CfOut(OUTPUT_LEVEL_VERBOSE, "",
                              "Bulk package schedule execution ok for %s (outcome cannot be promised by cf-agent)",
                              pi->name);
                    }
                    else
                    {
                        CfOut(OUTPUT_LEVEL_ERROR, "", "Bulk package schedule execution failed somewhere - unknown outcome for %s",
                              pi->name);
                    }
                }

                break;

            default:
                break;
            }
        }
    }

    if (command_string)
    {
        free(command_string);
    }

/* We have performed some operation on packages, our cache is invalid */
    InvalidateSoftwareCache();

    return retval;
}

static int ExecutePatch(PackageManager *schedule, PackageAction action)
{
    PackageItem *pi;
    PackageManager *pm;
    int size, estimated_size, retval = true, verify = false;
    char *command_string = NULL;
    Attributes a = { {0} };
    Promise *pp;

    for (pm = schedule; pm != NULL; pm = pm->next)
    {
        if (pm->action != action)
        {
            continue;
        }

        if (pm->patch_list == NULL)
        {
            continue;
        }

        estimated_size = 0;

        for (pi = pm->patch_list; pi != NULL; pi = pi->next)
        {
            size = strlen(pi->name) + strlen("  ");

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

        pp = pm->patch_list->pp;
        a = GetPackageAttributes(pp);

        switch (action)
        {
        case PACKAGE_ACTION_PATCH:

            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Execute scheduled package patch");

            if (a.packages.package_patch_command == NULL)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Package patch command undefined");
                return false;
            }

            command_string = xmalloc(estimated_size + strlen(a.packages.package_patch_command) + 2);
            strcpy(command_string, a.packages.package_patch_command);
            break;

        default:
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, "Unknown action attempted");
            return false;
        }

        /* if the command ends with $ then we assume the package manager does not accept package names */

        if (*(command_string + strlen(command_string) - 1) == '$')
        {
            *(command_string + strlen(command_string) - 1) = '\0';
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Command does not allow arguments");
            if (ExecPackageCommand(command_string, verify, true, a, pp))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Package patching seemed to succeed (outcome cannot be promised by cf-agent)");
            }
            else
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Package patching failed");
            }
        }
        else
        {
            strcat(command_string, " ");

            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Command prefix: %s\n", command_string);

            switch (pm->policy)
            {
                int ok;

            case PACKAGE_ACTION_POLICY_INDIVIDUAL:

                for (pi = pm->patch_list; pi != NULL; pi = pi->next)
                {
                    char *offset = command_string + strlen(command_string);

                    strcat(offset, pi->name);

                    if (ExecPackageCommand(command_string, verify, true, a, pp))
                    {
                        CfOut(OUTPUT_LEVEL_VERBOSE, "",
                              "Package schedule execution ok for %s (outcome cannot be promised by cf-agent)",
                              pi->name);
                    }
                    else
                    {
                        CfOut(OUTPUT_LEVEL_ERROR, "", "Package schedule execution failed for %s", pi->name);
                    }

                    *offset = '\0';
                }

                break;

            case PACKAGE_ACTION_POLICY_BULK:

                for (pi = pm->patch_list; pi != NULL; pi = pi->next)
                {
                    if (pi->name)
                    {
                        strcat(command_string, pi->name);
                        strcat(command_string, " ");
                    }
                }

                ok = ExecPackageCommand(command_string, verify, true, a, pp);

                for (pi = pm->patch_list; pi != NULL; pi = pi->next)
                {
                    if (ok)
                    {
                        CfOut(OUTPUT_LEVEL_VERBOSE, "",
                              "Bulk package schedule execution ok for %s (outcome cannot be promised by cf-agent)",
                              pi->name);
                    }
                    else
                    {
                        CfOut(OUTPUT_LEVEL_ERROR, "", "Bulk package schedule execution failed somewhere - unknown outcome for %s",
                              pi->name);
                    }
                }

                break;

            default:
                break;
            }

        }
    }

    if (command_string)
    {
        free(command_string);
    }

/* We have performed some operation on packages, our cache is invalid */
    InvalidateSoftwareCache();

    return retval;
}

static void ExecutePackageSchedule(PackageManager *schedule)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "   Offering these package-promise suggestions to the managers\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

    /* Normal ordering */

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Deletion schedule...\n");

    if (!ExecuteSchedule(schedule, PACKAGE_ACTION_DELETE))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Aborting package schedule");
        return;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Addition schedule...\n");

    if (!ExecuteSchedule(schedule, PACKAGE_ACTION_ADD))
    {
        return;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Update schedule...\n");

    if (!ExecuteSchedule(schedule, PACKAGE_ACTION_UPDATE))
    {
        return;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Patch schedule...\n");

    if (!ExecutePatch(schedule, PACKAGE_ACTION_PATCH))
    {
        return;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Verify schedule...\n");

    if (!ExecuteSchedule(schedule, PACKAGE_ACTION_VERIFY))
    {
        return;
    }
}

void ExecuteScheduledPackages(void)
{
    if (PACKAGE_SCHEDULE)
    {
        ExecutePackageSchedule(PACKAGE_SCHEDULE);
    }
}

/** Cleanup **/

void CleanScheduledPackages(void)
{
    DeletePackageManagers(PACKAGE_SCHEDULE);
    PACKAGE_SCHEDULE = NULL;
    DeletePackageManagers(INSTALLED_PACKAGE_LISTS);
    INSTALLED_PACKAGE_LISTS = NULL;
}

/** Utils **/

static PackageManager *NewPackageManager(PackageManager **lists, char *mgr, PackageAction pa,
                                         PackageActionPolicy policy)
{
    PackageManager *np;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Looking for a package manager called %s", mgr);

    if ((mgr == NULL) || (strlen(mgr) == 0))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Attempted to create a package manager with no name");
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
    if (pi)
    {
        free(pi->name);
        free(pi->version);
        free(pi->arch);
        PromiseDestroy(pi->pp);
        free(pi);
    }
}

static void DeletePackageManagers(PackageManager *newlist)
{
    PackageManager *np, *next;

    for (np = newlist; np != NULL; np = next)
    {
        next = np->next;
        DeletePackageItems(np->pack_list);
        free((char *) np);
    }
}

char *PrefixLocalRepository(Rlist *repositories, char *package)
{
    static char quotedPath[CF_MAXVARSIZE];
    Rlist *rp;
    struct stat sb;
    char path[CF_BUFSIZE];

    for (rp = repositories; rp != NULL; rp = rp->next)
    {
        strncpy(path, rp->item, CF_MAXVARSIZE);

        AddSlash(path);

        strcat(path, package);

        if (cfstat(path, &sb) != -1)
        {
            snprintf(quotedPath, sizeof(quotedPath), "\"%s\"", path);
            return quotedPath;
        }
    }

    return NULL;
}

int ExecPackageCommand(char *command, int verify, int setCmdClasses, Attributes a, Promise *pp)
{
    int retval = true;
    char line[CF_BUFSIZE], lineSafe[CF_BUFSIZE], *cmd;
    FILE *pfp;
    int packmanRetval = 0;

    if ((!a.packages.package_commands_useshell) && (!IsExecutable(CommandArg0(command))))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "The proposed package schedule command \"%s\" was not executable", command);
        return false;
    }

    if (DONTDO)
    {
        return true;
    }

/* Use this form to avoid limited, intermediate argument processing - long lines */

    if (a.packages.package_commands_useshell)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Running %s in shell", command);
        if ((pfp = cf_popen_sh(command, "r")) == NULL)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "cf_popen_sh", pp, a, "Couldn't start command %20s...\n", command);
            return false;
        }
    }
    else
    {
        if ((pfp = cf_popen(command, "r")) == NULL)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "cf_popen", pp, a, "Couldn't start command %20s...\n", command);
            return false;
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Executing %-.60s...\n", command);

/* Look for short command summary */
    for (cmd = command; (*cmd != '\0') && (*cmd != ' '); cmd++)
    {
    }

    while ((*(cmd - 1) != FILE_SEPARATOR) && (cmd >= command))
    {
        cmd--;
    }

    while (!feof(pfp))
    {
        if (ferror(pfp))        /* abortable */
        {
            fflush(pfp);
            cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "read", pp, a, "Couldn't start command %20s...\n", command);
            break;
        }

        line[0] = '\0';
        if (CfReadLine(line, CF_BUFSIZE - 1, pfp) == -1)
        {
            FatalError("Error in CfReadLine");
        }

        ReplaceStr(line, lineSafe, sizeof(lineSafe), "%", "%%");
        CfOut(OUTPUT_LEVEL_INFORM, "", "Q:%20.20s ...:%s", cmd, lineSafe);

        if (verify && (line[0] != '\0'))
        {
            if (a.packages.package_noverify_regex)
            {
                if (FullTextMatch(a.packages.package_noverify_regex, line))
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, "Package verification error in %-.40s ... :%s", cmd, lineSafe);
                    retval = false;
                }
            }
        }

    }

    packmanRetval = cf_pclose(pfp);

    if (verify && (a.packages.package_noverify_returncode != CF_NOINT))
    {
        if (a.packages.package_noverify_returncode == packmanRetval)
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, "!! Package verification error (returned %d)", packmanRetval);
            retval = false;
        }
        else
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_NOP, "", pp, a, "-> Package verification succeeded (returned %d)", packmanRetval);
        }
    }
    else if (verify && (a.packages.package_noverify_regex))
    {
        if (retval)             // set status if we succeeded above
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_NOP, "", pp, a,
                 "-> Package verification succeeded (no match with package_noverify_regex)");
        }
    }
    else if (setCmdClasses)     // generic return code check
    {
        retval = VerifyCommandRetcode(packmanRetval, true, a, pp);
    }

    return retval;
}

int PrependPackageItem(PackageItem ** list, const char *name, const char *version, const char *arch, Attributes a, Promise *pp)
{
    PackageItem *pi;

    if ((strlen(name) == 0) || (strlen(version) == 0) || (strlen(arch) == 0))
    {
        return false;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Package (%s,%s,%s) found", name, version, arch);

    pi = xmalloc(sizeof(PackageItem));

    if (list)
    {
        pi->next = *list;
    }
    else
    {
        pi->next = NULL;
    }

    pi->name = xstrdup(name);
    pi->version = xstrdup(version);
    pi->arch = xstrdup(arch);
    *list = pi;

/* Finally we need these for later schedule exec, once this iteration context has gone */

    pi->pp = DeRefCopyPromise("this", pp);
    return true;
}

static int PackageInItemList(PackageItem * list, char *name, char *version, char *arch)
{
    PackageItem *pi;

    if ((strlen(name) == 0) || (strlen(version) == 0) || (strlen(arch) == 0))
    {
        return false;
    }

    for (pi = list; pi != NULL; pi = pi->next)
    {
        if ((strcmp(pi->name, name) == 0) && (strcmp(pi->version, version) == 0) && (strcmp(pi->arch, arch) == 0))
        {
            return true;
        }
    }

    return false;
}

static int PrependPatchItem(PackageItem ** list, char *item, PackageItem * chklist, const char *default_arch, Attributes a, Promise *pp)
{
    char name[CF_MAXVARSIZE];
    char arch[CF_MAXVARSIZE];
    char version[CF_MAXVARSIZE];
    char vbuff[CF_MAXVARSIZE];

    strncpy(vbuff, ExtractFirstReference(a.packages.package_patch_name_regex, item), CF_MAXVARSIZE - 1);
    sscanf(vbuff, "%s", name);  /* trim */
    strncpy(vbuff, ExtractFirstReference(a.packages.package_patch_version_regex, item), CF_MAXVARSIZE - 1);
    sscanf(vbuff, "%s", version);       /* trim */

    if (a.packages.package_patch_arch_regex)
    {
        strncpy(vbuff, ExtractFirstReference(a.packages.package_patch_arch_regex, item), CF_MAXVARSIZE - 1);
        sscanf(vbuff, "%s", arch);      /* trim */
    }
    else
    {
        strncpy(arch, default_arch, CF_MAXVARSIZE - 1);
    }

    if ((strcmp(name, "CF_NOMATCH") == 0) || (strcmp(version, "CF_NOMATCH") == 0) || (strcmp(arch, "CF_NOMATCH") == 0))
    {
        return false;
    }

    CfDebug(" ?? Patch line: \"%s\"", item);
    CfDebug(" -?      with name \"%s\"\n", name);
    CfDebug(" -?      with version \"%s\"\n", version);
    CfDebug(" -?      with architecture \"%s\"\n", arch);

    if (PackageInItemList(chklist, name, version, arch))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Patch for (%s,%s,%s) found, but it appears to be installed already", name, version,
              arch);
        return false;
    }

    return PrependPackageItem(list, name, version, arch, a, pp);
}

static int PrependMultiLinePackageItem(PackageItem ** list, char *item, int reset, const char *default_arch, Attributes a, Promise *pp)
{
    static char name[CF_MAXVARSIZE];
    static char arch[CF_MAXVARSIZE];
    static char version[CF_MAXVARSIZE];
    static char vbuff[CF_MAXVARSIZE];

    if (reset)
    {
        if ((strcmp(name, "CF_NOMATCH") == 0) || (strcmp(version, "CF_NOMATCH") == 0))
        {
            return false;
        }

        if ((strcmp(name, "") != 0) || (strcmp(version, "") != 0))
        {
            CfDebug(" -? Extracted package name \"%s\"\n", name);
            CfDebug(" -?      with version \"%s\"\n", version);
            CfDebug(" -?      with architecture \"%s\"\n", arch);

            PrependPackageItem(list, name, version, arch, a, pp);
        }

        strcpy(name, "CF_NOMATCH");
        strcpy(version, "CF_NOMATCH");
        strcpy(arch, default_arch);
    }

    if (FullTextMatch(a.packages.package_list_name_regex, item))
    {
        strlcpy(vbuff, ExtractFirstReference(a.packages.package_list_name_regex, item), CF_MAXVARSIZE);
        sscanf(vbuff, "%s", name);      /* trim */
    }

    if (FullTextMatch(a.packages.package_list_version_regex, item))
    {
        strncpy(vbuff, ExtractFirstReference(a.packages.package_list_version_regex, item), CF_MAXVARSIZE - 1);
        sscanf(vbuff, "%s", version);   /* trim */
    }

    if ((a.packages.package_list_arch_regex) && (FullTextMatch(a.packages.package_list_arch_regex, item)))
    {
        if (a.packages.package_list_arch_regex)
        {
            strncpy(vbuff, ExtractFirstReference(a.packages.package_list_arch_regex, item), CF_MAXVARSIZE - 1);
            sscanf(vbuff, "%s", arch);  /* trim */
        }
    }

    return false;
}

static int PrependListPackageItem(PackageItem ** list, char *item, const char *default_arch, Attributes a, Promise *pp)
{
    char name[CF_MAXVARSIZE];
    char arch[CF_MAXVARSIZE];
    char version[CF_MAXVARSIZE];
    char vbuff[CF_MAXVARSIZE];

    strncpy(vbuff, ExtractFirstReference(a.packages.package_list_name_regex, item), CF_MAXVARSIZE - 1);
    sscanf(vbuff, "%s", name);  /* trim */

    strncpy(vbuff, ExtractFirstReference(a.packages.package_list_version_regex, item), CF_MAXVARSIZE - 1);
    sscanf(vbuff, "%s", version);       /* trim */

    if (a.packages.package_list_arch_regex)
    {
        strncpy(vbuff, ExtractFirstReference(a.packages.package_list_arch_regex, item), CF_MAXVARSIZE - 1);
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

    CfDebug(" -? Package line \"%s\"\n", item);
    CfDebug(" -?      with name \"%s\"\n", name);
    CfDebug(" -?      with version \"%s\"\n", version);
    CfDebug(" -?      with architecture \"%s\"\n", arch);

    return PrependPackageItem(list, name, version, arch, a, pp);
}

static char *GetDefaultArch(const char *command)
{
    if (command == NULL)
    {
        return xstrdup("default");
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Obtaining default architecture for package manager: %s", command);

    FILE *fp = cf_popen_sh(command, "r");
    if (fp == NULL)
    {
        return NULL;
    }

    char arch[CF_BUFSIZE];

    if ((CfReadLine(arch, CF_BUFSIZE, fp) == false) || (strlen(arch) == 0))
    {
        cf_pclose(fp);
        return NULL;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Default architecture for package manager is '%s'", arch);

    cf_pclose(fp);
    return xstrdup(arch);
}
