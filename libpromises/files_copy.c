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


bool CopyRegularFileDisk(const char *source, const char *destination)
{
    bool ok1 = false, ok2 = false;       /* initialize before the goto end; */

    int sd = safe_open(source, O_RDONLY | O_BINARY);
    if (sd == -1)
    {
        Log(LOG_LEVEL_INFO, "Can't copy '%s' (open: %s)",
            source, GetErrorStr());
        goto end;
    }

    /* We need to stat the file to get the right source permissions. */
    struct stat statbuf;
    if (stat(source, &statbuf) == -1)
    {
        Log(LOG_LEVEL_INFO, "Can't copy '%s' (stat: %s)",
            source, GetErrorStr());
        goto end;
    }

    /* unlink() + safe_open(O_CREAT|O_EXCL) to avoid
       symlink attacks and races. */
    unlink(destination);

    int dd = safe_open(destination,
                       O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY,
                       statbuf.st_mode);
    if (dd == -1)
    {
        Log(LOG_LEVEL_INFO,
            "Unable to open destination file while copying '%s' to '%s'"
            " (open: %s)", source, destination, GetErrorStr());
        goto end;
    }

    size_t total_bytes_written;
    bool   last_write_was_hole;
    ok1 = FileSparseCopy(sd, source, dd, destination,
                         ST_BLKSIZE(statbuf),
                         &total_bytes_written, &last_write_was_hole);
    bool do_sync = false;
    ok2= FileSparseClose(dd, destination, do_sync,
                         total_bytes_written, last_write_was_hole);

    if (!ok1 || !ok2)
    {
        unlink(destination);
    }

  end:
    if (sd != -1)
    {
        close(sd);
    }
    return ok1 && ok2;
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
