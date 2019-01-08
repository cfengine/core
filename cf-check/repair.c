#include <platform.h>
#include <repair.h>

#if defined(__MINGW32__)

int repair_main(int argc, char **argv)
{
    printf("repair not supported on Windows\n");
    return 1;
}

#elif !defined(LMDB)

int repair_main(int argc, char **argv)
{
    printf("repair only implemented for LMDB.\n");
    return 1;
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
        printf("Removing: '%s'\n", filename);

        if (unlink(filename) != 0)
        {
            printf(
                "Failed to remove '%s' (%d - %s).\n",
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
        printf("Failed to remove %d files.\n", failures);
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
        printf(
            "%d corrupt database%s to fix.\n",
            corruptions,
            corruptions != 1 ? "s" : "");

        if (backup_files(files) != 0)
        {
            printf("Backup failed, stopping.\n");
            SeqDestroy(corrupt);
            return 1;
        }

        int ret = remove_files(corrupt);

        SeqDestroy(corrupt);
        if (ret == 0)
        {
            printf("Database repair successful.\n");
        }
        else
        {
            printf("Database repair failed.\n");
        }

        return ret;
    }

    assert(corrupt == NULL);
    printf("No corruption - nothing to do.\n");
    return 0;
}

int repair_main(int argc, char **argv)
{
    Seq *files = argv_to_lmdb_files(argc, argv);
    const int ret = repair_files(files);
    SeqDestroy(files);
    return ret;
}

#endif
