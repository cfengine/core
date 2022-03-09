/*
  Copyright 2021 Northern.tech AS

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

#include <platform.h>

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
#include <set.h>                /* StringSet */
#include <map.h>                /* StringMap */
#include <csv_parser.h>         /* GetCsvLineNext() */
#include <unix.h>               /* GetGroupName(), GetUserName() */

#include <simulate_mode.h>

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

    char name[CF_SMALLBUF];
    if (GetUserName(st->st_uid, name, sizeof(name), LOG_LEVEL_ERR))
    {
        printf("Uid: (%ju/%s)   ", (uintmax_t) st->st_uid, name);
    }
    if (GetGroupName(st->st_gid, name, sizeof(name), LOG_LEVEL_ERR))
    {
        printf("Gid: (%ju/%s)\n", (uintmax_t) st->st_gid, name);
    }

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
        free(command);
        return false;
    }
    ret = cf_pclose(f);
    free(command);
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

bool AuditChangedFiles(EvalMode mode, StringSet **audited_files)
{
    assert((mode == EVAL_MODE_SIMULATE_MANIFEST) ||
           (mode == EVAL_MODE_SIMULATE_MANIFEST_FULL) ||
           (mode == EVAL_MODE_SIMULATE_DIFF));

    bool success = true;

    /* Renames are part of the EVAL_MODE_SIMULATE_MANIFEST audit output which
     * shows changes. */
    if (mode != EVAL_MODE_SIMULATE_MANIFEST_FULL)
    {
        success = ManifestRenamedFiles();
    }

    const char *action;
    const char *action_ing;
    const char *file_category;
    const char *files_list_file;
    if (mode == EVAL_MODE_SIMULATE_MANIFEST)
    {
        action = "manifest";
        action_ing = "Manifesting";
        file_category = "changed";
        files_list_file = ToChangesChroot(CHROOT_CHANGES_LIST_FILE);
    }
    else if (mode == EVAL_MODE_SIMULATE_MANIFEST_FULL)
    {
        action = "manifest";
        action_ing = "Manifesting";
        file_category = "unmodified";
        files_list_file = ToChangesChroot(CHROOT_KEPT_LIST_FILE);
    }
    else
    {
        action = "show diff for";
        action_ing = "Showing diff for";
        file_category = "changed";
        files_list_file = ToChangesChroot(CHROOT_CHANGES_LIST_FILE);
    }

    /* If the file doesn't exist, there were no changes recorded. */
    if (access(files_list_file, F_OK) != 0)
    {
        Log(LOG_LEVEL_INFO, "No %s files to %s", file_category, action);
        return true;
    }

    int fd = safe_open(files_list_file, O_RDONLY);
    if (fd == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to open the file with list of %s files: %s", file_category, GetErrorStr());
        return false;
    }

    Log(LOG_LEVEL_INFO, "%s %s files (in the changes chroot)", action_ing, file_category);
    if (*audited_files == NULL)
    {
        *audited_files = StringSetNew();
    }
    bool done = false;
    while (!done)
    {
        /* TODO: read into a PATH_MAX buffer */
        char *path;
        int ret = ReadLenPrefixedString(fd, &path);
        if (ret > 0)
        {
            /* Each file should only be audited once. */
            if (!StringSetContains(*audited_files, path))
            {
                if ((mode == EVAL_MODE_SIMULATE_MANIFEST) ||
                    (mode == EVAL_MODE_SIMULATE_MANIFEST_FULL))
                {
                    success = (success && ManifestFile(path, true));
                }
                else
                {
                    success = (success && DiffFile(path));
                }
                StringSetAdd(*audited_files, path);
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
            Log(LOG_LEVEL_ERR, "Failed to read the list of %s files", file_category);
            success = false;
            done = true;
        }
    }
    close(fd);
    return success;
}

bool ManifestChangedFiles(StringSet **audited_files)
{
    return AuditChangedFiles(EVAL_MODE_SIMULATE_MANIFEST, audited_files);
}

bool ManifestAllFiles(StringSet **audited_files)
{
    return AuditChangedFiles(EVAL_MODE_SIMULATE_MANIFEST_FULL, audited_files);
}

bool DiffChangedFiles(StringSet **audited_files)
{
    return AuditChangedFiles(EVAL_MODE_SIMULATE_DIFF, audited_files);
}


typedef struct PkgOperationRecord_ {
    char *msg;
    char *pkg_ver;
} PkgOperationRecord;

static PkgOperationRecord *PkgOperationRecordNew(char *msg, char *pkg_ver)
{
    PkgOperationRecord *ret = xmalloc(sizeof(PkgOperationRecord));
    ret->msg = msg;
    ret->pkg_ver = pkg_ver;

    return ret;
}

static void PkgOperationRecordDestroy(PkgOperationRecord *pkg_op)
{
    if (pkg_op != NULL)
    {
        free(pkg_op->msg);
        free(pkg_op->pkg_ver);
        free(pkg_op);
    }
}

static inline bool PkgVersionIsGreater(const char *ver1, const char *ver2)
{
    /* Empty/missing versions should be handled separately based on the
     * operations they appear in */
    assert(!NULL_OR_EMPTY(ver1) && !NULL_OR_EMPTY(ver2));

    /* "latest" is greater than any version */
    if (StringEqual(ver1, "latest"))
    {
        return true;
    }

    /* TODO: do real version comparison */
    return (StringSafeCompare(ver1, ver2) == 1);
}

static inline char *GetPkgOperationMsg(ChrootPkgOperationCode op, const char *pkg_name, const char *pkg_arch, const char *pkg_ver)
{
    const char *op_str = "";
    switch (op)
    {
    case CHROOT_PKG_OPERATION_CODE_INSTALL:
        op_str = "installed";
        break;
    case CHROOT_PKG_OPERATION_CODE_REMOVE:
        op_str = "removed";
        break;
    case CHROOT_PKG_OPERATION_CODE_PRESENT:
        op_str = "present";
        break;
    case CHROOT_PKG_OPERATION_CODE_ABSENT:
        op_str = "absent";
        break;
    default:
        debug_abort_if_reached();
    }

    char *msg;
    if (!NULL_OR_EMPTY(pkg_arch) && !NULL_OR_EMPTY(pkg_ver))
    {
        xasprintf(&msg, "Package '%s-%s [%s]' would be %s\n", pkg_name, pkg_arch, pkg_ver, op_str);
    }
    else if (!NULL_OR_EMPTY(pkg_arch))
    {
        xasprintf(&msg, "Package '%s-%s' would be %s\n", pkg_name, pkg_arch, op_str);
    }
    else if (!NULL_OR_EMPTY(pkg_ver))
    {
        xasprintf(&msg, "Package '%s [%s]' would be %s\n", pkg_name, pkg_ver, op_str);
    }
    else
    {
        xasprintf(&msg, "Package '%s' would be %s\n", pkg_name, op_str);
    }

    return msg;
}

bool DiffPkgOperations()
{
    const char *pkgs_ops_csv_file = ToChangesChroot(CHROOT_PKGS_OPS_FILE);
    if (access(pkgs_ops_csv_file, F_OK) != 0)
    {
        Log(LOG_LEVEL_INFO, "No package operations done by the agent run");
        return true;
    }

    FILE *csv_file = safe_fopen(pkgs_ops_csv_file, "r");

    if (csv_file == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to open the file with package operations records");
        return false;
    }

    Map *installed = MapNew(StringHash_untyped, StringEqual_untyped, free, (MapDestroyDataFn) PkgOperationRecordDestroy);
    Map *removed = MapNew(StringHash_untyped, StringEqual_untyped, free, (MapDestroyDataFn) PkgOperationRecordDestroy);
    char *line;
    while ((line = GetCsvLineNext(csv_file)) != NULL)
    {
        Seq *fields = SeqParseCsvString(line);
        free(line);
        if ((fields == NULL) || (SeqLength(fields) != 4))
        {
            Log(LOG_LEVEL_ERR, "Invalid package operation record: '%s'", line);
            SeqDestroy(fields);
            continue;
        }

        /* See RecordPkgOperationInChroot() */
        const char *op       = SeqAt(fields, 0);
        const char *pkg_name = SeqAt(fields, 1);
        const char *pkg_ver  = SeqAt(fields, 2);
        const char *pkg_arch = SeqAt(fields, 3);

        /* These two must always be set properly. */
        assert(!NULL_OR_EMPTY(op));
        assert(!NULL_OR_EMPTY(pkg_name));

        /* We need to have a key for the map encoding package name and
         * architecture. Let's use the sequence "-_-" as a separator to avoid
         * collisions with possible weird package names. */
        char *name_arch;
        xasprintf(&name_arch, "%s-_-%s", pkg_name, pkg_arch ? pkg_arch : "");

        if (*op == CHROOT_PKG_OPERATION_CODE_PRESENT)
        {
            /* 'present' operation means that the package was requested to be present and it already
             * was, so installation didn't happen. However, package operations are not properly
             * simulated, so the package may have been simulate-removed before and the 'present'
             * operation would in reality mean the package would be installed back. It's only
             * recorded as a 'present' operation because the removal doesn't happen in simulate mode
             * and so the package is still seen as present in the system (package cache).
             *
             * This means that a 'present' operation after 'remove' operation results in no
             * difference (the package would be installed back) so the potential message about the
             * removal should be removed. */
            MapRemove(removed, name_arch);
        }
        else if (*op == CHROOT_PKG_OPERATION_CODE_ABSENT)
        {
            /* The same logic as above applies here for an originally absent package that is
             * installed and then reported as absent again. No diff to report, just remove the
             * message about the package installation. */

            /* However, if a different specific version is reported as absent than the version that
             * would have been installed, this removal would not remove the installed package
             * (because of version mismatch). */
            if (NULL_OR_EMPTY(pkg_ver))
            {
                MapRemove(installed, name_arch);
            }
            else
            {
                PkgOperationRecord *record = MapGet(installed, name_arch);
                if ((record != NULL) && StringEqual(pkg_ver, record->pkg_ver))
                {
                    /* Matching version being removed -> cancel the installation */
                    MapRemove(installed, name_arch);
                }
            }
        }
        else if (*op == CHROOT_PKG_OPERATION_CODE_INSTALL)
        {
            /* Package would be installed if there is no previous 'install' operation record with a
             * higher version.
             *                                   OR
             * Package would be removed and now it would be installed. However, if the 'install'
             * operation had the same version as what is already present in the system (remove in
             * simulation mode doesn't remove the package), it would be reported as 'present'
             * operation. So 'install' operation must mean a newer version than what's present would
             * be installed. */

            PkgOperationRecord *prev_record = MapGet(installed, name_arch);
            if ((prev_record == NULL) || PkgVersionIsGreater(pkg_ver, prev_record->pkg_ver))
            {
                char *msg = GetPkgOperationMsg(CHROOT_PKG_OPERATION_CODE_INSTALL,
                                               pkg_name, pkg_arch, pkg_ver);
                PkgOperationRecord *record = PkgOperationRecordNew(msg, SafeStringDuplicate(pkg_ver));
                MapInsert(installed, name_arch, record);
                name_arch = NULL; /* name_arch is now owned by the map (as a key) */
            }

            /* Package installation cancels a previous removal (if any). */
            MapRemove(removed, name_arch);
        }
        else
        {
            assert(*op == CHROOT_PKG_OPERATION_CODE_REMOVE); /* The only option not covered above. */

            /* If there is a previous 'remove' operation record with version specified, prefer that
             * message over a new message without version specification as the net result would be
             * the package being removed, in the version that was pressent. */
            PkgOperationRecord *prev_record = MapGet(removed, name_arch);
            bool insert_new_msg = ((prev_record == NULL) || (NULL_OR_EMPTY(prev_record->pkg_ver)));

            /* If there is a previous 'install' operation and now there is a 'remove' operation it
             * means that the package was initially present, then updated by the 'install' operation
             * and now it is attempted to be removed. If the 'remove' operation specifies a version
             * then in case the versions match, the net result would be no change (install and
             * remove). If there is a mismatch between the versions, the installation would happen,
             * but the removal would fail. If no version is specified for the 'remove' operation. it
             * would remove the installed package. */
            if (NULL_OR_EMPTY(pkg_ver))
            {
                /* No version specified, remove the installation message (if any). */
                MapRemove(installed, name_arch);
            }
            else
            {
                PkgOperationRecord *inst_record = MapGet(installed, name_arch);
                if ((inst_record != NULL) && (StringEqual(pkg_ver, inst_record->pkg_ver)))
                {
                    MapRemove(installed, name_arch);
                }
                else
                {
                    /* Keeping the install message, the removal would make no change. */
                    insert_new_msg = false;
                }
            }

            if (insert_new_msg)
            {
                char *msg = GetPkgOperationMsg(CHROOT_PKG_OPERATION_CODE_REMOVE,
                                               pkg_name, pkg_arch, pkg_ver);
                PkgOperationRecord *record = PkgOperationRecordNew(msg, SafeStringDuplicate(pkg_ver));
                MapInsert(removed, name_arch, record);
                name_arch = NULL; /* name_arch is now owned by the map (as a key) */
            }
        }
        SeqDestroy(fields);
        free(name_arch);
    }
    fclose(csv_file);

    if ((MapSize(installed) == 0) && (MapSize(removed) == 0))
    {
        Log(LOG_LEVEL_INFO, "No differences in installed packages to report");

        MapDestroy(installed);
        MapDestroy(removed);

        return true;
    }

    Log(LOG_LEVEL_INFO, "Showing differences in installed packages");
    MapIterator i = MapIteratorInit(installed);
    MapKeyValue *item;
    while ((item = MapIteratorNext(&i)))
    {
        PkgOperationRecord *value = item->value;
        const char *msg = value->msg;
        puts(msg);
    }
    i = MapIteratorInit(removed);
    while ((item = MapIteratorNext(&i)))
    {
        PkgOperationRecord *value = item->value;
        const char *msg = value->msg;
        puts(msg);
    }

    MapDestroy(installed);
    MapDestroy(removed);

    return true;
}

bool ManifestPkgOperations()
{
    const char *pkgs_ops_csv_file = ToChangesChroot(CHROOT_PKGS_OPS_FILE);
    if (access(pkgs_ops_csv_file, F_OK) != 0)
    {
        Log(LOG_LEVEL_INFO, "No package operations done by the agent run");
        return true;
    }

    FILE *csv_file = safe_fopen(pkgs_ops_csv_file, "r");

    if (csv_file == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to open the file with package operations records");
        return false;
    }

    Map *present = MapNew(StringHash_untyped, StringEqual_untyped, free, (MapDestroyDataFn) PkgOperationRecordDestroy);
    Map *absent = MapNew(StringHash_untyped, StringEqual_untyped, free, (MapDestroyDataFn) PkgOperationRecordDestroy);
    char *line;
    while ((line = GetCsvLineNext(csv_file)) != NULL)
    {
        Seq *fields = SeqParseCsvString(line);
        free(line);
        if ((fields == NULL) || (SeqLength(fields) != 4))
        {
            Log(LOG_LEVEL_ERR, "Invalid package operation record: '%s'", line);
            SeqDestroy(fields);
            continue;
        }

        /* See RecordPkgOperationInChroot() */
        const char *op       = SeqAt(fields, 0);
        const char *pkg_name = SeqAt(fields, 1);
        const char *pkg_ver  = SeqAt(fields, 2);
        const char *pkg_arch = SeqAt(fields, 3);

        /* These two must always be set properly. */
        assert(!NULL_OR_EMPTY(op));
        assert(!NULL_OR_EMPTY(pkg_name));

        /* We need to have a key for the map encoding package name and
         * architecture. Let's use the sequence "-_-" as a separator to avoid
         * collisions with possible weird package names. */
        char *name_arch;
        xasprintf(&name_arch, "%s-_-%s", pkg_name, pkg_arch ? pkg_arch : "");

        if ((*op == CHROOT_PKG_OPERATION_CODE_INSTALL) ||
            (*op == CHROOT_PKG_OPERATION_CODE_PRESENT))
        {
            /* If there is a previous install/present operation, we want to choose the message with
             * the higher version or the message which has a specific version (if any). */
            PkgOperationRecord *prev_record = MapGet(present, name_arch);
            if ((prev_record == NULL) ||
                (NULL_OR_EMPTY(prev_record->pkg_ver) && !NULL_OR_EMPTY(pkg_ver)) ||
                (!NULL_OR_EMPTY(pkg_ver) && !NULL_OR_EMPTY(prev_record->pkg_ver) &&
                 PkgVersionIsGreater(pkg_ver, prev_record->pkg_ver)))
            {
                char *msg = GetPkgOperationMsg(CHROOT_PKG_OPERATION_CODE_PRESENT,
                                               pkg_name, pkg_arch, pkg_ver);
                PkgOperationRecord *record = PkgOperationRecordNew(msg, SafeStringDuplicate(pkg_ver));
                MapInsert(present, name_arch, record);
                name_arch = NULL; /* name_arch is now owned by the map (as a key) */
            }

            /* Cancels any previous remove/absent message. */
            MapRemove(absent, name_arch);
        }
        else
        {
            assert((*op == CHROOT_PKG_OPERATION_CODE_REMOVE) ||
                   (*op == CHROOT_PKG_OPERATION_CODE_ABSENT));

            /* If there is a 'present' message with a different version than the version specified
             * here (if any), we know that the removal would fail and so it should not be
             * reported. */
            PkgOperationRecord *present_record = MapGet(present, name_arch);
            if ((present_record == NULL) ||
                NULL_OR_EMPTY(present_record->pkg_ver) || NULL_OR_EMPTY(pkg_ver) ||
                StringEqual(present_record->pkg_ver, pkg_ver))
            {
                /* Remove the 'present' message now that we know the removal would/should work. */
                MapRemove(present, name_arch);

                /* If there is a previous 'absent' message, we want to use the message with no
                 * version specification (if any) because it's more generic. */
                PkgOperationRecord *prev_record = MapGet(absent, name_arch);
                if ((prev_record == NULL) ||
                    (!NULL_OR_EMPTY(prev_record->pkg_ver) && NULL_OR_EMPTY(pkg_ver)))
                {
                    char *msg = GetPkgOperationMsg(CHROOT_PKG_OPERATION_CODE_ABSENT,
                                                   pkg_name, pkg_arch, pkg_ver);
                    PkgOperationRecord *record = PkgOperationRecordNew(msg, SafeStringDuplicate(pkg_ver));
                    MapInsert(absent, name_arch, record);
                    name_arch = NULL; /* name_arch is now owned by the map (as a key) */
                }
            }
        }
        SeqDestroy(fields);
        free(name_arch);
    }
    fclose(csv_file);

    /* If there were package operations (the file with the records exists, which is checked above),
     * there must be something to manifest. Otherwise, there's a flaw in the logic above,
     * manipulating the maps. */
    assert((MapSize(present) != 0) || (MapSize(absent) != 0));

    Log(LOG_LEVEL_INFO, "Manifesting present and absent packages");
    MapIterator i = MapIteratorInit(present);
    MapKeyValue *item;
    while ((item = MapIteratorNext(&i)))
    {
        PkgOperationRecord *value = item->value;
        const char *msg = value->msg;
        puts(msg);
    }
    i = MapIteratorInit(absent);
    while ((item = MapIteratorNext(&i)))
    {
        PkgOperationRecord *value = item->value;
        const char *msg = value->msg;
        puts(msg);
    }

    MapDestroy(present);
    MapDestroy(absent);

    return true;
}
