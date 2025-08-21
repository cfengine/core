#include <override_fsattrs.h>
#include <platform.h>
#include <logging.h>
#include <stdlib.h>
#include <files_copy.h>
#include <cf3.defs.h>
#include <string_lib.h>
#include <file_lib.h>
#include <fsattrs.h>

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
        Log(LOG_LEVEL_ERR,
            "Failed to stat file '%s' during immutable operations",
            orig);
        unlink(copy);
        return false;
    }

    if (chmod(copy, sb.st_mode) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to change mode bits on file '%s' to %04jo during immutable operations: %s",
            orig,
            (uintmax_t) sb.st_mode,
            GetErrorStr());
        unlink(copy);
        return false;
    }

    return OverrideImmutableRename(copy, orig, override);
}

FSAttrsResult TemporarilyClearImmutableBit(
    const char *filename, bool override, bool *was_immutable)
{
    if (!override)
    {
        return FS_ATTRS_FAILURE;
    }

    FSAttrsResult res = FSAttrsGetImmutableFlag(filename, was_immutable);
    if (res == FS_ATTRS_SUCCESS)
    {
        if (*was_immutable)
        {
            res = FSAttrsUpdateImmutableFlag(filename, false);
            if (res == FS_ATTRS_SUCCESS)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Temporarily cleared immutable bit for file '%s'",
                    filename);
            }
            else
            {
                if (res == FS_ATTRS_FAILURE)
                {
                    Log(LOG_LEVEL_ERR,
                        "Failed to temporarily clear immutable bit for file '%s': %s",
                        filename,
                        FSAttrsErrorCodeToString(res));
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE,
                        "Could not temporarily clear immutable bit for file '%s': %s",
                        filename,
                        FSAttrsErrorCodeToString(res));
                }
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
        Log((res == FS_ATTRS_FAILURE) ? LOG_LEVEL_ERR : LOG_LEVEL_VERBOSE,
            "Failed to get immutable bit from file '%s': %s",
            filename,
            FSAttrsErrorCodeToString(res));
    }

    return res;
}

void ResetTemporarilyClearedImmutableBit(
    const char *filename, bool override, FSAttrsResult res, bool was_immutable)
{
    if (!override)
    {
        return;
    }

    if ((res == FS_ATTRS_SUCCESS) && was_immutable)
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

bool OverrideImmutableChmod(const char *filename, mode_t mode, bool override)
{
    assert(filename != NULL);

    bool is_immutable;
    FSAttrsResult res =
        TemporarilyClearImmutableBit(filename, override, &is_immutable);

    int ret = safe_chmod(filename, mode);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to change mode on file '%s': %s",
            filename,
            GetErrorStr());
    }

    ResetTemporarilyClearedImmutableBit(filename, override, res, is_immutable);

    return ret == 0;
}

bool OverrideImmutableRename(
    const char *old_filename, const char *new_filename, bool override)
{
    assert(old_filename != NULL);
    assert(new_filename != NULL);

    bool new_is_immutable;
    TemporarilyClearImmutableBit(new_filename, override, &new_is_immutable);

    bool old_is_immutable;
    FSAttrsResult res_old = TemporarilyClearImmutableBit(
        old_filename, override, &old_is_immutable);

    if (rename(old_filename, new_filename) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to replace original file '%s' with copy '%s': %s",
            new_filename,
            old_filename,
            GetErrorStr());
        unlink(old_filename);
        return false;
    }

    ResetTemporarilyClearedImmutableBit(
        new_filename, override, res_old, new_is_immutable);

    return true;
}

bool OverrideImmutableDelete(const char *filename, bool override)
{
    assert(filename != NULL);

    bool is_immutable = false;
    TemporarilyClearImmutableBit(filename, override, &is_immutable);

    return unlink(filename) == 0;
}

bool OverrideImmutableUtime(
    const char *filename, bool override, const struct utimbuf *times)
{
    assert(filename != NULL);

    bool is_immutable;
    FSAttrsResult res =
        TemporarilyClearImmutableBit(filename, override, &is_immutable);

    int ret = utime(filename, times);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to update access and modification times of file '%s': %s",
            filename,
            GetErrorStr());
    }

    ResetTemporarilyClearedImmutableBit(filename, override, res, is_immutable);

    return ret == 0;
}
