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

#include "files_operators.h"

#include "acl.h"
#include "env_context.h"
#include "constraints.h"
#include "promises.h"
#include "dir.h"
#include "dbm_api.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "files_repository.h"
#include "vars.h"
#include "item_lib.h"
#include "conversion.h"
#include "expand.h"
#include "scope.h"
#include "matching.h"
#include "attributes.h"
#include "cfstream.h"
#include "client_code.h"
#include "pipes.h"
#include "transaction.h"
#include "logging.h"
#include "string_lib.h"

#include <assert.h>

extern AgentConnection *COMS;


static void DeleteDirectoryTree(char *path, Promise *pp, const ReportContext *report_context);


/*****************************************************************************/

FILE *CreateEmptyStream()
{
    FILE *fp;

    fp = fopen(NULLFILE, "r");

    if (fp == NULL)
    {
        CfOut(cf_error, "", "!! Open of NULLFILE failed");
        return NULL;
    }

// get to EOF
    fgetc(fp);

    if (!feof(fp))
    {
        CfOut(cf_error, "", "!! Could not create empty stream");
        fclose(fp);
        return NULL;
    }

    return fp;
}

/*****************************************************************************/

int CfCreateFile(char *file, Promise *pp, Attributes attr,
                 const ReportContext *report_context)
{
    int fd;

    /* If name ends in /. then this is a directory */

// attr.move_obstructions for MakeParentDirectory

    if (!IsAbsoluteFileName(file))
    {
        cfPS(cf_inform, CF_FAIL, "creat", pp, attr,
             " !! Cannot create a relative filename %s - has no invariant meaning\n", file);
        return false;
    }

    if (strcmp(".", ReadLastNode(file)) == 0)
    {
        CfDebug("File object \"%s \"seems to be a directory\n", file);

        if (!DONTDO && attr.transaction.action != cfa_warn)
        {
            if (!MakeParentDirectory(file, attr.move_obstructions, report_context))
            {
                cfPS(cf_inform, CF_FAIL, "creat", pp, attr, " !! Error creating directories for %s\n", file);
                return false;
            }

            cfPS(cf_inform, CF_CHG, "", pp, attr, " -> Created directory %s\n", file);
        }
        else
        {
            CfOut(cf_error, "", " !! Warning promised, need to create directory %s", file);
            return false;
        }
    }
    else
    {
        if (!DONTDO && attr.transaction.action != cfa_warn)
        {
            mode_t saveumask = umask(0);
            mode_t filemode = 0600;     /* Decide the mode for filecreation */

            if (GetConstraintValue("mode", pp, CF_SCALAR) == NULL)
            {
                /* Relying on umask is risky */
                filemode = 0600;
                CfOut(cf_verbose, "", " -> No mode was set, choose plain file default %ju\n", (uintmax_t)filemode);
            }
            else
            {
                filemode = attr.perms.plus & ~(attr.perms.minus);
            }

            MakeParentDirectory(file, attr.move_obstructions, report_context);

            if ((fd = creat(file, filemode)) == -1)
            {
                cfPS(cf_inform, CF_FAIL, "creat", pp, attr, " !! Error creating file %s, mode = %ju\n", file, (uintmax_t)filemode);
                umask(saveumask);
                return false;
            }
            else
            {
                cfPS(cf_inform, CF_CHG, "", pp, attr, " -> Created file %s, mode = %ju\n", file, (uintmax_t)filemode);
                close(fd);
                umask(saveumask);
            }
        }
        else
        {
            CfOut(cf_error, "", " !! Warning promised, need to create file %s\n", file);
            return false;
        }
    }

    return true;
}

/*****************************************************************************/


/*****************************************************************************/


/*****************************************************************************/



/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int MoveObstruction(char *from, Attributes attr, Promise *pp, const ReportContext *report_context)
{
    struct stat sb;
    char stamp[CF_BUFSIZE], saved[CF_BUFSIZE];
    time_t now_stamp = time((time_t *) NULL);

    if (lstat(from, &sb) == 0)
    {
        if (!attr.move_obstructions)
        {
            cfPS(cf_verbose, CF_FAIL, "", pp, attr, " !! Object %s exists and is obstructing our promise\n", from);
            return false;
        }

        if (!S_ISDIR(sb.st_mode))
        {
            if (DONTDO)
            {
                return false;
            }

            saved[0] = '\0';
            strcpy(saved, from);

            if (attr.copy.backup == cfa_timestamp || attr.edits.backup == cfa_timestamp)
            {
                snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(cf_ctime(&now_stamp)));
                strcat(saved, stamp);
            }

            strcat(saved, CF_SAVED);

            cfPS(cf_verbose, CF_CHG, "", pp, attr, " -> Moving file object %s to %s\n", from, saved);

            if (cf_rename(from, saved) == -1)
            {
                cfPS(cf_error, CF_FAIL, "cf_rename", pp, attr, " !! Can't rename %s to %s\n", from, saved);
                return false;
            }

            if (ArchiveToRepository(saved, attr, pp, report_context))
            {
                unlink(saved);
            }

            return true;
        }

        if (S_ISDIR(sb.st_mode))
        {
            cfPS(cf_verbose, CF_CHG, "", pp, attr, " -> Moving directory %s to %s%s\n", from, from, CF_SAVED);

            if (DONTDO)
            {
                return false;
            }

            saved[0] = '\0';
            strcpy(saved, from);

            snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(cf_ctime(&now_stamp)));
            strcat(saved, stamp);
            strcat(saved, CF_SAVED);
            strcat(saved, ".dir");

            if (cfstat(saved, &sb) != -1)
            {
                cfPS(cf_error, CF_FAIL, "", pp, attr, " !! Couldn't save directory %s, since %s exists already\n", from,
                     saved);
                CfOut(cf_error, "", "Unable to force link to existing directory %s\n", from);
                return false;
            }

            if (cf_rename(from, saved) == -1)
            {
                cfPS(cf_error, CF_FAIL, "cf_rename", pp, attr, "Can't rename %s to %s\n", from, saved);
                return false;
            }
        }
    }

    return true;
}

/*********************************************************************/



/*********************************************************************/


/*********************************************************************/

void VerifyFileChanges(char *file, struct stat *sb, Attributes attr, Promise *pp)
{
    struct stat cmpsb;
    CF_DB *dbp;
    char message[CF_BUFSIZE];
    int ok = true;

    if ((attr.change.report_changes != cfa_statschange) && (attr.change.report_changes != cfa_allchanges))
    {
        return;
    }

    if (!OpenDB(&dbp, dbid_filestats))
    {
        return;
    }

    if (!ReadDB(dbp, file, &cmpsb, sizeof(struct stat)))
    {
        if (!DONTDO)
        {
            WriteDB(dbp, file, sb, sizeof(struct stat));
            CloseDB(dbp);
            return;
        }
    }

    if (cmpsb.st_mode != sb->st_mode)
    {
        ok = false;
    }

    if (cmpsb.st_uid != sb->st_uid)
    {
        ok = false;
    }

    if (cmpsb.st_gid != sb->st_gid)
    {
        ok = false;
    }

    if (cmpsb.st_dev != sb->st_dev)
    {
        ok = false;
    }

    if (cmpsb.st_ino != sb->st_ino)
    {
        ok = false;
    }

    if (cmpsb.st_mtime != sb->st_mtime)
    {
        ok = false;
    }

    if (ok)
    {
        CloseDB(dbp);
        return;
    }

    if (EXCLAIM)
    {
        CfOut(cf_error, "", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }

    if (cmpsb.st_mode != sb->st_mode)
    {
        snprintf(message, CF_BUFSIZE - 1, "ALERT: Permissions for %s changed %jo -> %jo", file,
                 (uintmax_t)cmpsb.st_mode, (uintmax_t)sb->st_mode);
        CfOut(cf_error, "", "%s", message);

        char msg_temp[CF_MAXVARSIZE] = { 0 };
        snprintf(msg_temp, sizeof(msg_temp), "Permission: %jo -> %jo",
                 (uintmax_t)cmpsb.st_mode, (uintmax_t)sb->st_mode);

        LogHashChange(file, cf_file_stats_changed, msg_temp, pp);
    }

    if (cmpsb.st_uid != sb->st_uid)
    {
        snprintf(message, CF_BUFSIZE - 1, "ALERT: owner for %s changed %jd -> %jd", file, (uintmax_t) cmpsb.st_uid,
                 (uintmax_t) sb->st_uid);
        CfOut(cf_error, "", "%s", message);

        char msg_temp[CF_MAXVARSIZE] = { 0 };
        snprintf(msg_temp, sizeof(msg_temp), "Owner: %jd -> %jd",
                 (uintmax_t)cmpsb.st_uid, (uintmax_t)sb->st_uid);

        LogHashChange(file, cf_file_stats_changed, msg_temp, pp);
    }

    if (cmpsb.st_gid != sb->st_gid)
    {
        snprintf(message, CF_BUFSIZE - 1, "ALERT: group for %s changed %jd -> %jd", file, (uintmax_t) cmpsb.st_gid,
                 (uintmax_t) sb->st_gid);
        CfOut(cf_error, "", "%s", message);

        char msg_temp[CF_MAXVARSIZE] = { 0 };
        snprintf(msg_temp, sizeof(msg_temp), "Group: %jd -> %jd",
                 (uintmax_t)cmpsb.st_gid, (uintmax_t)sb->st_gid);

        LogHashChange(file, cf_file_stats_changed, msg_temp, pp);
    }

    if (cmpsb.st_dev != sb->st_dev)
    {
        CfOut(cf_error, "", "ALERT: device for %s changed %jd -> %jd", file, (intmax_t) cmpsb.st_dev,
              (intmax_t) sb->st_dev);
    }

    if (cmpsb.st_ino != sb->st_ino)
    {
        CfOut(cf_error, "", "ALERT: inode for %s changed %ju -> %ju", file, (uintmax_t) cmpsb.st_ino,
              (uintmax_t) sb->st_ino);
    }

    if (cmpsb.st_mtime != sb->st_mtime)
    {
        char from[CF_MAXVARSIZE];
        char to[CF_MAXVARSIZE];

        strcpy(from, cf_ctime(&(cmpsb.st_mtime)));
        strcpy(to, cf_ctime(&(sb->st_mtime)));
        Chop(from);
        Chop(to);
        CfOut(cf_error, "", "ALERT: Last modified time for %s changed %s -> %s", file, from, to);
    }

    if (pp->ref)
    {
        CfOut(cf_error, "", "Preceding promise: %s", pp->ref);
    }

    if (EXCLAIM)
    {
        CfOut(cf_error, "", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }

    if (attr.change.update && !DONTDO)
    {
        DeleteDB(dbp, file);
        WriteDB(dbp, file, sb, sizeof(struct stat));
    }

    CloseDB(dbp);
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

/*******************************************************************/

int MakeParentDirectory2(char *parentandchild, int force, const ReportContext *report_context, bool enforce_promise)
/**
 * Like MakeParentDirectory, but honours warn-only and dry-run mode.
 * We should eventually migrate to this function to avoid making changes
 * in these scenarios.
 **/
{
    if(enforce_promise)
    {
        return MakeParentDirectory(parentandchild, force, report_context);
    }

    char *parent_dir = GetParentDirectoryCopy(parentandchild);

    bool parent_exists = IsDir(parent_dir);

    free(parent_dir);

    return parent_exists;
}

/*******************************************************************/

int MakeParentDirectory(char *parentandchild, int force, const ReportContext *report_context)
/**
 * Please consider using MakeParentDirectory2() instead.
 **/
{
    char *spc, *sp;
    char currentpath[CF_BUFSIZE];
    char pathbuf[CF_BUFSIZE];
    struct stat statbuf;
    mode_t mask;
    int rootlen;
    char Path_File_Separator;

#ifdef DARWIN
/* Keeps track of if dealing w. resource fork */
    int rsrcfork;

    rsrcfork = 0;

    char *tmpstr;
#endif

    CfDebug("Trying to create a parent directory for %s%s", parentandchild, force ? " (force applied)" : "");

    if (!IsAbsoluteFileName(parentandchild))
    {
        CfOut(cf_error, "", "Will not create directories for a relative filename (%s). Has no invariant meaning\n",
              parentandchild);
        return false;
    }

    strncpy(pathbuf, parentandchild, CF_BUFSIZE - 1);   /* local copy */

#ifdef DARWIN
    if (strstr(pathbuf, _PATH_RSRCFORKSPEC) != NULL)
    {
        rsrcfork = 1;
    }
#endif

/* skip link name */
/* This cast is necessary, as  you can't have (char* -> char*)
   and (const char* -> const char*) functions in C */
    sp = (char *) LastFileSeparator(pathbuf);

    if (sp == NULL)
    {
        sp = pathbuf;
    }
    *sp = '\0';

    DeleteSlash(pathbuf);

    if (lstat(pathbuf, &statbuf) != -1)
    {
        if (S_ISLNK(statbuf.st_mode))
        {
            CfOut(cf_verbose, "", "INFO: %s is a symbolic link, not a true directory!\n", pathbuf);
        }

        if (force)              /* force in-the-way directories aside */
        {
            struct stat dir;

            stat(pathbuf, &dir);

            if (!S_ISDIR(dir.st_mode))  /* if the dir exists - no problem */
            {
                struct stat sbuf;

                if (DONTDO)
                {
                    return true;
                }

                strcpy(currentpath, pathbuf);
                DeleteSlash(currentpath);
                strcat(currentpath, ".cf-moved");
                CfOut(cf_inform, "", "Moving obstructing file/link %s to %s to make directory", pathbuf, currentpath);

                /* If cfagent, remove an obstructing backup object */

                if (lstat(currentpath, &sbuf) != -1)
                {
                    if (S_ISDIR(sbuf.st_mode))
                    {
                        DeleteDirectoryTree(currentpath, NULL, report_context);
                    }
                    else
                    {
                        if (unlink(currentpath) == -1)
                        {
                            CfOut(cf_inform, "unlink", "Couldn't remove file/link %s while trying to remove a backup\n",
                                  currentpath);
                        }
                    }
                }

                /* And then move the current object out of the way... */

                if (cf_rename(pathbuf, currentpath) == -1)
                {
                    CfOut(cf_inform, "cf_rename", "Warning. The object %s is not a directory.\n", pathbuf);
                    return (false);
                }
            }
        }
        else
        {
            if (!S_ISLNK(statbuf.st_mode) && !S_ISDIR(statbuf.st_mode))
            {
                CfOut(cf_inform, "",
                      "The object %s is not a directory. Cannot make a new directory without deleting it.", pathbuf);
                return (false);
            }
        }
    }

/* Now we can make a new directory .. */

    currentpath[0] = '\0';

    rootlen = RootDirLength(parentandchild);
    strncpy(currentpath, parentandchild, rootlen);

    for (sp = parentandchild + rootlen, spc = currentpath + rootlen; *sp != '\0'; sp++)
    {
        if (!IsFileSep(*sp) && *sp != '\0')
        {
            *spc = *sp;
            spc++;
        }
        else
        {
            Path_File_Separator = *sp;
            *spc = '\0';

            if (strlen(currentpath) == 0)
            {
            }
            else if (cfstat(currentpath, &statbuf) == -1)
            {
                CfDebug("cfengine: Making directory %s, mode %" PRIoMAX "\n", currentpath, (uintmax_t)DEFAULTMODE);

                if (!DONTDO)
                {
                    mask = umask(0);

                    if (cf_mkdir(currentpath, DEFAULTMODE) == -1)
                    {
                        CfOut(cf_error, "cf_mkdir", "Unable to make directories to %s\n", parentandchild);
                        umask(mask);
                        return (false);
                    }
                    umask(mask);
                }
            }
            else
            {
                if (!S_ISDIR(statbuf.st_mode))
                {
#ifdef DARWIN
                    /* Ck if rsrc fork */
                    if (rsrcfork)
                    {
                        tmpstr = xmalloc(CF_BUFSIZE);
                        strncpy(tmpstr, currentpath, CF_BUFSIZE);
                        strncat(tmpstr, _PATH_FORKSPECIFIER, CF_BUFSIZE);

                        /* Cfengine removed terminating slashes */
                        DeleteSlash(tmpstr);

                        if (strncmp(tmpstr, pathbuf, CF_BUFSIZE) == 0)
                        {
                            free(tmpstr);
                            return (true);
                        }
                        free(tmpstr);
                    }
#endif

                    CfOut(cf_error, "", "Cannot make %s - %s is not a directory! (use forcedirs=true)\n", pathbuf,
                          currentpath);
                    return (false);
                }
            }

            /* *spc = FILE_SEPARATOR; */
            *spc = Path_File_Separator;
            spc++;
        }
    }

    CfDebug("Directory for %s exists. Okay\n", parentandchild);
    return (true);
}

/**********************************************************************/

/*********************************************************************/
static char FileStateToChar(FileState status)
{
    switch(status)
    {
    case cf_file_new:
        return 'N';

    case cf_file_removed:
        return 'R';

    case cf_file_content_changed:
        return 'C';

    case cf_file_stats_changed:
        return 'S';

    default:
        FatalError("Invalid Filechange status supplied");
    }
}
/*********************************************************************/
void LogHashChange(char *file, FileState status, char *msg, Promise *pp)
{
    FILE *fp;
    char fname[CF_BUFSIZE];
    time_t now = time(NULL);
    mode_t perm = 0600;
    static char prevFile[CF_MAXVARSIZE] = { 0 };

// we might get called twice..
    if (strcmp(file, prevFile) == 0)
    {
        return;
    }

    strlcpy(prevFile, file, CF_MAXVARSIZE);

/* This is inefficient but we don't want to lose any data */

    snprintf(fname, CF_BUFSIZE, "%s/state/%s", CFWORKDIR, CF_FILECHANGE_NEW);
    MapName(fname);

#ifndef MINGW
    struct stat sb;
    if (cfstat(fname, &sb) != -1)
    {
        if (sb.st_mode & (S_IWGRP | S_IWOTH))
        {
            CfOut(cf_error, "", "File %s (owner %ju) is writable by others (security exception)", fname, (uintmax_t)sb.st_uid);
        }
    }
#endif /* NOT MINGW */

    if ((fp = fopen(fname, "a")) == NULL)
    {
        CfOut(cf_error, "fopen", "Could not write to the hash change log");
        return;
    }

    const char *handle = PromiseID(pp);

    fprintf(fp, "%ld,%s,%s,%c,%s\n", (long) now, handle, file, FileStateToChar(status), msg);
    fclose(fp);

    cf_chmod(fname, perm);
}

/*******************************************************************/

void RotateFiles(char *name, int number)
{
    int i, fd;
    struct stat statbuf;
    char from[CF_BUFSIZE], to[CF_BUFSIZE];

    if (IsItemIn(ROTATED, name))
    {
        return;
    }

    PrependItem(&ROTATED, name, NULL);

    if (cfstat(name, &statbuf) == -1)
    {
        CfOut(cf_verbose, "", "No access to file %s\n", name);
        return;
    }

    for (i = number - 1; i > 0; i--)
    {
        snprintf(from, CF_BUFSIZE, "%s.%d", name, i);
        snprintf(to, CF_BUFSIZE, "%s.%d", name, i + 1);

        if (cf_rename(from, to) == -1)
        {
            CfDebug("Rename failed in RotateFiles %s -> %s\n", name, from);
        }

        snprintf(from, CF_BUFSIZE, "%s.%d.gz", name, i);
        snprintf(to, CF_BUFSIZE, "%s.%d.gz", name, i + 1);

        if (cf_rename(from, to) == -1)
        {
            CfDebug("Rename failed in RotateFiles %s -> %s\n", name, from);
        }

        snprintf(from, CF_BUFSIZE, "%s.%d.Z", name, i);
        snprintf(to, CF_BUFSIZE, "%s.%d.Z", name, i + 1);

        if (cf_rename(from, to) == -1)
        {
            CfDebug("Rename failed in RotateFiles %s -> %s\n", name, from);
        }

        snprintf(from, CF_BUFSIZE, "%s.%d.bz", name, i);
        snprintf(to, CF_BUFSIZE, "%s.%d.bz", name, i + 1);

        if (cf_rename(from, to) == -1)
        {
            CfDebug("Rename failed in RotateFiles %s -> %s\n", name, from);
        }

        snprintf(from, CF_BUFSIZE, "%s.%d.bz2", name, i);
        snprintf(to, CF_BUFSIZE, "%s.%d.bz2", name, i + 1);

        if (cf_rename(from, to) == -1)
        {
            CfDebug("Rename failed in RotateFiles %s -> %s\n", name, from);
        }
    }

    snprintf(to, CF_BUFSIZE, "%s.1", name);

    if (CopyRegularFileDisk(name, to, false) == -1)
    {
        CfDebug("cfengine: copy failed in RotateFiles %s -> %s\n", name, to);
        return;
    }

    cf_chmod(to, statbuf.st_mode);
    chown(to, statbuf.st_uid, statbuf.st_gid);
    cf_chmod(name, 0600);       /* File must be writable to empty .. */

    if ((fd = creat(name, statbuf.st_mode)) == -1)
    {
        CfOut(cf_error, "creat", "Failed to create new %s in disable(rotate)\n", name);
    }
    else
    {
        chown(name, statbuf.st_uid, statbuf.st_gid);    /* NT doesn't have fchown */
        fchmod(fd, statbuf.st_mode);
        close(fd);
    }
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

static void DeleteDirectoryTree(char *path, Promise *pp, const ReportContext *report_context)
{
    Promise promise = { 0 };
    char s[CF_MAXVARSIZE];
    time_t now = time(NULL);

// Check that tree is a directory

    promise.promiser = path;
    promise.promisee = (Rval) {NULL, CF_NOPROMISEE};
    promise.classes = "any";

    if (pp != NULL)
    {
        promise.bundletype = pp->bundletype;
        promise.offset.line = pp->offset.line;
        promise.bundle = xstrdup(pp->bundle);
        promise.ref = pp->ref;
    }
    else
    {
        promise.bundletype = "agent";
        promise.offset.line = 0;
        promise.bundle = "embedded";
        promise.ref = "Embedded deletion of direction";
    }

    promise.audit = AUDITPTR;
    promise.agentsubtype = "files";
    promise.done = false;
    promise.next = NULL;
    promise.donep = false;

    promise.conlist = NULL;

    snprintf(s, CF_MAXVARSIZE, "0,%ld", (long) now);

    ConstraintAppendToPromise(&promise, "action", (Rval) {"true", CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&promise, "ifelapsed", (Rval) {"0", CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&promise, "delete", (Rval) {"true", CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&promise, "dirlinks", (Rval) {"delete", CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&promise, "rmdirs", (Rval) {"true", CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&promise, "depth_search", (Rval) {"true", CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&promise, "depth", (Rval) {"inf", CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&promise, "file_select", (Rval) {"true", CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&promise, "mtime", (Rval) {s, CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&promise, "file_result", (Rval) {"mtime", CF_SCALAR}, "any", false);

    struct stat sb;
    lstat(path, &sb);

    Attributes a = GetFilesAttributes(&promise);
    SetSearchDevice(&sb, &promise);

    DepthSearch(promise.promiser, &sb, 0, a, &promise, report_context);

    rmdir(path);
}

#ifndef MINGW

/*******************************************************************/
/* Unix-specific implementations of file functions                 */
/*******************************************************************/


/*****************************************************************************/

int VerifyOwner(char *file, Promise *pp, Attributes attr, struct stat *sb)
{
    struct passwd *pw;
    struct group *gp;
    UidList *ulp;
    GidList *glp;
    short uidmatch = false, gidmatch = false;
    uid_t uid = CF_SAME_OWNER;
    gid_t gid = CF_SAME_GROUP;

    CfDebug("VerifyOwner: %" PRIdMAX "\n", (uintmax_t) sb->st_uid);

    for (ulp = attr.perms.owners; ulp != NULL; ulp = ulp->next)
    {
        if (ulp->uid == CF_SAME_OWNER || sb->st_uid == ulp->uid)        /* "same" matches anything */
        {
            uid = ulp->uid;
            uidmatch = true;
            break;
        }
    }

    if (attr.perms.groups->next == NULL && attr.perms.groups->gid == CF_UNKNOWN_GROUP)  // Only one non.existent item
    {
        cfPS(cf_inform, CF_FAIL, "", pp, attr, " !! Unable to make file belong to an unknown group");
    }

    if (attr.perms.owners->next == NULL && attr.perms.owners->uid == CF_UNKNOWN_OWNER)  // Only one non.existent item
    {
        cfPS(cf_inform, CF_FAIL, "", pp, attr, " !! Unable to make file belong to an unknown user");
    }

    for (glp = attr.perms.groups; glp != NULL; glp = glp->next)
    {
        if (glp->gid == CF_SAME_GROUP || sb->st_gid == glp->gid)        /* "same" matches anything */
        {
            gid = glp->gid;
            gidmatch = true;
            break;
        }
    }

    if (uidmatch && gidmatch)
    {
        return false;
    }
    else
    {
        if (!uidmatch)
        {
            for (ulp = attr.perms.owners; ulp != NULL; ulp = ulp->next)
            {
                if (attr.perms.owners->uid != CF_UNKNOWN_OWNER)
                {
                    uid = attr.perms.owners->uid;       /* default is first (not unknown) item in list */
                    break;
                }
            }
        }

        if (!gidmatch)
        {
            for (glp = attr.perms.groups; glp != NULL; glp = glp->next)
            {
                if (attr.perms.groups->gid != CF_UNKNOWN_GROUP)
                {
                    gid = attr.perms.groups->gid;       /* default is first (not unknown) item in list */
                    break;
                }
            }
        }

        switch (attr.transaction.action)
        {
        case cfa_fix:

            if (uid == CF_SAME_OWNER && gid == CF_SAME_GROUP)
            {
                CfOut(cf_verbose, "", " -> Touching %s\n", file);
            }
            else
            {
                if (uid != CF_SAME_OWNER)
                {
                    CfDebug("(Change owner to uid %" PRIuMAX " if possible)\n", (uintmax_t)uid);
                }

                if (gid != CF_SAME_GROUP)
                {
                    CfDebug("Change group to gid %" PRIuMAX " if possible)\n", (uintmax_t)gid);
                }
            }

            if (!DONTDO && S_ISLNK(sb->st_mode))
            {
# ifdef HAVE_LCHOWN
                CfDebug("Using LCHOWN function\n");
                if (lchown(file, uid, gid) == -1)
                {
                    CfOut(cf_inform, "lchown", " !! Cannot set ownership on link %s!\n", file);
                }
                else
                {
                    return true;
                }
# endif
            }
            else if (!DONTDO)
            {
                if (!uidmatch)
                {
                    cfPS(cf_inform, CF_CHG, "", pp, attr, " -> Owner of %s was %ju, setting to %ju", file, (uintmax_t)sb->st_uid,
                         (uintmax_t)uid);
                }

                if (!gidmatch)
                {
                    cfPS(cf_inform, CF_CHG, "", pp, attr, " -> Group of %s was %ju, setting to %ju", file, (uintmax_t)sb->st_gid,
                         (uintmax_t)gid);
                }

                if (!S_ISLNK(sb->st_mode))
                {
                    if (chown(file, uid, gid) == -1)
                    {
                        cfPS(cf_inform, CF_DENIED, "chown", pp, attr, " !! Cannot set ownership on file %s!\n", file);
                    }
                    else
                    {
                        return true;
                    }
                }
            }
            break;

        case cfa_warn:

            if ((pw = getpwuid(sb->st_uid)) == NULL)
            {
                CfOut(cf_error, "", "File %s is not owned by anybody in the passwd database\n", file);
                CfOut(cf_error, "", "(uid = %ju,gid = %ju)\n", (uintmax_t)sb->st_uid, (uintmax_t)sb->st_gid);
                break;
            }

            if ((gp = getgrgid(sb->st_gid)) == NULL)
            {
                cfPS(cf_error, CF_WARN, "", pp, attr, " !! File %s is not owned by any group in group database\n",
                     file);
                break;
            }

            cfPS(cf_error, CF_WARN, "", pp, attr, " !! File %s is owned by [%s], group [%s]\n", file, pw->pw_name,
                 gp->gr_name);
            break;
        }
    }

    return false;
}

/*********************************************************************/

UidList *MakeUidList(char *uidnames)
{
    UidList *uidlist;
    Item *ip, *tmplist;
    char uidbuff[CF_BUFSIZE];
    char *sp;
    int offset;
    struct passwd *pw;
    char *machine, *user, *domain, *usercopy = NULL;
    int uid;
    int tmp = -1;

    uidlist = NULL;

    for (sp = uidnames; *sp != '\0'; sp += strlen(uidbuff))
    {
        if (*sp == ',')
        {
            sp++;
        }

        if (sscanf(sp, "%[^,]", uidbuff))
        {
            if (uidbuff[0] == '+')      /* NIS group - have to do this in a roundabout     */
            {                   /* way because calling getpwnam spoils getnetgrent */
                offset = 1;
                if (uidbuff[1] == '@')
                {
                    offset++;
                }

                setnetgrent(uidbuff + offset);
                tmplist = NULL;

                while (getnetgrent(&machine, &user, &domain))
                {
                    if (user != NULL)
                    {
                        AppendItem(&tmplist, user, NULL);
                    }
                }

                endnetgrent();

                for (ip = tmplist; ip != NULL; ip = ip->next)
                {
                    if ((pw = getpwnam(ip->name)) == NULL)
                    {
                        CfOut(cf_inform, "", " !! Unknown user \'%s\'\n", ip->name);
                        uid = CF_UNKNOWN_OWNER; /* signal user not found */
                        usercopy = ip->name;
                    }
                    else
                    {
                        uid = pw->pw_uid;
                    }

                    AddSimpleUidItem(&uidlist, uid, usercopy);
                }

                DeleteItemList(tmplist);
                continue;
            }

            if (isdigit((int) uidbuff[0]))
            {
                sscanf(uidbuff, "%d", &tmp);
                uid = (uid_t) tmp;
            }
            else
            {
                if (strcmp(uidbuff, "*") == 0)
                {
                    uid = CF_SAME_OWNER;        /* signals wildcard */
                }
                else if ((pw = getpwnam(uidbuff)) == NULL)
                {
                    CfOut(cf_inform, "", "Unknown user \'%s\'\n", uidbuff);
                    uid = CF_UNKNOWN_OWNER;     /* signal user not found */
                    usercopy = uidbuff;
                }
                else
                {
                    uid = pw->pw_uid;
                }
            }

            AddSimpleUidItem(&uidlist, uid, usercopy);
        }
    }

    if (uidlist == NULL)
    {
        AddSimpleUidItem(&uidlist, CF_SAME_OWNER, (char *) NULL);
    }

    return (uidlist);
}

/*********************************************************************/

GidList *MakeGidList(char *gidnames)
{
    GidList *gidlist;
    char gidbuff[CF_BUFSIZE];
    char *sp, *groupcopy = NULL;
    struct group *gr;
    int gid;
    int tmp = -1;

    gidlist = NULL;

    for (sp = gidnames; *sp != '\0'; sp += strlen(gidbuff))
    {
        if (*sp == ',')
        {
            sp++;
        }

        if (sscanf(sp, "%[^,]", gidbuff))
        {
            if (isdigit((int) gidbuff[0]))
            {
                sscanf(gidbuff, "%d", &tmp);
                gid = (gid_t) tmp;
            }
            else
            {
                if (strcmp(gidbuff, "*") == 0)
                {
                    gid = CF_SAME_GROUP;        /* signals wildcard */
                }
                else if ((gr = getgrnam(gidbuff)) == NULL)
                {
                    CfOut(cf_inform, "", " !! Unknown group %s\n", gidbuff);
                    gid = CF_UNKNOWN_GROUP;
                    groupcopy = gidbuff;
                }
                else
                {
                    gid = gr->gr_gid;
                }
            }

            AddSimpleGidItem(&gidlist, gid, groupcopy);
        }
    }

    if (gidlist == NULL)
    {
        AddSimpleGidItem(&gidlist, CF_SAME_GROUP, NULL);
    }

    return (gidlist);
}

/*****************************************************************************/


/*****************************************************************************/


/*******************************************************************/

void AddSimpleUidItem(UidList ** uidlist, uid_t uid, char *uidname)
{
    UidList *ulp = xcalloc(1, sizeof(UidList));

    ulp->uid = uid;

    if (uid == CF_UNKNOWN_OWNER)        /* unknown user */
    {
        ulp->uidname = xstrdup(uidname);
    }

    if (*uidlist == NULL)
    {
        *uidlist = ulp;
    }
    else
    {
        UidList *u;

        for (u = *uidlist; u->next != NULL; u = u->next)
        {
        }
        u->next = ulp;
    }
}

/*******************************************************************/

void AddSimpleGidItem(GidList ** gidlist, gid_t gid, char *gidname)
{
    GidList *glp = xcalloc(1, sizeof(GidList));

    glp->gid = gid;

    if (gid == CF_UNKNOWN_GROUP)        /* unknown group */
    {
        glp->gidname = xstrdup(gidname);
    }

    if (*gidlist == NULL)
    {
        *gidlist = glp;
    }
    else
    {
        GidList *g;

        for (g = *gidlist; g->next != NULL; g = g->next)
        {
        }
        g->next = glp;
    }
}

#endif /* NOT MINGW */

/*********************************************************************/

int SaveAsFile(SaveCallbackFn callback, void *param, const char *file, Attributes a, Promise *pp,
                       const ReportContext *report_context)
{
    struct stat statbuf;
    char new[CF_BUFSIZE], backup[CF_BUFSIZE];
    mode_t mask;
    char stamp[CF_BUFSIZE];
    time_t stamp_now;

#ifdef WITH_SELINUX
    int selinux_enabled = 0;
    security_context_t scontext = NULL;

    selinux_enabled = (is_selinux_enabled() > 0);

    if (selinux_enabled)
    {
        /* get current security context */
        getfilecon(file, &scontext);
    }
#endif

    stamp_now = time((time_t *) NULL);

    if (cfstat(file, &statbuf) == -1)
    {
        cfPS(cf_error, CF_FAIL, "stat", pp, a, " !! Can no longer access file %s, which needed editing!\n", file);
        return false;
    }

    strcpy(backup, file);

    if (a.edits.backup == cfa_timestamp)
    {
        snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(cf_ctime(&stamp_now)));
        strcat(backup, stamp);
    }

    strcat(backup, ".cf-before-edit");

    strcpy(new, file);
    strcat(new, ".cf-after-edit");
    unlink(new);                /* Just in case of races */

    if ((*callback)(new, file, param, a, pp) == false)
    {
        return false;
    }

    if (cf_rename(file, backup) == -1)
    {
        cfPS(cf_error, CF_FAIL, "cf_rename", pp, a,
             " !! Can't rename %s to %s - so promised edits could not be moved into place\n", file, backup);
        return false;
    }

    if (a.edits.backup == cfa_rotate)
    {
        RotateFiles(backup, a.edits.rotate);
        unlink(backup);
    }

    if (a.edits.backup != cfa_nobackup)
    {
        if (ArchiveToRepository(backup, a, pp, report_context))
        {
            unlink(backup);
        }
    }

    else
    {
        unlink(backup);
    }

    if (cf_rename(new, file) == -1)
    {
        cfPS(cf_error, CF_FAIL, "cf_rename", pp, a,
             " !! Can't rename %s to %s - so promised edits could not be moved into place\n", new, file);
        return false;
    }

    mask = umask(0);
    cf_chmod(file, statbuf.st_mode);    /* Restore file permissions etc */
    chown(file, statbuf.st_uid, statbuf.st_gid);
    umask(mask);

#ifdef WITH_SELINUX
    if (selinux_enabled)
    {
        /* restore file context */
        setfilecon(file, scontext);
    }
#endif

    return true;
}

/*********************************************************************/

bool SaveItemListCallback(const char *dest_filename, const char *orig_filename, void *param, Attributes a, Promise *pp)
{
    Item *liststart = param, *ip;
    FILE *fp;

    //saving list to file
    if ((fp = fopen(dest_filename, "w")) == NULL)
    {
        cfPS(cf_error, CF_FAIL, "fopen", pp, a, "Couldn't write file %s after editing\n", dest_filename);
        return false;
    }

    for (ip = liststart; ip != NULL; ip = ip->next)
    {
        fprintf(fp, "%s\n", ip->name);
    }

    if (fclose(fp) == -1)
    {
        cfPS(cf_error, CF_FAIL, "fclose", pp, a, "Unable to close file while writing");
        return false;
    }

    cfPS(cf_inform, CF_CHG, "", pp, a, " -> Edited file %s \n", orig_filename);
    return true;
}

/*********************************************************************/

int SaveItemListAsFile(Item *liststart, const char *file, Attributes a, Promise *pp,
                       const ReportContext *report_context)
{
    return SaveAsFile(&SaveItemListCallback, liststart, file, a, pp, report_context);
}

/*********************************************************************/

int LoadFileAsItemList(Item **liststart, const char *file, Attributes a, const Promise *pp)
{
    FILE *fp;
    struct stat statbuf;
    char line[CF_BUFSIZE], concat[CF_BUFSIZE];
    int join = false;

    if (cfstat(file, &statbuf) == -1)
    {
        CfOut(cf_verbose, "stat", " ** Information: the proposed file \"%s\" could not be loaded", file);
        return false;
    }

    if (a.edits.maxfilesize != 0 && statbuf.st_size > a.edits.maxfilesize)
    {
        CfOut(cf_inform, "", " !! File %s is bigger than the limit edit.max_file_size = %jd > %d bytes\n", file,
              (intmax_t) statbuf.st_size, a.edits.maxfilesize);
        return (false);
    }

    if (!S_ISREG(statbuf.st_mode))
    {
        cfPS(cf_inform, CF_INTERPT, "", pp, a, "%s is not a plain file\n", file);
        return false;
    }

    if ((fp = fopen(file, "r")) == NULL)
    {
        cfPS(cf_inform, CF_INTERPT, "fopen", pp, a, "Couldn't read file %s for editing\n", file);
        return false;
    }

    memset(line, 0, CF_BUFSIZE);
    memset(concat, 0, CF_BUFSIZE);

    while (!feof(fp))
    {
        if (CfReadLine(line, CF_BUFSIZE - 1, fp) == -1)
        {
            FatalError("Error in CfReadLine");
        }

        if (a.edits.joinlines && *(line + strlen(line) - 1) == '\\')
        {
            join = true;
        }
        else
        {
            join = false;
        }

        if (join)
        {
            *(line + strlen(line) - 1) = '\0';
            JoinSuffix(concat, line);
        }
        else
        {
            JoinSuffix(concat, line);

            if (!feof(fp) || (strlen(concat) != 0))
            {
                AppendItem(liststart, concat, NULL);
            }

            concat[0] = '\0';
            join = false;
        }

        line[0] = '\0';
    }

    fclose(fp);
    return (true);
}

int FileSanityChecks(char *path, Attributes a, Promise *pp)
{
    if ((a.havelink) && (a.havecopy))
    {
        CfOut(cf_error, "",
              " !! Promise constraint conflicts - %s file cannot both be a copy of and a link to the source", path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if ((a.havelink) && (!a.link.source))
    {
        CfOut(cf_error, "", " !! Promise to establish a link at %s has no source", path);
        PromiseRef(cf_error, pp);
        return false;
    }

/* We can't do this verification during parsing as we did not yet read the body,
 * so we can't distinguish between link and copy source. In post-verification
 * all bodies are already expanded, so we don't have the information either */

    if ((a.havecopy) && (a.copy.source) && (!FullTextMatch(CF_ABSPATHRANGE, a.copy.source)))
    {
        /* FIXME: somehow redo a PromiseRef to be able to embed it into a string */
        CfOut(cf_error, "", " !! Non-absolute path in source attribute (have no invariant meaning): %s", a.copy.source);
        PromiseRef(cf_error, pp);
        FatalError("Bailing out");
    }

    if ((a.haveeditline) && (a.haveeditxml))
    {
        CfOut(cf_error, "", " !! Promise constraint conflicts - %s editing file as both line and xml makes no sense",
              path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if ((a.havedepthsearch) && (a.haveedit))
    {
        CfOut(cf_error, "", " !! Recursive depth_searches are not compatible with general file editing");
        PromiseRef(cf_error, pp);
        return false;
    }

    if ((a.havedelete) && ((a.create) || (a.havecopy) || (a.haveedit) || (a.haverename)))
    {
        CfOut(cf_error, "", " !! Promise constraint conflicts - %s cannot be deleted and exist at the same time", path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if ((a.haverename) && ((a.create) || (a.havecopy) || (a.haveedit)))
    {
        CfOut(cf_error, "",
              " !! Promise constraint conflicts - %s cannot be renamed/moved and exist there at the same time", path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if ((a.havedelete) && (a.havedepthsearch) && (!a.haveselect))
    {
        CfOut(cf_error, "",
              " !! Dangerous or ambiguous promise - %s specifies recursive deletion but has no file selection criteria",
              path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if ((a.haveselect) && (!a.select.result))
    {
        CfOut(cf_error, "", " !! File select constraint body promised no result (check body definition)");
        PromiseRef(cf_error, pp);
        return false;
    }

    if ((a.havedelete) && (a.haverename))
    {
        CfOut(cf_error, "", " !! File %s cannot promise both deletion and renaming", path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if ((a.havecopy) && (a.havedepthsearch) && (a.havedelete))
    {
        CfOut(cf_inform, "",
              " !! Warning: depth_search of %s applies to both delete and copy, but these refer to different searches (source/destination)",
              pp->promiser);
        PromiseRef(cf_inform, pp);
    }

    if ((a.transaction.background) && (a.transaction.audit))
    {
        CfOut(cf_error, "", " !! Auditing cannot be performed on backgrounded promises (this might change).");
        PromiseRef(cf_error, pp);
        return false;
    }

    if (((a.havecopy) || (a.havelink)) && (a.transformer))
    {
        CfOut(cf_error, "", " !! File object(s) %s cannot both be a copy of source and transformed simultaneously",
              pp->promiser);
        PromiseRef(cf_error, pp);
        return false;
    }

    if ((a.haveselect) && (a.select.result == NULL))
    {
        CfOut(cf_error, "", " !! Missing file_result attribute in file_select body");
        PromiseRef(cf_error, pp);
        return false;
    }

    if ((a.havedepthsearch) && (a.change.report_diffs))
    {
        CfOut(cf_error, "", " !! Difference reporting is not allowed during a depth_search");
        PromiseRef(cf_error, pp);
        return false;
    }

    return true;
}
