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

#include "file_lib.h"

int FullWrite(int desc, const char *ptr, size_t len)
{
    int total_written = 0;

    while (len > 0)
    {
        int written = write(desc, ptr, len);

        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return written;
        }

        total_written += written;
        ptr += written;
        len -= written;
    }

    return total_written;
}

int FullRead(int fd, char *ptr, size_t len)
{
    int total_read = 0;

    while (len > 0)
    {
        int bytes_read = read(fd, ptr, len);

        if (bytes_read < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return -1;
        }

        if (bytes_read == 0)
        {
            return total_read;
        }

        total_read += bytes_read;
        ptr += bytes_read;
        len -= bytes_read;
    }

    return total_read;
}

#ifdef TEST_SYMLINK_ATOMICITY
void test_switch_symlink();
#define TEST_SYMLINK_SWITCH_POINT test_switch_symlink();
#else
#define TEST_SYMLINK_SWITCH_POINT
#endif

/**
 * Opens a file safely. It will follow symlinks, but only if the symlink is trusted,
 * that is, if the owner of the symlink and the owner of the target are the same.
 * All components are checked, even symlinks encountered in earlier parts of the
 * path name.
 *
 * It should always be used when opening a file or directory that is not guaranteed
 * to be owned by root.
 *
 * @param pathname The path to open.
 * @param flags Same flags as for system open().
 * @param ... Optional mode argument, as per system open().
 * @return Same errors as open().
 */
int safe_open(const char *pathname, int flags, ...)
{
    if (!pathname)
    {
        errno = EINVAL;
        return -1;
    }

    if (*pathname == '\0')
    {
        errno = ENOENT;
        return -1;
    }

    mode_t mode;
    if (flags & O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    else
    {
        mode = 0;
    }

#ifdef __MINGW32__
    // Windows gets off easy. No symlinks there.
    return open(pathname, flags, mode);
#else // !__MINGW32__

    char path[strlen(pathname) + 1];
    strcpy(path, pathname);
    int currentfd;
    const char *first_dir;
    char *next_component;
    bool trunc = false;
    int orig_flags = flags;

    if (flags & O_TRUNC)
    {
        trunc = true;
        /* We need to check after we have opened the file whether we opened the
         * right one. But if we truncate it, the damage is already done, we have
         * destroyed the contents. So save that flag and apply the truncation
         * afterwards instead. */
        flags &= ~O_TRUNC;
    }

    next_component = path;
    if (*next_component == '/')
    {
        first_dir = "/";
        // Eliminate double slashes.
        while (*(++next_component) == '/') { /*noop*/ }
        if (!*next_component)
        {
            next_component = NULL;
        }
    }
    else
    {
        first_dir = ".";
    }
    currentfd = open(first_dir, O_RDONLY);
    if (currentfd < 0)
    {
        return -1;
    }

    while (next_component)
    {
        char *component = next_component;
        next_component = strchr(component + 1, '/');
        if (next_component)
        {
            *next_component = '\0';
            // Eliminate double slashes.
            while (*(++next_component) == '/') { /*noop*/ }
            if (!*next_component)
            {
                next_component = NULL;
            }
        }

        struct stat stat_before, stat_after;
        int stat_before_result = fstatat(currentfd, component, &stat_before, AT_SYMLINK_NOFOLLOW);
        if (stat_before_result < 0
            && (errno != ENOENT
                || next_component // Meaning "not a leaf".
                || !(flags & O_CREAT)))
        {
            close(currentfd);
            return -1;
        }

        TEST_SYMLINK_SWITCH_POINT

        if (!next_component)
        {
            // Last component.
            if (stat_before_result < 0)
            {
                // Doesn't exist. Make *sure* we create it.
                flags |= O_EXCL;
            }
            else
            {
                // Already exists. Make sure we *don't* create it.
                flags &= ~O_CREAT;
            }
            int filefd = openat(currentfd, component, flags, mode);
            close(currentfd);
            if (filefd < 0)
            {
                if (stat_before_result < 0 && errno == EEXIST)
                {
                    errno = EACCES;
                }
                else if (stat_before_result >= 0 && orig_flags & O_CREAT && errno == ENOENT)
                {
                    errno = EACCES;
                }
                return -1;
            }
            currentfd = filefd;
        }
        else
        {
            int new_currentfd = openat(currentfd, component, O_RDONLY);
            close(currentfd);
            if (new_currentfd < 0)
            {
                return -1;
            }
            currentfd = new_currentfd;
        }

        if (stat_before_result == 0)
        {
            if (fstat(currentfd, &stat_after) < 0)
            {
                close(currentfd);
                return -1;
            }
            if (stat_before.st_uid != stat_after.st_uid || stat_before.st_gid != stat_after.st_gid)
            {
                close(currentfd);
                errno = EACCES;
                return -1;
            }
        }
    }

    if (trunc)
    {
        if (ftruncate(currentfd, 0) < 0)
        {
            close(currentfd);
            return -1;
        }
    }

    return currentfd;
#endif // !__MINGW32__
}

/**
 * Opens a file safely. It will follow symlinks, but only if the symlink is trusted,
 * that is, if the owner of the symlink and the owner of the target are the same.
 * All components are checked, even symlinks encountered in earlier parts of the
 * path name.
 *
 * It should always be used when opening a directory that is not guaranteed to be
 * owned by root.
 *
 * @param pathname The path to open.
 * @param flags Same mode as for system fopen().
 * @return Same errors as fopen().
 */
FILE *safe_fopen(const char *path, const char *mode)
{
    if (!path || !mode)
    {
        errno = EINVAL;
        return NULL;
    }

    int flags = 0;
    for (int c = 0; mode[c]; c++)
    {
        switch (mode[c])
        {
        case 'r':
            flags |= O_RDONLY;
            break;
        case 'w':
            flags |= O_WRONLY | O_TRUNC | O_CREAT;
            break;
        case 'a':
            flags |= O_WRONLY | O_CREAT;
            break;
        case '+':
            flags &= ~(O_RDONLY | O_WRONLY);
            flags |= O_RDWR;
            break;
        default:
            break;
        }
    }
    int fd = safe_open(path, flags, 0666);
    if (fd < 0)
    {
        return NULL;
    }
    FILE *ret = fdopen(fd, mode);
    if (!ret)
    {
        close(fd);
        return NULL;
    }

    if (mode[0] == 'a')
    {
        if (fseek(ret, 0, SEEK_END) < 0)
        {
            fclose(ret);
            return NULL;
        }
    }

    return ret;
}

/**
 * Use this instead of chdir(). It changes into the directory safely, using safe_open().
 * @param path Path to change into.
 * @return Same return values as chdir().
 */
int safe_chdir(const char *path)
{
#ifdef __MINGW32__
    return chdir(path);
#else // !__MINGW32__
    int fd = safe_open(path, O_RDONLY);
    if (fd < 0)
    {
        return -1;
    }
    if (fchdir(fd) < 0)
    {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
#endif // !__MINGW32__
}

/**
 * Use this instead of chown(). It changes file owner safely, using safe_open().
 * @param path Path to operate on.
 * @param owner Owner.
 * @param group Group.
 * @return Same return values as chown().
 */
int safe_chown(const char *path, uid_t owner, gid_t group)
{
#ifdef __MINGW32__
    return chown(path, owner, group);
#else // !__MINGW32__
    int fd = safe_open(path, 0);
    if (fd < 0)
    {
        return -1;
    }

    int ret = fchown(fd, owner, group);
    close(fd);
    return ret;
#endif // !__MINGW32__
}

/**
 * Use this instead of lchown(). It changes file owner safely, using safe_open().
 * @param path Path to operate on.
 * @param owner Owner.
 * @param group Group.
 * @return Same return values as lchown().
 */
#ifndef __MINGW32__
int safe_lchown(const char *path, uid_t owner, gid_t group)
{
    if (*path == '\0')
    {
        errno = EINVAL;
        return -1;
    }

    size_t orig_size = strlen(path);
    char parent_dir[orig_size + 1];
    char *slash_pos = strrchr(path, '/');
    const char *leaf;
    if (slash_pos)
    {
        strcpy(parent_dir, path);
        // Same value, but relative to parent_dir.
        leaf = slash_pos = &parent_dir[slash_pos - path];
        leaf++;
        // Remove all superflous slashes, but not if it's the root slash.
        while (*slash_pos == '/' && slash_pos != parent_dir)
        {
            *(slash_pos--) = '\0';
        }
    }
    else
    {
        parent_dir[0] = '.';
        parent_dir[1] = '\0';
        leaf = path;
    }

    int parent_fd = safe_open(parent_dir, O_RDONLY);
    if (parent_fd < 0)
    {
        return -1;
    }

    int ret = fchownat(parent_fd, leaf, owner, group, AT_SYMLINK_NOFOLLOW);
    close(parent_fd);
    return ret;
}
#endif // !__MINGW32__

/**
 * Use this instead of chmod(). It changes file permissions safely, using safe_open().
 * @param path Path to operate on.
 * @param mode Permissions.
 * @return Same return values as chmod().
 */
int safe_chmod(const char *path, mode_t mode)
{
#ifdef __MINGW32__
    return chmod(path, mode);
#else // !__MINGW32__
    int fd = safe_open(path, 0);
    if (fd < 0)
    {
        return -1;
    }

    int ret = fchmod(fd, mode);
    close(fd);
    return ret;
#endif // !__MINGW32__
}

/**
 * Use this instead of creat(). It creates a file safely, using safe_open().
 * @param path Path to operate on.
 * @param mode Permissions.
 * @return Same return values as creat().
 */
int safe_creat(const char *pathname, mode_t mode)
{
    return safe_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}
