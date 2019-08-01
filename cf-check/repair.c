#include <platform.h>
#include <repair.h>
#include <logging.h>

#if defined(__MINGW32__) || !defined(LMDB)

int repair_main(ARG_UNUSED int argc, ARG_UNUSED const char *const *const argv)
{
    Log(LOG_LEVEL_ERR,
        "cf-check repair not available on this platform/build");
    return 1;
}

int repair_default()
{
    Log(LOG_LEVEL_INFO,
        "database repair not available on this platform/build");
    return 0;
}

#else

#include <stdio.h>
#include <lmdump.h>
#include <lmdb.h>
#include <diagnose.h>
#include <backup.h>
#include <sequence.h>
#include <utilities.h>
#include <diagnose.h>
#include <string_lib.h>

int remove_files(Seq *files)
{
    assert(files != NULL);

    size_t corruptions = SeqLength(files);
    int failures = 0;

    for (size_t i = 0; i < corruptions; ++i)
    {
        const char *filename = SeqAt(files, i);
        assert(filename != NULL);
        Log(LOG_LEVEL_INFO, "Removing: '%s'", filename);

        if (unlink(filename) != 0)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to remove '%s' (%d - %s)",
                filename,
                errno,
                strerror(errno));
            ++failures;
            continue;
        }

        char *lock_file = StringConcatenate(2, filename, ".lock");
        unlink(lock_file);
        free(lock_file);

        lock_file = StringConcatenate(2, filename, "-lock");
        unlink(lock_file);
        free(lock_file);
    }
    if (failures != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to remove %d files", failures);
    }
    return failures;
}

int repair_files(Seq *files)
{
    assert(files != NULL);
    assert(SeqLength(files) > 0);

    Seq *corrupt = NULL;

    const int corruptions = diagnose_files(files, &corrupt);

    if (corruptions != 0)
    {
        assert(corrupt != NULL);
        Log(LOG_LEVEL_NOTICE,
            "%d corrupt database%s to fix",
            corruptions,
            corruptions != 1 ? "s" : "");

        if (backup_files(files) != 0)
        {
            Log(LOG_LEVEL_ERR, "Backup failed, stopping");
            SeqDestroy(corrupt);
            return 1;
        }

        int ret = remove_files(corrupt);

        SeqDestroy(corrupt);
        if (ret == 0)
        {
            Log(LOG_LEVEL_NOTICE, "Database repair successful");
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Database repair failed");
        }

        return ret;
    }

    assert(corrupt == NULL);
    Log(LOG_LEVEL_INFO, "No corruption - nothing to do");
    return 0;
}

int repair_main(int argc, const char *const *const argv)
{
    Seq *files = argv_to_lmdb_files(argc, argv);
    if (files == NULL || SeqLength(files) == 0)
    {
        Log(LOG_LEVEL_ERR, "No database files to repair");
        return 1;
    }
    const int ret = repair_files(files);
    SeqDestroy(files);
    return ret;
}

int repair_default()
{
    // This function is used by cf-execd and cf-agent, not cf-check

    // Consistency checks are not enabled by default (--skip-db-check=yes)
    // This log message can be changed to verbose if it happens by default:
    Log(LOG_LEVEL_INFO, "Running internal DB (LMDB) consistency checks");

    Seq *files = default_lmdb_files();
    if (files == NULL)
    {
        // Error message printed default_lmdb_files()
        return 1;
    }
    if (SeqLength(files) == 0)
    {
        // First agent run - no LMDB files
        Log(LOG_LEVEL_INFO, "Skipping local database repair, no lmdb files");
        return 0;
    }
    const int ret = repair_files(files);
    SeqDestroy(files);

    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Something went wrong during database repair");
        Log(LOG_LEVEL_ERR, "Try running `cf-check repair` manually");
    }
    return ret;
}

#endif
