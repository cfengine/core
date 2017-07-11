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
#include <unistd.h>
#include <stdarg.h>

#ifndef __MINGW32__

typedef struct
{
    const char *pathname;
    int flags;
    mode_t mode;
    int fd;
} openat_data;

static int openat_inner(void *generic_data)
{
    openat_data *data = generic_data;
    data->fd = open(data->pathname, data->flags, data->mode);
    return data->fd;
}

static void cleanup(void *generic_data)
{
    openat_data *data = generic_data;
    if (data->fd >= 0)
    {
        close(data->fd);
    }
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
    openat_data data;

    data.pathname = pathname;
    data.flags = flags;
    if (flags & O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        data.mode = va_arg(ap, int);
        va_end(ap);
    }
    else
    {
        data.mode = 0;
    }
    data.fd = -1;

    return generic_at_function(dirfd, &openat_inner, &cleanup, &data);
}

#endif // !__MINGW32__
