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
                orig,
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

bool OverrideImmutableCommit(
    const char *orig, const char *copy, bool override, bool abort)
{
    if (!override)
    {
        return true;
    }

    if (abort)
    {
        return unlink(copy) == 0;
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

    return OverrideImmutableRename(copy, orig, override);
}

static void TemporarilyClearImmutableBit(
    const char *filename,
    bool override,
    FSAttrsResult *res,
    bool *is_immutable)
{
    if (!override)
    {
        return;
    }

    *res = FSAttrsGetImmutableFlag(filename, is_immutable);
    if (*res == FS_ATTRS_SUCCESS)
    {
        if (*is_immutable)
        {
            *res = FSAttrsUpdateImmutableFlag(filename, false);
            if (*res == FS_ATTRS_SUCCESS)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Temporarily cleared immutable bit for file '%s'",
                    filename);
            }
            else
            {
                Log((*res == FS_ATTRS_FAILURE) ? LOG_LEVEL_ERR
                                               : LOG_LEVEL_VERBOSE,
                    "Failed to temporarily clear immutable bit for file '%s': %s",
                    filename,
                    FSAttrsErrorCodeToString(*res));
            }
        }
        else
        {
            Log(LOG_LEVEL_DEBUG,
                "The immutable bit is not set on file '%s'",
                filename);
        }
    }
    else
    {
        Log((*res == FS_ATTRS_FAILURE) ? LOG_LEVEL_ERR : LOG_LEVEL_VERBOSE,
            "Failed to get immutable bit from file '%s': %s",
            filename,
            FSAttrsErrorCodeToString(*res));
    }
}

static void ResetTemporarilyClearedImmutableBit(
    const char *filename, bool override, FSAttrsResult res, bool is_immutable)
{
    if (!override)
    {
        return;
    }

    if ((res == FS_ATTRS_SUCCESS) && is_immutable)
    {
        res = FSAttrsUpdateImmutableFlag(filename, true);
        if (res == FS_ATTRS_SUCCESS)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Reset immutable bit after temporarily clearing it from file '%s'",
                filename);
        }
        else
        {
            Log((res == FS_ATTRS_FAILURE) ? LOG_LEVEL_ERR : LOG_LEVEL_VERBOSE,
                "Failed to reset immutable bit after temporarily clearing it from file '%s': %s",
                filename,
                FSAttrsErrorCodeToString(res));
        }
    }
}

bool OverrideImmutableRename(
    const char *old_filename, const char *new_filename, bool override)
{
    assert(old_filename != NULL);
    assert(new_filename != NULL);

    FSAttrsResult res;
    bool is_immutable;

    TemporarilyClearImmutableBit(new_filename, override, &res, &is_immutable);

    if (rename(old_filename, new_filename) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to replace original file '%s' with copy '%s'",
            new_filename,
            old_filename);
        unlink(old_filename);
        return false;
    }

    ResetTemporarilyClearedImmutableBit(
        new_filename, override, res, is_immutable);

    return true;
}

bool OverrideImmutableDelete(const char *filename, bool override)
{
    assert(filename != NULL);

    FSAttrsResult res;
    bool is_immutable = false;

    TemporarilyClearImmutableBit(filename, override, &res, &is_immutable);

    return unlink(filename) == 0;
}

bool OverrideImmutableUtime(
    const char *filename, bool override, const struct utimbuf *times)
{
    assert(filename != NULL);

    FSAttrsResult res;
    bool is_immutable;

    TemporarilyClearImmutableBit(filename, override, &res, &is_immutable);

    int ret = utime(filename, times);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to update access and modification times of file '%s'",
            filename);
    }

    ResetTemporarilyClearedImmutableBit(filename, override, res, is_immutable);

    return ret == 0;
}
