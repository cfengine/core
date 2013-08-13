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

#include <enterprise_extension.h>
#include <sysinfo.h>

#include <dlfcn.h>
#include <pthread.h>

static pthread_once_t enterprise_library_once = PTHREAD_ONCE_INIT;
static void *enterprise_library_handle = NULL;

void enterprise_library_assign();
void *enterprise_library_open_impl();

void *enterprise_library_open()
{
    if (THIS_AGENT_TYPE == AGENT_TYPE_EXECUTOR)
    {
        // This is to protect the upgrade path. cf-execd should not be allowed
        // to load the plugin because it may be the last daemon around when
        // doing an upgrade.
        Log(LOG_LEVEL_DEBUG, "Not loading Enterprise plugin for cf-execd");
        return NULL;
    }

    if (getenv("CFENGINE_TEST_OVERRIDE_ENTERPRISE_LIBRARY_DO_CLOSE") != NULL)
    {
        return enterprise_library_open_impl();
    }

    int ret = pthread_once(&enterprise_library_once, &enterprise_library_assign);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Could not initialize Enterprise Library: %s", strerror(ret));
        return NULL;
    }
    return enterprise_library_handle;
}

void enterprise_library_assign()
{
    enterprise_library_handle = enterprise_library_open_impl();
}

void *enterprise_library_open_impl()
{
    const char *dir = getenv("CFENGINE_TEST_OVERRIDE_ENTERPRISE_LIBRARY_DIR");
    char lib[] = "/lib";
    if (dir)
    {
        lib[0] = '\0';
    }
    else
    {
        dir = GetWorkDir();
    }
    char path[strlen(dir) + strlen(lib) + strlen(ENTERPRISE_LIBRARY_NAME) + 2];
    sprintf(path, "%s%s/%s", dir, lib, ENTERPRISE_LIBRARY_NAME);
    return shlib_open(path);
}

void enterprise_library_close(void *handle)
{
    if (getenv("CFENGINE_TEST_OVERRIDE_ENTERPRISE_LIBRARY_DO_CLOSE") != NULL)
    {
        return shlib_close(handle);
    }

    // Normally we don't ever close the enterprise library, because we may have
    // pointer references to it.
}

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
    return dlsym(handle, symbol_name);
}

void shlib_close(void *handle)
{
    dlclose(handle);
}
