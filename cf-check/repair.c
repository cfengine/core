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

int repair_lmdb_default(ARG_UNUSED bool force)
{
    Log(LOG_LEVEL_INFO,
        "database repair not available on this platform/build");
    return 0;
}

#else

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <lmdump.h>
#include <lmdb.h>
#include <diagnose.h>
#include <backup.h>
#include <sequence.h>
#include <utilities.h>
#include <diagnose.h>
#include <string_lib.h>
#include <file_lib.h>
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

static bool record_repair_timestamp(int fd_tstamp)
{
    time_t this_timestamp = time(NULL);
    lseek(fd_tstamp, 0, SEEK_SET);
    ssize_t n_written = write(fd_tstamp, &this_timestamp, sizeof(time_t));
    if (n_written != sizeof(time_t))
    {
        /* should never happen */
        return false;
    }
    return true;
}


/**
 * @param file      LMDB file to repair
 * @param fd_tstamp An open FD to the repair timestamp file or -1
 *
 * @note If #fd_tstamp != -1 then it is expected to be open and with file locks
 *       taken care of. If #fd_tstamp == -1, this function opens the repair
 *       timestamp file on its own and takes care of the file locks.
 */
int repair_lmdb_file(const char *file, int fd_tstamp)
{
    int ret;
    char *dest_file = StringFormat("%s"REPAIR_FILE_EXTENSION, file);

    FileLock lock = EMPTY_FILE_LOCK;
    if (fd_tstamp == -1)
    {
        char *tstamp_file = StringFormat("%s.repaired", file);
        int lock_ret = ExclusiveFileLockPath(&lock, tstamp_file, true); /* wait=true */
        free(tstamp_file);
        if (lock_ret < 0)
        {
            /* Should never happen because we tried to wait for the lock. */
            Log(LOG_LEVEL_ERR,
                "Failed to acquire lock for the '%s' DB repair timestamp file",
                file);
            ret = -1;
            goto cleanup;
        }
        fd_tstamp = lock.fd;
    }
    pid_t child_pid = fork();
    if (child_pid == 0)
    {
        /* child */
        /* The process can receive a SIGBUS signal while trying to read a
         * corrupted LMDB file. This has a special handling in cf-agent and
         * other processes, but this child process should just die in case of
         * SIGBUS (which is then detected by the parent and handled
         * accordingly).  */
        signal(SIGBUS, SIG_DFL);
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
            ret = -1;
            goto cleanup;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) != CF_CHECK_OK
            && WEXITSTATUS(status) != CF_CHECK_LMDB_CORRUPT_PAGE)
        {
            Log(LOG_LEVEL_ERR, "Failed to repair file '%s', removing", file);
            if (unlink(file) != 0)
            {
                Log(LOG_LEVEL_ERR, "Failed to remove file '%s'", file);
                ret = -1;
            }
            else
            {
                if (!record_repair_timestamp(fd_tstamp))
                {
                    Log(LOG_LEVEL_ERR, "Failed to write the timestamp of repair of the '%s' file",
                        file);
                }
                ret = WEXITSTATUS(status);
            }
            goto cleanup;
        }
        else if (WIFSIGNALED(status))
        {
            Log(LOG_LEVEL_ERR, "Failed to repair file '%s', child process signaled (%d), removing",
                file, WTERMSIG(status));
            if (unlink(file) != 0)
            {
                Log(LOG_LEVEL_ERR, "Failed to remove file '%s'", file);
                ret = -1;
            }
            else
            {
                if (!record_repair_timestamp(fd_tstamp))
                {
                    Log(LOG_LEVEL_ERR, "Failed to write the timestamp of repair of the '%s' file",
                        file);
                }
                ret = signal_to_cf_check_code(WTERMSIG(status));
            }
            goto cleanup;
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
                ret = -1;
                goto cleanup;
            }
            if (!record_repair_timestamp(fd_tstamp))
            {
                Log(LOG_LEVEL_ERR, "Failed to write the timestamp of repair of the '%s' file",
                    file);
            }
            ret = 0;
        }
    }
  cleanup:
    free(dest_file);
    if (lock.fd != -1)
    {
        ExclusiveFileUnlock(&lock, true); /* close=true */
    }
    return ret;
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
        const int corruptions = diagnose_files(files, &corrupt, false, false, false);
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
        const char *file = SeqAt(corrupt, i);
        if (repair_lmdb_file(file, -1) == -1)
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
        if (StringMatchesOption(argv[1], "--force", "-f"))
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

int repair_lmdb_default(bool force)
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
    const int ret = repair_lmdb_files(files, force);
    SeqDestroy(files);

    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Something went wrong during database repair");
        Log(LOG_LEVEL_ERR, "Try running `cf-check repair` manually");
    }
    return ret;
}

#endif
