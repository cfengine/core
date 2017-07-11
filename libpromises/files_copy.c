/*
   Copyright 2017 Northern.tech AS

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
 * Copy data jumping over areas filled by '\0', so files automatically become sparse if possible.
 */
static bool CopyData(const char *source, int sd, const char *destination, int dd, char *buf, size_t buf_size)
{
    off_t n_read_total = 0;

    while (true)
    {
        ssize_t n_read = read(sd, buf, buf_size);

        if (n_read == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            Log(LOG_LEVEL_ERR, "Unable to read source file while copying '%s' to '%s'. (read: %s)", source, destination, GetErrorStr());
            return false;
        }

        if (n_read == 0)
        {
            /*
             * As the tail of file may contain of bytes '\0' (and hence
             * lseek(2)ed on destination instead of being written), do a
             * ftruncate(2) here to ensure the whole file is written to the
             * disc.
             */
            if (ftruncate(dd, n_read_total) < 0)
            {
                Log(LOG_LEVEL_ERR, "Copy failed (no space?) while copying '%s' to '%s'. (ftruncate: %s)", source, destination, GetErrorStr());
                return false;
            }

            return true;
        }

        n_read_total += n_read;

        /* Copy/seek */

        void *cur = buf;
        void *end = buf + n_read;

        while (cur < end)
        {
            void *skip_span = MemSpan(cur, 0, end - cur);
            if (skip_span > cur)
            {
                if (lseek(dd, skip_span - cur, SEEK_CUR) < 0)
                {
                    Log(LOG_LEVEL_ERR, "Failed while copying '%s' to '%s' (no space?). (lseek: %s)", source, destination, GetErrorStr());
                    return false;
                }

                cur = skip_span;
            }


            void *copy_span = MemSpanInverse(cur, 0, end - cur);
            if (copy_span > cur)
            {
                if (FullWrite(dd, cur, copy_span - cur) < 0)
                {
                    Log(LOG_LEVEL_ERR, "Failed while copying '%s' to '%s' (no space?). (write: %s)", source, destination, GetErrorStr());
                    return false;
                }

                cur = copy_span;
            }
        }
    }
}

bool CopyRegularFileDisk(const char *source, const char *destination)
{
    int sd;
    int dd = 0;
    char *buf = 0;
    bool result = false;

    if ((sd = safe_open(source, O_RDONLY | O_BINARY)) == -1)
    {
        Log(LOG_LEVEL_INFO, "Can't copy '%s'. (open: %s)", source, GetErrorStr());
        goto end;
    }
    /*
     * We need to stat the file in order to get the right source permissions.
     */
    struct stat statbuf;

    if (stat(source, &statbuf) == -1)
    {
        Log(LOG_LEVEL_INFO, "Can't copy '%s'. (stat: %s)", source, GetErrorStr());
        goto end;
    }

    unlink(destination);                /* To avoid link attacks */

    if ((dd = safe_open(destination, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY, statbuf.st_mode)) == -1)
    {
        Log(LOG_LEVEL_INFO, "Unable to open destination file while copying '%s' to '%s'. (open: %s)", source, destination, GetErrorStr());
        goto end;
    }

    int buf_size = ST_BLKSIZE(dstat);
    buf = xmalloc(buf_size);

    result = CopyData(source, sd, destination, dd, buf, buf_size);
    if (!result)
    {
        goto end;
    }

end:
    if (buf)
    {
        free(buf);
    }
    if (dd)
    {
        close(dd);
    }
    if (!result)
    {
        unlink(destination);
    }
    close(sd);
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
