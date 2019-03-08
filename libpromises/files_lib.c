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

#include <files_lib.h>

#include <files_interfaces.h>
#include <files_names.h>
#include <files_copy.h>
#include <item_lib.h>
#include <promises.h>
#include <matching.h>
#include <misc_lib.h>
#include <dir.h>
#include <policy.h>
#include <string_lib.h>


static Item *ROTATED = NULL; /* GLOBAL_X */


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
            Log(LOG_LEVEL_VERBOSE,
                "Purging file '%s' from '%s' list as it no longer exists",
                ip->name, name);
            DeleteItemLiteral(list, ip->name);
        }
    }

    DeleteItemList(copy);
}

bool FileWriteOver(char *filename, char *contents)
{
    FILE *fp = safe_fopen(filename, "w");

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

/**
 * Like MakeParentDirectory, but honours warn-only and dry-run mode.
 * We should eventually migrate to this function to avoid making changes
 * in these scenarios.
 *
 * @WARNING like MakeParentDirectory, this function will not behave right on
 *          Windows if the path contains double (back)slashes!
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
 *
 * @WARNING this function will not behave right on Windows if the path
 *          contains double (back)slashes!
 **/

bool MakeParentDirectory(const char *parentandchild, bool force)
{
    char *sp;
    char currentpath[CF_BUFSIZE];
    char pathbuf[CF_BUFSIZE];
    struct stat statbuf;
    mode_t mask;
    int rootlen;

#ifdef __APPLE__
/* Keeps track of if dealing w. resource fork */
    int rsrcfork;

    rsrcfork = 0;

    char *tmpstr;
#endif

    Log(LOG_LEVEL_DEBUG, "Trying to create a parent directory%s for: %s",
        force ? " (force applied)" : "",
        parentandchild);

    if (!IsAbsoluteFileName(parentandchild))
    {
        Log(LOG_LEVEL_ERR,
            "Will not create directories for a relative filename: %s",
            parentandchild);
        return false;
    }

    strlcpy(pathbuf, parentandchild, CF_BUFSIZE);   /* local copy */

#ifdef __APPLE__
    if (strstr(pathbuf, _PATH_RSRCFORKSPEC) != NULL)
    {
        rsrcfork = 1;
    }
#endif

/* skip link name */

    sp = (char *) LastFileSeparator(pathbuf);                /* de-constify */

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
            Log(LOG_LEVEL_VERBOSE, "'%s' is a symbolic link, not a directory",
                pathbuf);
        }

        if (force)              /* force in-the-way directories aside */
        {
            struct stat dir;
            stat(pathbuf, &dir);

            /* If the target directory exists as a directory, no problem. */
            /* If the target directory exists but is not a directory, then
             * rename it to ".cf-moved": */
            if (!S_ISDIR(dir.st_mode))
            {
                struct stat sbuf;

                if (DONTDO)
                {
                    return true;
                }

                strcpy(currentpath, pathbuf);
                DeleteSlash(currentpath);
                /* TODO overflow check! */
                strlcat(currentpath, ".cf-moved", sizeof(currentpath));
                Log(LOG_LEVEL_INFO,
                    "Moving obstructing file/link %s to %s to make directory",
                    pathbuf, currentpath);

                /* Remove possibly pre-existing ".cf-moved" backup object. */
                if (lstat(currentpath, &sbuf) != -1)
                {
                    if (S_ISDIR(sbuf.st_mode))                 /* directory */
                    {
                        DeleteDirectoryTree(currentpath);
                    }
                    else                                 /* not a directory */
                    {
                        if (unlink(currentpath) == -1)
                        {
                            Log(LOG_LEVEL_INFO, "Couldn't remove file/link"
                                " '%s' while trying to remove a backup"
                                " (unlink: %s)",
                                currentpath, GetErrorStr());
                        }
                    }
                }

                /* And then rename the current object to ".cf-moved". */
                if (rename(pathbuf, currentpath) == -1)
                {
                    Log(LOG_LEVEL_INFO,
                        "Couldn't rename '%s' to .cf-moved"
                        " (rename: %s)", pathbuf, GetErrorStr());
                    return false;
                }
            }
        }
        else
        {
            if (!S_ISLNK(statbuf.st_mode) && !S_ISDIR(statbuf.st_mode))
            {
                Log(LOG_LEVEL_INFO, "The object '%s' is not a directory."
                    " Cannot make a new directory without deleting it.",
                    pathbuf);
                return false;
            }
        }
    }

/* Now we make directories descending from the root folder down to the leaf */

    currentpath[0] = '\0';

    rootlen = RootDirLength(parentandchild);
    /* currentpath is not NULL terminated on purpose! */
    strncpy(currentpath, parentandchild, rootlen);

    for (size_t z = rootlen; parentandchild[z] != '\0'; z++)
    {
        const char c = parentandchild[z];

        /* Copy up to the next separator. */
        if (!IsFileSep(c))
        {
            currentpath[z] = c;
            continue;
        }

        const char path_file_separator = c;
        currentpath[z]                 = '\0';

        /* currentpath is complete path for each of the parent directories.  */

        if (currentpath[0] == '\0')
        {
            /* We are at dir "/" of an absolute path, no need to create. */
        }
        /* WARNING: on Windows stat() fails if path has a trailing slash! */
        else if (stat(currentpath, &statbuf) == -1)
        {
            if (!DONTDO)
            {
                mask = umask(0);

                if (mkdir(currentpath, DEFAULTMODE) == -1 && errno != EEXIST)
                {
                    Log(LOG_LEVEL_ERR,
                        "Unable to make directory: %s (mkdir: %s)",
                        currentpath, GetErrorStr());
                    umask(mask);
                    return false;
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
                    strlcpy(tmpstr, currentpath, CF_BUFSIZE);
                    strncat(tmpstr, _PATH_FORKSPECIFIER, CF_BUFSIZE);

                    /* CFEngine removed terminating slashes */
                    DeleteSlash(tmpstr);

                    if (strncmp(tmpstr, pathbuf, CF_BUFSIZE) == 0)
                    {
                        free(tmpstr);
                        return true;
                    }
                    free(tmpstr);
                }
#endif

                Log(LOG_LEVEL_ERR,
                    "Cannot make %s - %s is not a directory!"
                    " (use forcedirs=true)", pathbuf, currentpath);
                return false;
            }
        }

        currentpath[z] = path_file_separator;
    }

    Log(LOG_LEVEL_DEBUG, "Directory for '%s' exists. Okay", parentandchild);
    return true;
}

int LoadFileAsItemList(Item **liststart, const char *file, EditDefaults edits)
{
    {
        struct stat statbuf;
        if (stat(file, &statbuf) == -1)
        {
            Log(LOG_LEVEL_VERBOSE, "The proposed file '%s' could not be loaded. (stat: %s)", file, GetErrorStr());
            return false;
        }

        if (edits.maxfilesize != 0 && statbuf.st_size > edits.maxfilesize)
        {
            Log(LOG_LEVEL_INFO, "File '%s' is bigger than the edit limit. max_file_size = %jd > %d bytes", file,
                  (intmax_t) statbuf.st_size, edits.maxfilesize);
            return (false);
        }

        if (!S_ISREG(statbuf.st_mode))
        {
            Log(LOG_LEVEL_INFO, "%s is not a plain file", file);
            return false;
        }
    }

    FILE *fp = safe_fopen(file, "rt");
    if (!fp)
    {
        Log(LOG_LEVEL_INFO, "Couldn't read file '%s' for editing. (fopen: %s)", file, GetErrorStr());
        return false;
    }

    Buffer *concat = BufferNew();

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    bool result = true;

    for (;;)
    {
        ssize_t num_read = CfReadLine(&line, &line_size, fp);
        if (num_read == -1)
        {
            if (!feof(fp))
            {
                Log(LOG_LEVEL_ERR,
                    "Unable to read contents of file: %s (fread: %s)",
                    file, GetErrorStr());
                result = false;
            }
            break;
        }

        if (edits.joinlines && *(line + strlen(line) - 1) == '\\')
        {
            *(line + strlen(line) - 1) = '\0';

            BufferAppend(concat, line, num_read);
        }
        else
        {
            BufferAppend(concat, line, num_read);
            if (!feof(fp) || (BufferSize(concat) > 0))
            {
                AppendItem(liststart, BufferData(concat), NULL);
            }
        }

        BufferClear(concat);
    }

    free(line);
    BufferDestroy(concat);
    fclose(fp);
    return result;
}

bool TraverseDirectoryTreeInternal(const char *base_path,
                                   const char *current_path,
                                   int (*callback)(const char *, const struct stat *, void *),
                                   void *user_data)
{
    Dir *dirh = DirOpen(base_path);
    if (!dirh)
    {
        if (errno == ENOENT)
        {
            return true;
        }

        Log(LOG_LEVEL_INFO, "Unable to open directory '%s' during traversal of directory tree '%s' (opendir: %s)",
            current_path, base_path, GetErrorStr());
        return false;
    }

    bool failed = false;
    for (const struct dirent *dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, ".."))
        {
            continue;
        }

        char sub_path[CF_BUFSIZE];
        snprintf(sub_path, CF_BUFSIZE, "%s" FILE_SEPARATOR_STR "%s", current_path, dirp->d_name);

        struct stat lsb;
        if (lstat(sub_path, &lsb) == -1)
        {
            if (errno == ENOENT)
            {
                /* File disappeared on its own */
                continue;
            }

            Log(LOG_LEVEL_VERBOSE, "Unable to stat file '%s' during traversal of directory tree '%s' (lstat: %s)",
                current_path, base_path, GetErrorStr());
            failed = true;
        }
        else
        {
            if (S_ISDIR(lsb.st_mode))
            {
                if (!TraverseDirectoryTreeInternal(base_path, sub_path, callback, user_data))
                {
                    failed = true;
                }
            }
            else
            {
                if (callback(sub_path, &lsb, user_data) == -1)
                {
                    failed = true;
                }
            }
        }
    }

    DirClose(dirh);
    return !failed;
}

bool TraverseDirectoryTree(const char *path,
                           int (*callback)(const char *, const struct stat *, void *),
                           void *user_data)
{
    return TraverseDirectoryTreeInternal(path, path, callback, user_data);
}

typedef struct
{
    unsigned char buffer[1024];
    const char **extensions_filter;
    EVP_MD_CTX *crypto_context;
    unsigned char **digest;
} HashDirectoryTreeState;

int HashDirectoryTreeCallback(const char *filename, ARG_UNUSED const struct stat *sb, void *user_data)
{
    HashDirectoryTreeState *state = user_data;
    bool ignore = true;
    for (size_t i = 0; state->extensions_filter[i]; i++)
    {
        if (StringEndsWith(filename, state->extensions_filter[i]))
        {
            ignore = false;
            break;
        }
    }

    if (ignore)
    {
        return 0;
    }

    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        Log(LOG_LEVEL_ERR, "Cannot open file for hashing '%s'. (fopen: %s)", filename, GetErrorStr());
        return -1;
    }

    size_t len = 0;
    char buffer[1024];
    while ((len = fread(buffer, 1, 1024, file)))
    {
        EVP_DigestUpdate(state->crypto_context, state->buffer, len);
    }

    fclose(file);
    return 0;
}

bool HashDirectoryTree(const char *path,
                       const char **extensions_filter,
                       EVP_MD_CTX *crypto_context)
{
    HashDirectoryTreeState state;
    memset(state.buffer, 0, 1024);
    state.extensions_filter = extensions_filter;
    state.crypto_context = crypto_context;

    return TraverseDirectoryTree(path, HashDirectoryTreeCallback, &state);
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

    safe_chmod(to, statbuf.st_mode);
    if (safe_chown(to, statbuf.st_uid, statbuf.st_gid))
    {
        UnexpectedError("Failed to chown %s", to);
    }
    safe_chmod(name, 0600);       /* File must be writable to empty .. */

    if ((fd = safe_creat(name, statbuf.st_mode)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to create new '%s' in disable(rotate). (create: %s)",
            name, GetErrorStr());
    }
    else
    {
        if (safe_chown(name, statbuf.st_uid, statbuf.st_gid))  /* NT doesn't have fchown */
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

    if ((tempfd = safe_open(name, O_CREAT | O_EXCL | O_WRONLY, 0600)) < 0)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open a file '%s'. (open: %s)", name, GetErrorStr());
    }

    close(tempfd);
}

#endif


