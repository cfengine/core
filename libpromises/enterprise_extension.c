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

#ifndef ENTERPRISE_BUILTIN_EXTENSIONS

#include <sysinfo.h>
#include <misc_lib.h>

#include <pthread.h>

/*
 * A note regarding the loading of the enterprise plugin:
 *
 * The enterprise plugin was originally statically linked into each agent,
 * but was then refactored into a plugin.
 * Therefore, since it hasn't been written according to plugin guidelines,
 * it is not safe to assume that we can unload it once we have
 * loaded it, since it may allocate resources that are not freed. It is also
 * not safe to assume that we can load it after initialization is complete
 * (for example if the plugin is dropped into the directory after cf-serverd
 * has already been running for some time), because some data structures may
 * not have been initialized.
 *
 * Therefore, the load strategy is as follows:
 *
 * - If we find the plugin immediately, load it, and keep it loaded.
 *
 * - If we don't find the plugin, NEVER attempt to load it again afterwards.
 *
 * - Never unload the plugin.
 *
 * - Any installation/upgrade/removal of the plugin requires daemon restarts.
 *
 * - The exception is for testing (see getenv below).
 */

static pthread_once_t enterprise_library_once = PTHREAD_ONCE_INIT;
static void *enterprise_library_handle = NULL;
static bool enable_enterprise_library = true;

static void enterprise_library_assign();
static void *enterprise_library_open_impl();

void enterprise_library_disable()
{
    if (enterprise_library_handle)
    {
        ProgrammingError("enterprise_library_disable() MUST be called before any call to extension functions");
    }
    enable_enterprise_library = false;
}

void *enterprise_library_open()
{
    if (!enable_enterprise_library)
    {
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

static void enterprise_library_assign()
{
    enterprise_library_handle = enterprise_library_open_impl();
}

static void *enterprise_library_open_impl()
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
    void *handle = shlib_open(path);
    if (!handle)
    {
        return handle;
    }

    // Version check, to avoid binary incompatible plugins.
    const char * (*GetExtensionLibraryVersion)() = shlib_load(handle, "GetExtensionLibraryVersion");
    if (!GetExtensionLibraryVersion)
    {
        Log(LOG_LEVEL_ERR, "Could not retreive version from Enterprise plugin. Not loading the plugin.");
        goto close_and_fail;
    }

    const char *plugin_version = GetExtensionLibraryVersion();
    unsigned int bin_major, bin_minor, bin_patch;
    unsigned int plug_major, plug_minor, plug_patch;
    if (sscanf(VERSION, "%u.%u.%u", &bin_major, &bin_minor, &bin_patch) != 3)
    {
        Log(LOG_LEVEL_ERR, "Not able to extract version number from binary. Not loading Enterprise plugin.");
        goto close_and_fail;
    }
    if (sscanf(plugin_version, "%u.%u.%u", &plug_major, &plug_minor, &plug_patch) != 3)
    {
        Log(LOG_LEVEL_ERR, "Not able to extract version number from plugin. Not loading Enterprise plugin.");
        goto close_and_fail;
    }

    if (bin_major != plug_major || bin_minor != plug_minor || bin_patch != plug_patch)
    {
        Log(LOG_LEVEL_ERR, "Enterprise plugin version does not match CFEngine Community version "
            "(CFEngine Community v%u.%u.%u, Enterprise v%u.%u.%u). Refusing to load it.",
            bin_major, bin_minor, bin_patch, plug_major, plug_minor, plug_patch);
        goto close_and_fail;
    }

    return handle;

close_and_fail:
    shlib_close(handle);
    return NULL;
}

void enterprise_library_close(void *handle)
{
    if (getenv("CFENGINE_TEST_OVERRIDE_ENTERPRISE_LIBRARY_DO_CLOSE") != NULL)
    {
        return shlib_close(handle);
    }

    // Normally we don't ever close the extension library, because we may have
    // pointer references to it.
}

#else // ENTERPRISE_BUILTIN_EXTENSIONS

// Has no effect when using builtin extensions.
void enterprise_library_disable()
{
}

#endif // ENTERPRISE_BUILTIN_EXTENSIONS
