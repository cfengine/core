/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <platform.h>

#include <files_copy.h>

#include <files_names.h>
#include <files_interfaces.h>
#include <instrumentation.h>
#include <policy.h>
#include <files_lib.h>
#include <file_lib.h>
#include <string_lib.h>
#include <acl_tools.h>

/*
 * Copy data jumping over areas filled by '\0' greater than buf_size, so
 * files automatically become sparse if possible.
 */
static bool CopyData(const char *source, int sd,
                     const char *destination, int dd,
                     char *buf, size_t buf_size)
{
    assert(buf_size > 0);

    off_t n_read_total = 0;
    bool all_zeroes    = false;

    while (true)
    {
        ssize_t n_read = FullRead(sd, buf, buf_size);
        if (n_read < 0)
        {
            Log(LOG_LEVEL_ERR,
                "Unable to read source file while copying '%s' to '%s'"
                " (read: %s)", source, destination, GetErrorStr());
            return false;
        }
        else if (n_read == 0)                                   /* EOF */
        {
            /*
             * If the tail of the file was a hole (and hence lseek(2)ed on
             * destination instead of being written), do a ftruncate(2) here
             * to ensure the whole file is written to the disc. But
             * ftruncate() fails with EPERM on non-native Linux filesystems
             * (e.g. vfat, vboxfs) when the count is greater than the size of
             * the file. So we write() one byte and then ftruncate() it back.
             */
            if (all_zeroes)
            {
                if (FullWrite(dd, "", 1)        < 0 ||
                    ftruncate(dd, n_read_total) < 0)
                {
                    Log(LOG_LEVEL_ERR,
                        "Copy failed (no space?) while copying '%s' to '%s'"
                        " (ftruncate: %s)",
                        source, destination, GetErrorStr());
                    return false;
                }
            }
            return true;
        }

        n_read_total += n_read;
        all_zeroes    = (memcchr(buf, '\0', n_read) == NULL);

        if (all_zeroes)                                     /* Write a hole */
        {
            off_t seek_ret = lseek(dd, n_read, SEEK_CUR);
            if (seek_ret == (off_t) -1)
            {
                Log(LOG_LEVEL_ERR,
                    "Failed while copying '%s' to '%s'"
                    " (no space?) (lseek: %s)",
                    source, destination, GetErrorStr());
                return false;
            }
        }
        else
        {
            ssize_t w_ret = FullWrite(dd, buf, n_read);
            if (w_ret < 0)
            {
                Log(LOG_LEVEL_ERR,
                    "Failed while copying '%s' to '%s'"
                    " (no space?) (write: %s)",
                    source, destination, GetErrorStr());
                return false;
            }
        }
    }
}

bool CopyRegularFileDisk(const char *source, const char *destination)
{
    int sd;
    int dd = -1;
    char *buf = NULL;
    bool result = false;

    sd = safe_open(source, O_RDONLY | O_BINARY);
    if (sd == -1)
    {
        Log(LOG_LEVEL_INFO, "Can't copy '%s' (open: %s)",
            source, GetErrorStr());
        goto end;
    }
    /*
     * We need to stat the file in order to get the right source permissions.
     */
    struct stat statbuf;

    if (stat(source, &statbuf) == -1)
    {
        Log(LOG_LEVEL_INFO, "Can't copy '%s' (stat: %s)",
            source, GetErrorStr());
        goto end;
    }

    /* Unlink to avoid link attacks, TODO race condition. */
    unlink(destination);

    dd = safe_open(destination,
                   O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY,
                   statbuf.st_mode);
    if (dd == -1)
    {
        Log(LOG_LEVEL_INFO,
            "Unable to open destination file while copying '%s' to '%s'"
            " (open: %s)", source, destination, GetErrorStr());
        goto end;
    }

    int buf_size = ST_BLKSIZE(statbuf);
    buf = xmalloc(buf_size);

    result = CopyData(source, sd, destination, dd, buf, buf_size);
    if (!result)
    {
        unlink(destination);
    }

    free(buf);

end:
    if (dd != -1)
    {
        close(dd);
    }
    if (sd != -1)
    {
        close(sd);
    }
    return result;
}

bool CopyFilePermissionsDisk(const char *source, const char *destination)
{
    struct stat statbuf;

    if (stat(source, &statbuf) == -1)
    {
        Log(LOG_LEVEL_INFO, "Can't copy permissions '%s'. (stat: %s)", source, GetErrorStr());
        return false;
    }

    if (safe_chmod(destination, statbuf.st_mode) != 0)
    {
        Log(LOG_LEVEL_INFO, "Can't copy permissions '%s'. (chmod: %s)", source, GetErrorStr());
        return false;
    }

    if (safe_chown(destination, statbuf.st_uid, statbuf.st_gid) != 0)
    {
        Log(LOG_LEVEL_INFO, "Can't copy permissions '%s'. (chown: %s)", source, GetErrorStr());
        return false;
    }

    if (!CopyFileExtendedAttributesDisk(source, destination))
    {
        return false;
    }

    return true;
}

bool CopyFileExtendedAttributesDisk(const char *source, const char *destination)
{
#if defined(WITH_XATTR)
    // Extended attributes include both POSIX ACLs and SELinux contexts.
    ssize_t attr_raw_names_size;
    char attr_raw_names[CF_BUFSIZE];

    attr_raw_names_size = llistxattr(source, attr_raw_names, sizeof(attr_raw_names));
    if (attr_raw_names_size < 0)
    {
        if (errno == ENOTSUP || errno == ENODATA)
        {
            return true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Can't copy extended attributes from '%s' to '%s'. (llistxattr: %s)",
                source, destination, GetErrorStr());
            return false;
        }
    }

    int pos;
    for (pos = 0; pos < attr_raw_names_size;)
    {
        const char *current = attr_raw_names + pos;
        pos += strlen(current) + 1;

        char data[CF_BUFSIZE];
        int datasize = lgetxattr(source, current, data, sizeof(data));
        if (datasize < 0)
        {
            if (errno == ENOTSUP)
            {
                continue;
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Can't copy extended attributes from '%s' to '%s'. (lgetxattr: %s: %s)",
                    source, destination, GetErrorStr(), current);
                return false;
            }
        }

        int ret = lsetxattr(destination, current, data, datasize, 0);
        if (ret < 0)
        {
            if (errno == ENOTSUP)
            {
                continue;
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Can't copy extended attributes from '%s' to '%s'. (lsetxattr: %s: %s)",
                    source, destination, GetErrorStr(), current);
                return false;
            }
        }
    }

#else // !WITH_XATTR
    // ACLs are included in extended attributes, but fall back to CopyACLs if xattr is not available.
    if (!CopyACLs(source, destination))
    {
        return false;
    }
#endif

    return true;
}
