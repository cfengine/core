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

#include <chdir_lock.h>
#include <misc_lib.h>
#include <logging.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#ifndef __MINGW32__

// Using fchdir() is ugly but it's the only way to be secure.
// See comments in openat.c.
int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags)
{
    int cwd;
    int mutex_err;
    int result;
    int saved_errno;
    int fchdir_ret;

    mutex_err = pthread_mutex_lock(&CHDIR_LOCK);
    if (mutex_err)
    {
        ProgrammingError("Error when locking CHDIR_LOCK. Should never happen. (pthread_mutex_lock: '%s')",
                         strerror(mutex_err));
    }

    cwd = open(".", O_RDONLY);
    if (cwd < 0)
    {
        mutex_err = pthread_mutex_unlock(&CHDIR_LOCK);
        if (mutex_err)
        {
            ProgrammingError("Error when unlocking CHDIR_LOCK. Should never happen. (pthread_mutex_unlock: '%s')",
                             strerror(mutex_err));
        }

        return -1;
    }

    if (fchdir(dirfd) < 0)
    {
        mutex_err = pthread_mutex_unlock(&CHDIR_LOCK);
        if (mutex_err)
        {
            ProgrammingError("Error when unlocking CHDIR_LOCK. Should never happen. (pthread_mutex_unlock: '%s')",
                             strerror(mutex_err));
        }

        close(cwd);
        return -1;
    }

    if (flags & AT_SYMLINK_NOFOLLOW)
    {
        result = lstat(pathname, buf);
    }
    else
    {
        result = stat(pathname, buf);
    }
    saved_errno = errno;
    fchdir_ret = fchdir(cwd);
    close(cwd);

    mutex_err = pthread_mutex_unlock(&CHDIR_LOCK);
    if (mutex_err)
    {
        ProgrammingError("Error when unlocking CHDIR_LOCK. Should never happen. (pthread_mutex_unlock: '%s')",
                         strerror(mutex_err));
    }

    if (fchdir_ret < 0)
    {
        Log(LOG_LEVEL_WARNING, "Could not return to original working directory in '%s'. "
            "Things may not behave as expected. (fchdir: '%s')", __FUNCTION__, GetErrorStr());
        return -1;
    }

    if (result < 0)
    {
        errno = saved_errno;
        return -1;
    }

    return result;
}

#endif // !__MINGW32__
