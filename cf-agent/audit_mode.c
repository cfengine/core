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

#include <stdlib.h>
#include <stdio.h>

#include <logging.h>
#include <eval_context.h>       /* ToChangesChroot() */
#include <file_lib.h>           /* IsAbsoluteFileName() */
#include <dir.h>                /* DirOpen(),...*/
#include <string_lib.h>         /* StringEqual() */

#include <audit_mode.h>

#define DELIM_CHAR '='

/* Taken from coreutils/lib/stat-macros.h. */
#define CHMOD_MODE_BITS (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)

static inline void PrintDelimiter()
{
    char *columns = getenv("COLUMNS");
    int n_columns = 0;
    if (columns != NULL)
    {
        n_columns = atoi(columns);
    }
    n_columns = MAX(n_columns, 80) - 5;
    for (int i = n_columns; i > 0; i--)
    {
        putchar(DELIM_CHAR);
    }
    putchar('\n');
}

#ifndef __MINGW32__
static void ManifestStatInfo(const struct stat *st)
{
    assert(st != NULL);
    struct passwd *owner = getpwuid(st->st_uid);
    struct group *group = getgrgid(st->st_gid);

    /* Inspired by the output from the 'stat' command */
    char mode_str[10] = {
        st->st_mode & S_IRUSR ? 'r' : '-',
        st->st_mode & S_IWUSR ? 'w' : '-',
        (st->st_mode & S_ISUID
         ? (st->st_mode & S_IXUSR ? 's' : 'S')
         : (st->st_mode & S_IXUSR ? 'x' : '-')),
        st->st_mode & S_IRGRP ? 'r' : '-',
        st->st_mode & S_IWGRP ? 'w' : '-',
        (st->st_mode & S_ISGID
         ? (st->st_mode & S_IXGRP ? 's' : 'S')
         : (st->st_mode & S_IXGRP ? 'x' : '-')),
        st->st_mode & S_IROTH ? 'r' : '-',
        st->st_mode & S_IWOTH ? 'w' : '-',
        (st->st_mode & S_ISVTX
         ? (st->st_mode & S_IXOTH ? 't' : 'T')
         : (st->st_mode & S_IXOTH ? 'x' : '-')),
        '\0'
    };
    printf("Size: %ju\n", (uintmax_t) st->st_size);
    printf("Access: (%04o/%s)  ", st->st_mode & CHMOD_MODE_BITS, mode_str);
    printf("Uid: (%ju/%s)   ",
           (uintmax_t)st->st_uid, owner->pw_name);
    printf("Gid: (%ju/%s)\n",
           (uintmax_t)st->st_gid, group->gr_name);

#define MAX_TIMESTAMP_SIZE (sizeof("2020-10-05 12:56:18 +0200"))
    char buf[MAX_TIMESTAMP_SIZE] = {0};

    size_t ret = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z",
                          localtime((time_t*) &(st->st_atime)));
    assert((ret > 0) && (ret < MAX_TIMESTAMP_SIZE));
    printf("Access: %s\n", buf);

    ret = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z",
                   localtime((time_t*) &(st->st_mtime)));
    assert((ret > 0) && (ret < MAX_TIMESTAMP_SIZE));
    printf("Modify: %s\n", buf);

    ret = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z",
                   localtime((time_t*) &(st->st_ctime)));
    assert((ret > 0) && (ret < MAX_TIMESTAMP_SIZE));
    printf("Change: %s\n", buf);
}
#else  /* !__MINGW32__ */
static void ManifestStatInfo(const struct stat *st)
{
    assert(st != NULL);

    /* Inspired by the output from the 'stat' command */
    char mode_str[10] = {
        st->st_mode & S_IRUSR ? 'r' : '-',
        st->st_mode & S_IWUSR ? 'w' : '-',
        st->st_mode & S_IXUSR ? 'x' : '-',
        st->st_mode & S_IRGRP ? 'r' : '-',
        st->st_mode & S_IWGRP ? 'w' : '-',
        st->st_mode & S_IXGRP ? 'x' : '-',
        st->st_mode & S_IROTH ? 'r' : '-',
        st->st_mode & S_IWOTH ? 'w' : '-',
        st->st_mode & S_IXOTH ? 'x' : '-',
        '\0'
    };
    printf("Size: %ju\n", (uintmax_t) st->st_size);
    printf("Access: (%04o/%s)  ", st->st_mode, mode_str);
    printf("Uid: %ju   ", (uintmax_t)st->st_uid);
    printf("Gid: %ju\n", (uintmax_t)st->st_gid);

#define MAX_TIMESTAMP_SIZE (sizeof("2020-10-05 12:56:18 +0200"))
    char buf[MAX_TIMESTAMP_SIZE] = {0};

    size_t ret = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z",
                          localtime((time_t*) &(st->st_atime)));
    assert((ret > 0) && (ret < MAX_TIMESTAMP_SIZE));
    printf("Access: %s\n", buf);

    ret = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z",
                   localtime((time_t*) &(st->st_mtime)));
    assert((ret > 0) && (ret < MAX_TIMESTAMP_SIZE));
    printf("Modify: %s\n", buf);

    ret = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z",
                   localtime((time_t*) &(st->st_ctime)));
    assert((ret > 0) && (ret < MAX_TIMESTAMP_SIZE));
    printf("Change: %s\n", buf);
}
#endif  /* !__MINGW32__ */

#ifndef __MINGW32__
static inline void ManifestLinkTarget(const char *link_path, bool chrooted)
{
    char target[PATH_MAX] = {0};
    if (readlink(link_path, target, sizeof(target)) > 0)
    {
        const char *real_target = target;
        if (chrooted && IsAbsoluteFileName(target))
        {
            real_target = ToNormalRoot(target);
        }
        printf("Target: '%s'\n", real_target);
    }
    else
    {
        printf("Invalid target\n");
    }
}
#endif  /* !__MINGW32__ */

static inline void ManifestFileContents(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f != NULL)
    {
        puts("Contents of the file:");
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed to open file for reading: %s", GetErrorStr());
        return;
    }

    bool binary = false;
    char last_char = '\n';

    char buf[CF_BUFSIZE];
    size_t n_read;
    bool done = false;
    while (!done && ((n_read = fread(buf, 1, sizeof(buf), f)) > 0))
    {
        bool is_ascii = true;
        for (size_t i = 0; is_ascii && (i < n_read); i++)
        {
            is_ascii = isascii(buf[i]);
        }
        last_char = buf[n_read - 1];
        if (is_ascii)
        {
            size_t to_write = n_read;
            while (to_write > 0)
            {
                size_t n_written = fwrite(buf, 1, to_write, stdout);
                if (n_written > 0)
                {
                    to_write -= n_written;
                }
                else
                {
                    Log(LOG_LEVEL_ERR, "Failed to print contents of the file");
                    break;
                }
            }
        }
        else
        {
            puts("File contains non-ASCII data");
            binary = true;
            done = true;
        }
    }
    if (!binary && (last_char != '\n'))
    {
        puts("\n\\no newline at the end of file");
    }
    if (!done && !feof(f))
    {
        Log(LOG_LEVEL_ERR, "Failed to print contents of the file");
    }
    fclose(f);
}

static inline void ManifestDirectoryListing(const char *path)
{
    Dir *dir = DirOpen(path);
    if (dir == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to open the directory: %s", GetErrorStr());
        return;
    }
    else
    {
        puts("Directory contents:");
    }
    for (const struct dirent *dir_p = DirRead(dir);
         dir_p != NULL;
         dir_p = DirRead(dir))
    {
        if (StringEqual(dir_p->d_name, ".") ||
            StringEqual(dir_p->d_name, ".."))
        {
            continue;
        }
        else
        {
            puts(dir_p->d_name);
        }
    }
    DirClose(dir);
}

bool ManifestFile(const char *path, bool chrooted)
{
    PrintDelimiter();
    const char *real_path = path;
    if (chrooted)
    {
        real_path = ToChangesChroot(path);
    }

    /* TODO: handle renames */
    struct stat st;
    if (lstat(real_path, &st) == -1)
    {
        printf("'%s' no longer exists\n", path);
        return true;
    }

    switch (st.st_mode & S_IFMT) {
    case S_IFBLK:
        printf("'%s' is a block device\n", path);
        break;
    case S_IFCHR:
        printf("'%s' is a character device\n", path);
        break;
    case S_IFDIR:
        printf("'%s' is a directory\n", path);
        break;
    case S_IFIFO:
        printf("'%s' is a FIFO/pipe\n", path);
        break;
#ifndef __MINGW32__
    case S_IFLNK:
        printf("'%s' is a symbolic link\n", path);
        break;
#endif
    case S_IFREG:
        printf("'%s' is a regular file\n", path);
        break;
    case S_IFSOCK:
        printf("'%s' is a socket\n", path);
        break;
    default:
        debug_abort_if_reached();
    }

    ManifestStatInfo(&st);

    switch (st.st_mode & S_IFMT) {
    case S_IFREG:
        puts(""); /* blank line */
        ManifestFileContents(real_path);
        break;
#ifndef __MINGW32__
    case S_IFLNK:
        puts(""); /* blank line */
        ManifestLinkTarget(real_path, chrooted);
        break;
#endif
    case S_IFDIR:
        puts(""); /* blank line */
        ManifestDirectoryListing(real_path);
        break;
    default:
        /* nothing to do for other types */
        break;
    }

    return true;
}

bool ManifestRename(const char *orig_name, const char *new_name)
{
    PrintDelimiter();
    printf("'%s' is the new name of '%s'\n", new_name, orig_name);
    return true;
}
