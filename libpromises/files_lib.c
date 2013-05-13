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

#include "files_lib.h"

#include "files_interfaces.h"
#include "files_names.h"
#include "files_copy.h"
#include "item_lib.h"
#include "promises.h"
#include "matching.h"
#include "misc_lib.h"
#include "dir.h"
#include "policy.h"

#include <assert.h>

static Item *ROTATED = NULL;


bool FileCanOpen(const char *path, const char *modes)
{
    FILE *test = NULL;

    if ((test = fopen(path, modes)) != NULL)
    {
        fclose(test);
        return true;
    }
    else
    {
        return false;
    }
}

/*********************************************************************/

void PurgeItemList(Item **list, char *name)
{
    Item *ip, *copy = NULL;
    struct stat sb;

    CopyList(&copy, *list);

    for (ip = copy; ip != NULL; ip = ip->next)
    {
        if (stat(ip->name, &sb) == -1)
        {
            Log(LOG_LEVEL_VERBOSE, "Purging file \"%s\" from %s list as it no longer exists", ip->name, name);
            DeleteItemLiteral(list, ip->name);
        }
    }

    DeleteItemList(copy);
}

/*********************************************************************/

int RawSaveItemList(const Item *liststart, const char *file)
{
    char new[CF_BUFSIZE], backup[CF_BUFSIZE];
    FILE *fp;

    strcpy(new, file);
    strcat(new, CF_EDITED);

    strcpy(backup, file);
    strcat(backup, CF_SAVED);

    unlink(new);                /* Just in case of races */

    if ((fp = fopen(new, "w")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't write file '%s'. (fopen: %s)", new, GetErrorStr());
        return false;
    }

    for (const Item *ip = liststart; ip != NULL; ip = ip->next)
    {
        fprintf(fp, "%s\n", ip->name);
    }

    if (fclose(fp) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to close file '%s' while writing. (fclose: %s)", new, GetErrorStr());
        return false;
    }

    if (rename(new, file) == -1)
    {
        Log(LOG_LEVEL_INFO, "Error while renaming file '%s' to '%s'. (rename: %s)", new, file, GetErrorStr());
        return false;
    }

    return true;
}


/*********************************************************************/

ssize_t FileRead(const char *filename, char *buffer, size_t bufsize)
{
    FILE *f = fopen(filename, "rb");

    if (f == NULL)
    {
        return -1;
    }
    ssize_t ret = fread(buffer, bufsize, 1, f);

    if (ferror(f))
    {
        fclose(f);
        return -1;
    }
    fclose(f);
    return ret;
}

/*********************************************************************/

bool FileWriteOver(char *filename, char *contents)
{
    FILE *fp = fopen(filename, "w");

    if(fp == NULL)
    {
        return false;
    }

    int bytes_to_write = strlen(contents);

    size_t bytes_written = fwrite(contents, 1, bytes_to_write, fp);

    bool res = true;

    if(bytes_written != bytes_to_write)
    {
        res = false;
    }

    if(fclose(fp) != 0)
    {
        res = false;
    }

    return res;
}


/*********************************************************************/

ssize_t FileReadMax(char **output, const char *filename, size_t size_max)
// TODO: there is CfReadFile and FileRead with slightly different semantics, merge
// free(output) should be called on positive return value
{
    assert(size_max > 0);

    struct stat sb;
    if (stat(filename, &sb) == -1)
    {
        return -1;
    }

    FILE *fin;

    if ((fin = fopen(filename, "r")) == NULL)
    {
        return -1;
    }

    ssize_t bytes_to_read = MIN(sb.st_size, size_max);
    *output = xcalloc(bytes_to_read + 1, 1);
    ssize_t bytes_read = fread(*output, 1, bytes_to_read, fin);

    if (ferror(fin))
    {
        Log(LOG_LEVEL_ERR, "FileContentsRead: Error while reading file '%s'. (ferror: %s)", filename, GetErrorStr());
        fclose(fin);
        free(*output);
        *output = NULL;
        return -1;
    }

    if (fclose(fin) != 0)
    {
        Log(LOG_LEVEL_ERR, "FileContentsRead: Could not close file '%s'. (fclose: %s)", filename, GetErrorStr());
    }

    return bytes_read;
}

/**
 * Like MakeParentDirectory, but honours warn-only and dry-run mode.
 * We should eventually migrate to this function to avoid making changes
 * in these scenarios.
 **/

int MakeParentDirectory2(char *parentandchild, int force, bool enforce_promise)
{
    if(enforce_promise)
    {
        return MakeParentDirectory(parentandchild, force);
    }

    char *parent_dir = GetParentDirectoryCopy(parentandchild);

    if (parent_dir)
    {
        bool parent_exists = IsDir(parent_dir);
        free(parent_dir);
        return parent_exists;
    }
    else
    {
        return false;
    }
}

/**
 * Please consider using MakeParentDirectory2() instead.
 **/

int MakeParentDirectory(char *parentandchild, int force)
{
    char *spc, *sp;
    char currentpath[CF_BUFSIZE];
    char pathbuf[CF_BUFSIZE];
    struct stat statbuf;
    mode_t mask;
    int rootlen;
    char Path_File_Separator;

#ifdef __APPLE__
/* Keeps track of if dealing w. resource fork */
    int rsrcfork;

    rsrcfork = 0;

    char *tmpstr;
#endif

    Log(LOG_LEVEL_DEBUG, "Trying to create a parent directory for '%s%s'", parentandchild, force ? " (force applied)" : "");

    if (!IsAbsoluteFileName(parentandchild))
    {
        Log(LOG_LEVEL_ERR, "Will not create directories for a relative filename '%s'. Has no invariant meaning",
              parentandchild);
        return false;
    }

    strncpy(pathbuf, parentandchild, CF_BUFSIZE - 1);   /* local copy */

#ifdef __APPLE__
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
            Log(LOG_LEVEL_VERBOSE, "INFO: %s is a symbolic link, not a true directory!", pathbuf);
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
                Log(LOG_LEVEL_INFO, "Moving obstructing file/link %s to %s to make directory", pathbuf, currentpath);

                /* If cfagent, remove an obstructing backup object */

                if (lstat(currentpath, &sbuf) != -1)
                {
                    if (S_ISDIR(sbuf.st_mode))
                    {
                        DeleteDirectoryTree(currentpath);
                    }
                    else
                    {
                        if (unlink(currentpath) == -1)
                        {
                            Log(LOG_LEVEL_INFO, "Couldn't remove file/link '%s' while trying to remove a backup. (unlink: %s)",
                                  currentpath, GetErrorStr());
                        }
                    }
                }

                /* And then move the current object out of the way... */

                if (rename(pathbuf, currentpath) == -1)
                {
                    Log(LOG_LEVEL_INFO, "Warning: The object '%s' is not a directory. (rename: %s)", pathbuf, GetErrorStr());
                    return (false);
                }
            }
        }
        else
        {
            if (!S_ISLNK(statbuf.st_mode) && !S_ISDIR(statbuf.st_mode))
            {
                Log(LOG_LEVEL_INFO,
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
            else if (stat(currentpath, &statbuf) == -1)
            {
                if (!DONTDO)
                {
                    mask = umask(0);

                    if (mkdir(currentpath, DEFAULTMODE) == -1)
                    {
                        Log(LOG_LEVEL_ERR, "Unable to make directories to '%s'. (mkdir: %s)", parentandchild, GetErrorStr());
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
#ifdef __APPLE__
                    /* Ck if rsrc fork */
                    if (rsrcfork)
                    {
                        tmpstr = xmalloc(CF_BUFSIZE);
                        strncpy(tmpstr, currentpath, CF_BUFSIZE);
                        strncat(tmpstr, _PATH_FORKSPECIFIER, CF_BUFSIZE);

                        /* CFEngine removed terminating slashes */
                        DeleteSlash(tmpstr);

                        if (strncmp(tmpstr, pathbuf, CF_BUFSIZE) == 0)
                        {
                            free(tmpstr);
                            return (true);
                        }
                        free(tmpstr);
                    }
#endif

                    Log(LOG_LEVEL_ERR, "Cannot make %s - %s is not a directory! (use forcedirs=true)", pathbuf,
                          currentpath);
                    return (false);
                }
            }

            /* *spc = FILE_SEPARATOR; */
            *spc = Path_File_Separator;
            spc++;
        }
    }

    Log(LOG_LEVEL_DEBUG, "Directory for '%s' exists. Okay", parentandchild);
    return (true);
}

int LoadFileAsItemList(Item **liststart, const char *file, EditDefaults edits)
{
    FILE *fp;
    struct stat statbuf;
    char line[CF_BUFSIZE], concat[CF_BUFSIZE];
    int join = false;

    if (stat(file, &statbuf) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "The proposed file '%s' could not be loaded. (stat: %s)", file, GetErrorStr());
        return false;
    }

    if (edits.maxfilesize != 0 && statbuf.st_size > edits.maxfilesize)
    {
        Log(LOG_LEVEL_INFO, "File '%s' is bigger than the limit edit.max_file_size = %jd > %d bytes", file,
              (intmax_t) statbuf.st_size, edits.maxfilesize);
        return (false);
    }

    if (!S_ISREG(statbuf.st_mode))
    {
        Log(LOG_LEVEL_INFO, "%s is not a plain file", file);
        return false;
    }

    if ((fp = fopen(file, "r")) == NULL)
    {
        Log(LOG_LEVEL_INFO, "Couldn't read file '%s' for editing. (fopen: %s)", file, GetErrorStr());
        return false;
    }

    memset(line, 0, CF_BUFSIZE);
    memset(concat, 0, CF_BUFSIZE);

    for (;;)
    {
        ssize_t res = CfReadLine(line, CF_BUFSIZE, fp);
        if (res == 0)
        {
            break;
        }

        if (res == -1)
        {
            Log(LOG_LEVEL_ERR, "Unable to read contents of '%s'. (fread: %s)", file, GetErrorStr());
            fclose(fp);
            return false;
        }

        if (edits.joinlines && *(line + strlen(line) - 1) == '\\')
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

            if (strlcat(concat, line, CF_BUFSIZE) >= CF_BUFSIZE)
            {
                Log(LOG_LEVEL_ERR, "Internal limit 3: Buffer ran out of space constructing string. Tried to add '%s' to '%s'",
                    concat, line);
            }
        }
        else
        {
            if (strlcat(concat, line, CF_BUFSIZE) >= CF_BUFSIZE)
            {
                Log(LOG_LEVEL_ERR, "Internal limit 3: Buffer ran out of space constructing string. Tried to add '%s' to '%s'",
                    concat, line);
            }

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
    return true;
}

static bool DeleteDirectoryTreeInternal(const char *basepath, const char *path)
{
    Dir *dirh = DirOpen(path);
    const struct dirent *dirp;
    bool failed = false;

    if (dirh == NULL)
    {
        if (errno == ENOENT)
        {
            /* Directory disappeared on its own */
            return true;
        }

        Log(LOG_LEVEL_INFO, "Unable to open directory '%s' during purge of directory tree '%s' (opendir: %s)",
            path, basepath, GetErrorStr());
        return false;
    }

    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, ".."))
        {
            continue;
        }

        char subpath[CF_BUFSIZE];
        snprintf(subpath, CF_BUFSIZE, "%s" FILE_SEPARATOR_STR "%s", path, dirp->d_name);

        struct stat lsb;
        if (lstat(subpath, &lsb) == -1)
        {
            if (errno == ENOENT)
            {
                /* File disappeared on its own */
                continue;
            }

            Log(LOG_LEVEL_VERBOSE, "Unable to stat file '%s' during purge of directory tree '%s' (lstat: %s)", path, basepath, GetErrorStr());
            failed = true;
        }
        else
        {
            if (S_ISDIR(lsb.st_mode))
            {
                if (!DeleteDirectoryTreeInternal(basepath, subpath))
                {
                    failed = true;
                }
            }
            else
            {
                if (unlink(subpath) == -1)
                {
                    if (errno == ENOENT)
                    {
                        /* File disappeared on its own */
                        continue;
                    }

                    Log(LOG_LEVEL_VERBOSE, "Unable to remove file '%s' during purge of directory tree '%s'. (unlink: %s)",
                        subpath, basepath, GetErrorStr());
                    failed = true;
                }
            }
        }
    }

    DirClose(dirh);
    return !failed;
}

bool DeleteDirectoryTree(const char *path)
{
    return DeleteDirectoryTreeInternal(path, path);
}

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

    if (stat(name, &statbuf) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "No access to file %s", name);
        return;
    }

    for (i = number - 1; i > 0; i--)
    {
        snprintf(from, CF_BUFSIZE, "%s.%d", name, i);
        snprintf(to, CF_BUFSIZE, "%s.%d", name, i + 1);

        if (rename(from, to) == -1)
        {
            Log(LOG_LEVEL_DEBUG, "Rename failed in RotateFiles '%s' -> '%s'", name, from);
        }

        snprintf(from, CF_BUFSIZE, "%s.%d.gz", name, i);
        snprintf(to, CF_BUFSIZE, "%s.%d.gz", name, i + 1);

        if (rename(from, to) == -1)
        {
            Log(LOG_LEVEL_DEBUG, "Rename failed in RotateFiles '%s' -> '%s'", name, from);
        }

        snprintf(from, CF_BUFSIZE, "%s.%d.Z", name, i);
        snprintf(to, CF_BUFSIZE, "%s.%d.Z", name, i + 1);

        if (rename(from, to) == -1)
        {
            Log(LOG_LEVEL_DEBUG, "Rename failed in RotateFiles '%s' -> '%s'", name, from);
        }

        snprintf(from, CF_BUFSIZE, "%s.%d.bz", name, i);
        snprintf(to, CF_BUFSIZE, "%s.%d.bz", name, i + 1);

        if (rename(from, to) == -1)
        {
            Log(LOG_LEVEL_DEBUG, "Rename failed in RotateFiles '%s' -> '%s'", name, from);
        }

        snprintf(from, CF_BUFSIZE, "%s.%d.bz2", name, i);
        snprintf(to, CF_BUFSIZE, "%s.%d.bz2", name, i + 1);

        if (rename(from, to) == -1)
        {
            Log(LOG_LEVEL_DEBUG, "Rename failed in RotateFiles '%s' -> '%s'", name, from);
        }
    }

    snprintf(to, CF_BUFSIZE, "%s.1", name);

    if (CopyRegularFileDisk(name, to) == false)
    {
        Log(LOG_LEVEL_DEBUG, "Copy failed in RotateFiles '%s' -> '%s'", name, to);
        return;
    }

    chmod(to, statbuf.st_mode);
    if (chown(to, statbuf.st_uid, statbuf.st_gid))
    {
        UnexpectedError("Failed to chown %s", to);
    }
    chmod(name, 0600);       /* File must be writable to empty .. */

    if ((fd = creat(name, statbuf.st_mode)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to create new '%s' in disable(rotate). (creat: %s)",
            name, GetErrorStr());
    }
    else
    {
        if (chown(name, statbuf.st_uid, statbuf.st_gid))  /* NT doesn't have fchown */
        {
            UnexpectedError("Failed to chown '%s'", name);
        }
        fchmod(fd, statbuf.st_mode);
        close(fd);
    }
}

#ifndef __MINGW32__

void CreateEmptyFile(char *name)
{
    int tempfd;

    if (unlink(name) == -1)
    {
        if (errno != ENOENT)
        {
            Log(LOG_LEVEL_DEBUG, "Unable to remove existing file '%s'. (unlink: %s)", name, GetErrorStr());
        }
    }

    if ((tempfd = open(name, O_CREAT | O_EXCL | O_WRONLY, 0600)) < 0)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open a file '%s'. (open: %s)", name, GetErrorStr());
    }

    close(tempfd);
}

#endif


