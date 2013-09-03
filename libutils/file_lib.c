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

#include <file_lib.h>

#include <alloc.h>
#include <logging.h>

bool FileCanOpen(const char *path, const char *modes)
{
    FILE *test = NULL;

    if ((test = fopen(path, modes)) != NULL)
    {
        fclose(test);
        return true;
    }
    else
    {
        return false;
    }
}

ssize_t FileRead(const char *filename, char *buffer, size_t bufsize)
{
    FILE *f = fopen(filename, "rb");

    if (f == NULL)
    {
        return -1;
    }
    ssize_t ret = fread(buffer, bufsize, 1, f);

    if (ferror(f))
    {
        fclose(f);
        return -1;
    }
    fclose(f);
    return ret;
}


ssize_t FileReadMax(char **output, const char *filename, size_t size_max)
// TODO: there is CfReadFile and FileRead with slightly different semantics, merge
// free(output) should be called on positive return value
{
    assert(size_max > 0);

    struct stat sb;
    if (stat(filename, &sb) == -1)
    {
        return -1;
    }

    FILE *fin;

    if ((fin = fopen(filename, "r")) == NULL)
    {
        return -1;
    }

    ssize_t bytes_to_read = MIN(sb.st_size, size_max);
    *output = xcalloc(bytes_to_read + 1, 1);
    ssize_t bytes_read = fread(*output, 1, bytes_to_read, fin);

    if (ferror(fin))
    {
        Log(LOG_LEVEL_ERR, "FileContentsRead: Error while reading file '%s'. (ferror: %s)", filename, GetErrorStr());
        fclose(fin);
        free(*output);
        *output = NULL;
        return -1;
    }

    if (fclose(fin) != 0)
    {
        Log(LOG_LEVEL_ERR, "FileContentsRead: Could not close file '%s'. (fclose: %s)", filename, GetErrorStr());
    }

    return bytes_read;
}

int FullWrite(int desc, const char *ptr, size_t len)
{
    int total_written = 0;

    while (len > 0)
    {
        int written = write(desc, ptr, len);

        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return written;
        }

        total_written += written;
        ptr += written;
        len -= written;
    }

    return total_written;
}

int FullRead(int fd, char *ptr, size_t len)
{
    int total_read = 0;

    while (len > 0)
    {
        int bytes_read = read(fd, ptr, len);

        if (bytes_read < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return -1;
        }

        if (bytes_read == 0)
        {
            return total_read;
        }

        total_read += bytes_read;
        ptr += bytes_read;
        len -= bytes_read;
    }

    return total_read;
}
