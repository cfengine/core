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

#include <dir.h>
#include <dir_priv.h>
#include <file_lib.h>
#include <alloc.h>


struct Dir_
{
    DIR *dirh;
    struct dirent *entrybuf;
};

static size_t GetNameMax(DIR *dirp);
static size_t GetDirentBufferSize(size_t path_len);

Dir *DirOpen(const char *dirname)
{
    Dir *ret = xcalloc(1, sizeof(Dir));
    int safe_fd;

    safe_fd = safe_open(dirname, O_RDONLY);
    if (safe_fd < 0)
    {
        free(ret);
        return NULL;
    }

    ret->dirh = opendir(dirname);
    if (ret->dirh == NULL)
    {
        close(safe_fd);
        free(ret);
        return NULL;
    }

    struct stat safe_stat, dir_stat;
    bool stat_failed = fstat(safe_fd, &safe_stat) < 0 || fstat(dirfd(ret->dirh), &dir_stat) < 0;
    close(safe_fd);
    if (stat_failed)
    {
        closedir(ret->dirh);
        free(ret);
        return NULL;
    }

    // Make sure we opened the same file descriptor as safe_open did.
    if (safe_stat.st_dev != dir_stat.st_dev || safe_stat.st_ino != dir_stat.st_ino)
    {
        closedir(ret->dirh);
        free(ret);
        errno = EACCES;
        return NULL;
    }

    size_t dirent_buf_size = GetDirentBufferSize(GetNameMax(ret->dirh));

    ret->entrybuf = xcalloc(1, dirent_buf_size);

    return ret;
}

/*
 * Returns NULL on EOF or error.
 *
 * Sets errno to 0 for EOF and non-0 for error.
 */
const struct dirent *DirRead(Dir *dir)
{
    errno = 0;

    struct dirent *ret;
    int err = readdir_r((DIR *) dir->dirh, dir->entrybuf, &ret);

    if (err != 0)
    {
        errno = err;
        return NULL;
    }

    if (ret == NULL)
    {
        return NULL;
    }

    return ret;
}

void DirClose(Dir *dir)
{
    closedir((DIR *) dir->dirh);
    free(dir->entrybuf);
    free(dir);
}

/*
 * Taken from http://womble.decadent.org.uk/readdir_r-advisory.html
 *
 * Issued by Ben Hutchings <ben@decadent.org.uk>, 2005-11-02.
 *
 * Licence
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following condition:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Calculate the required buffer size (in bytes) for directory entries read from
 * the given directory handle.  Return -1 if this this cannot be done.
 *
 * This code does not trust values of NAME_MAX that are less than 255, since
 * some systems (including at least HP-UX) incorrectly define it to be a smaller
 * value.
 *
 * If you use autoconf, include fpathconf and dirfd in your AC_CHECK_FUNCS list.
 * Otherwise use some other method to detect and use them where available.
 */

#if defined(HAVE_FPATHCONF) && defined(_PC_NAME_MAX)

static size_t GetNameMax(DIR *dirp)
{
    long name_max = fpathconf(dirfd(dirp), _PC_NAME_MAX);

    if (name_max != -1)
    {
        return name_max;
    }

# if defined(NAME_MAX)
    return (NAME_MAX > 255) ? NAME_MAX : 255;
# else
    return (size_t) (-1);
# endif
}

#else /* FPATHCONF && _PC_NAME_MAX */

# if defined(NAME_MAX)
static size_t GetNameMax(DIR *dirp)
{
    return (NAME_MAX > 255) ? NAME_MAX : 255;
}
# else
#  error "buffer size for readdir_r cannot be determined"
# endif

#endif /* FPATHCONF && _PC_NAME_MAX */

/*
 * Returns size of memory enough to hold path name_len bytes long.
 */
static size_t GetDirentBufferSize(size_t name_len)
{
    size_t name_end = (size_t) offsetof(struct dirent, d_name) + name_len + 1;

    return MAX(name_end, sizeof(struct dirent));
}

struct dirent *AllocateDirentForFilename(const char *filename)
{
    struct dirent *entry = xcalloc(1, GetDirentBufferSize(strlen(filename)));

    strcpy(entry->d_name, filename);
    return entry;
}
