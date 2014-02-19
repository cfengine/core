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

#include <enterprise_extension.h>

#include <known_dirs.h>
#include <misc_lib.h>

#include <pthread.h>

/*
 * A note regarding the loading of the extension plugins:
 *
 * The extension plugin was originally statically linked into each agent,
 * but was then refactored into plugins.
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

#ifndef BUILTIN_EXTENSIONS

static bool enable_extension_libraries = true; /* GLOBAL_X */
static bool attempted_loading = false; /* GLOBAL_X */

void extension_libraries_disable()
{
    if (attempted_loading)
    {
        ProgrammingError("extension_libraries_disable() MUST be called before any call to extension functions");
    }
    enable_extension_libraries = false;
}

// LOG_LEVEL_DEBUG and LOG_LEVEL_VERBOSE have no effect here
// because they happen before the generic agent has parsed its options!
void *extension_library_open(const char *name)
{
    Log(LOG_LEVEL_DEBUG, "Checking for Enterprise extension plugin %s", name);
    if (!enable_extension_libraries)
    {
        return NULL;
    }

    Log(LOG_LEVEL_DEBUG, "Enterprise extension plugin are enabled in the agent, proceeding with %s", name);
    if (getenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DO_CLOSE") == NULL)
    {
        // Only do loading checks if we are not doing tests.
        attempted_loading = true;
    }

    const char *dir = getenv("CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR");
    char lib[] = "/lib";
    if (dir)
    {
        lib[0] = '\0';
    }
    else
    {
        dir = GetWorkDir();
    }

    char path[strlen(dir) + strlen(lib) + strlen(name) + 2];
    sprintf(path, "%s%s/%s", dir, lib, name);

    Log(LOG_LEVEL_DEBUG, "Trying to shlib_open Enterprise extension plugin %s from %s", name, path);

    void *handle = shlib_open(path);
    if (!handle)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not shlib_open Enterprise extension plugin %s from %s", name, path);

        // try to load from the real WORKDIR in case GetWorkDir returned the local workdir to the user
        // We try this because enterprise "make install" is in WORKDIR, not per user
        if (0 != strcmp(WORKDIR, dir))
        {
            sprintf(path, "%s%s/%s", WORKDIR, lib, name);

            Log(LOG_LEVEL_DEBUG, "Trying to fallback shlib_open Enterprise extension plugin %s from workdir %s", name, path);
            handle = shlib_open(path);

            if (!handle)
            {
                Log(LOG_LEVEL_VERBOSE, "Fallback failed to shlib_open Enterprise extension plugin %s from workdir %s", name, path);
                return handle;
            }
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Successfully did shlib_open of Enterprise extension plugin %s from %s", name, path);

    // Version check, to avoid binary incompatible plugins.
    const char * (*GetExtensionLibraryVersion)() = shlib_load(handle, "GetExtensionLibraryVersion");
    if (!GetExtensionLibraryVersion)
    {
        Log(LOG_LEVEL_ERR, "Could not retreive version from Enterprise extension plugin (%s). Not loading the plugin.", name);
        goto close_and_fail;
    }

    const char *plugin_version = GetExtensionLibraryVersion();
    unsigned int bin_major, bin_minor, bin_patch;
    unsigned int plug_major, plug_minor, plug_patch;
    if (sscanf(VERSION, "%u.%u.%u", &bin_major, &bin_minor, &bin_patch) != 3)
    {
        Log(LOG_LEVEL_ERR, "Not able to extract version number from binary (%s). Not loading extension plugin.", name);
        goto close_and_fail;
    }
    if (sscanf(plugin_version, "%u.%u.%u", &plug_major, &plug_minor, &plug_patch) != 3)
    {
        Log(LOG_LEVEL_ERR, "Not able to extract version number from plugin (%s). Not loading extension plugin.", name);
        goto close_and_fail;
    }

    if (bin_major != plug_major || bin_minor != plug_minor || bin_patch != plug_patch)
    {
        Log(LOG_LEVEL_ERR, "Extension plugin version does not match CFEngine Community version "
            "(CFEngine Community v%u.%u.%u, Extension (%s) v%u.%u.%u). Refusing to load it.",
            bin_major, bin_minor, bin_patch, name, plug_major, plug_minor, plug_patch);
        goto close_and_fail;
    }

    Log(LOG_LEVEL_VERBOSE, "Successfully loaded Enterprise extension plugin %s from %s", name, path);

    return handle;

close_and_fail:
    shlib_close(handle);
    return NULL;
}

void extension_library_close(void *handle)
{
    shlib_close(handle);
}

#endif // !BUILTIN_EXTENSIONS
