#include <platform.h>
#include <stdio.h>
#include <utilities.h>
#include <backup.h>
#include <logging.h>
#include <file_lib.h>
#include <known_dirs.h>
#include <string_lib.h>
#include <diagnose.h>
#include <alloc.h>
#include <libgen.h>     /* basename() (on some platforms) */
#include <assert.h>

#if defined(__MINGW32__) || !defined(LMDB)

int backup_main(ARG_UNUSED int argc, ARG_UNUSED const char *const *const argv)
{
    Log(LOG_LEVEL_ERR,
        "cf-check backup not available on this platform/build");
    return 1;
}

int backup_files_copy(ARG_UNUSED Seq *filenames)
{
    Log(LOG_LEVEL_INFO,
        "database backup not available on this platform/build");
    return 0;
}

#else

#include <replicate_lmdb.h>

static void print_usage(void)
{
    printf("Usage: cf-check backup [-d] [FILE ...]\n");
    printf("Example: cf-check backup /var/cfengine/state/cf_lastseen.lmdb\n");
    printf("Options: -d|--dump use dump strategy instead of plain copy");
}

const char *create_backup_dir()
{
    static char backup_dir[PATH_MAX];
    static char backup_root[PATH_MAX];
    snprintf(
        backup_root,
        PATH_MAX,
        "%s%c%s%c",
        GetWorkDir(),
        FILE_SEPARATOR,
        "backups",
        FILE_SEPARATOR);

    if (mkdir(backup_root, 0700) != 0)
    {
        if (errno != EEXIST)
        {
            Log(LOG_LEVEL_ERR,
                "Could not create directory '%s' (%s)",
                backup_root,
                strerror(errno));
            return NULL;
        }
    }

    time_t ts = time(NULL);
    if (ts == (time_t)-1)
    {
        Log(LOG_LEVEL_ERR, "Could not get current time");
        return NULL;
    }

    const int n =
        snprintf(backup_dir, PATH_MAX, "%s%jd/", backup_root, (intmax_t)ts);
    if (n >= PATH_MAX)
    {
        Log(LOG_LEVEL_ERR,
            "Backup path too long: %jd/%jd",
            (intmax_t)n,
            (intmax_t)PATH_MAX);
        return NULL;
    }

    if (mkdir(backup_dir, 0700) != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Could not create directory '%s' (%s)",
            backup_dir,
            strerror(errno));
        return NULL;
    }

    return backup_dir;
}

int backup_files_copy(Seq *filenames)
{
    assert(filenames != NULL);
    const size_t length = SeqLength(filenames);

    // Attempting to back up 0 files is considered a failure:
    assert_or_return(length > 0, 1);

    const char *backup_dir = create_backup_dir();

    Log(LOG_LEVEL_INFO, "Backing up to '%s'", backup_dir);

    int ret = 0;
    for (int i = 0; i < length; ++i)
    {
        const char *file = SeqAt(filenames, i);
        if (!File_CopyToDir(file, backup_dir))
        {
            Log(LOG_LEVEL_ERR, "Copying '%s' failed", file);
            ret++;
        }
    }
    return ret;
}

/**
 * Replicate LMDB files by reading their entries and writing them into new LMDB files.
 *
 * @return the number of files that failed to be replicated or -1 in case of
 *         some internal failure
 */
static int backup_files_replicate(const Seq *files)
{
    assert(files != NULL);
    const size_t length = SeqLength(files);

    // Attempting to back up 0 files is considered a failure:
    assert_or_return(length > 0, 1);

    const char *backup_dir = create_backup_dir();

    Log(LOG_LEVEL_INFO, "Backing up to '%s' using dump strategy", backup_dir);

    int ret = 0;
    for (int i = 0; i < length; ++i)
    {
        const char *file = SeqAt(files, i);
        assert(StringEndsWith(backup_dir, "/"));
        char *file_copy = xstrdup(file); /* basename() can modify the string */
        char *dest_file = StringFormat("%s%s", backup_dir, basename(file_copy));
        free(file_copy);
        pid_t child_pid = fork();
        if (child_pid == 0)
        {
            /* child */
            exit(replicate_lmdb(file, dest_file));
        }
        else
        {
            /* parent */
            int status;
            pid_t pid = waitpid(child_pid, &status, 0);
            if (pid != child_pid)
            {
                /* real error that should never happen */
                return -1;
            }
            if (WIFEXITED(status) && WEXITSTATUS(status) != CF_CHECK_OK
                && WEXITSTATUS(status) != CF_CHECK_LMDB_CORRUPT_PAGE)
            {
                Log(LOG_LEVEL_ERR, "Failed to backup file '%s'", file);
                ret++;
            }
            if (WIFSIGNALED(status))
            {
                Log(LOG_LEVEL_ERR, "Failed to backup file '%s', child process signaled (%d)",
                    file, WTERMSIG(status));
                ret++;
            }
        }
        free(dest_file);
    }
    return ret;
}

/**
 * @return the number of files that failed to be replicated or -1 in case of
 *         some internal failure
 */
int backup_main(int argc, const char *const *const argv)
{
    size_t offset = 1;
    bool do_dump = false;
    if (argc > 1 && argv[1] != NULL && argv[1][0] == '-')
    {
        if (matches_option(argv[1], "--dump", "-d"))
        {
            offset++;
            do_dump = true;
        }
        else
        {
            print_usage();
            printf("Unrecognized option: '%s'\n", argv[1]);
            return 1;
        }
    }

    Seq *files = argv_to_lmdb_files(argc, argv, offset);
    if (files == NULL || SeqLength(files) == 0)
    {
        Log(LOG_LEVEL_ERR, "No database files to back up");
        return 1;
    }
    int ret;
    if (do_dump)
    {
        ret = backup_files_replicate(files);
    }
    else
    {
        ret = backup_files_copy(files);
    }
    SeqDestroy(files);
    return ret;
}

#endif
