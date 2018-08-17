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

#include <file_lib.h>
#include <misc_lib.h>
#include <dir.h>

#include <alloc.h>
#include <libgen.h>
#include <logging.h>
#include <string_lib.h>                                         /* memcchr */

#ifndef __MINGW32__
#include <glob.h>
#endif

#define SYMLINK_MAX_DEPTH 32

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

#define READ_BUFSIZE 4096

Writer *FileRead(const char *filename, size_t max_size, bool *truncated)
{
    int fd = safe_open(filename, O_RDONLY);
    if (fd == -1)
    {
        return NULL;
    }

    Writer *w = FileReadFromFd(fd, max_size, truncated);
    close(fd);
    return w;
}

Writer *FileReadFromFd(int fd, size_t max_size, bool *truncated)
{
    if (truncated)
    {
        *truncated = false;
    }

    Writer *w = StringWriter();
    for (;;)
    {
        char buf[READ_BUFSIZE];
        /* Reading more data than needed is deliberate. It is a truncation detection. */
        ssize_t read_ = read(fd, buf, READ_BUFSIZE);

        if (read_ == 0)
        {
            /* Done. */
            return w;
        }
        else if (read_ < 0)
        {
            if (errno != EINTR)
            {
                /* Something went wrong. */
                WriterClose(w);
                return NULL;
            }
            /* Else: interrupted - try again. */
        }
        else if (read_ + StringWriterLength(w) > max_size)
        {
            WriterWriteLen(w, buf, max_size - StringWriterLength(w));
            /* Reached limit - stop. */
            if (truncated)
            {
                *truncated = true;
            }
            return w;
        }
        else /* Filled buffer; copy and ask for more. */
        {
            WriterWriteLen(w, buf, read_);
        }
    }
}

ssize_t FullWrite(int desc, const char *ptr, size_t len)
{
    ssize_t total_written = 0;

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

ssize_t FullRead(int fd, char *ptr, size_t len)
{
    ssize_t total_read = 0;

    while (len > 0)
    {
        ssize_t bytes_read = read(fd, ptr, len);

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

/**
 * @return 1 if dir, 0 if not, -1 in case of error.
 * @note difference with files_names.h:IsDir() is that this doesn't
 *       follow symlinks, so a symlink is never a directory...
 */
int IsDirReal(const char *path)
{
    struct stat s;

    if (lstat(path, &s) == -1)
    {
        return -1;
    }

    return (S_ISDIR(s.st_mode) != 0);
}

#ifndef __MINGW32__
NewLineMode FileNewLineMode(ARG_UNUSED const char *file)
{
    return NewLineMode_Unix;
}
#endif // !__MINGW32__

bool IsAbsoluteFileName(const char *f)
{
    int off = 0;

// Check for quoted strings

    for (off = 0; f[off] == '\"'; off++)
    {
    }

#ifdef _WIN32
    if (IsFileSep(f[off]) && IsFileSep(f[off + 1]))
    {
        return true;
    }

    if (isalpha(f[off]) && f[off + 1] == ':' && IsFileSep(f[off + 2]))
    {
        return true;
    }
#endif
    if (IsFileSep(f[off]))
    {
        return true;
    }

    return false;
}

/* We assume that s is at least MAX_FILENAME large.
 * MapName() is thread-safe, but the argument is modified. */

#ifdef _WIN32
# if defined(__MINGW32__)

char *MapNameCopy(const char *s)
{
    char *str = xstrdup(s);

    char *c = str;
    while ((c = strchr(c, '/')))
    {
        *c = '\\';
    }

    return str;
}

char *MapName(char *s)
{
    char *c = s;

    while ((c = strchr(c, '/')))
    {
        *c = '\\';
    }
    return s;
}

# elif defined(__CYGWIN__)

char *MapNameCopy(const char *s)
{
    Writer *w = StringWriter();

    /* c:\a\b -> /cygdrive/c\a\b */
    if (s[0] && isalpha(s[0]) && s[1] == ':')
    {
        WriterWriteF(w, "/cygdrive/%c", s[0]);
        s += 2;
    }

    for (; *s; s++)
    {
        /* a//b//c -> a/b/c */
        /* a\\b\\c -> a\b\c */
        if (IsFileSep(*s) && IsFileSep(*(s + 1)))
        {
            continue;
        }

        /* a\b\c -> a/b/c */
        WriterWriteChar(w, *s == '\\' ? '/' : *s);
    }

    return StringWriterClose(w);
}

char *MapName(char *s)
{
    char *ret = MapNameCopy(s);

    if (strlcpy(s, ret, MAX_FILENAME) >= MAX_FILENAME)
    {
        FatalError(ctx, "Expanded path (%s) is longer than MAX_FILENAME ("
                   TOSTRING(MAX_FILENAME) ") characters",
                   ret);
    }
    free(ret);

    return s;
}

# else/* !__MINGW32__ && !__CYGWIN__ */
#  error Unknown NT-based compilation environment
# endif/* __MINGW32__ || __CYGWIN__ */
#else /* !_WIN32 */

char *MapName(char *s)
{
    return s;
}

char *MapNameCopy(const char *s)
{
    return xstrdup(s);
}

#endif /* !_WIN32 */

char *MapNameForward(char *s)
/* Like MapName(), but maps all slashes to forward */
{
    while ((s = strchr(s, '\\')))
    {
        *s = '/';
    }
    return s;
}


#ifdef TEST_SYMLINK_ATOMICITY
void switch_symlink_hook();
#define TEST_SYMLINK_SWITCH_POINT switch_symlink_hook();
#else
#define TEST_SYMLINK_SWITCH_POINT
#endif

/**
 * Opens a file safely. It will follow symlinks, but only if the symlink is trusted,
 * that is, if the owner of the symlink and the owner of the target are the same,
 * or if the owner of the symlink is either root or the user running the current process.
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
    if (flags & O_TRUNC)
    {
        /* Undefined behaviour otherwise, according to the standard. */
        assert((flags & O_RDWR) || (flags & O_WRONLY));
    }

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
    bool trunc = false;
    const int orig_flags = flags;
    char *next_component = path;
    bool p_uid;

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
    currentfd = openat(AT_FDCWD, first_dir, O_RDONLY);
    if (currentfd < 0)
    {
        return -1;
    }

    // current process user id
    p_uid = geteuid();

    size_t final_size = (size_t) -1;
    while (next_component)
    {
        char *component = next_component;
        next_component = strchr(component + 1, '/');
        // Used to restore the slashes in the final path component.
        char *restore_slash = NULL;
        if (next_component)
        {
            restore_slash = next_component;
            *next_component = '\0';
            // Eliminate double slashes.
            while (*(++next_component) == '/') { /*noop*/ }
            if (!*next_component)
            {
                next_component = NULL;
            }
            else
            {
                restore_slash = NULL;
            }
        }

        // In cases of a race condition when creating a file, our attempt to open it may fail
        // (see O_EXCL and O_CREAT flags below). However, this can happen even under normal
        // circumstances, if we are unlucky; it does not mean that someone is up to something bad.
        // So retry it a few times before giving up.
        int attempts = 3;
        trunc = false;
        while (true)
        {

            if ((  (orig_flags & O_RDWR) || (orig_flags & O_WRONLY))
                && (orig_flags & O_TRUNC))
            {
                trunc = true;
                /* We need to check after we have opened the file whether we
                 * opened the right one. But if we truncate it, the damage is
                 * already done, we have destroyed the contents, and that file
                 * might have been a symlink to /etc/shadow! So save that flag
                 * and apply the truncation afterwards instead. */
                flags &= ~O_TRUNC;
            }

            if (restore_slash)
            {
                *restore_slash = '\0';
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

            /*
             * This testing entry point is about the following real-world
             * scenario: There can be an attacker that at this point
             * overwrites the existing file or writes a file, invalidating
             * basically the previous fstatat().
             *
             * - We make sure that can't happen if the file did not exist, by
             *   creating with O_EXCL.
             * - We make sure that can't happen if the file existed, by
             *   comparing with fstat() result after the open().
             *
             */
            TEST_SYMLINK_SWITCH_POINT

            if (!next_component)                          /* last component */
            {
                if (stat_before_result < 0)
                {
                    assert(flags & O_CREAT);

                    // Doesn't exist. Make sure *we* create it.
                    flags |= O_EXCL;

                    /* No need to ftruncate() the file at the end. */
                    trunc = false;
                }
                else
                {
                    if ((flags & O_CREAT) && (flags & O_EXCL))
                    {
                        close(currentfd);
                        errno = EEXIST;
                        return -1;
                    }

                    // Already exists. Make sure we *don't* create it.
                    flags &= ~O_CREAT;
                }
                if (restore_slash)
                {
                    *restore_slash = '/';
                }
                int filefd = openat(currentfd, component, flags, mode);
                if (filefd < 0)
                {
                    if ((stat_before_result < 0  && !(orig_flags & O_EXCL)  && errno == EEXIST) ||
                        (stat_before_result >= 0 &&  (orig_flags & O_CREAT) && errno == ENOENT))
                    {
                        if (--attempts >= 0)
                        {
                            // Might be our fault. Try again.
                            flags = orig_flags;
                            continue;
                        }
                        else
                        {
                            // Too many attempts. Give up.
                            // Most likely a link that is in the way of file creation, but can also
                            // be a file that is constantly created and deleted (race condition).
                            // It is not possible to separate between the two, so return EACCES to
                            // signal that we denied access.
                            errno = EACCES;
                        }
                    }
                    close(currentfd);
                    return -1;
                }
                close(currentfd);
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

            /* If file did exist, we fstat() again and compare with previous. */

            if (stat_before_result == 0)
            {
                if (fstat(currentfd, &stat_after) < 0)
                {
                    close(currentfd);
                    return -1;
                }
                // Some attacks may use symlinks to get higher privileges
                // The safe cases are:
                // * symlinks owned by root
                // * symlinks owned by the user running the process
                // * symlinks that have the same owner and group as the destination
                if (stat_before.st_uid != 0 &&
                    stat_before.st_uid != p_uid &&
                    (stat_before.st_uid != stat_after.st_uid || stat_before.st_gid != stat_after.st_gid))
                {
                    close(currentfd);
                    Log(LOG_LEVEL_ERR, "Cannot follow symlink '%s'; it is not "
                        "owned by root or the user running this process, and "
                        "the target owner and/or group differs from that of "
                        "the symlink itself.", pathname);
                    // Return ENOLINK to signal that the link cannot be followed
                    // ('Link has been severed').
                    errno = ENOLINK;
                    return -1;
                }

                final_size = (size_t) stat_after.st_size;
            }

            // If we got here, we've been successful, so don't try again.
            break;
        }
    }

    /* Truncate if O_CREAT and the file preexisted. */
    if (trunc)
    {
        /* Do not truncate if the size is already zero, some
         * filesystems don't support ftruncate() with offset>=size. */
        assert(final_size != (size_t) -1);

        if (final_size != 0)
        {
            int tr_ret = ftruncate(currentfd, 0);
            if (tr_ret < 0)
            {
                Log(LOG_LEVEL_ERR,
                    "safe_open: unexpected failure (ftruncate: %s)",
                    GetErrorStr());
                close(currentfd);
                return -1;
            }
        }
    }

    return currentfd;
#endif // !__MINGW32__
}

/**
 * Opens a file safely. It will follow symlinks, but only if the symlink is trusted,
 * that is, if the owner of the symlink and the owner of the target are the same,
 * or if the owner of the symlink is either root or the user running the current process.
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
        case 'b':
            flags |= O_BINARY;
            break;
        case 't':
            flags |= O_TEXT;
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

#ifndef __MINGW32__

/**
 * Opens the true parent dir of the file in the path given. The notable
 * difference from doing it the naive way (open(dirname(path))) is that it
 * can follow the symlinks of the path, ending up in the true parent dir of the
 * path. It follows the same safe mechanisms as `safe_open()` to do so. If
 * AT_SYMLINK_NOFOLLOW is given, it is equivalent to doing it the naive way (but
 * still following "safe" semantics).
 * @param path           Path to open parent directory of.
 * @param flags          Flags to use for fchownat.
 * @param link_user      If we have traversed a link already, which user was it.
 * @param link_group     If we have traversed a link already, which group was it.
 * @param traversed_link Whether we have traversed a link. If this is false the
 *                       two previus arguments are ignored. This is used enforce
 *                       the correct UID/GID combination when following links.
 *                       Initially this is false, but will be set to true in
 *                       sub invocations if we follow a link.
 * @param loop_countdown Protection against infinite loop following.
 * @return File descriptor pointing to the parent directory of path, or -1 on
 *         error.
 */
static int safe_open_true_parent_dir(const char *path,
                                     int flags,
                                     uid_t link_user,
                                     gid_t link_group,
                                     bool traversed_link,
                                     int loop_countdown)
{
    int dirfd = -1;
    int ret = -1;

    char *parent_dir_alloc = xstrdup(path);
    char *leaf_alloc = xstrdup(path);
    char *parent_dir = dirname(parent_dir_alloc);
    char *leaf = basename(leaf_alloc);
    struct stat statbuf;
    uid_t p_uid = geteuid();

    if ((dirfd = safe_open(parent_dir, O_RDONLY)) == -1)
    {
        goto cleanup;
    }

    if ((ret = fstatat(dirfd, leaf, &statbuf, AT_SYMLINK_NOFOLLOW)) == -1)
    {
        goto cleanup;
    }

    // Some attacks may use symlinks to get higher privileges
    // The safe cases are:
    // * symlinks owned by root
    // * symlinks owned by the user running the process
    // * symlinks that have the same owner and group as the destination
    if (traversed_link &&
        link_user != 0 &&
        link_user != p_uid &&
        (link_user != statbuf.st_uid || link_group != statbuf.st_gid))
    {
        errno = ENOLINK;
        ret = -1;
        goto cleanup;
    }

    if (S_ISLNK(statbuf.st_mode) && !(flags & AT_SYMLINK_NOFOLLOW))
    {
        if (--loop_countdown <= 0)
        {
            ret = -1;
            errno = ELOOP;
            goto cleanup;
        }

        // Add one byte for '\0', and one byte to make sure size doesn't change
        // in between calls.
        char *link = xmalloc(statbuf.st_size + 2);
        ret = readlinkat(dirfd, leaf, link, statbuf.st_size + 1);
        if (ret < 0 || ret > statbuf.st_size)
        {
            // Link either disappeared or was changed under our feet. Be safe
            // and bail out.
            free(link);
            errno = ENOLINK;
            ret = -1;
            goto cleanup;
        }
        link[ret] = '\0';

        char *resolved_link;
        if (link[0] == FILE_SEPARATOR)
        {
            // Takes ownership of link's memory, so no free().
            resolved_link = link;
        }
        else
        {
            xasprintf(&resolved_link, "%s%c%s", parent_dir,
                      FILE_SEPARATOR, link);
            free(link);
        }

        ret = safe_open_true_parent_dir(resolved_link, flags, statbuf.st_uid,
                                        statbuf.st_gid, true, loop_countdown);

        free(resolved_link);
        goto cleanup;
    }

    // We now know it either isn't a link, or we don't want to follow it if it
    // is. Return the parent dir.
    ret = dirfd;
    dirfd = -1;

cleanup:
    free(parent_dir_alloc);
    free(leaf_alloc);

    if (dirfd != -1)
    {
        close(dirfd);
    }

    return ret;
}

/**
 * Implementation of safe_chown.
 * @param path Path to chown.
 * @param owner          Owner to set on path.
 * @param group          Group to set on path.
 * @param flags          Flags to use for fchownat.
 * @param link_user      If we have traversed a link already, which user was it.
 * @param link_group     If we have traversed a link already, which group was it.
 * @param traversed_link Whether we have traversed a link. If this is false the
 *                       two previus arguments are ignored. This is used enforce
 *                       the correct UID/GID combination when following links.
 *                       Initially this is false, but will be set to true in
 *                       sub invocations if we follow a link.
 * @param loop_countdown Protection against infinite loop following.
 */
int safe_chown_impl(const char *path, uid_t owner, gid_t group, int flags)
{
    int dirfd = safe_open_true_parent_dir(path, flags, 0, 0, false, SYMLINK_MAX_DEPTH);
    if (dirfd < 0)
    {
        return -1;
    }

    char *leaf_alloc = xstrdup(path);
    char *leaf = basename(leaf_alloc);

    // We now know it either isn't a link, or we don't want to follow it if it
    // is. In either case make sure we don't try to follow it.
    flags |= AT_SYMLINK_NOFOLLOW;

    int ret = fchownat(dirfd, leaf, owner, group, flags);
    free(leaf_alloc);
    close(dirfd);
    return ret;
}

#endif // !__MINGW32__

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
    return safe_chown_impl(path, owner, group, 0);
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
    return safe_chown_impl(path, owner, group, AT_SYMLINK_NOFOLLOW);
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
    int dirfd = -1;
    int ret = -1;

    char *leaf_alloc = xstrdup(path);
    char *leaf = basename(leaf_alloc);
    struct stat statbuf;
    uid_t olduid = 0;

    if ((dirfd = safe_open_true_parent_dir(path, 0, 0, 0, false, SYMLINK_MAX_DEPTH)) == -1)
    {
        goto cleanup;
    }

    if ((ret = fstatat(dirfd, leaf, &statbuf, AT_SYMLINK_NOFOLLOW)) == -1)
    {
        goto cleanup;
    }

    if (S_ISFIFO(statbuf.st_mode) || S_ISSOCK(statbuf.st_mode))
    {
        /* For FIFOs/sockets we cannot resort to the method of opening the file
           first, since it might block. But we also cannot use chmod directly,
           because the file may be switched with a symlink to a sensitive file
           under our feet, and there is no way to avoid following it. So
           instead, switch effective UID to the owner of the FIFO, and then use
           chmod.
        */

        /* save old euid */
        olduid = geteuid();

        if ((ret = seteuid(statbuf.st_uid)) == -1)
        {
            goto cleanup;
        }

        ret = fchmodat(dirfd, leaf, mode, 0);

        // Make sure EUID is set back before we check error condition, so that we
        // never return with lowered privileges.
        if (seteuid(olduid) == -1)
        {
            ProgrammingError("safe_chmod: Could not set EUID back. Should never happen.");
        }

        goto cleanup;
    }

    int file_fd = safe_open(path, 0);
    if (file_fd < 0)
    {
        ret = -1;
        goto cleanup;
    }

    ret = fchmod(file_fd, mode);
    close(file_fd);

cleanup:
    free(leaf_alloc);

    if (dirfd != -1)
    {
        close(dirfd);
    }

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

// Windows implementation in Enterprise.
#ifndef _WIN32
bool SetCloseOnExec(int fd, bool enable)
{
    int flags = fcntl(fd, F_GETFD);
    if (enable)
    {
        flags |= FD_CLOEXEC;
    }
    else
    {
        flags &= ~FD_CLOEXEC;
    }
    return (fcntl(fd, F_SETFD, flags) == 0);
}
#endif // !_WIN32

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

        char subpath[PATH_MAX];
        snprintf(subpath, sizeof(subpath), "%s" FILE_SEPARATOR_STR "%s", path, dirp->d_name);

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

                if (rmdir(subpath) == -1)
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

/**
 * @NOTE Better use FileSparseCopy() if you are copying file to file
 *       (that one callse this function).
 *
 * @NOTE Always use FileSparseWrite() to close the file descriptor, to avoid
 *       losing data.
 */
bool FileSparseWrite(int fd, const void *buf, size_t count,
                     bool *wrote_hole)
{
    bool all_zeroes = (memcchr(buf, '\0', count) == NULL);

    if (all_zeroes)                                     /* write a hole */
    {
        off_t seek_ret = lseek(fd, count, SEEK_CUR);
        if (seek_ret == (off_t) -1)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to write a hole in sparse file (lseek: %s)",
                GetErrorStr());
            return false;
        }
    }
    else                                              /* write normally */
    {
        ssize_t w_ret = FullWrite(fd, buf, count);
        if (w_ret < 0)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to write to destination file (write: %s)",
                GetErrorStr());
            return false;
        }
    }

    *wrote_hole = all_zeroes;
    return true;
}

/**
 * Copy data jumping over areas filled by '\0' greater than blk_size, so
 * files automatically become sparse if possible.
 *
 * File descriptors should already be open, the filenames #source and
 * #destination are only for logging purposes.
 *
 * @NOTE Always use FileSparseClose() to close the file descriptor, to avoid
 *       losing data.
 */
bool FileSparseCopy(int sd, const char *src_name,
                    int dd, const char *dst_name,
                    size_t blk_size,
                    size_t *total_bytes_written,
                    bool   *last_write_was_a_hole)
{
    assert(total_bytes_written   != NULL);
    assert(last_write_was_a_hole != NULL);

    const size_t buf_size  = blk_size;
    void *buf              = xmalloc(buf_size);

    size_t n_read_total = 0;
    bool   retval       = false;

    *last_write_was_a_hole = false;

    while (true)
    {
        ssize_t n_read = FullRead(sd, buf, buf_size);
        if (n_read < 0)
        {
            Log(LOG_LEVEL_ERR,
                "Unable to read source file while copying '%s' to '%s'"
                " (read: %s)", src_name, dst_name, GetErrorStr());
            break;
        }
        else if (n_read == 0)                                   /* EOF */
        {
            retval = true;
            break;
        }

        bool ret = FileSparseWrite(dd, buf, n_read,
                                   last_write_was_a_hole);
        if (!ret)
        {
            Log(LOG_LEVEL_ERR, "Failed to copy '%s' to '%s'",
                src_name, dst_name);
            break;
        }

        n_read_total += n_read;
    }

    free(buf);
    *total_bytes_written   = n_read_total;
    return retval;
}

/**
 * Always close a written sparse file using this function, else truncation
 * might occur if the last part was a hole.
 *
 * If the tail of the file was a hole (and hence lseek(2)ed on destination
 * instead of being written), do a ftruncate(2) here to ensure the whole file
 * is written to the disc. But ftruncate() fails with EPERM on non-native
 * Linux filesystems (e.g. vfat, vboxfs) when the count is >= than the
 * size of the file. So we write() one byte and then ftruncate() it back.
 *
 * No need for this function to return anything, since the filedescriptor is
 * (attempted to) closed in either success or failure.
 *
 * TODO? instead of needing the #total_bytes_written parameter, we could
 * figure the offset after writing the one byte using lseek(fd,0,SEEK_CUR) and
 * truncate -1 from that offset. It's probably not worth adding an extra
 * system call for simplifying code.
 */
bool FileSparseClose(int fd, const char *filename,
                     bool do_sync,
                     size_t total_bytes_written,
                     bool last_write_was_hole)
{
    if (last_write_was_hole)
    {
        ssize_t ret1 = FullWrite(fd, "", 1);
        if (ret1 == -1)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to close sparse file '%s' (write: %s)",
                filename, GetErrorStr());
            close(fd);
            return false;
        }

        int ret2 = ftruncate(fd, total_bytes_written);
        if (ret2 == -1)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to close sparse file '%s' (ftruncate: %s)",
                filename, GetErrorStr());
            close(fd);
            return false;
        }
    }

    if (do_sync)
    {
        if (fsync(fd) != 0)
        {
            Log(LOG_LEVEL_WARNING,
                "Could not sync to disk file '%s' (fsync: %s)",
                filename, GetErrorStr());
        }
    }

    int ret3 = close(fd);
    if (ret3 == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to close file '%s' (close: %s)",
            filename, GetErrorStr());
        return false;
    }

    return true;
}

ssize_t CfReadLine(char **buff, size_t *size, FILE *fp)
{
    ssize_t b = getline(buff, size, fp);
    assert(b != 0 && "To the best of my knowledge, getline never returns zero");

    if (b > 0)
    {
        if ((*buff)[b - 1] == '\n')
        {
            (*buff)[b - 1] = '\0';
            b--;
        }
    }

    return b;
}

StringSet* GlobFileList(const char *pattern)
{
    StringSet *set = StringSetNew();
    glob_t globbuf;
    int globflags = 0; // TODO: maybe add GLOB_BRACE later

    const char* r_candidates[] = { "*", "*/*", "*/*/*", "*/*/*/*", "*/*/*/*/*", "*/*/*/*/*/*" };
    bool starstar = ( strstr(pattern, "**") != NULL );
    const char** candidates   = starstar ? r_candidates : NULL;
    const int candidate_count = starstar ? 6 : 1;

    for (int pi = 0; pi < candidate_count; pi++)
    {
        char *expanded = starstar ?
            SearchAndReplace(pattern, "**", candidates[pi]) :
            xstrdup(pattern);

#ifdef _WIN32
        if (strchr(expanded, '\\'))
        {
            Log(LOG_LEVEL_VERBOSE, "Found backslash escape character in glob pattern '%s'. "
                "Was forward slash intended?", expanded);
        }
#endif

        if (glob(expanded, globflags, NULL, &globbuf) == 0)
        {
            for (int i = 0; i < globbuf.gl_pathc; i++)
            {
                StringSetAdd(set, xstrdup(globbuf.gl_pathv[i]));
            }

            globfree(&globbuf);
        }

        free(expanded);
    }

    return set;
}

/*******************************************************************/

const char* GetRelocatedProcdirRoot()
{
    const char *procdir = getenv("CFENGINE_TEST_OVERRIDE_PROCDIR");
    if (procdir == NULL)
    {
        procdir = "";
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Overriding /proc location to be %s", procdir);
    }

    return procdir;
}
