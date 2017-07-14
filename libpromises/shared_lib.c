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

#ifndef __MINGW32__

#include <shared_lib.h>
#include <logging.h>
#include <mutex.h>

#include <dlfcn.h>

void *shlib_open(const char *lib_name)
{
    struct stat statbuf;
    if (stat(lib_name, &statbuf) == -1)
    {
        Log(LOG_LEVEL_DEBUG, "Could not open shared library: %s\n", GetErrorStr());
        return NULL;
    }

    void * ret = dlopen(lib_name, RTLD_NOW);
    if (!ret)
    {
        Log(LOG_LEVEL_ERR, "Could not open shared library: %s\n", dlerror());
    }
    return ret;
}

void *shlib_load(void *handle, const char *symbol_name)
{
    static pthread_mutex_t dlsym_mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
    ThreadLock(&dlsym_mutex);
    void *ret = dlsym(handle, symbol_name);
    ThreadUnlock(&dlsym_mutex);
    return ret;
}

void shlib_close(void *handle)
{
    dlclose(handle);
}

#endif // !__MINGW32__
