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
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>

#ifndef __MINGW32__

// To prevent several threads from stepping on each other's toes
// when using fchdir().
pthread_mutex_t CHDIR_LOCK = PTHREAD_MUTEX_INITIALIZER;

/*
 * Using fchdir() is ugly but it's the only way to be secure.
 * Using fchdir() in file opening means that we can potentially
 * conflict with chdir()/fchdir() being used elsewhere. For this to be
 * safe, the program must fulfill at least one of the following
 * criteria:
 *   1. Be single threaded.
 *   2. Not use chdir() anywhere else but here.
 *   3. Do all file operations (including chdir) in one thread.
 *   4. Use the CHDIR_LOCK in this file.
 * Currently, cf-agent fulfills criterion 1. All the others fulfill
 * criterion 2.
 */
int openat(int dirfd, const char *pathname, int flags, ...)
{
    mode_t mode;
    int cwd;
    int mutex_err;
    int fd;
    int saved_errno;
    int fchdir_ret;

    if (flags & O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    else
    {
        mode = 0;
    }

    mutex_err = pthread_mutex_lock(&CHDIR_LOCK);
    if (mutex_err)
    {
        ProgrammingError("Error when locking CHDIR_LOCK. Should never happen. (pthread_mutex_lock: '%s')",
                         GetErrorStrFromCode(mutex_err));
    }

    cwd = open(".", O_RDONLY);
    if (cwd < 0)
    {
        mutex_err = pthread_mutex_unlock(&CHDIR_LOCK);
        if (mutex_err)
        {
            ProgrammingError("Error when unlocking CHDIR_LOCK. Should never happen. (pthread_mutex_unlock: '%s')",
                             GetErrorStrFromCode(mutex_err));
        }
        return -1;
    }

    if (fchdir(dirfd) < 0)
    {
        mutex_err = pthread_mutex_unlock(&CHDIR_LOCK);
        if (mutex_err)
        {
            ProgrammingError("Error when unlocking CHDIR_LOCK. Should never happen. (pthread_mutex_unlock: '%s')",
                             GetErrorStrFromCode(mutex_err));
        }

        close(cwd);
        return -1;
    }

    fd = open(pathname, flags, mode);
    saved_errno = errno;

    fchdir_ret = fchdir(cwd);
    close(cwd);

    mutex_err = pthread_mutex_unlock(&CHDIR_LOCK);
    if (mutex_err)
    {
        ProgrammingError("Error when unlocking CHDIR_LOCK. Should never happen. (pthread_mutex_unlock: '%s')",
                         GetErrorStrFromCode(mutex_err));
    }

    if (fchdir_ret < 0)
    {
        if (fd >= 0)
        {
            close(fd);
        }
        Log(LOG_LEVEL_WARNING, "Could not return to original working directory in '%s'. "
            "Things may not behave as expected. (fchdir: '%s')", __FUNCTION__, GetErrorStr());
        return -1;
    }
    if (fd < 0)
    {
        errno = saved_errno;
        return -1;
    }

    return fd;
}

#endif // !__MINGW32__
