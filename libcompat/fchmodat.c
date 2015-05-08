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
#include <misc_lib.h>
#include <logging.h>
#include <generic_at.h>

#include <fcntl.h>
#include <grp.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef __MINGW32__

typedef struct
{
    const char *pathname;
    mode_t mode;
    int dirfd;
    int flags;
} fchmodat_data;

static int fchmodat_inner(void *generic_data)
{
    struct stat buf;
    int ret = 0;
    uid_t olduid = 0;

    fchmodat_data *data = generic_data;
    if (data->flags & AT_SYMLINK_NOFOLLOW)
    {
        errno = ENOTSUP;
        return -1;
    }

    /* {l,}stat(2) is used here since we don't want to lock on the inner
     * function
     */
    if ((ret = lstat(data->pathname, &buf)))
    {
        return ret;
    }

    /* save old euid */
    olduid = geteuid();

    if ((ret = seteuid(buf.st_uid)))
    {
        return ret;
    }

    ret = chmod(data->pathname, data->mode);

    if (seteuid(olduid))
    {
        return -1;
    }

    return ret;
}

static void cleanup(ARG_UNUSED void *generic_data)
{
}

int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags)
{
    fchmodat_data data = {
        .pathname = pathname,
        .mode = mode,
        .flags = flags,
        .dirfd = dirfd,
    };

    return generic_at_function(dirfd, &fchmodat_inner, &cleanup, &data);
}

#endif // !__MINGW32__
