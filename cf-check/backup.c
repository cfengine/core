#include <platform.h>
#include <stdio.h>
#include <utilities.h>
#include <backup.h>
#include <logging.h>
#include <file_lib.h>
#include <known_dirs.h>

#if defined(__MINGW32__) || !defined(LMDB)

int backup_main(int argc, char **argv)
{
    Log(LOG_LEVEL_ERR,
        "cf-check backup not available on this platform/build");
    return 1;
}

int backup_files(Seq *filenames)
{
    Log(LOG_LEVEL_INFO,
        "database backup not available on this platform/build");
    return 0;
}

#else

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

int backup_files(Seq *filenames)
{
    assert(filenames != NULL);
    const size_t length = SeqLength(filenames);

    // Attempting to back up 0 files is considered a failure:
    assert_or_return(length > 0, 1);

    const char *backup_dir = create_backup_dir();

    Log(LOG_LEVEL_INFO, "Backing up to '%s'", backup_dir);

    for (int i = 0; i < length; ++i)
    {
        const char *file = SeqAt(filenames, i);
        if (!File_CopyToDir(file, backup_dir))
        {
            Log(LOG_LEVEL_ERR, "Copying '%s' failed", file);
            return 1;
        }
    }
    return 0;
}

int backup_main(int argc, char **argv)
{
    Seq *files = argv_to_lmdb_files(argc, argv);
    const int ret = backup_files(files);
    SeqDestroy(files);
    return ret;
}

#endif
