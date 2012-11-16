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

#include "cf3.defs.h"

#include "files_names.h"
#include "files_interfaces.h"
#include "files_operators.h"
#include "instrumentation.h"
#include "cfstream.h"

/*****************************************************************************/

void *CopyFileSources(char *destination, Attributes attr, Promise *pp, const ReportContext *report_context)
{
    char *source = attr.copy.source;
    char *server = pp->this_server;
    char vbuff[CF_BUFSIZE];
    struct stat ssb, dsb;
    struct timespec start;
    char eventname[CF_BUFSIZE];

    CfDebug("CopyFileSources(%s,%s)", source, destination);

    if ((pp->conn != NULL) && (!pp->conn->authenticated))
    {
        cfPS(cf_verbose, CF_FAIL, "", pp, attr, "No authenticated source %s in files.copyfrom promise\n", source);
        return NULL;
    }

    if (cf_stat(attr.copy.source, &ssb, attr, pp) == -1)
    {
        cfPS(cf_inform, CF_FAIL, "", pp, attr, "Can't stat %s in files.copyfrom promise\n", source);
        return NULL;
    }

    start = BeginMeasure();

    strncpy(vbuff, destination, CF_BUFSIZE - 4);

    if (S_ISDIR(ssb.st_mode))   /* could be depth_search */
    {
        AddSlash(vbuff);
        strcat(vbuff, ".");
    }

    if (!MakeParentDirectory(vbuff, attr.move_obstructions, report_context))
    {
        cfPS(cf_inform, CF_FAIL, "", pp, attr, "Can't make directories for %s in files.copyfrom promise\n", vbuff);
        return NULL;
    }

    if (S_ISDIR(ssb.st_mode))   /* could be depth_search */
    {
        if (attr.copy.purge)
        {
            CfOut(cf_verbose, "", " !! (Destination purging enabled)\n");
        }

        CfOut(cf_verbose, "", " ->>  Entering %s\n", source);
        SetSearchDevice(&ssb, pp);
        SourceSearchAndCopy(source, destination, attr.recursion.depth, attr, pp, report_context);

        if (cfstat(destination, &dsb) != -1)
        {
            if (attr.copy.check_root)
            {
                VerifyCopiedFileAttributes(destination, &dsb, &ssb, attr, pp, report_context);
            }
        }
    }
    else
    {
        VerifyCopy(source, destination, attr, pp, report_context);
    }

    snprintf(eventname, CF_BUFSIZE - 1, "Copy(%s:%s > %s)", server, source, destination);
    EndMeasure(eventname, start);

    return NULL;
}

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

#if !defined(MINGW)
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

bool CopyRegularFileDiskReport(char *source, char *destination, Attributes attr, Promise *pp)
// TODO: return error codes in CopyRegularFileDisk and print them to cfPS here
{
    bool make_holes = false;

    if(pp && (pp->makeholes))
    {
        make_holes = true;
    }

    bool result = CopyRegularFileDisk(source, destination, make_holes);

    if(!result)
    {
        cfPS(cf_inform, CF_FAIL, "", pp, attr, "Failed copying file %s to %s", source, destination);
    }

    return result;
}

/*********************************************************************/

bool CopyRegularFileDisk(char *source, char *destination, bool make_holes)
{
    int sd, dd, buf_size;
    char *buf, *cp;
    int n_read, *intp;
    long n_read_total = 0;
    int last_write_made_hole = 0;

    if ((sd = open(source, O_RDONLY | O_BINARY)) == -1)
    {
        CfOut(cf_inform, "open", "Can't copy %s!\n", source);
        unlink(destination);
        return false;
    }

    unlink(destination);                /* To avoid link attacks */

    if ((dd = open(destination, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY, 0600)) == -1)
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
                    CfOut(cf_error, "lseek", "Copy failed (no space?) while doing %s to %s\n", source, destination);
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
                CfOut(cf_error, "", "Copy failed (no space?) while doing %s to %s\n", source, destination);
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
            CfOut(cf_error, "write", "cfengine: full_write or ftruncate error in CopyReg\n");
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

/*********************************************************************/

int FSWrite(char *new, int dd, char *buf, int towrite, int *last_write_made_hole, int n_read, Attributes attr,
            Promise *pp)
{
    int *intp;
    char *cp;

    intp = 0;

    if (pp && (pp->makeholes))
    {
        buf[n_read] = 1;        /* Sentinel to stop loop.  */

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
                CfOut(cf_error, "lseek", "lseek in EmbeddedWrite, dest=%s\n", new);
                return false;
            }

            *last_write_made_hole = 1;
        }
        else
        {
            /* Clear to indicate that a normal write is needed. */
            intp = 0;
        }
    }

    if (intp == 0)
    {
        if (FullWrite(dd, buf, towrite) < 0)
        {
            CfOut(cf_error, "write", "Local disk write(%.256s) failed\n", new);
            pp->conn->error = true;
            return false;
        }

        *last_write_made_hole = 0;
    }

    return true;
}
