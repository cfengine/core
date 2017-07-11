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
#include <misc_lib.h>
#include <logging.h>
#include <generic_at.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef __MINGW32__

typedef struct
{
    const char *pathname;
    mode_t mode;
    int flags;
} fchmodat_data;

static int fchmodat_inner(void *generic_data)
{
    fchmodat_data *data = generic_data;
    if (data->flags & AT_SYMLINK_NOFOLLOW)
    {
        errno = ENOTSUP;
        return -1;
    }

    return chmod(data->pathname, data->mode);
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
    };

    return generic_at_function(dirfd, &fchmodat_inner, &cleanup, &data);
}

#endif // !__MINGW32__
