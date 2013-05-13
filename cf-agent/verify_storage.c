/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "verify_storage.h"

#include "dir.h"
#include "conversion.h"
#include "files_interfaces.h"
#include "files_operators.h"
#include "files_lib.h"
#include "files_links.h"
#include "files_properties.h"
#include "attributes.h"
#include "locks.h"
#include "nfs.h"
#include "rlist.h"
#include "policy.h"
#include "verify_files.h"
#include "promiser_regex_resolver.h"
#include "ornaments.h"
#include "env_context.h"

Rlist *MOUNTEDFSLIST;
int CF_MOUNTALL;

static void FindStoragePromiserObjects(EvalContext *ctx, Promise *pp);
static int VerifyFileSystem(EvalContext *ctx, char *name, Attributes a, Promise *pp);
static int VerifyFreeSpace(EvalContext *ctx, char *file, Attributes a, Promise *pp);
static void VolumeScanArrivals(char *file, Attributes a, Promise *pp);
#if !defined(__MINGW32__)
static int FileSystemMountedCorrectly(Rlist *list, char *name, Attributes a);
static int IsForeignFileSystem(struct stat *childstat, char *dir);
#endif

#ifndef __MINGW32__
static int VerifyMountPromise(EvalContext *ctx, char *file, Attributes a, Promise *pp);
#endif /* !__MINGW32__ */

/*****************************************************************************/

void *FindAndVerifyStoragePromises(EvalContext *ctx, Promise *pp)
{
    PromiseBanner(pp);
    FindStoragePromiserObjects(ctx, pp);

    return (void *) NULL;
}

/*****************************************************************************/

static void FindStoragePromiserObjects(EvalContext *ctx, Promise *pp)
{
/* Check if we are searching over a regular expression */

    LocateFilePromiserGroup(ctx, pp->promiser, pp, VerifyStoragePromise);
}

/*****************************************************************************/

void VerifyStoragePromise(EvalContext *ctx, char *path, Promise *pp)
{
    Attributes a = { {0} };
    CfLock thislock;

    a = GetStorageAttributes(ctx, pp);

#ifdef __MINGW32__
    if (!a.havemount)
    {
        Log(LOG_LEVEL_VERBOSE, "storage.mount is not supported on Windows");
    }
#endif

/* No parameter conflicts here */

    if (a.mount.unmount)
    {
        if ((a.mount.mount_source))
        {
            Log(LOG_LEVEL_VERBOSE, "An unmount promise indicates a mount-source information - probably an error");
        }
        if ((a.mount.mount_server))
        {
            Log(LOG_LEVEL_VERBOSE, "An unmount promise indicates a mount-server information - probably an error");
        }
    }
    else if (a.havemount)
    {
        if ((a.mount.mount_source == NULL) || (a.mount.mount_server == NULL))
        {
            Log(LOG_LEVEL_ERR, "Insufficient specification in mount promise - need source and server");
            return;
        }
    }

    thislock = AcquireLock(ctx, path, VUQNAME, CFSTARTTIME, a.transaction, pp, false);

    if (thislock.lock == NULL)
    {
        return;
    }

/* Do mounts first */

#ifndef __MINGW32__
    if ((!MOUNTEDFSLIST) && (!LoadMountInfo(&MOUNTEDFSLIST)))
    {
        Log(LOG_LEVEL_ERR, "Couldn't obtain a list of mounted filesystems - aborting");
        YieldCurrentLock(thislock);
        return;
    }

    if (a.havemount)
    {
        VerifyMountPromise(ctx, path, a, pp);
    }
#endif /* !__MINGW32__ */

/* Then check file system */

    if (a.havevolume)
    {
        VerifyFileSystem(ctx, path, a, pp);

        if (a.volume.freespace != CF_NOINT)
        {
            VerifyFreeSpace(ctx, path, a, pp);
        }

        if (a.volume.scan_arrivals)
        {
            VolumeScanArrivals(path, a, pp);
        }
    }

    YieldCurrentLock(thislock);
}

/*******************************************************************/
/** Level                                                          */
/*******************************************************************/

static int VerifyFileSystem(EvalContext *ctx, char *name, Attributes a, Promise *pp)
{
    struct stat statbuf, localstat;
    Dir *dirh;
    const struct dirent *dirp;
    off_t sizeinbytes = 0;
    long filecount = 0;
    char buff[CF_BUFSIZE];

    Log(LOG_LEVEL_VERBOSE, "Checking required filesystem %s", name);

    if (stat(name, &statbuf) == -1)
    {
        return (false);
    }

    if (S_ISLNK(statbuf.st_mode))
    {
        KillGhostLink(ctx, name, a, pp);
        return (true);
    }

    if (S_ISDIR(statbuf.st_mode))
    {
        if ((dirh = DirOpen(name)) == NULL)
        {
            Log(LOG_LEVEL_ERR, "Can't open directory '%s' which checking required/disk. (opendir: %s)", name, GetErrorStr());
            return false;
        }

        for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
        {
            if (!ConsiderLocalFile(dirp->d_name, name))
            {
                continue;
            }

            filecount++;

            strcpy(buff, name);

            if (buff[strlen(buff)] != FILE_SEPARATOR)
            {
                strcat(buff, FILE_SEPARATOR_STR);
            }

            strcat(buff, dirp->d_name);

            if (lstat(buff, &localstat) == -1)
            {
                if (S_ISLNK(localstat.st_mode))
                {
                    KillGhostLink(ctx, buff, a, pp);
                    continue;
                }

                Log(LOG_LEVEL_ERR, "Can't stat volume '%s'. (lstat: %s)", buff, GetErrorStr());
                continue;
            }

            sizeinbytes += localstat.st_size;
        }

        DirClose(dirh);

        if (sizeinbytes < 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Internal error: count of byte size was less than zero!");
            return true;
        }

        if (sizeinbytes < a.volume.sensible_size)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "File system %s is suspiciously small! (%jd bytes)\n", name,
                 (intmax_t) sizeinbytes);
            return (false);
        }

        if (filecount < a.volume.sensible_count)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Filesystem %s has only %ld files/directories.\n", name,
                 filecount);
            return (false);
        }
    }

    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, a, "Filesystem %s's content seems to be sensible as promised\n", name);
    return (true);
}

/*******************************************************************/

static int VerifyFreeSpace(EvalContext *ctx, char *file, Attributes a, Promise *pp)
{
    struct stat statbuf;

#ifdef __MINGW32__
    if (!a.volume.check_foreign)
    {
        Log(LOG_LEVEL_VERBOSE, "storage.volume.check_foreign is not supported on Windows (checking every mount)");
    }
#endif /* __MINGW32__ */

    if (stat(file, &statbuf) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't stat '%s' while checking diskspace. (stat: %s)", file, GetErrorStr());
        return true;
    }

#ifndef __MINGW32__
    if (!a.volume.check_foreign)
    {
        if (IsForeignFileSystem(&statbuf, file))
        {
            Log(LOG_LEVEL_INFO, "Filesystem %s is mounted from a foreign system, so skipping it", file);
            return true;
        }
    }
#endif /* !__MINGW32__ */

    if (a.volume.freespace < 0)
    {
        int threshold_percentage = -a.volume.freespace;
        int free_percentage = GetDiskUsage(file, cfpercent);

        if (free_percentage < threshold_percentage)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "Free disk space is under %d%% for volume containing %s (%d%% free)\n",
                 threshold_percentage, file, free_percentage);
            return false;
        }
    }
    else
    {
        off_t threshold = a.volume.freespace;
        off_t free_bytes = GetDiskUsage(file, cfabs);

        if (free_bytes < threshold)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Disk space under %jd kB for volume containing %s (%jd kB free)\n",
                 (intmax_t) (threshold / 1024), file, (intmax_t) (free_bytes / 1024));
            return false;
        }
    }

    return true;
}

/*******************************************************************/

static void VolumeScanArrivals(ARG_UNUSED char *file, ARG_UNUSED Attributes a, ARG_UNUSED Promise *pp)
{
    Log(LOG_LEVEL_VERBOSE, "Scan arrival sequence . not yet implemented");
}

/*******************************************************************/

/*********************************************************************/
/*  Unix-specific implementations                                    */
/*********************************************************************/

#if !defined(__MINGW32__)
static int FileSystemMountedCorrectly(Rlist *list, char *name, Attributes a)
{
    Rlist *rp;
    Mount *mp;
    int found = false;

    for (rp = list; rp != NULL; rp = rp->next)
    {
        mp = (Mount *) rp->item;

        if (mp == NULL)
        {
            continue;
        }

        /* Give primacy to the promised / affected object */

        if (strcmp(name, mp->mounton) == 0)
        {
            /* We have found something mounted on the promiser dir */

            found = true;

            if ((a.mount.mount_source) && (strcmp(mp->source, a.mount.mount_source) != 0))
            {
                Log(LOG_LEVEL_INFO, "A different file system (%s:%s) is mounted on %s than what is promised",
                      mp->host, mp->source, name);
                return false;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "File system %s seems to be mounted correctly", mp->source);
                break;
            }
        }
    }

    if (!found)
    {
        if (!a.mount.unmount)
        {
            Log(LOG_LEVEL_VERBOSE, "File system %s seems not to be mounted correctly", name);
            CF_MOUNTALL = true;
        }
    }

    return found;
}

/*********************************************************************/
/* Mounting */
/*********************************************************************/

static int IsForeignFileSystem(struct stat *childstat, char *dir)
 /* Is FS NFS mounted ? */
{
    struct stat parentstat;
    char vbuff[CF_BUFSIZE];

    strncpy(vbuff, dir, CF_BUFSIZE - 1);

    if (vbuff[strlen(vbuff) - 1] == FILE_SEPARATOR)
    {
        strcat(vbuff, "..");
    }
    else
    {
        strcat(vbuff, FILE_SEPARATOR_STR);
        strcat(vbuff, "..");
    }

    if (stat(vbuff, &parentstat) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Unable to stat '%s'. (stat: %s)", vbuff, GetErrorStr());
        return false;
    }

    if (childstat->st_dev != parentstat.st_dev)
    {
        Rlist *rp;
        Mount *entry;

        Log(LOG_LEVEL_DEBUG, "'%s' is on a different file system, not descending", dir);

        for (rp = MOUNTEDFSLIST; rp != NULL; rp = rp->next)
        {
            entry = (Mount *) rp->item;

            if (!strcmp(entry->mounton, dir))
            {
                if ((entry->options) && (strstr(entry->options, "nfs")))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

static int VerifyMountPromise(EvalContext *ctx, char *name, Attributes a, Promise *pp)
{
    char *options;
    char dir[CF_BUFSIZE];
    int changes = 0;

    Log(LOG_LEVEL_VERBOSE, "Verifying mounted file systems on %s", name);

    snprintf(dir, CF_BUFSIZE, "%s/.", name);

    if (!IsPrivileged())
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Only root can mount filesystems.\n");
        return false;
    }

    options = Rlist2String(a.mount.mount_options, ",");

    if (!FileSystemMountedCorrectly(MOUNTEDFSLIST, name, a))
    {
        if (!a.mount.unmount)
        {
            if (!MakeParentDirectory(dir, a.move_obstructions))
            {
            }

            if (a.mount.editfstab)
            {
                changes += VerifyInFstab(ctx, name, a, pp);
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a,
                     "Filesystem %s was not mounted as promised, and no edits were promised in %s\n", name,
                     VFSTAB[VSYSTEMHARDCLASS]);
                // Mount explicitly
                VerifyMount(ctx, name, a, pp);
            }
        }
        else
        {
            if (a.mount.editfstab)
            {
                changes += VerifyNotInFstab(ctx, name, a, pp);
            }
        }

        if (changes)
        {
            CF_MOUNTALL = true;
        }
    }
    else
    {
        if (a.mount.unmount)
        {
            VerifyUnmount(ctx, name, a, pp);
            if (a.mount.editfstab)
            {
                VerifyNotInFstab(ctx, name, a, pp);
            }
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, a, "Filesystem %s seems to be mounted as promised\n", name);
        }
    }

    free(options);
    return true;
}

#endif /* !__MINGW32__ */

void DeleteStorageContext(void)
{
#ifndef __MINGW32__
    CleanupNFS();

    if (!DONTDO && CF_MOUNTALL)
    {
        Log(LOG_LEVEL_VERBOSE, "Mounting all filesystems");
        MountAll();
    }
#endif /* !__MINGW32__ */
}
