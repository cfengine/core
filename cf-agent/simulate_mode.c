/*
  Copyright 2024 Northern.tech AS

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
#include <json.h>               /* JsonElement */
#include <writer.h>             /* StringWriter() */
#include <hash.h>               /* HashFile(), HashPrintSafe() */
#include <definitions.h>        /* CF_PERMS_DEFAULT */

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

    NDEBUG_UNUSED size_t ret = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z",
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

    NDEBUG_UNUSED size_t ret = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z",
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
    strncpy(diff_path, GetBinDir(), sizeof(diff_path) - 1);
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

typedef struct PkgOperation_ {
    char *name;
    char *arch;
    char *version;
} PkgOperation;

static PkgOperation *PkgOperationNew(char *name, char *arch, char *version)
{
    PkgOperation *ret = xmalloc(sizeof(PkgOperation));
    ret->name = name;
    ret->arch = arch;
    ret->version = version;

    return ret;
}

static void PkgOperationDestroy(PkgOperation *pkg_op)
{
    if (pkg_op != NULL)
    {
        free(pkg_op->name);
        free(pkg_op->arch);
        free(pkg_op->version);
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

/* Reduce the package operations recorded during the agent run to the net set
 * of packages that would be installed and removed. On success, either both
 * #installed_out and #removed_out are set to maps of PkgOperation items keyed
 * by package name and architecture, or both are set to NULL if no package
 * operations were recorded during the run. */
static bool CollectPkgOperations(Map **installed_out, Map **removed_out)
{
    assert(installed_out != NULL);
    assert(removed_out != NULL);

    *installed_out = NULL;
    *removed_out = NULL;

    const char *pkgs_ops_csv_file = ToChangesChroot(CHROOT_PKGS_OPS_FILE);
    if (access(pkgs_ops_csv_file, F_OK) != 0)
    {
        return true;
    }

    FILE *csv_file = safe_fopen(pkgs_ops_csv_file, "r");

    if (csv_file == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to open the file with package operations records");
        return false;
    }

    Map *installed = MapNew(StringHash_untyped, StringEqual_untyped, free, (MapDestroyDataFn) PkgOperationDestroy);
    Map *removed = MapNew(StringHash_untyped, StringEqual_untyped, free, (MapDestroyDataFn) PkgOperationDestroy);
    char *line;
    while ((line = GetCsvLineNext(csv_file)) != NULL)
    {
        Seq *fields = SeqParseCsvString(line);
        if ((fields == NULL) || (SeqLength(fields) != 4))
        {
            Log(LOG_LEVEL_ERR, "Invalid package operation record: '%s'", line);
            free(line);
            SeqDestroy(fields);
            continue;
        }
        free(line);

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
             * difference (the package would be installed back) so the potential record about the
             * removal should be removed. */
            MapRemove(removed, name_arch);
        }
        else if (*op == CHROOT_PKG_OPERATION_CODE_ABSENT)
        {
            /* The same logic as above applies here for an originally absent package that is
             * installed and then reported as absent again. No diff to report, just remove the
             * record about the package installation. */

            /* However, if a different specific version is reported as absent than the version that
             * would have been installed, this removal would not remove the installed package
             * (because of version mismatch). */
            if (NULL_OR_EMPTY(pkg_ver))
            {
                MapRemove(installed, name_arch);
            }
            else
            {
                PkgOperation *pkg_op = MapGet(installed, name_arch);
                if ((pkg_op != NULL) && StringEqual(pkg_ver, pkg_op->version))
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

            /* Package installation cancels a previous removal (if any). */
            MapRemove(removed, name_arch);

            PkgOperation *prev_op = MapGet(installed, name_arch);
            if ((prev_op == NULL) || PkgVersionIsGreater(pkg_ver, prev_op->version))
            {
                PkgOperation *pkg_op = PkgOperationNew(SafeStringDuplicate(pkg_name),
                                                       SafeStringDuplicate(pkg_arch),
                                                       SafeStringDuplicate(pkg_ver));
                MapInsert(installed, name_arch, pkg_op);
                name_arch = NULL; /* name_arch is now owned by the map (as a key) */
            }
        }
        else
        {
            assert(*op == CHROOT_PKG_OPERATION_CODE_REMOVE); /* The only option not covered above. */

            /* If there is a previous 'remove' operation record with version specified, prefer that
             * record over a new record without version specification as the net result would be
             * the package being removed, in the version that was pressent. */
            PkgOperation *prev_op = MapGet(removed, name_arch);
            bool insert_new_record = ((prev_op == NULL) || (NULL_OR_EMPTY(prev_op->version)));

            /* If there is a previous 'install' operation and now there is a 'remove' operation it
             * means that the package was initially present, then updated by the 'install' operation
             * and now it is attempted to be removed. If the 'remove' operation specifies a version
             * then in case the versions match, the net result would be no change (install and
             * remove). If there is a mismatch between the versions, the installation would happen,
             * but the removal would fail. If no version is specified for the 'remove' operation. it
             * would remove the installed package. */
            if (NULL_OR_EMPTY(pkg_ver))
            {
                /* No version specified, remove the installation record (if any). */
                MapRemove(installed, name_arch);
            }
            else
            {
                PkgOperation *inst_op = MapGet(installed, name_arch);
                if ((inst_op != NULL) && (StringEqual(pkg_ver, inst_op->version)))
                {
                    MapRemove(installed, name_arch);
                }
                else
                {
                    /* Keeping the install record, the removal would make no change. */
                    insert_new_record = false;
                }
            }

            if (insert_new_record)
            {
                PkgOperation *pkg_op = PkgOperationNew(SafeStringDuplicate(pkg_name),
                                                       SafeStringDuplicate(pkg_arch),
                                                       SafeStringDuplicate(pkg_ver));
                MapInsert(removed, name_arch, pkg_op);
                name_arch = NULL; /* name_arch is now owned by the map (as a key) */
            }
        }
        SeqDestroy(fields);
        free(name_arch);
    }
    fclose(csv_file);

    *installed_out = installed;
    *removed_out = removed;

    return true;
}

bool DiffPkgOperations()
{
    Map *installed = NULL;
    Map *removed = NULL;
    if (!CollectPkgOperations(&installed, &removed))
    {
        return false;
    }

    if (installed == NULL)
    {
        assert(removed == NULL);
        Log(LOG_LEVEL_INFO, "No package operations done by the agent run");
        return true;
    }

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
        const PkgOperation *pkg_op = item->value;
        char *msg = GetPkgOperationMsg(CHROOT_PKG_OPERATION_CODE_INSTALL,
                                       pkg_op->name, pkg_op->arch, pkg_op->version);
        puts(msg);
        free(msg);
    }
    i = MapIteratorInit(removed);
    while ((item = MapIteratorNext(&i)))
    {
        const PkgOperation *pkg_op = item->value;
        char *msg = GetPkgOperationMsg(CHROOT_PKG_OPERATION_CODE_REMOVE,
                                       pkg_op->name, pkg_op->arch, pkg_op->version);
        puts(msg);
        free(msg);
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
        if ((fields == NULL) || (SeqLength(fields) != 4))
        {
            Log(LOG_LEVEL_ERR, "Invalid package operation record: '%s'", line);
            free(line);
            SeqDestroy(fields);
            continue;
        }
        free(line);

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
            /* Cancels any previous remove/absent message. */
            MapRemove(absent, name_arch);

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


/* Version of the JSON change set document written by WriteChangesJson(). Must
 * be incremented whenever the structure or the semantics of the document
 * change in a way that existing consumers cannot safely ignore. */
#define CHANGES_JSON_FORMAT_VERSION 1

static JsonElement *ChangedFileAsJson(const char *path)
{
    assert(path != NULL);

    JsonElement *file_info = JsonObjectCreate(8);
    JsonObjectAppendString(file_info, "path", path);

    const char *chrooted_path = ToChangesChroot(path);
    struct stat st;
    if (lstat(chrooted_path, &st) == -1)
    {
        /* Deleted by the run (or created and deleted again, in which case the
         * file doesn't exist before the run either). */
        JsonObjectAppendString(file_info, "change", "deleted");
        return file_info;
    }

    struct stat st_orig;
    if (lstat(path, &st_orig) == -1)
    {
        JsonObjectAppendString(file_info, "change", "created");
    }
    else
    {
        JsonObjectAppendString(file_info, "change", "modified");
    }

    /* All the information below describes the file as it would be after the
     * run (the file in the changes chroot). */
    JsonObjectAppendString(file_info, "type",
                           GetFileTypeDescription(st.st_mode));

    char perms[5];
    xsnprintf(perms, sizeof(perms), "%04jo",
              (uintmax_t) (st.st_mode & CHMOD_MODE_BITS));
    JsonObjectAppendString(file_info, "permissions", perms);

    /* uid_t and gid_t are unsigned, so an id above INT_MAX (e.g. the common
     * 'nobody' uid 4294967294) needs the 64-bit writer to not turn
     * negative. */
    JsonObjectAppendInteger64(file_info, "uid", (int64_t) st.st_uid);
    JsonObjectAppendInteger64(file_info, "gid", (int64_t) st.st_gid);

    if (S_ISREG(st.st_mode))
    {
        JsonObjectAppendInteger64(file_info, "size", (int64_t) st.st_size);

        unsigned char digest[EVP_MAX_MD_SIZE + 1] = {0};
        HashFile(chrooted_path, digest, HASH_METHOD_SHA256, false);

        /* HashFile() cannot report a failure, but it leaves the digest
         * zeroed when one occurs and a real SHA-256 digest is never all
         * zeros. Omit the field rather than presenting zeros as a real
         * digest. */
        const size_t digest_len = HashSizeFromId(HASH_METHOD_SHA256);
        bool have_digest = false;
        for (size_t i = 0; !have_digest && (i < digest_len); i++)
        {
            have_digest = (digest[i] != 0);
        }
        if (have_digest)
        {
            char digest_str[CF_HOSTKEY_STRING_SIZE];
            HashPrintSafe(digest_str, sizeof(digest_str), digest,
                          HASH_METHOD_SHA256, false);
            JsonObjectAppendString(file_info, "sha256", digest_str);
        }
        else
        {
            Log(LOG_LEVEL_ERR,
                "Failed to compute the SHA-256 digest of '%s'",
                chrooted_path);
        }
    }
#ifndef __MINGW32__
    else if (S_ISLNK(st.st_mode))
    {
        char target[PATH_MAX] = {0};
        ssize_t target_len = readlink(chrooted_path, target,
                                      sizeof(target) - 1);

        /* A target of sizeof(target) - 1 bytes or more would be truncated
         * and presented as a wrong (but plausible) path, better to omit it
         * than to report it wrong. */
        if ((target_len > 0) && ((size_t) target_len < (sizeof(target) - 1)))
        {
            const char *real_target = target;
            if (IsAbsoluteFileName(target))
            {
                real_target = ToNormalRoot(target);
            }
            JsonObjectAppendString(file_info, "target", real_target);
        }
    }
#endif  /* !__MINGW32__ */

    return file_info;
}

static bool AddChangedFilesToJson(JsonElement *files)
{
    assert(files != NULL);

    const char *files_list_file = ToChangesChroot(CHROOT_CHANGES_LIST_FILE);

    /* If the file doesn't exist, there were no changes recorded. */
    if (access(files_list_file, F_OK) != 0)
    {
        return true;
    }

    int fd = safe_open(files_list_file, O_RDONLY);
    if (fd == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to open the file with list of changed files: %s",
            GetErrorStr());
        return false;
    }

    StringSet *recorded_files = StringSetNew();
    bool success = true;
    bool done = false;
    while (!done)
    {
        char *path;
        int ret = ReadLenPrefixedString(fd, &path);
        if (ret > 0)
        {
            /* Each file should only be reported once. */
            if (!StringSetContains(recorded_files, path))
            {
                JsonArrayAppendObject(files, ChangedFileAsJson(path));

                /* The set takes ownership of path. */
                StringSetAdd(recorded_files, path);
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
    close(fd);
    StringSetDestroy(recorded_files);
    return success;
}

static bool AddRenamedFilesToJson(JsonElement *renames)
{
    assert(renames != NULL);

    const char *renamed_files_file = ToChangesChroot(CHROOT_RENAMES_LIST_FILE);

    /* If the file doesn't exist, there were no renames recorded. */
    if (access(renamed_files_file, F_OK) != 0)
    {
        return true;
    }

    int fd = safe_open(renamed_files_file, O_RDONLY);
    if (fd == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to open the file with list of renamed files: %s",
            GetErrorStr());
        return false;
    }

    bool success = true;
    bool done = false;
    while (!done)
    {
        /* The CHROOT_RENAMES_LIST_FILE contains lines where two consecutive
         * lines represent the original and the new name of a file (see
         * RecordFileRenamedInChroot(). */
        char *orig_name;
        int ret = ReadLenPrefixedString(fd, &orig_name);
        if (ret > 0)
        {
            char *new_name;
            ret = ReadLenPrefixedString(fd, &new_name);
            if (ret > 0)
            {
                JsonElement *rename = JsonObjectCreate(2);
                JsonObjectAppendString(rename, "old_name", orig_name);
                JsonObjectAppendString(rename, "new_name", new_name);
                JsonArrayAppendObject(renames, rename);
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
            Log(LOG_LEVEL_ERR, "Failed to read the list of renamed files");
            success = false;
            done = true;
        }
    }
    close(fd);
    return success;
}

static void JsonArrayAppendPkgOperations(JsonElement *packages, Map *pkg_ops,
                                         const char *operation)
{
    assert(packages != NULL);
    assert(pkg_ops != NULL);

    MapIterator i = MapIteratorInit(pkg_ops);
    MapKeyValue *item;
    while ((item = MapIteratorNext(&i)))
    {
        const PkgOperation *pkg_op = item->value;
        JsonElement *op_info = JsonObjectCreate(4);
        JsonObjectAppendString(op_info, "operation", operation);
        JsonObjectAppendString(op_info, "name", pkg_op->name);
        if (!NULL_OR_EMPTY(pkg_op->arch))
        {
            JsonObjectAppendString(op_info, "architecture", pkg_op->arch);
        }
        if (!NULL_OR_EMPTY(pkg_op->version))
        {
            JsonObjectAppendString(op_info, "version", pkg_op->version);
        }
        JsonArrayAppendObject(packages, op_info);
    }
}

static bool AddPkgOperationsToJson(JsonElement *packages)
{
    assert(packages != NULL);

    Map *installed = NULL;
    Map *removed = NULL;
    if (!CollectPkgOperations(&installed, &removed))
    {
        return false;
    }

    if (installed == NULL)
    {
        /* No package operations recorded. */
        assert(removed == NULL);
        return true;
    }

    JsonArrayAppendPkgOperations(packages, installed, "install");
    JsonArrayAppendPkgOperations(packages, removed, "remove");

    MapDestroy(installed);
    MapDestroy(removed);

    return true;
}

/* Returns the length (2 to 4) of the valid multi-byte UTF-8 sequence at the
 * beginning of the #n bytes at #bytes, or 0 if they do not start with one
 * (overlong encodings, surrogates and code points above U+10FFFF are
 * invalid). */
static size_t ValidUtf8SequenceLength(const unsigned char *bytes, size_t n)
{
    assert(bytes != NULL);

    if (n < 2)
    {
        return 0;
    }

    /* The ranges of the lead byte and the first continuation byte, see RFC
     * 3629. */
    size_t len;
    unsigned char cont_min = 0x80;
    unsigned char cont_max = 0xbf;
    if ((bytes[0] >= 0xc2) && (bytes[0] <= 0xdf))
    {
        len = 2;
    }
    else if ((bytes[0] >= 0xe0) && (bytes[0] <= 0xef))
    {
        len = 3;
        if (bytes[0] == 0xe0)
        {
            cont_min = 0xa0;    /* overlong otherwise */
        }
        else if (bytes[0] == 0xed)
        {
            cont_max = 0x9f;    /* surrogate otherwise */
        }
    }
    else if ((bytes[0] >= 0xf0) && (bytes[0] <= 0xf4))
    {
        len = 4;
        if (bytes[0] == 0xf0)
        {
            cont_min = 0x90;    /* overlong otherwise */
        }
        else if (bytes[0] == 0xf4)
        {
            cont_max = 0x8f;    /* above U+10FFFF otherwise */
        }
    }
    else
    {
        /* A continuation byte, an overlong lead byte (0xc0, 0xc1) or a byte
         * that cannot appear in UTF-8 at all. */
        return 0;
    }

    if (n < len)
    {
        return 0;
    }
    if ((bytes[1] < cont_min) || (bytes[1] > cont_max))
    {
        return 0;
    }
    for (size_t i = 2; i < len; i++)
    {
        if ((bytes[i] < 0x80) || (bytes[i] > 0xbf))
        {
            return 0;
        }
    }
    return len;
}

/* If #c points at a "\uXXXX" escape sequence encoding a single byte (XXXX <=
 * 0x00FF, the only form JsonEncodeStringWriter() produces), stores the byte
 * in #byte_out and returns true, otherwise returns false. */
static bool GetJsonEscapedByte(const char *c, unsigned char *byte_out)
{
    assert(c != NULL);
    assert(byte_out != NULL);

    if ((c[0] != '\\') || (c[1] != 'u'))
    {
        return false;
    }

    unsigned int value = 0;
    for (size_t i = 2; i < 6; i++)
    {
        const char h = c[i];
        value <<= 4;
        if ((h >= '0') && (h <= '9'))
        {
            value |= (h - '0');
        }
        else if ((h >= 'a') && (h <= 'f'))
        {
            value |= (h - 'a' + 10);
        }
        else if ((h >= 'A') && (h <= 'F'))
        {
            value |= (h - 'A' + 10);
        }
        else
        {
            return false;
        }
    }
    if (value > 0xff)
    {
        return false;
    }

    *byte_out = value;
    return true;
}

/* JsonWrite() (JsonEncodeStringWriter() in libntech) escapes every byte
 * outside printable ASCII as an individual "\u00XX" escape sequence. That is
 * well-formed JSON, but "\u00XX" denotes the code point U+00XX, so a
 * conformant JSON parser decodes each byte of a multi-byte UTF-8 character
 * (e.g. in a file name) as a separate, wrong character. Since the change set
 * is meant for consumption by other programs, rewrite the escapes that
 * encode a valid UTF-8 sequence back to the raw bytes, which a JSON string
 * can carry verbatim. Escaped bytes that are not part of a valid UTF-8
 * sequence are left as "\u00XX" -- there is no way to represent them exactly
 * in a JSON document, which has to be valid UTF-8 itself. Returns the
 * rewritten copy of #json_str. */
static char *RestoreUtf8InJson(const char *json_str)
{
    assert(json_str != NULL);

    Writer *writer = StringWriter();
    const char *c = json_str;
    while (*c != '\0')
    {
        /* Everything JsonWrite() emits is ASCII and every backslash starts
         * an escape sequence inside a string. Collect up to 4 consecutive
         * escapes of non-ASCII bytes -- the longest possible UTF-8
         * sequence. */
        unsigned char bytes[4];
        size_t n_bytes = 0;
        unsigned char byte;
        while ((n_bytes < 4) &&
               GetJsonEscapedByte(c + (6 * n_bytes), &byte) &&
               (byte >= 0x80))
        {
            bytes[n_bytes] = byte;
            n_bytes++;
        }

        if (n_bytes == 0)
        {
            WriterWriteChar(writer, *c);
            c++;
            if ((c[-1] == '\\') && (*c != '\0'))
            {
                /* Copy the escaped character too so that an escaped
                 * backslash ("\\") is not mistaken for the start of a new
                 * escape sequence. */
                WriterWriteChar(writer, *c);
                c++;
            }
        }
        else
        {
            const size_t seq_len = ValidUtf8SequenceLength(bytes, n_bytes);
            if (seq_len > 0)
            {
                for (size_t i = 0; i < seq_len; i++)
                {
                    WriterWriteChar(writer, bytes[i]);
                }
                c += 6 * seq_len;
            }
            else
            {
                /* Not (the start of) a valid UTF-8 sequence, keep the first
                 * escape and reconsider the rest in the next iteration. */
                for (size_t i = 0; i < 6; i++)
                {
                    WriterWriteChar(writer, c[i]);
                }
                c += 6;
            }
        }
    }
    return StringWriterClose(writer);
}

bool WriteChangesJson(const char *output_file)
{
    assert(output_file != NULL);

    const char *mode_str;
    switch (EVAL_MODE)
    {
    case EVAL_MODE_SIMULATE_MANIFEST:
        mode_str = "manifest";
        break;
    case EVAL_MODE_SIMULATE_MANIFEST_FULL:
        mode_str = "manifest-full";
        break;
    case EVAL_MODE_SIMULATE_DIFF:
        mode_str = "diff";
        break;
    default:
        debug_abort_if_reached();
        mode_str = "unknown";
        break;
    }

    JsonElement *json = JsonObjectCreate(5);
    JsonObjectAppendInteger(json, "format_version",
                            CHANGES_JSON_FORMAT_VERSION);
    JsonObjectAppendString(json, "simulate_mode", mode_str);

    JsonElement *files = JsonArrayCreate(10);
    bool success = AddChangedFilesToJson(files);
    JsonObjectAppendArray(json, "files", files);

    JsonElement *renames = JsonArrayCreate(10);
    success = success && AddRenamedFilesToJson(renames);
    JsonObjectAppendArray(json, "renames", renames);

    JsonElement *packages = JsonArrayCreate(10);
    success = success && AddPkgOperationsToJson(packages);
    JsonObjectAppendArray(json, "packages", packages);

    if (!success)
    {
        /* An incomplete document could be mistaken for the complete change
         * set, better to not produce any output at all. */
        JsonDestroy(json);
        return false;
    }

    Log(LOG_LEVEL_INFO, "Writing the simulated change set to '%s'",
        output_file);

    Writer *writer = StringWriter();
    JsonWrite(writer, json, 0);
    WriterWrite(writer, "\n");
    JsonDestroy(json);
    char *escaped = StringWriterClose(writer);
    char *document = RestoreUtf8InJson(escaped);
    free(escaped);

    /* Write the document to a temporary file and rename() it to
     * #output_file only once it is complete, so that a failure in the
     * middle can never leave a truncated document behind or destroy a
     * pre-existing #output_file. rename() also replaces a symbolic link at
     * #output_file instead of writing through it. */
    char *tmp_file;
    xasprintf(&tmp_file, "%s.new", output_file);
    unlink(tmp_file);           /* leftover from a previous run (if any) */
    int fd = safe_open_create_perms(tmp_file, O_WRONLY | O_CREAT | O_EXCL,
                                    CF_PERMS_DEFAULT);
    if (fd == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to open '%s' for writing the simulated change set (open: %s)",
            tmp_file, GetErrorStr());
        free(tmp_file);
        free(document);
        return false;
    }

    const size_t document_len = strlen(document);
    success = (FullWrite(fd, document, document_len) == (ssize_t) document_len);
    free(document);
    if (close(fd) == -1)
    {
        success = false;
    }
    if (!success)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to write the simulated change set to '%s' (write: %s)",
            tmp_file, GetErrorStr());
        unlink(tmp_file);
        free(tmp_file);
        return false;
    }

#ifdef __MINGW32__
    /* rename() cannot replace an existing file on Windows. The document is
     * safely written at this point, so the worst case is being left with
     * just the temporary file (and a failure below). */
    unlink(output_file);
#endif
    if (rename(tmp_file, output_file) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to move the simulated change set to '%s' (rename: %s)",
            output_file, GetErrorStr());
        unlink(tmp_file);
        free(tmp_file);
        return false;
    }
    free(tmp_file);

    return true;
}
