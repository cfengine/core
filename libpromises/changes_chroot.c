/*
  Copyright 2020 Northern.tech AS

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

#include <alloc.h>              /* xstrdup() */
#include <dir.h>                /* DirOpen(), DirRead(), DirClose() */
#include <eval_context.h>       /* ToChangesChroot() */
#include <file_lib.h>           /* FILE_SEPARATOR */
#include <files_lib.h>          /* MakeParentDirectory() */
#include <files_copy.h>         /* CopyRegularFileDisk(), CopyFilePermissionsDisk() */
#include <files_names.h>        /* IsAbsPath(), JoinPaths() */
#include <files_links.h>        /* ExpandLinks() */
#include <string_lib.h>         /* StringEqual() */

#include <changes_chroot.h>

static inline const char *GetLastFileSeparator(const char *path, const char *end)
{
    const char *cp = end;
    for (; (cp > path) && (*cp != FILE_SEPARATOR); cp--);
    return cp;
}

static char *GetFirstNonExistingParentDir(const char *path, char *buf)
{
    /* Get rid of the trailing file separator (if any). */
    size_t path_len = strlen(path);
    if (path[path_len - 1] == FILE_SEPARATOR)
    {
        strncpy(buf, path, path_len);
        buf[path_len] = '\0';
    }
    else
    {
        strncpy(buf, path, path_len + 1);
    }

    char *last_sep = (char *) GetLastFileSeparator(buf, buf + path_len);
    while (last_sep != buf)
    {
        *last_sep = '\0';
        if (access(buf, F_OK) == 0)
        {
            *last_sep = FILE_SEPARATOR;
            *(last_sep + 1) = '\0';
            return buf;
        }
        last_sep = (char *) GetLastFileSeparator(buf, last_sep - 1);
    }
    *last_sep = '\0';
    return buf;
}

static bool MirrorDirTreePermsToChroot(const char *path)
{
    const char *chrooted = ToChangesChroot(path);

    if (!CopyFilePermissionsDisk(path, chrooted))
    {
        return false;
    }

    size_t path_len = strlen(path);
    char path_copy[path_len + 1];
    strcpy(path_copy, path);

    const char *const path_copy_end = path_copy + (path_len - 1);
    const char *const chrooted_end = chrooted + strlen(chrooted) - 1;

    char *last_sep = (char *) GetLastFileSeparator(path_copy, path_copy_end);
    while (last_sep != path_copy)
    {
        char *last_sep_chrooted = (char *) chrooted_end - (path_copy_end - last_sep);
        *last_sep = '\0';
        *last_sep_chrooted = '\0';
        if (!CopyFilePermissionsDisk(path_copy, chrooted))
        {
            return false;
        }
        *last_sep = FILE_SEPARATOR;
        *last_sep_chrooted = FILE_SEPARATOR;
        last_sep = (char *) GetLastFileSeparator(path_copy, last_sep - 1);
    }
    return true;
}

#ifndef __MINGW32__
/**
 * Mirror the symlink #path to #chrooted_path together with its target, then
 * mirror the target if it is a symlink too,..., recursively.
 */
static void ChrootSymlinkDeep(const char *path, const char *chrooted_path, struct stat *sb)
{
    assert(sb != NULL);

    size_t target_size = (sb->st_size != 0 ? sb->st_size + 1 : PATH_MAX);
    char target[target_size];
    ssize_t ret = readlink(path, target, target_size);
    if (ret == -1)
    {
        /* Should never happen, but nothing to do here if it does. */
        return;
    }
    target[ret] = '\0';

    if (IsAbsPath(target))
    {
        const char *chrooted_target = ToChangesChroot(target);
        if (symlink(chrooted_target, chrooted_path) != 0)
        {
            /* Should never happen, but nothing to do here if it does. */
            return;
        }
        else
        {
            PrepareChangesChroot(target);
        }
    }
    else
    {
        if (symlink(target, chrooted_path) != 0)
        {
            /* Should never happen, but nothing to do here if it does. */
            return;
        }
        else
        {
            char expanded_target[PATH_MAX];
            if (!ExpandLinks(expanded_target, path, 0, 1))
            {
                /* Should never happen, but nothing to do here if it does. */
                return;
            }
            PrepareChangesChroot(expanded_target);
        }
    }
}
#endif  /* __MINGW32__ */

void PrepareChangesChroot(const char *path)
{
    struct stat sb;
    if (lstat(path, &sb) == -1)
    {
        /* 'path' doesn't exist, nothing to do here */
        return;
    }

    /* We need to create a copy because ToChangesChroot() returns a pointer to
     * its internal buffer which gets overwritten by later calls of the
     * function. */
    char *chrooted = xstrdup(ToChangesChroot(path));
    if (lstat(chrooted, &sb) != -1)
    {
        /* chrooted 'path' already exists, we are done */
        free(chrooted);
        return;
    }

    {
        char first_nonexisting_parent[PATH_MAX];
        GetFirstNonExistingParentDir(path, first_nonexisting_parent);
        const char *first_nonexisting_parent_chrooted = ToChangesChroot(first_nonexisting_parent);

        MakeParentDirectory(first_nonexisting_parent_chrooted, true, NULL);
        MirrorDirTreePermsToChroot(first_nonexisting_parent);
    }

#ifndef __MINGW32__
    if (S_ISLNK(sb.st_mode))
    {
        ChrootSymlinkDeep(path, chrooted, &sb);
    }
    else
#endif  /* __MINGW32__ */
    if (S_ISDIR(sb.st_mode))
    {
        mkdir(chrooted, sb.st_mode);
        Dir *dir = DirOpen(path);
        if (dir == NULL)
        {
            /* Should never happen, but nothing to do here if it does. */
            free(chrooted);
            return;
        }

        for (const struct dirent *entry = DirRead(dir); entry != NULL; entry = DirRead(dir))
        {
            if (StringEqual(entry->d_name, ".") || StringEqual(entry->d_name, ".."))
            {
                continue;
            }
            char entry_path[PATH_MAX];
            strcpy(entry_path, path);
            JoinPaths(entry_path, PATH_MAX, entry->d_name);
            PrepareChangesChroot(entry_path);
        }
        DirClose(dir);
    }
    else
    {
        /* TODO: sockets, pipes, devices,... ? */
        CopyRegularFileDisk(path, chrooted);
    }
    CopyFilePermissionsDisk(path, chrooted);

    free(chrooted);
}
