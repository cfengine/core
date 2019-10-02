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
    if (offset >= argc)
    {
        Log(LOG_LEVEL_INFO,
            "No filenames specified, defaulting to .lmdb files in %s",
            GetStateDir());
        return default_lmdb_files();
    }

    return SeqFromArgv(argc - offset, argv + offset);
}

bool matches_option(
    const char *const supplied,
    const char *const longopt,
    const char *const shortopt)
{
    assert(supplied != NULL);
    assert(shortopt != NULL);
    assert(longopt != NULL);
    assert(strlen(shortopt) == 2);
    assert(strlen(longopt) >= 3);
    assert(shortopt[0] == '-' && shortopt[1] != '-');
    assert(longopt[0] == '-' && longopt[1] == '-' && longopt[2] != '-');

    const size_t length = strlen(supplied);
    if (length <= 1)
    {
        return false;
    }
    else if (length == 2)
    {
        return StringSafeEqual(supplied, shortopt);
    }
    return StringSafeEqualN_IgnoreCase(supplied, longopt, length);
}
