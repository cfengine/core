#include <override_fsattrs.h>
#include <platform.h>
#include <fsattrs.h>
#include <logging.h>
#include <stdlib.h>
#include <files_copy.h>
#include <cf3.defs.h>
#include <string_lib.h>

bool OverrideImmutableBegin(
    const char *orig, char *copy, size_t copy_len, bool override)
{
    if (!override)
    {
        size_t ret = strlcpy(copy, orig, copy_len);
        if (ret >= copy_len)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to copy filename '%s': Filename too long (%zu >= %zu)",
                ret,
                copy_len);
            return false;
        }
        return true;
    }

    srand(time(NULL)); /* Seed random number generator */
    int rand_number = rand() % 999999;
    assert(rand_number >= 0);

    /* Inspired by mkstemp(3) */
    int ret = snprintf(copy, copy_len, "%s.%06d" CF_NEW, orig, rand_number);
    if (ret < 0 || (size_t) ret >= copy_len)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to generate name for temporary copy of '%s': Filename is too long (%d >= %zu)",
            orig,
            ret,
            copy_len);
        return false;
    }

    /* We'll match the original file permissions on commit */
    if (!CopyRegularFileDiskPerms(orig, copy, 0600))
    {
        Log(LOG_LEVEL_ERR,
            "Failed to copy file '%s' to temporary file '%s'",
            orig,
            copy);
        return false;
    }

    return true;
}

bool OverrideImmutableCommit(const char *orig, const char *copy, bool override)
{
    if (!override)
    {
        return true;
    }

    struct stat sb;
    if (lstat(orig, &sb) == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to stat file '%s'", orig);
        unlink(copy);
        return false;
    }

    if (chmod(copy, sb.st_mode) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to change mode bits on file '%s' to %04jo: %s",
            orig,
            (uintmax_t) sb.st_mode,
            GetErrorStr());
        unlink(copy);
        return false;
    }

    /* If the operations on the file system attributes fails for any reason,
     * we can still proceed to try to replace the original file. We will only
     * log an actual error in case of an unexpected failure (i.e., when
     * FS_ATTRS_FAILURE is returned). Other failures will be logged as verbose
     * messages because they can be useful, but are be quite verbose. */

    bool is_immutable;
    FSAttrsResult res = FSAttrsGetImmutableFlag(orig, &is_immutable);
    if (res == FS_ATTRS_SUCCESS)
    {
        if (is_immutable)
        {
            res = FSAttrsUpdateImmutableFlag(orig, false);
            if (res == FS_ATTRS_SUCCESS)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Temporarily cleared immutable bit for file '%s'",
                    orig);
            }
            else
            {
                Log((res == FS_ATTRS_FAILURE) ? LOG_LEVEL_ERR
                                              : LOG_LEVEL_VERBOSE,
                    "Failed to temporarily clear immutable bit for file '%s': %s",
                    orig,
                    FSAttrsErrorCodeToString(res));
            }
        }
        else
        {
            Log(LOG_LEVEL_DEBUG,
                "The immutable bit is not set on file '%s'",
                orig);
        }
    }
    else
    {
        Log((res == FS_ATTRS_FAILURE) ? LOG_LEVEL_ERR : LOG_LEVEL_VERBOSE,
            "Failed to get immutable bit from file '%s': %s",
            orig,
            FSAttrsErrorCodeToString(res));
    }

    if (rename(copy, orig) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to replace original file '%s' with copy '%s'",
            orig,
            copy);
        unlink(copy);
        return false;
    }

    if ((res == FS_ATTRS_SUCCESS) && is_immutable)
    {
        res = FSAttrsUpdateImmutableFlag(orig, true);
        if (res == FS_ATTRS_SUCCESS)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Reset immutable bit after temporarily clearing it from file '%s'",
                orig);
        }
        else
        {
            Log((res == FS_ATTRS_FAILURE) ? LOG_LEVEL_ERR : LOG_LEVEL_VERBOSE,
                "Failed to reset immutable bit after temporarily clearing it from file '%s': %s",
                orig,
                FSAttrsErrorCodeToString(res));
        }
    }

    return true;
}

bool OverrideImmutableAbort(
    ARG_UNUSED const char *orig, const char *copy, bool override)
{
    assert(copy != NULL);
    if (!override)
    {
        return true;
    }
    return unlink(copy) == 0;
}
