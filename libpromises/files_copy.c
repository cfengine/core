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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "files_copy.h"

#include "files_names.h"
#include "files_interfaces.h"
#include "instrumentation.h"
#include "policy.h"
#include "files_lib.h"
#include "string_lib.h"

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
    int sd, dd;

    if ((sd = open(source, O_RDONLY | O_BINARY)) == -1)
    {
        Log(LOG_LEVEL_INFO, "Can't copy '%s'. (open: %s)", source, GetErrorStr());
        unlink(destination);
        return false;
    }
    /*
     * We need to stat the file in order to get the right source permissions.
     */
    struct stat statbuf;

    if (stat(source, &statbuf) == -1)
    {
        Log(LOG_LEVEL_INFO, "Can't copy '%s'. (stat: %s)", source, GetErrorStr());
        unlink(destination);
        return false;
    }

    unlink(destination);                /* To avoid link attacks */

    if ((dd = open(destination, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY, statbuf.st_mode)) == -1)
    {
        Log(LOG_LEVEL_INFO, "Unable to open destination file while copying '%s' to '%s'. (open: %s)", source, destination, GetErrorStr());
        close(sd);
        unlink(destination);
        return false;
    }

    int buf_size = ST_BLKSIZE(dstat);
    char *buf = xmalloc(buf_size);

    bool result = CopyData(source, sd, destination, dd, buf, buf_size);

    if (!result)
    {
        unlink(destination);
    }

    close(sd);
    close(dd);
    free(buf);
    return result;
}
