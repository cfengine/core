#include <platform.h>
#include <utilities.h>
#include <dir.h>
#include <sequence.h>
#include <string_lib.h>
#include <alloc.h>
#include <logging.h>
#include <file_lib.h>

Seq *default_lmdb_files()
{
    const char *state = "/var/cfengine/state/";
    Log(LOG_LEVEL_INFO,
        "No filenames specified, defaulting to .lmdb files in %s",
        state);
    Seq *files = ListDir(state, ".lmdb");
    if (files == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not open %s", state);
    }
    return files;
}

Seq *argv_to_lmdb_files(int argc, char **argv)
{
    if (argc <= 1)
    {
        return default_lmdb_files();
    }

    return SeqFromArgv(argc - 1, argv + 1);
}
