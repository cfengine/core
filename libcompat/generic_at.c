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

#include <errno.h>
#include <unistd.h>

#ifndef __MINGW32__

/*
 * Implements a generic interface to implement the POSIX-2008 *at
 * functions (openat, fstatat, fchownat, etc.).
 */

/*
 * This function uses fchdir() to preserve integrity when querying
 * a file from a directory descriptor. It's ugly but it's the only way
 * to be secure.
 * Using fchdir() in *at functions means that we can potentially
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

// To prevent several threads from stepping on each other's toes
// when using fchdir().
static pthread_mutex_t CHDIR_LOCK = PTHREAD_MUTEX_INITIALIZER;

/**
 * Generic *at function.
 * @param dirfd File descriptor pointing to directory to do lookup in.
 *              AT_FDCWD constant means to look in current directory.
 * @param func Function to call while in the directory.
 * @param cleanup Function to call if we need to clean up because of a failed call.
 * @param data Private data for the supplied functions.
 */
int generic_at_function(int dirfd, int (*func)(void *data), void (*cleanup)(void *data), void *data)
{
    int cwd;
    int mutex_err;
    int saved_errno;

    mutex_err = pthread_mutex_lock(&CHDIR_LOCK);
    if (mutex_err)
    {
        UnexpectedError("Error when locking CHDIR_LOCK. Should never happen. (pthread_mutex_lock: '%s')",
                        GetErrorStrFromCode(mutex_err));
    }

    if (dirfd != AT_FDCWD)
    {
        cwd = open(".", O_RDONLY);
        if (cwd < 0)
        {
            mutex_err = pthread_mutex_unlock(&CHDIR_LOCK);
            if (mutex_err)
            {
                UnexpectedError("Error when unlocking CHDIR_LOCK. Should never happen. (pthread_mutex_unlock: '%s')",
                                GetErrorStrFromCode(mutex_err));
            }
            return -1;
        }

        if (fchdir(dirfd) < 0)
        {
            close(cwd);

            mutex_err = pthread_mutex_unlock(&CHDIR_LOCK);
            if (mutex_err)
            {
                UnexpectedError("Error when unlocking CHDIR_LOCK. Should never happen. (pthread_mutex_unlock: '%s')",
                                GetErrorStrFromCode(mutex_err));
            }

            return -1;
        }
    }

    int result = func(data);
    saved_errno = errno;

    int fchdir_ret = -1; // initialize to error to catch code paths that don't set but test
    if (dirfd != AT_FDCWD)
    {
        fchdir_ret = fchdir(cwd);
        close(cwd);
    }

    mutex_err = pthread_mutex_unlock(&CHDIR_LOCK);
    if (mutex_err)
    {
        UnexpectedError("Error when unlocking CHDIR_LOCK. Should never happen. (pthread_mutex_unlock: '%s')",
                        GetErrorStrFromCode(mutex_err));
    }

    if (dirfd != AT_FDCWD)
    {
        if (fchdir_ret < 0)
        {
            cleanup(data);
            Log(LOG_LEVEL_WARNING, "Could not return to original working directory in '%s'. "
                "Things may not behave as expected. (fchdir: '%s')", __FUNCTION__, GetErrorStr());
            return -1;
        }
    }

    errno = saved_errno;

    return result;
}

#endif // !__MINGW32__
