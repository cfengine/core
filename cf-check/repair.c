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

int repair_lmdb_default()
{
    Log(LOG_LEVEL_INFO,
        "database repair not available on this platform/build");
    return 0;
}

#else

#include <stdio.h>
#include <errno.h>
#include <lmdump.h>
#include <lmdb.h>
#include <diagnose.h>
#include <backup.h>
#include <sequence.h>
#include <utilities.h>
#include <diagnose.h>
#include <string_lib.h>
#include <replicate_lmdb.h>

static void print_usage(void)
{
    printf("Usage: cf-check repair [-f] [FILE ...]\n");
    printf("Example: cf-check repair /var/cfengine/state/cf_lastseen.lmdb\n");
    printf("Options: -f|--force repair LMDB files that look OK ");
}

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

int repair_lmdb_file(const char *file)
{
    char *dest_file = StringFormat("%s"REPAIR_FILE_EXTENSION, file);
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
            Log(LOG_LEVEL_ERR, "Failed to repair file '%s', removing", file);
            unlink(file);
            free(dest_file);
            return WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status))
        {
            Log(LOG_LEVEL_ERR, "Failed to repair file '%s', child process signaled (%d), removing",
                file, WTERMSIG(status));
            unlink(file);
            free(dest_file);
            return signal_to_cf_check_code(WTERMSIG(status));
        }
        else
        {
            /* replication successfull */
            Log(LOG_LEVEL_INFO, "Replacing '%s' with the new copy", file);
            if (rename(dest_file, file) != 0)
            {
                Log(LOG_LEVEL_ERR,
                    "Failed to replace file '%s' with the repaired copy: %s",
                    file, strerror(errno));
                unlink(dest_file);
                free(dest_file);
                return -1;
            }
            free(dest_file);
            return 0;
        }
    }
}

int repair_lmdb_files(Seq *files, bool force)
{
    assert(files != NULL);
    assert(SeqLength(files) > 0);

    Seq *corrupt;
    if (force)
    {
        corrupt = files;
    }
    else
    {
        const int corruptions = diagnose_files(files, &corrupt, false);
        if (corruptions != 0)
        {
            assert(corrupt != NULL);
            Log(LOG_LEVEL_NOTICE,
                "%d corrupt database%s to fix",
                corruptions,
                corruptions != 1 ? "s" : "");
        }
        else
        {
            Log(LOG_LEVEL_INFO, "No corrupted LMDB files - nothing to do");
            return 0;
        }
    }

    int ret = 0;
    const size_t length = SeqLength(corrupt);
    assert(length > 0);
    backup_files_copy(corrupt);
    for (int i = 0; i < length; ++i)
    {
        const char *file = SeqAt(files, i);
        if (repair_lmdb_file(file) != 0)
        {
            ret++;
        }
    }

    if (!force)
    {
        /* see 'if (force)' above */
        SeqDestroy(corrupt);
    }

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

int repair_main(int argc, const char *const *const argv)
{
    size_t offset = 1;
    bool force = false;
    if (argc > 1 && argv[1] != NULL && argv[1][0] == '-')
    {
        if (matches_option(argv[1], "--force", "-f"))
        {
            offset++;
            force = true;
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
        Log(LOG_LEVEL_ERR, "No database files to repair");
        return 1;
    }
    const int ret = repair_lmdb_files(files, force);
    SeqDestroy(files);
    return ret;
}

int repair_lmdb_default()
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
    const int ret = repair_lmdb_files(files, false);
    SeqDestroy(files);

    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Something went wrong during database repair");
        Log(LOG_LEVEL_ERR, "Try running `cf-check repair` manually");
    }
    return ret;
}

#endif
