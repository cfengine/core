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
#include <string_sequence.h>    /* ReadLenPrefixedString() */
#include <changes_chroot.h>     /* CHROOT_CHANGES_LIST_FILE */
#include <known_dirs.h>         /* GetBinDir() */
#include <files_names.h>        /* JoinPaths() */
#include <pipes.h>              /* cf_popen(), cf_pclose() */

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
            size_t offset = 0;
            size_t to_write = n_read;
            while (to_write > 0)
            {
                size_t n_written = fwrite(buf + offset, 1, to_write, stdout);
                if (n_written > 0)
                {
                    to_write -= n_written;
                    offset += n_written;
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

static inline const char *GetFileTypeDescription(mode_t st_mode)
{
    switch (st_mode & S_IFMT) {
    case S_IFBLK:
        return "block device";
    case S_IFCHR:
        return "character device";
    case S_IFDIR:
        return "directory";
    case S_IFIFO:
        return "FIFO/pipe";
#ifndef __MINGW32__
    case S_IFLNK:
        return "symbolic link";
#endif
    case S_IFREG:
        return "regular file";
    case S_IFSOCK:
        return "socket";
    default:
        debug_abort_if_reached();
        return "unknown";
    }
}

static inline void ManifestFileDetails(const char *path, struct stat *st, bool chrooted)
{
    assert(st != NULL);

    switch (st->st_mode & S_IFMT) {
    case S_IFREG:
        puts(""); /* blank line */
        ManifestFileContents(path);
        break;
#ifndef __MINGW32__
    case S_IFLNK:
        puts(""); /* blank line */
        ManifestLinkTarget(path, chrooted);
        break;
#endif
    case S_IFDIR:
        puts(""); /* blank line */
        ManifestDirectoryListing(path);
        break;
    default:
        /* nothing to do for other types */
        break;
    }
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

    printf("'%s' is a %s\n", path, GetFileTypeDescription(st.st_mode));
    ManifestStatInfo(&st);
    ManifestFileDetails(real_path, &st, chrooted);

    return true;
}

bool ManifestRename(const char *orig_name, const char *new_name)
{
    PrintDelimiter();
    printf("'%s' is the new name of '%s'\n", new_name, orig_name);
    return true;
}

static bool RunDiff(const char *path1, const char *path2)
{
    char diff_path[PATH_MAX];
    strncpy(diff_path, GetBinDir(), sizeof(diff_path));
    JoinPaths(diff_path, sizeof(diff_path), "diff");

    /* We use the '--label' option to override the paths in the output, for example:
     * --- original /etc/motd.d/cfengine
     * +++ changed  /etc/motd.d/cfengine
     * @@ -1,1 +1,1 @@
     * -One line
     * +New line
     */
    char *command;
    int ret = xasprintf(&command, "%s -u --label 'original %s' --label 'changed  %s' '%s' '%s'",
                        diff_path, path1, path1, path1, path2);
    assert(ret != -1); /* should never happen */

    FILE *f = cf_popen(command, "r", true);

    char buf[CF_BUFSIZE];
    size_t n_read;
    bool failure = false;
    while (!failure && ((n_read = fread(buf, 1, sizeof(buf), f)) > 0))
    {
        size_t offset = 0;
        size_t to_write = n_read;
        while (to_write > 0)
        {
            size_t n_written = fwrite(buf + offset, 1, to_write, stdout);
            if (n_written > 0)
            {
                to_write -= n_written;
                offset += n_written;
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Failed to print results from 'diff' for '%s' and '%s'",
                    path1, path2);
                failure = true;
                break;
            }
        }
    }
    if (!feof(f))
    {
        Log(LOG_LEVEL_ERR, "Failed to read output from the 'diff' utility");
        cf_pclose(f);
        return false;
    }
    ret = cf_pclose(f);
    if (ret == 2)
    {
        Log(LOG_LEVEL_ERR, "'diff -u %s %s' failed", path1, path2);
        return false;
    }
    return !failure;
}

static bool DiffFile(const char *path)
{
    const char *chrooted_path = ToChangesChroot(path);

    struct stat st_orig;
    struct stat st_chrooted;
    if (lstat(path, &st_orig) == -1)
    {
        /* Original final doesn't exist, must be a new file in the changes
         * chroot, let's just manifest it instead of running 'diff' on it. */
        ManifestFile(path, true);
        return true;
    }

    PrintDelimiter();
    if (lstat(chrooted_path, &st_chrooted) == -1)
    {
        /* TODO: should this do print info about the original file? */
        printf("'%s' no longer exists\n", path);
        return true;
    }

    if ((st_orig.st_mode & S_IFMT) != (st_chrooted.st_mode & S_IFMT))
    {
        /* File type changed. */
        printf("'%s' changed type from %s to %s\n", path,
               GetFileTypeDescription(st_orig.st_mode),
               GetFileTypeDescription(st_chrooted.st_mode));
        ManifestStatInfo(&st_chrooted);
        ManifestFileDetails(path, &st_chrooted, true);
        return true;
    }
    else
    {
        switch (st_chrooted.st_mode & S_IFMT) {
        case S_IFREG:
        case S_IFDIR:
            return RunDiff(path, chrooted_path);
        default:
            printf("'%s' is a %s\n", path, GetFileTypeDescription(st_chrooted.st_mode));
            ManifestStatInfo(&st_chrooted);
            ManifestFileDetails(path, &st_chrooted, true);
            return true;
        }
    }
}

bool ManifestRenamedFiles()
{
    bool success = true;

    const char *renamed_files_file = ToChangesChroot(CHROOT_RENAMES_LIST_FILE);
    if (access(renamed_files_file, F_OK) == 0)
    {
        int fd = safe_open(renamed_files_file, O_RDONLY);
        if (fd == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to open the file with list of renamed files: %s", GetErrorStr());
            success = false;
        }

        Log(LOG_LEVEL_INFO, "Manifesting renamed files (in the changes chroot)");
        bool done = false;
        while (!done)
        {
            /* The CHROOT_RENAMES_LIST_FILE contains lines where two consecutive
             * lines represent the original and the new name of a file (see
             * RecordFileRenamedInChroot(). */

            /* TODO: read into a PATH_MAX buffers */
            char *orig_name;
            int ret = ReadLenPrefixedString(fd, &orig_name);
            if (ret > 0)
            {
                char *new_name;
                ret = ReadLenPrefixedString(fd, &new_name);
                if (ret > 0)
                {
                    success = (success && ManifestRename(orig_name, new_name));
                    free(new_name);
                }
                else
                {
                    /* If there was the line with the original name, there
                     * must be a line with the new name. */
                    Log(LOG_LEVEL_ERR, "Invalid data about renamed files");
                    success = false;
                    done = true;
                }
                free(orig_name);
            }
            else if (ret == 0)
            {
                /* EOF */
                done = true;
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Failed to read the list of changed files");
                success = false;
                done = true;
            }
        }
        close(fd);
    }
    return success;
}

bool AuditChangedFiles(EvalMode mode)
{
    assert((mode == EVAL_MODE_AUDIT_MANIFEST) || (mode == EVAL_MODE_AUDIT_DIFF));

    bool success = ManifestRenamedFiles();

    const char *action;
    const char *action_ing;
    if (mode == EVAL_MODE_AUDIT_MANIFEST)
    {
        action = "manifest";
        action_ing = "Manifesting";
    }
    else
    {
        action = "show diff for";
        action_ing = "Showing diff for";
    }

    const char *changed_files_file = ToChangesChroot(CHROOT_CHANGES_LIST_FILE);

    /* If the file doesn't exist, there were no changes recorded. */
    if (access(changed_files_file, F_OK) != 0)
    {
        Log(LOG_LEVEL_INFO, "No changed files to %s", action);
        return true;
    }

    int fd = safe_open(changed_files_file, O_RDONLY);
    if (fd == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to open the file with list of changed files: %s", GetErrorStr());
        return false;
    }

    Log(LOG_LEVEL_INFO, "%s changed files (in the changes chroot)", action_ing);
    StringSet *manifested_files = StringSetNew();
    bool done = false;
    while (!done)
    {
        /* TODO: read into a PATH_MAX buffer */
        char *path;
        int ret = ReadLenPrefixedString(fd, &path);
        if (ret > 0)
        {
            /* Each file should only be manifested once. */
            if (!StringSetContains(manifested_files, path))
            {
                if (mode == EVAL_MODE_AUDIT_MANIFEST)
                {
                    success = (success && ManifestFile(path, true));
                }
                else
                {
                    success = (success && DiffFile(path));
                }
                StringSetAdd(manifested_files, path);
            }
            else
            {
                free(path);
            }
        }
        else if (ret == 0)
        {
            /* EOF */
            done = true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Failed to read the list of changed files");
            success = false;
            done = true;
        }
    }
    StringSetDestroy(manifested_files);
    close(fd);
    return success;
}

bool ManifestChangedFiles()
{
    return AuditChangedFiles(EVAL_MODE_AUDIT_MANIFEST);
}

bool DiffChangedFiles()
{
    return AuditChangedFiles(EVAL_MODE_AUDIT_DIFF);
}
