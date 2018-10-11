/*
   Copyright 2018 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#include <verify_files_hashes.h>

#include <actuator.h>
#include <rlist.h>
#include <policy.h>
#include <client_code.h>
#include <files_interfaces.h>
#include <files_lib.h>
#include <files_hashes.h>
#include <misc_lib.h>
#include <eval_context.h>
#include <known_dirs.h>

int CompareFileHashes(const char *file1, const char *file2, struct stat *sstat, struct stat *dstat, FileCopy fc, AgentConnection *conn)
{
    unsigned char digest1[EVP_MAX_MD_SIZE + 1] = { 0 }, digest2[EVP_MAX_MD_SIZE + 1] = { 0 };
    int i;

    if (sstat->st_size != dstat->st_size)
    {
        Log(LOG_LEVEL_DEBUG, "File sizes differ, no need to compute checksum");
        return true;
    }

    if (conn == NULL)
    {
        HashFile(file1, digest1, CF_DEFAULT_DIGEST, false);
        HashFile(file2, digest2, CF_DEFAULT_DIGEST, false);

        for (i = 0; i < EVP_MAX_MD_SIZE; i++)
        {
            if (digest1[i] != digest2[i])
            {
                return true;
            }
        }

        Log(LOG_LEVEL_DEBUG, "Files were identical");
        return false;           /* only if files are identical */
    }
    else
    {
        assert(fc.servers && strcmp(RlistScalarValue(fc.servers), "localhost"));
        return CompareHashNet(file1, file2, fc.encrypt, conn);  /* client.c */
    }
}

int CompareBinaryFiles(const char *file1, const char *file2, struct stat *sstat, struct stat *dstat, FileCopy fc, AgentConnection *conn)
{
    int fd1, fd2, bytes1, bytes2;
    char buff1[BUFSIZ], buff2[BUFSIZ];

    if (sstat->st_size != dstat->st_size)
    {
        Log(LOG_LEVEL_DEBUG, "File sizes differ, no need to compute checksum");
        return true;
    }

    if (conn == NULL)
    {
        fd1 = safe_open(file1, O_RDONLY | O_BINARY, 0400);
        fd2 = safe_open(file2, O_RDONLY | O_BINARY, 0400);

        do
        {
            bytes1 = read(fd1, buff1, BUFSIZ);
            bytes2 = read(fd2, buff2, BUFSIZ);

            if ((bytes1 != bytes2) || (memcmp(buff1, buff2, bytes1) != 0))
            {
                Log(LOG_LEVEL_VERBOSE, "Binary Comparison mismatch...");
                close(fd2);
                close(fd1);
                return true;
            }
        } while (bytes1 > 0);

        close(fd2);
        close(fd1);

        return false;           /* only if files are identical */
    }
    else
    {
        assert(fc.servers && strcmp(RlistScalarValue(fc.servers), "localhost"));
        Log(LOG_LEVEL_DEBUG, "Using network checksum instead");
        return CompareHashNet(file1, file2, fc.encrypt, conn);  /* client.c */
    }
}
