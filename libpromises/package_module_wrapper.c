/*
  Copyright 2026 Northern.tech AS

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

#include <package_module_wrapper.h>
#include <pipes.h>
#include <exec_tools.h>
#include <buffer.h>
#include <string_lib.h>
#include <path.h>
#include <file_lib.h>
#include <known_dirs.h>
#include <rlist.h>

static void GetPackageModuleExecInfo(const PackageModuleBody *package_module, char **exec_path,
                                     char **script_path, char **script_path_quoted, char **script_exec_opts);
static int NegotiateSupportedAPIVersion(PackageModuleWrapper *wrapper);

void DeletePackageModuleWrapper(PackageModuleWrapper *wrapper)
{
    if (wrapper != NULL)
    {
        free(wrapper->path);
        free(wrapper->script_path);
        free(wrapper->script_path_quoted);
        free(wrapper->script_exec_opts);
        free(wrapper->name);
        free(wrapper);
    }
}

PackageModuleWrapper *NewPackageModuleWrapper(PackageModuleBody *package_module,
                                              int timeout_secs)
{
    assert(package_module && package_module->name);

    PackageModuleWrapper *wrapper = xmalloc(sizeof(PackageModuleWrapper));

    GetPackageModuleExecInfo(package_module, &(wrapper->path),
                             &(wrapper->script_path), &(wrapper->script_path_quoted),
                             &(wrapper->script_exec_opts));
    wrapper->name = SafeStringDuplicate(package_module->name);
    wrapper->package_module = package_module;
    wrapper->timeout_secs = timeout_secs;

    if (wrapper->path == NULL)
    {
        Log(LOG_LEVEL_ERR, "No executable for the package module '%s'", package_module->name);
        DeletePackageModuleWrapper(wrapper);
        return NULL;
    }

    /* Check if the given files exist and have the required permissions. */
    if (wrapper->script_path != NULL)
    {
        /* we were given a path to a script to execute with the given
         * interpreter */
        const char *interpreter_path = wrapper->path;
        const char *script_path = wrapper->script_path;

        if (access(interpreter_path, X_OK) != 0)
        {
            Log(LOG_LEVEL_ERR,
                "Cannot find package module interpreter at location '%s'"
                " or access to the file is restricted: %s",
                wrapper->path, GetErrorStr());
            DeletePackageModuleWrapper(wrapper);
            return NULL;
        }
        if (access(script_path, R_OK) != 0)
        {
            Log(LOG_LEVEL_ERR,
                "Cannot find package module script at location '%s'"
                " or access to the file is restricted: %s",
                wrapper->script_path, GetErrorStr());
            DeletePackageModuleWrapper(wrapper);
            return NULL;
        }
    }
    else if (access(wrapper->path, X_OK) != 0)
    {
        /* no script path specified --> the given path has to be executable */
        Log(LOG_LEVEL_ERR,
            "Cannot find package module at location '%s' or access to file is restricted: %s",
            wrapper->path, GetErrorStr());
        DeletePackageModuleWrapper(wrapper);
        return NULL;
    }

    /* Negotiate API version */
    wrapper->supported_api_version = NegotiateSupportedAPIVersion(wrapper);
    if (wrapper->supported_api_version != 1)
    {
        Log(LOG_LEVEL_ERR,
            "Unsupported package module wrapper API version: %d",
            wrapper->supported_api_version);
        DeletePackageModuleWrapper(wrapper);
        return NULL;
    }

    Log(LOG_LEVEL_DEBUG,
        "Successfully created package module wrapper for '%s' package module.",
        package_module->name);

    return wrapper;
}

int PackageWrapperCommunicate(const PackageModuleWrapper *wrapper, const char *args,
                              const char *request, Rlist **response)
{
    char *all_args = NULL;
    if (wrapper->script_path != NULL)
    {
        if (wrapper->script_exec_opts == NULL)
        {
            all_args = StringConcatenate(3, wrapper->script_path_quoted, " ", args);
            args = all_args;
        }
        else
        {
            all_args = StringConcatenate(5, wrapper->script_exec_opts, " ",
                                         wrapper->script_path_quoted, " ", args);
            args = all_args;
        }
    }
    int ret = PipeReadWriteData(wrapper->path, args, request, response,
                                wrapper->timeout_secs,
                                PACKAGE_PROMISE_TERMINATION_CHECK_SEC);
    free(all_args);

    return ret;
}

char *PackageModuleFormatOptions(Rlist *options)
{
    if (RlistIsNullList(options))
    {
        return SafeStringDuplicate("");
    }

    Buffer *data = BufferNew();
    for (Rlist *rp = options; rp != NULL; rp = rp->next)
    {
        char *value = RlistScalarValue(rp);
        BufferAppendString(data, "options=");
        BufferAppendString(data, value);
        BufferAppendString(data, "\n");
    }
    return BufferClose(data);
}

static int NegotiateSupportedAPIVersion(PackageModuleWrapper *wrapper)
{
    assert(wrapper);

    Log(LOG_LEVEL_DEBUG, "Getting supported API version.");

    int api_version = -1;

    Rlist *response = NULL;
    if (PackageWrapperCommunicate(wrapper, "supports-api-version", "",
                                  &response) != 0)
    {
        Log(LOG_LEVEL_INFO,
            "Error occurred while getting supported API version.");
        return -1;
    }

    if (response)
    {
        if (RlistLen(response) == 1)
        {
            api_version = atoi(RlistScalarValue(response));
            Log(LOG_LEVEL_DEBUG, "package wrapper API version: %d", api_version);
        }
        RlistDestroy(response);
    }
    return api_version;
}

static void GetPackageModuleExecInfo(const PackageModuleBody *package_module, char **exec_path,
                                     char **script_path, char **script_path_quoted,
                                     char **script_exec_opts)
{

    assert(exec_path != NULL);
    assert(script_path != NULL);
    assert(script_path_quoted != NULL);

    char *package_module_path = NULL;

    if (package_module->module_path != NULL && !StringEqual(package_module->module_path, ""))
    {
        package_module_path = xstrdup(package_module->module_path);
    }
    else
    {
        package_module_path = StringFormat("%s%c%s%c%s%c%s", GetWorkDir(), FILE_SEPARATOR,
                                           "modules", FILE_SEPARATOR, "packages", FILE_SEPARATOR,
                                           package_module->name);
    }

    if (package_module->interpreter && !StringEqual(package_module->interpreter, ""))
    {
        *script_path = package_module_path;
        *script_path_quoted = Path_GetQuoted(*script_path);

        if (strchr(package_module->interpreter, ' ') == NULL)
        {
            /* No spaces in the 'interpreter' string, easy! Just interpreter path given, no opts. */
            *exec_path = xstrdup(package_module->interpreter);
            *script_exec_opts = NULL;
        }
        else
        {
            /* we need to split the interpreter command from the (potential)
             * interpreter args */
            ArgGetExecutableAndArgs(package_module->interpreter, exec_path, script_exec_opts);
        }
    }
    else
    {
        *exec_path = package_module_path;
        *script_path = NULL;
        *script_path_quoted = NULL;
        *script_exec_opts = NULL;
    }
}
