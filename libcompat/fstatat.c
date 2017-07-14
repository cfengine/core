/*
   Copyright 2017 Northern.tech AS

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

#include <platform.h>
#include <misc_lib.h>
#include <logging.h>
#include <generic_at.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef __MINGW32__

typedef struct
{
    const char *pathname;
    struct stat *buf;
    int flags;
} fstatat_data;

static int fstatat_inner(void *generic_data)
{
    fstatat_data *data = generic_data;
    if (data->flags & AT_SYMLINK_NOFOLLOW)
    {
        return lstat(data->pathname, data->buf);
    }
    else
    {
        return stat(data->pathname, data->buf);
    }
}

static void cleanup(ARG_UNUSED void *generic_data)
{
}

int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags)
{
    fstatat_data data;
    data.pathname = pathname;
    data.buf = buf;
    data.flags = flags;

    return generic_at_function(dirfd, &fstatat_inner, &cleanup, &data);
}

#endif // !__MINGW32__
