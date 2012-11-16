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

#include "cf3.defs.h"

#include "dir.h"
#include "conversion.h"
#include "files_interfaces.h"
#include "files_operators.h"
#include "attributes.h"
#include "cfstream.h"

static void FindStoragePromiserObjects(Promise *pp, const ReportContext *report_context);
static int VerifyFileSystem(char *name, Attributes a, Promise *pp);
static int VerifyFreeSpace(char *file, Attributes a, Promise *pp);
static void VolumeScanArrivals(char *file, Attributes a, Promise *pp);
#if !defined(__MINGW32__)
static int FileSystemMountedCorrectly(Rlist *list, char *name, char *options, Attributes a, Promise *pp);
static int IsForeignFileSystem(struct stat *childstat, char *dir);
#endif

#ifndef MINGW
static int VerifyMountPromise(char *file, Attributes a, Promise *pp, const ReportContext *report_context);
#endif /* NOT MINGW */

/*****************************************************************************/

void *FindAndVerifyStoragePromises(Promise *pp, const ReportContext *report_context)
{
    PromiseBanner(pp);
    FindStoragePromiserObjects(pp, report_context);

    return (void *) NULL;
}

/*****************************************************************************/

static void FindStoragePromiserObjects(Promise *pp, const ReportContext *report_context)
{
/* Check if we are searching over a regular expression */

    LocateFilePromiserGroup(pp->promiser, pp, VerifyStoragePromise, report_context);
}

/*****************************************************************************/

void VerifyStoragePromise(char *path, Promise *pp, const ReportContext *report_context)
{
    Attributes a = { {0} };
    CfLock thislock;

    a = GetStorageAttributes(pp);

    CF_OCCUR++;

#ifdef MINGW
    if (!a.havemount)
    {
        CfOut(cf_verbose, "", "storage.mount is not supported on Windows");
    }
#endif

/* No parameter conflicts here */

    if (a.mount.unmount)
    {
        if ((a.mount.mount_source) || (a.mount.mount_server))
        {
            CfOut(cf_verbose, "", " !! An unmount promise indicates a mount-source information - probably in error\n");
        }
    }
    else if (a.havemount)
    {
        if ((a.mount.mount_source == NULL) || (a.mount.mount_server == NULL))
        {
            CfOut(cf_error, "", " !! Insufficient specification in mount promise - need source and server\n");
            return;
        }
    }

    thislock = AcquireLock(path, VUQNAME, CFSTARTTIME, a, pp, false);

    if (thislock.lock == NULL)
    {
        return;
    }

/* Do mounts first */

#ifndef MINGW
    if ((!MOUNTEDFSLIST) && (!LoadMountInfo(&MOUNTEDFSLIST)))
    {
        CfOut(cf_error, "", "Couldn't obtain a list of mounted filesystems - aborting\n");
        YieldCurrentLock(thislock);
        return;
    }

    if (a.havemount)
    {
        VerifyMountPromise(path, a, pp, report_context);
    }
#endif /* NOT MINGW */

/* Then check file system */

    if (a.havevolume)
    {
        VerifyFileSystem(path, a, pp);

        if (a.volume.freespace != CF_NOINT)
        {
            VerifyFreeSpace(path, a, pp);
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

static int VerifyFileSystem(char *name, Attributes a, Promise *pp)
{
    struct stat statbuf, localstat;
    Dir *dirh;
    const struct dirent *dirp;
    off_t sizeinbytes = 0;
    long filecount = 0;
    char buff[CF_BUFSIZE];

    CfOut(cf_verbose, "", " -> Checking required filesystem %s\n", name);

    if (cfstat(name, &statbuf) == -1)
    {
        return (false);
    }

    if (S_ISLNK(statbuf.st_mode))
    {
        KillGhostLink(name, a, pp);
        return (true);
    }

    if (S_ISDIR(statbuf.st_mode))
    {
        if ((dirh = OpenDirLocal(name)) == NULL)
        {
            CfOut(cf_error, "opendir", "Can't open directory %s which checking required/disk\n", name);
            return false;
        }

        for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
        {
            if (!ConsiderFile(dirp->d_name, name, a, pp))
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
                    KillGhostLink(buff, a, pp);
                    continue;
                }

                CfOut(cf_error, "lstat", "Can't stat volume %s\n", buff);
                continue;
            }

            sizeinbytes += localstat.st_size;
        }

        CloseDir(dirh);

        if (sizeinbytes < 0)
        {
            CfOut(cf_verbose, "", "Internal error: count of byte size was less than zero!\n");
            return true;
        }

        if (sizeinbytes < a.volume.sensible_size)
        {
            cfPS(cf_error, CF_INTERPT, "", pp, a, " !! File system %s is suspiciously small! (%jd bytes)\n", name,
                 (intmax_t) sizeinbytes);
            return (false);
        }

        if (filecount < a.volume.sensible_count)
        {
            cfPS(cf_error, CF_INTERPT, "", pp, a, " !! Filesystem %s has only %ld files/directories.\n", name,
                 filecount);
            return (false);
        }
    }

    cfPS(cf_inform, CF_NOP, "", pp, a, " -> Filesystem %s's content seems to be sensible as promised\n", name);
    return (true);
}

/*******************************************************************/

static int VerifyFreeSpace(char *file, Attributes a, Promise *pp)
{
    struct stat statbuf;

#ifdef MINGW
    if (!a.volume.check_foreign)
    {
        CfOut(cf_verbose, "", "storage.volume.check_foreign is not supported on Windows (checking every mount)");
    }
#endif /* MINGW */

    if (cfstat(file, &statbuf) == -1)
    {
        CfOut(cf_error, "stat", "Couldn't stat %s checking diskspace\n", file);
        return true;
    }

#ifndef MINGW
    if (!a.volume.check_foreign)
    {
        if (IsForeignFileSystem(&statbuf, file))
        {
            CfOut(cf_inform, "", "Filesystem %s is mounted from a foreign system, so skipping it", file);
            return true;
        }
    }
#endif /* NOT MINGW */

    if (a.volume.freespace < 0)
    {
        int threshold_percentage = -a.volume.freespace;
        int free_percentage = GetDiskUsage(file, cfpercent);

        if (free_percentage < threshold_percentage)
        {
            cfPS(cf_error, CF_FAIL, "", pp, a,
                 " !! Free disk space is under %d%% for volume containing %s (%d%% free)\n",
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
            cfPS(cf_error, CF_FAIL, "", pp, a, " !! Disk space under %jd kB for volume containing %s (%jd kB free)\n",
                 (intmax_t) (threshold / 1024), file, (intmax_t) (free_bytes / 1024));
            return false;
        }
    }

    return true;
}

/*******************************************************************/

static void VolumeScanArrivals(char *file, Attributes a, Promise *pp)
{
    CfOut(cf_verbose, "", "Scan arrival sequence . not yet implemented\n");
}

/*******************************************************************/

/*********************************************************************/
/*  Unix-specific implementations                                    */
/*********************************************************************/

#if !defined(__MINGW32__)
static int FileSystemMountedCorrectly(Rlist *list, char *name, char *options, Attributes a, Promise *pp)
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
                CfOut(cf_inform, "", "A different file system (%s:%s) is mounted on %s than what is promised\n",
                      mp->host, mp->source, name);
                return false;
            }
            else
            {
                CfOut(cf_verbose, "", " -> File system %s seems to be mounted correctly\n", mp->source);
                break;
            }
        }
    }

    if (!found)
    {
        if (!a.mount.unmount)
        {
            CfOut(cf_verbose, "", " !! File system %s seems not to be mounted correctly\n", name);
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

    if (cfstat(vbuff, &parentstat) == -1)
    {
        CfOut(cf_verbose, "stat", " !! Unable to stat %s", vbuff);
        return (false);
    }

    if (childstat->st_dev != parentstat.st_dev)
    {
        Rlist *rp;
        Mount *entry;

        CfDebug("[%s is on a different file system, not descending]\n", dir);

        for (rp = MOUNTEDFSLIST; rp != NULL; rp = rp->next)
        {
            entry = (Mount *) rp->item;

            if (!strcmp(entry->mounton, dir))
            {
                if ((entry->options) && (strstr(entry->options, "nfs")))
                {
                    return (true);
                }
            }
        }
    }

    CfDebug("NotMountedFileSystem\n");
    return (false);
}

static int VerifyMountPromise(char *name, Attributes a, Promise *pp, const ReportContext *report_context)
{
    char *options;
    char dir[CF_BUFSIZE];
    int changes = 0;

    CfOut(cf_verbose, "", " -> Verifying mounted file systems on %s\n", name);

    snprintf(dir, CF_BUFSIZE, "%s/.", name);

    if (!IsPrivileged())
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a, "Only root can mount filesystems.\n");
        return false;
    }

    options = Rlist2String(a.mount.mount_options, ",");

    if (!FileSystemMountedCorrectly(MOUNTEDFSLIST, name, options, a, pp))
    {
        if (!a.mount.unmount)
        {
            if (!MakeParentDirectory(dir, a.move_obstructions, report_context))
            {
            }

            if (a.mount.editfstab)
            {
                changes += VerifyInFstab(name, a, pp);
            }
            else
            {
                cfPS(cf_inform, CF_FAIL, "", pp, a,
                     " -> Filesystem %s was not mounted as promised, and no edits were promised in %s\n", name,
                     VFSTAB[VSYSTEMHARDCLASS]);
                // Mount explicitly
                VerifyMount(name, a, pp);
            }
        }
        else
        {
            if (a.mount.editfstab)
            {
                changes += VerifyNotInFstab(name, a, pp);
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
            VerifyUnmount(name, a, pp);
            if (a.mount.editfstab)
            {
                VerifyNotInFstab(name, a, pp);
            }
        }
        else
        {
            cfPS(cf_inform, CF_NOP, "", pp, a, " -> Filesystem %s seems to be mounted as promised\n", name);
        }
    }

    free(options);
    return true;
}

#endif /* NOT MINGW */
