#include <platform.h>
#include <utilities.h>
#include <dir.h>
#include <sequence.h>
#include <string_lib.h>
#include <alloc.h>
#include <logging.h>
#include <file_lib.h>
#include <known_dirs.h>

Seq *default_lmdb_files()
{
    const char *state = GetStateDir();
    Seq *files = ListDir(state, ".lmdb");
    if (files == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not open %s", state);
    }
    return files;
}

Seq *argv_to_lmdb_files(
    const int argc, const char *const *const argv, const size_t offset)
{
    assert(argc >= 0);
    if (offset >= (size_t) argc)
    {
        Log(LOG_LEVEL_INFO,
            "No filenames specified, defaulting to .lmdb files in %s",
            GetStateDir());
        return default_lmdb_files();
    }

    return SeqFromArgv(argc - offset, argv + offset);
}
