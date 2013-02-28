/* 

   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "files_copy.h"

#include "files_names.h"
#include "files_interfaces.h"
#include "instrumentation.h"
#include "cfstream.h"
#include "policy.h"

/*****************************************************************************/
/* Local low level                                                           */
/*****************************************************************************/

void CheckForFileHoles(struct stat *sstat, Promise *pp)
/* Need a transparent way of getting this into CopyReg() */
/* Use a public member in struct Image                   */
{
    if (pp == NULL)
    {
        return;
    }

#if !defined(__MINGW32__)
    if (sstat->st_size > sstat->st_blocks * DEV_BSIZE)
#else
# ifdef HAVE_ST_BLOCKS
    if (sstat->st_size > sstat->st_blocks * DEV_BSIZE)
# else
    if (sstat->st_size > ST_NBLOCKS((*sstat)) * DEV_BSIZE)
# endif
#endif
    {
        pp->makeholes = 1;      /* must have a hole to get checksum right */
    }

    pp->makeholes = 0;
}

/*********************************************************************/

bool CopyRegularFileDisk(const char *source, const char *destination, bool make_holes)
{
    int sd, dd, buf_size;
    char *buf, *cp;
    int n_read, *intp;
    long n_read_total = 0;
    int last_write_made_hole = 0;

    if ((sd = open(source, O_RDONLY | O_BINARY)) == -1)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "open", "Can't copy %s!\n", source);
        unlink(destination);
        return false;
    }
    /*
     * We need to stat the file in order to get the right source permissions.
     */
    struct stat statbuf;

    if (cfstat(source, &statbuf) == -1)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "stat", "Can't copy %s!\n", source);
        unlink(destination);
        return false;
    }

    unlink(destination);                /* To avoid link attacks */

    if ((dd = open(destination, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY, statbuf.st_mode)) == -1)
    {
        close(sd);
        unlink(destination);
        return false;
    }

    buf_size = ST_BLKSIZE(dstat);
    buf = xmalloc(buf_size + sizeof(int));

    while (true)
    {
        if ((n_read = read(sd, buf, buf_size)) == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            close(sd);
            close(dd);
            free(buf);
            return false;
        }

        if (n_read == 0)
        {
            break;
        }

        n_read_total += n_read;

        intp = 0;

        if (make_holes)
        {
            buf[n_read] = 1;    /* Sentinel to stop loop.  */

            /* Find first non-zero *word*, or the word with the sentinel.  */

            intp = (int *) buf;

            while (*intp++ == 0)
            {
            }

            /* Find the first non-zero *byte*, or the sentinel.  */

            cp = (char *) (intp - 1);

            while (*cp++ == 0)
            {
            }

            /* If we found the sentinel, the whole input block was zero,
               and we can make a hole.  */

            if (cp > buf + n_read)
            {
                /* Make a hole.  */
                if (lseek(dd, (off_t) n_read, SEEK_CUR) < 0L)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "lseek", "Copy failed (no space?) while doing %s to %s\n", source, destination);
                    free(buf);
                    unlink(destination);
                    close(dd);
                    close(sd);
                    return false;
                }
                last_write_made_hole = 1;
            }
            else
            {
                /* Clear to indicate that a normal write is needed. */
                intp = 0;
            }
        }

        if (intp == 0)
        {
            if (FullWrite(dd, buf, n_read) < 0)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Copy failed (no space?) while doing %s to %s\n", source, destination);
                close(sd);
                close(dd);
                free(buf);
                unlink(destination);
                return false;
            }
            last_write_made_hole = 0;
        }
    }

    /* If the file ends with a `hole', something needs to be written at
       the end.  Otherwise the kernel would truncate the file at the end
       of the last write operation.  */

    if (last_write_made_hole)
    {
        /* Write a null character and truncate it again.  */

        if ((FullWrite(dd, "", 1) < 0) || (ftruncate(dd, n_read_total) < 0))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "write", "cfengine: full_write or ftruncate error in CopyReg\n");
            free(buf);
            unlink(destination);
            close(sd);
            close(dd);
            return false;
        }
    }

    close(sd);
    close(dd);

    free(buf);
    return true;
}
