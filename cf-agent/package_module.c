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

#include <package_module.h>
#include <pipes.h>
#include <signals.h>
#include <buffer.h>
#include <string_lib.h>
#include <actuator.h>
#include <file_lib.h>
#include <known_dirs.h>
#include <locks.h>
#include <rlist.h>
#include <policy.h>
#include <eval_context.h>

#define INVENTORY_LIST_BUFFER_SIZE 100 * 80 /* 100 entries with 80 characters 
                                             * per line */

static bool UpdateSinglePackageModuleCache(EvalContext *ctx,
                                    const PackageModuleWrapper *module_wrapper,
                                    UpdateType type, bool force_update);
static char *GetPackageModuleRealPath(const char *package_manager_name);
static int NegotiateSupportedAPIVersion(PackageModuleWrapper *wrapper);


void DeletePackageModuleWrapper(PackageModuleWrapper *wrapper)
{
    free(wrapper->path);
    free(wrapper->name);
    free(wrapper);
}

PackageModuleWrapper *NewPackageModuleWrapper(PackageModuleBody *package_module)
{
    assert(package_module && package_module->name);

    PackageModuleWrapper *wrapper = xmalloc(sizeof(PackageModuleWrapper));
    if (!wrapper)
    {
        return NULL;
    }
    
    wrapper->path = GetPackageModuleRealPath(package_module->name);
    wrapper->name = SafeStringDuplicate(package_module->name);
    wrapper->package_module = package_module;
    
    /* Check if file exists */
    struct stat sb;
    if (!wrapper->path || (stat(wrapper->path, &sb) != 0))
    {
        Log(LOG_LEVEL_VERBOSE,
            "can not find package wrapper in provided location: %s",
            wrapper->path);
        DeletePackageModuleWrapper(wrapper);
        return NULL;
    }
    
    /* Negotiate API version */
    wrapper->supported_api_version = NegotiateSupportedAPIVersion(wrapper);
    if (wrapper->supported_api_version != 1)
    {
        Log(LOG_LEVEL_VERBOSE,
            "unsupported package module wrapper API version: %d",
            wrapper->supported_api_version);
        DeletePackageModuleWrapper(wrapper);
        return NULL;
    }
    return wrapper;
}


void UpdatePackagesCache(EvalContext *ctx, bool force_update)
{
    Log(LOG_LEVEL_DEBUG, "Updating package cache.");
    
    PackagePromiseGlobalLock package_lock = 
            AcquireGlobalPackagePromiseLock(ctx);
                        
    if (package_lock.g_lock.lock == NULL)
    {
        Log(LOG_LEVEL_INFO, "Can not aquire global lock for package promise. "
            "Skipping updating cache.");
        return;
    }
                        
    Rlist *default_inventory = GetDefaultInventoryFromContext(ctx);
    
    for (const Rlist *rp = default_inventory; rp != NULL; rp = rp->next)
    {
        const char *pm_name =  RlistScalarValue(rp);
        
        /* We don't want inventory to be reported. */
        if (StringSafeEqual(pm_name, "cf_null"))
        {
            break;
        }
        
        PackageModuleBody *module = GetPackageModuleFromContext(ctx, pm_name);
        if (!module)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Can not find body for package module: %s", pm_name);
            continue;
        }

        PackageModuleWrapper *module_wrapper = NewPackageModuleWrapper(module);

        if (!module_wrapper)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Can not set up wrapper for module: %s", pm_name);
            continue;
        }

        UpdateSinglePackageModuleCache(ctx, module_wrapper,
                                       UPDATE_TYPE_INSTALLED, force_update);
        UpdateSinglePackageModuleCache(ctx, module_wrapper,
                                       force_update ? UPDATE_TYPE_LOCAL_UPDATES : 
                                           UPDATE_TYPE_UPDATES,
                                       force_update);

        DeletePackageModuleWrapper(module_wrapper);
    }
    YieldGlobalPackagePromiseLock(package_lock);
}

PackagePromiseGlobalLock AcquireGlobalPackagePromiseLock(EvalContext *ctx)
{
    Bundle bundle = {.name = "package_global"};
    PromiseType promise_type = {.name = "package_global",
                               .parent_bundle = &bundle};
    Promise pp = {.promiser = "package_global",
                  .parent_promise_type = &promise_type};
    
    CfLock package_promise_global_lock;
    
    package_promise_global_lock =
            AcquireLock(ctx, GLOBAL_PACKAGE_PROMISE_LOCK_NAME, VUQNAME, CFSTARTTIME,
                        (TransactionContext) {.ifelapsed = VIFELAPSED,
                                              .expireafter = VEXPIREAFTER},
                        &pp, false);
                        
    return (PackagePromiseGlobalLock) {.g_lock = package_promise_global_lock,
                                       .lock_ctx = ctx};
}

void YieldGlobalPackagePromiseLock(PackagePromiseGlobalLock lock)
{
    Bundle bundle = {.name = "package_global"};
    PromiseType promise_type = {.name = "package_global",
                               .parent_bundle = &bundle};
    Promise pp = {.promiser = "package_global",
                  .parent_promise_type = &promise_type};
    
    YieldCurrentLockAndRemoveFromCache(lock.lock_ctx, lock.g_lock,
                                       GLOBAL_PACKAGE_PROMISE_LOCK_NAME, &pp);
}

static void ParseAndLogErrorMessage(const Rlist *data)
{
    for (const Rlist *rp = data; rp != NULL; rp = rp->next)
    {
        char *line = RlistScalarValue(rp);
                   
        if (StringStartsWith(line, "Error="))
        {
            Log(LOG_LEVEL_VERBOSE, "Caught error: %s", line + strlen("Error="));
        }
        else if (StringStartsWith(line, "ErrorMessage="))
        {
            Log(LOG_LEVEL_VERBOSE, "Caught error message: %s", 
                line + strlen("ErrorMessage="));
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Caught unsupported error info: %s", line);
        }
    }
}

static int IsReadWriteReady(const IOData *io, int timeout_sec)
{
    fd_set  rset;
    FD_ZERO(&rset);
    FD_SET(io->read_fd, &rset);

    struct timeval tv = {
        .tv_sec = timeout_sec,
        .tv_usec = 0,
    };

    int ret = select(io->read_fd + 1, &rset, NULL, NULL, &tv);

    if(ret < 0)
    {
        Log(LOG_LEVEL_VERBOSE, "Failed checking for data. (select: %s)",
            GetErrorStr());
        return -1;
    }

    else if (FD_ISSET(io->read_fd, &rset))
    {
        return io->read_fd;
    }

    /* We have reached timeout */
    if(ret == 0)
    {
        Log(LOG_LEVEL_DEBUG, "Timeout reading from package module.");
        return 0;
    }

    Log(LOG_LEVEL_VERBOSE,
        "Unknown outcome (ret > 0 but our only fds is not set). (select: %s)",
        GetErrorStr());

    return -1;
}

static Rlist *ReadDataFromPackageScript(const IOData *io)
{
    char buff[CF_BUFSIZE] = {0};
    int red_data_size = 0;
    
    Buffer *data = BufferNew();
    if (!data)
    {
        Log(LOG_LEVEL_VERBOSE, 
            "Unable to allocate buffer for handling package module responses.");
        return NULL;
    }
    
    int timeout_seconds_left = PACKAGE_PROMISE_SCRIPT_TIMEOUT_SEC;
    
    while(!IsPendingTermination() && timeout_seconds_left > 0)
    {
        int fd = IsReadWriteReady(io, PACKAGE_PROMISE_TERMINATION_CHECK_SEC);
        
        if (fd < 0)
        {
            Log(LOG_LEVEL_VERBOSE, 
                "Error reading data from package module: %s",
                GetErrorStr());
            return NULL;
        }
        else if (fd == io->read_fd)
        {
            ssize_t res = read(fd, buff, sizeof(buff) - 1);
            if (res == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE,
                        "Unable to read output from package module: %s",
                        GetErrorStr());
                    BufferDestroy(data);
                    return NULL;
                }
            }
            else if (res == 0) /* reached EOF */
            {
                break;
            }
            Log(LOG_LEVEL_DEBUG, "Data red from package module: %zu [%s]",
                res, buff);
            red_data_size += res;

            BufferAppendString(data, buff);
            memset(buff, 0, sizeof(buff));
        }
        else if (fd == 0) /* timeout */
        {
            timeout_seconds_left -= PACKAGE_PROMISE_TERMINATION_CHECK_SEC;
            continue;
        }
    }
    
    char *red_string = BufferClose(data);
    Rlist *response_lines = RlistFromSplitString(red_string, '\n');
    free(red_string);
    
    return response_lines;
}

static int WriteScriptData(const char *data, IOData *io)
{
    /* If there is nothing to write close writing end of pipe. */
    if (data == NULL || strlen(data) == 0)
    {
        if (io->write_fd >= 0)
        {
            close(io->write_fd);
            io->write_fd = -1;
        }
        return 0;
    }
    
    ssize_t wrt = write(io->write_fd, data, strlen(data));
    
    /* Make sure to close write_fd after sending all data. */
    if (io->write_fd >= 0)
    {
        close(io->write_fd);
        io->write_fd = -1;
    }
    return wrt;
}

int WriteDataToPackageModule(const char *args, const char *data,
                             const PackageModuleWrapper *wrapper)
{
    char *command = StringFormat("%s %s", wrapper->path, args);
    IOData io = cf_popen_full_duplex(command, false);
    free(command);
    
    if (io.write_fd == 0 || io.read_fd == 0)
    {
        Log(LOG_LEVEL_VERBOSE, "Error occurred while opening pipes for "
            "communication with package module.");
        return -1;
    }
    
    int res = 0;
    if (WriteScriptData(data, &io) != strlen(data))
    {
        Log(LOG_LEVEL_VERBOSE,
            "Was not able to send whole data to package module.");
        res = -1;
    }
    
    /* If script returns non 0 status */
    if (cf_pclose_full_duplex(&io) != EXIT_SUCCESS)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Package module returned with non zero return code.");
        res = -1;
    }
    return res;
}

/* In some cases the response is expected to be not filled out. Some requests
   will have response filled only in case of errors. */
static int ReadWriteDataToPackageScript(const char *args, const char *request,
                                        Rlist **response,
                                        const PackageModuleWrapper *wrapper)
{
    assert(args && wrapper);
    
    char *command = StringFormat("%s %s", wrapper->path, args);
    IOData io = cf_popen_full_duplex(command, false);
    free(command);
    
    if (io.write_fd == 0 || io.read_fd == 0)
    {
        Log(LOG_LEVEL_INFO, "Some error occurred while communicating "
            "package module.");
        return -1;
    }
    
    if (WriteScriptData(request, &io) != strlen(request))
    {
        Log(LOG_LEVEL_VERBOSE, "Couldn't send whole data to package module.");
        return -1;
    }
    
    /* We can have some error message here. */
    Rlist *res = ReadDataFromPackageScript(&io);
    
    /* If script returns non 0 status */
    if (cf_pclose_full_duplex(&io) != EXIT_SUCCESS)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Package module returned with non zero return code.");
        RlistDestroy(res);
        return -1;
    }
    
    *response = res;
    return 0;
}

static void FreePackageInfo(PackageInfo *package_info)
{
    if (package_info)
    {
        free(package_info->arch);
        free(package_info->name);
        free(package_info->version);

        free(package_info);
    }
}

static char *ParseOptions(Rlist *options)
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

static PackageInfo *ParseAndCheckPackageDataReply(const Rlist *data)
{
    PackageInfo * package_data = xcalloc(1, sizeof(PackageInfo));
    
    for (const Rlist *rp = data; rp != NULL; rp = rp->next)
    {
        char *line = RlistScalarValue(rp);
                   
        if (StringStartsWith(line, "PackageType="))
        {
            char *type = line + strlen("PackageType=");
            if (StringSafeEqual(type, "file"))
            {
                package_data->type = PACKAGE_TYPE_FILE;
            }
            else if (StringSafeEqual(type, "repo"))
            {
                package_data->type = PACKAGE_TYPE_REPO;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "unsupported package type: %s", type);
                free(package_data);
                return NULL;
            }
        }
        else if (StringStartsWith(line, "Name="))
        {
            if (package_data->name)
            {
                /* Some error occurred as we already have name for 
                 * given package. */
                Log(LOG_LEVEL_VERBOSE, "duplicated package name "
                    "received: [%s] %s", line, package_data->name);
                free(package_data);
                return NULL;
            }
            package_data->name = 
                SafeStringDuplicate(line + strlen("Name="));
        }
        else if (StringStartsWith(line, "Version="))
        {
            if (package_data->version)
            {
                /* Some error occurred as we already have version for 
                 * given package. */
                Log(LOG_LEVEL_VERBOSE, "duplicated package version "
                    "received: [%s] %s", line, package_data->version);
                free(package_data);
                return NULL;
            }
            package_data->version = 
                SafeStringDuplicate(line + strlen("Version="));
        }
        else if (StringStartsWith(line, "Architecture="))
        {
            if (package_data->arch)
            {
                /* Some error occurred as we already have arch for 
                 * given package. */
               Log(LOG_LEVEL_VERBOSE, "duplicated package architecture "
                    "received: [%s] %s", line, package_data->arch);
                free(package_data);
                return NULL;
            }
            package_data->arch = 
                SafeStringDuplicate(line + strlen("Architecture="));
        }
        /* For handling errors */
        else if (StringStartsWith(line, "Error="))
        {
            Log(LOG_LEVEL_VERBOSE, "have error: %s",
                line + strlen("Error="));
        }
        else if (StringStartsWith(line, "ErrorMessage="))
        {
            Log(LOG_LEVEL_VERBOSE, "have error message: %s",
                line + strlen("ErrorMessage="));
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "unsupported option: %s", line);
        }
    }
    
    return package_data;
}

static int NegotiateSupportedAPIVersion(PackageModuleWrapper *wrapper)
{
    int api_version = -1;

    Rlist *response = NULL;
    if (ReadWriteDataToPackageScript("supports-api-version", "",
                                     &response, wrapper) != 0)
    {
        Log(LOG_LEVEL_INFO, "Some error occurred while communicating with "
            "wrapper while getting supported API version.");
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

/* IMPORTANT: this might not return all the data we need like version
              or architecture but package name MUST be known. */
static
PackageInfo *GetPackageData(const char *name, Rlist *options,
                            const PackageModuleWrapper *wrapper)
{   
    char *options_str = ParseOptions(options);
    char *request = StringFormat("%sFile=%s\n", options_str, name);
    
    Rlist *response = NULL;
    if (ReadWriteDataToPackageScript("get-package-data", request, &response,
            wrapper) != 0)
    {
        Log(LOG_LEVEL_INFO, "Some error occurred while communicating with "
            "package module while collecting package data.");
        free(options_str);
        free(request);
        return NULL;
    }
    
    PackageInfo *package_data = NULL;
    
    if (response)
    {   
        package_data = ParseAndCheckPackageDataReply(response);
        RlistDestroy(response);
        
        if (package_data)
        {
            /* At this point at least package name MUST be known (if no error) */
            if (!package_data->name || package_data->type == PACKAGE_TYPE_NONE)
            {
                Log(LOG_LEVEL_INFO, "Unknown package name.");
                FreePackageInfo(package_data);
                package_data = NULL;
            }
        }
    }
    free(options_str);
    free(request);
        
    return package_data;
}

static char *GetPackageModuleRealPath(const char *package_module_name)
{
    
    return StringFormat("%s%c%s%c%s", GetWorkDir(), FILE_SEPARATOR,
                        "package_modules", FILE_SEPARATOR,
                        package_module_name);
}

static int IsPackageInCache(EvalContext *ctx,
                            const PackageModuleWrapper *module_wrapper,
                            const char *name, const char *ver, const char *arch)
{
    const char *version = ver;
    /* Handle latest version in specific way for repo packages. 
     * Please note that for file packages 'latest' version is not supported
     * and check against that is made elsewhere. */
    if (version && StringSafeEqual(version, "latest"))
    {
        version = NULL;
    }
    
    /* Make sure cache is updated. */
    if (ctx)
    {
        if (!UpdateSinglePackageModuleCache(ctx, module_wrapper,
                                            UPDATE_TYPE_INSTALLED, false))
        {
            Log(LOG_LEVEL_INFO, "Can not update cache.");
        }
    }
    
    CF_DB *db_cached;
    if (!OpenSubDB(&db_cached, dbid_packages_installed,
                   module_wrapper->package_module->name))
    {
        Log(LOG_LEVEL_INFO, "Can not open cache database.");
        return -1;
    }
    
    char *key = NULL;
    if (version && arch)
    {
        key = StringFormat("N<%s>V<%s>A<%s>", name, version, arch);
    }
    else if (version)
    {
        key = StringFormat("N<%s>V<%s>", name, version);
    }
    else if (arch)
    {
        key = StringFormat("N<%s>A<%s>", name, arch);
    }
    else
    {
         key = StringFormat("N<%s>", name);
    }
    
    int is_in_cache = 0;
    char buff[1];
    
    Log(LOG_LEVEL_DEBUG, "looking for key in cache: %s", key);
    
    if (ReadDB(db_cached, key, buff, 1))
    {
        /* Just make sure DB is not corrupted. */
        if (buff[0] == '1')
        {
            is_in_cache = 1;
        }
        else
        {
            Log(LOG_LEVEL_INFO,
                "Seem to have corrupted data in cache database");
            is_in_cache = -1;
        }
    }
    
    Log(LOG_LEVEL_DEBUG,
        "Looking for package %s in cache returned: %d", name, is_in_cache);
    
    CloseDB(db_cached);
    
    return is_in_cache;
}

void WritePackageDataToDB(CF_DB *db_installed,
        const char *name, const char *ver, const char *arch,
        UpdateType type)
{
    char package_key[strlen(name) + strlen(ver) +
                     strlen(arch) + 11];
    
    xsnprintf(package_key, sizeof(package_key),
              "N<%s>", name);
    if ((type == UPDATE_TYPE_UPDATES || type == UPDATE_TYPE_LOCAL_UPDATES) && 
            HasKeyDB(db_installed, package_key, strlen(package_key) + 1))
    {
        size_t val_size =
                ValueSizeDB(db_installed, package_key, strlen(package_key));
        char buff[val_size + strlen(arch) + strlen(ver) + 8];
        
        ReadDB(db_installed, package_key, buff, val_size);
        xsnprintf(buff + val_size, sizeof(package_key), "V<%s>A<%s>\n", ver, arch);
        WriteDB(db_installed, package_key, buff, strlen(buff));
    }
    else if (type == UPDATE_TYPE_UPDATES || type == UPDATE_TYPE_LOCAL_UPDATES)
    {
        char buff[strlen(arch) + strlen(ver) + 8];
        xsnprintf(buff, sizeof(package_key), "V<%s>A<%s>\n", ver, arch);
        WriteDB(db_installed, package_key, buff, strlen(buff));
    }
    else /* UPDATE_TYPE_INSTALLED */
    {
        WriteDB(db_installed, package_key, "1", 1);
        xsnprintf(package_key, sizeof(package_key),
                "N<%s>V<%s>", name, ver);
        WriteDB(db_installed, package_key, "1", 1);
        xsnprintf(package_key, sizeof(package_key),
                "N<%s>A<%s>", name, arch);
        WriteDB(db_installed, package_key, "1", 1);
        xsnprintf(package_key, sizeof(package_key),
                "N<%s>V<%s>A<%s>", name, ver, arch);
        WriteDB(db_installed, package_key, "1", 1);
    }
}

int UpdatePackagesDB(Rlist *data, const char *pm_name, UpdateType type)
{
    assert(pm_name);

    CF_DB *db_cached;
    dbid db_id = type == UPDATE_TYPE_INSTALLED ? dbid_packages_installed :
                                                 dbid_packages_updates;
    if (OpenSubDB(&db_cached, db_id, pm_name))
    {
        CleanDB(db_cached);
        
        Buffer *inventory_data = BufferNewWithCapacity(INVENTORY_LIST_BUFFER_SIZE);

        char *package_data[3] = {NULL, NULL, NULL};

        for (const Rlist *rp = data; rp != NULL; rp = rp->next)
        {
            char *line = RlistScalarValue(rp);

            if (StringStartsWith(line, "Name="))
            {
                /* We have all the information we need from previous loop
                 * iteration. */
                if (package_data[0] && package_data[1] && package_data[2])
                {
                    WritePackageDataToDB(db_cached, package_data[0],
                                         package_data[1], package_data[2], type);
                    
                    char inventory_line[strlen(package_data[0]) +
                                        strlen(package_data[1]) +
                                        strlen(package_data[2]) + 4];
                    
                    xsnprintf(inventory_line, sizeof(inventory_line),
                              "%s,%s,%s\n", package_data[0], package_data[1],
                              package_data[2]);
                    BufferAppendString(inventory_data, inventory_line);

                    package_data[1] = NULL;
                    package_data[2] = NULL;
                }
                else if (package_data[0] && (!package_data[1] || !package_data[2]))
                {
                    /* some error occurred */
                    Log(LOG_LEVEL_VERBOSE, "Malformed response from package "
                        "manager for package %s", package_data[0]);
                    package_data[1] = NULL;
                    package_data[2] = NULL;
                }

                /* This must be the first entry on a list */
                package_data[0] = line + strlen("Name=");

            }
            else if (StringStartsWith(line, "Version="))
            {
                package_data[1] = line + strlen("Version=");
            }
            else if (StringStartsWith(line, "Architecture="))
            {
                package_data[2] = line + strlen("Architecture=");
            }
            else if (StringStartsWith(line, "Error="))
            {
                Log(LOG_LEVEL_VERBOSE, "have error: %s",
                    line + strlen("Error="));
            }
            else if (StringStartsWith(line, "ErrorMessage="))
            {
                Log(LOG_LEVEL_VERBOSE, "have error message: %s",
                    line + strlen("ErrorMessage="));
            }
        }
        /* We have one more entry left. */
        if (package_data[0] && package_data[1] && package_data[2])
        {
            WritePackageDataToDB(db_cached, package_data[0],
                             package_data[1], package_data[2], type);
            
            char inventory_line[strlen(package_data[0]) +
                                strlen(package_data[1]) +
                                strlen(package_data[2]) + 4];

            xsnprintf(inventory_line, sizeof(inventory_line),
                      "%s,%s,%s\n", package_data[0], package_data[1],
                      package_data[2]);
            BufferAppendString(inventory_data, inventory_line);
        }
        
        char *inventory_key = "<inventory>";
        char *inventory_list = BufferClose(inventory_data);
        
        /* We can have empty list of installed software or available updates. */
        if (!inventory_list)
        {
            WriteDB(db_cached, inventory_key, "\n", 1);
        }
        else
        {
            WriteDB(db_cached, inventory_key, inventory_list,
                    strlen(inventory_list));
            free(inventory_list);
        }
        
        CloseDB(db_cached);
        return 0;
    }
    /* Unable to open database. */
    return -1;
}


bool UpdateCache(Rlist* options, const PackageModuleWrapper *wrapper,
                 UpdateType type)
{
    char *options_str = ParseOptions(options);
    Rlist *response = NULL;
    
    const char *req_type = NULL;
    if (type == UPDATE_TYPE_INSTALLED)
    {
        req_type = "list-installed";
    }
    else if (type == UPDATE_TYPE_UPDATES)
    {
        req_type = "list-updates";
    }
    else if (type == UPDATE_TYPE_LOCAL_UPDATES)
    {
        req_type = "list-updates-local";
    }
    if (ReadWriteDataToPackageScript(req_type, options_str, &response,
            wrapper) != 0)
    {
        Log(LOG_LEVEL_VERBOSE, "Some error occurred while communicating with "
                "package module while updating cache.");
        free(options_str);
        return false;
    }
    
    if (!response)
    {
        Log(LOG_LEVEL_DEBUG,
            "Received empty packages list after requesting: %s", req_type);
    }
    
    /* We still need to update DB with empty data. */
    if (UpdatePackagesDB(response, wrapper->name, type) != 0)
    {
        Log(LOG_LEVEL_INFO, "Error parsing package cache.");
        free(options_str);
        return false;
    }
    
    RlistDestroy(response);
    free(options_str);
    return true;
}


PromiseResult ValidateChangedPackage(const NewPackages *policy_data,
                                     const PackageModuleWrapper *wrapper,
                                     const PackageInfo *package_info,
                                     NewPackageAction action_type)
{
    
    if (!UpdateCache(policy_data->package_options, wrapper,
                     UPDATE_TYPE_INSTALLED))
    {
        Log(LOG_LEVEL_INFO, "Can not update installed packages cache after "
            "package installation");
        return PROMISE_RESULT_FAIL;
    }
    
    if (!UpdateCache(policy_data->package_options, wrapper,
                     UPDATE_TYPE_LOCAL_UPDATES))
    {
        Log(LOG_LEVEL_INFO, "Can not update available updates cache after "
            "package installation");
        return PROMISE_RESULT_FAIL;
    }

    int is_in_cache = IsPackageInCache(NULL, wrapper, package_info->name,
                                       package_info->version,
                                       package_info->arch);
    if (is_in_cache == 1)
    {
        return action_type == NEW_PACKAGE_ACTION_PRESENT ? 
            PROMISE_RESULT_CHANGE : PROMISE_RESULT_FAIL;
    }
    else if (is_in_cache == 0)
    {
        return action_type == NEW_PACKAGE_ACTION_PRESENT ? 
            PROMISE_RESULT_FAIL : PROMISE_RESULT_CHANGE;
    }
    else
    {
        Log(LOG_LEVEL_INFO, "Some error occurred while reading installed "
            "packages cache.");
        return PROMISE_RESULT_FAIL;
    }
}

PromiseResult RemovePackage(const char *name, Rlist* options, 
                            const char *version, const char *architecture,
                            const PackageModuleWrapper *wrapper)
{
    Log(LOG_LEVEL_DEBUG, "Removing package '%s'", name);
             
    char *options_str = ParseOptions(options);
    char *ver = version ? 
        StringFormat("Version=%s\n", version) : NULL;
    char *arch = architecture ? 
        StringFormat("Architecture=%s\n", architecture) : NULL;
    char *request = StringFormat("%sName=%s\n%s%s",
            options_str, name, ver ? ver : "", arch ? arch : "");
    
    PromiseResult res = PROMISE_RESULT_CHANGE;
    
    Rlist *error_message = NULL;
    if (ReadWriteDataToPackageScript("remove", request, 
            &error_message, wrapper) != 0)
    {
        Log(LOG_LEVEL_INFO, "Some error occurred while communicating with "
            "package module while removing package.");
        res = PROMISE_RESULT_FAIL;
    }
    if (error_message)
    {
        ParseAndLogErrorMessage(error_message);
        res = PROMISE_RESULT_FAIL;
        RlistDestroy(error_message);
    }
    
    free(request);
    free(options_str);
    free(ver);
    free(arch);
    
    /* We assume that at this point package is removed correctly. */
    return res;    
}


static PromiseResult InstallPackageGeneric(Rlist *options, 
        PackageType type, const char *packages_list_formatted,
        const PackageModuleWrapper *wrapper)
{
    char *options_str = ParseOptions(options);
    char *request = StringFormat("%s%s", options_str, packages_list_formatted);
    
    PromiseResult res = PROMISE_RESULT_CHANGE;
    
    const char *package_install_command = NULL;
    if (type == PACKAGE_TYPE_FILE)
    {
        package_install_command = "file-install";
    }
    else if (type == PACKAGE_TYPE_REPO)
    {
        package_install_command = "repo-install";
    }
    else
    {
        /* If we end up here something bad has happened. */
        assert(0 && "unsupported package type");
    }
    
    Rlist *error_message = NULL;
    if (ReadWriteDataToPackageScript(package_install_command, request, 
                                     &error_message, wrapper) != 0)
    {
        Log(LOG_LEVEL_INFO, "Some error occurred while communicating with "
            "package module while installing package.");
        res = PROMISE_RESULT_FAIL;
    }
    if (error_message)
    {
        ParseAndLogErrorMessage(error_message);
        res = PROMISE_RESULT_FAIL;
        RlistDestroy(error_message);
    }
    
    free(request);
    free(options_str);
    
    return res;
}

PromiseResult InstallPackage(Rlist *options, 
        PackageType type, const char *package_to_install,
        const char *version, const char *architecture,
        const PackageModuleWrapper *wrapper)
{
    Log(LOG_LEVEL_DEBUG, "Installing package '%s'", package_to_install);
             
    char *ver = version ? 
        StringFormat("Version=%s\n", version) : NULL;
    char *arch = architecture ? 
        StringFormat("Architecture=%s\n", architecture) : NULL;
    char *request = NULL;
    
    PromiseResult res = PROMISE_RESULT_CHANGE;
    
    if (type == PACKAGE_TYPE_FILE)
    {
        request = StringFormat("File=%s\n%s%s",
            package_to_install, ver ? ver : "", arch ? arch : "");
    }
    else if (type == PACKAGE_TYPE_REPO)
    {
        request = StringFormat("Name=%s\n%s%s",
            package_to_install, ver ? ver : "", arch ? arch : "");
    }
    
    res = InstallPackageGeneric(options, type, request, wrapper);
    
    free(request);
    free(ver);
    free(arch);
    
    return res;
}

PromiseResult FileInstallPackage(const char *package_file_path, 
                                 const PackageInfo *info,
                                 const NewPackages *policy_data,
                                 const PackageModuleWrapper *wrapper,
                                 int is_in_cache, enum cfopaction action)
{
    Log(LOG_LEVEL_DEBUG, "Installing file type package.");
    
    /* We have some packages matching file package promise in cache. */
    if (is_in_cache == 1)
    {
        Log(LOG_LEVEL_VERBOSE, "Package exists in cache. Skipping installation.");
        return PROMISE_RESULT_NOOP;
    }
    
    PromiseResult res;
     
    if (action == cfa_warn || DONTDO)
    {
         Log(LOG_LEVEL_NOTICE, "Should install file type package: %s",
             package_file_path);
        res = PROMISE_RESULT_FAIL;
    }
    else
    {
        res = InstallPackage(policy_data->package_options,
                             PACKAGE_TYPE_FILE, package_file_path,
                             NULL, NULL, wrapper);
        if (res == PROMISE_RESULT_CHANGE)
        {
            Log(LOG_LEVEL_DEBUG, "Validating package: %s", package_file_path);
            return ValidateChangedPackage(policy_data, wrapper, info,
                                          NEW_PACKAGE_ACTION_PRESENT);
        }
    }
    return res;
}


Seq *GetVersionsFromUpdates(EvalContext *ctx, const PackageInfo *info,
                            const PackageModuleWrapper *module_wrapper)
{   
    CF_DB *db_updates;
    dbid db_id = dbid_packages_updates;
    Seq *updates_list = NULL;
    
    /* Make sure cache is updated. */
    if (ctx)
    {
        if (!UpdateSinglePackageModuleCache(ctx, module_wrapper,
                                            UPDATE_TYPE_UPDATES, false))
        {
            Log(LOG_LEVEL_INFO, "Can not update packages cache.");
        }
    }
    
    if (OpenSubDB(&db_updates, db_id, module_wrapper->package_module->name))
    {
        char package_key[strlen(info->name) + 4];

        xsnprintf(package_key, sizeof(package_key),
                "N<%s>", info->name);
        
        Log(LOG_LEVEL_DEBUG, "Looking for key in updates: %s", package_key);
         
        if (HasKeyDB(db_updates, package_key, sizeof(package_key)))
        {
            Log(LOG_LEVEL_DEBUG, "Found key in updates database");
            
            updates_list = SeqNew(3, FreePackageInfo);
            size_t val_size =
                    ValueSizeDB(db_updates, package_key, sizeof(package_key));
            char buff[val_size + 1];
            buff[val_size] = '\0';

            ReadDB(db_updates, package_key, buff, val_size);
            Seq* updates = SeqStringFromString(buff, '\n');
            
            for (int i = 0; i < SeqLength(updates); i++)
            {
                PackageInfo *package = calloc(1, sizeof(PackageInfo));

                char *package_line = SeqAt(updates, i);
                char version[strlen(package_line)];
                char arch[strlen(package_line)];

                if (sscanf(package_line, "V<%[^>]>A<%[^>]>", version, arch) == 2)
                {
                    package->version = SafeStringDuplicate(version);
                    package->arch = SafeStringDuplicate(arch);
                    SeqAppend(updates_list, package);
                }
                else
                {
                    Log(LOG_LEVEL_INFO, "Not able to parse available updates "
                        "line: %s", package_line);
                    /* Some error occurred while scanning package updates. */
                    FreePackageInfo(package);
                }
            }
        }
        CloseDB(db_updates);
    }
    return updates_list;
}

PromiseResult RepoInstall(EvalContext *ctx,
                          PackageInfo *package_info,
                          const NewPackages *policy_data,
                          const PackageModuleWrapper *wrapper,
                          int is_in_cache,
                          enum cfopaction action,
                          bool *verified)
{
    Log(LOG_LEVEL_DEBUG, "Installing repo type package: %d", is_in_cache);
    
    /* Package is not present in cache. */
    if (is_in_cache == 0)
    {
        const char *version = package_info->version;
        if (package_info->version &&
                StringSafeEqual(package_info->version, "latest"))
        {
            version = NULL;
        }
        if (action == cfa_warn || DONTDO)
        {
            Log(LOG_LEVEL_NOTICE, "Should install repo type package: %s",
                package_info->name);
            return PROMISE_RESULT_FAIL;
        }
        
        *verified = false; /* Verification will be done elsewhere. */
        return InstallPackage(policy_data->package_options, PACKAGE_TYPE_REPO,
                              package_info->name, version, package_info->arch,
                              wrapper);
    }
    
    
    /* We have some packages matching already installed at this point. */
    
    
    /* We have 'latest' version in policy. */
    if (package_info->version &&
                StringSafeEqual(package_info->version, "latest"))
    {
        /* This can return more than one latest version if we have packages
         * with different architectures installed. */
        Seq *latest_versions = 
                GetVersionsFromUpdates(ctx, package_info, wrapper);
        if (!latest_versions)
        {
            Log(LOG_LEVEL_VERBOSE, "Package '%s' is already in the latest "
                "version. Skipping instalation.", package_info->name);

            return PROMISE_RESULT_NOOP;
        }
        
        PromiseResult res = PROMISE_RESULT_NOOP;
        Buffer *install_buffer = BufferNew();
        Seq *packages_to_install = SeqNew(1, NULL);

        /* Loop through possible updates. */
        for (int i = 0; i < SeqLength(latest_versions); i++)
        {
            PackageInfo *update_package = SeqAt(latest_versions, i);
            
            Log(LOG_LEVEL_DEBUG, "Checking for package '%s' version '%s' in "
                "available updates", package_info->name,
                update_package->version);
            
            /* Just in case some package managers will report highest possible 
             * version in updates list instead of removing entry if package is 
             * already in the latest version. */
            if (IsPackageInCache(ctx, wrapper, package_info->name,
                                 update_package->version,
                                 update_package->arch) == 1)
            {
                Log(LOG_LEVEL_VERBOSE, "Package version from updates matches "
                    "one installed. Skipping package instalation.");
                res = PromiseResultUpdate(res, PROMISE_RESULT_NOOP);
                continue;
            }
            else
            {
                if (action == cfa_warn || DONTDO)
                {
                    Log(LOG_LEVEL_NOTICE, "Should install repo type package: %s",
                        package_info->name);
                    res = PromiseResultUpdate(res, PROMISE_RESULT_FAIL);
                    continue;
                }
                
                /* Append package data to buffer. At the end all packages 
                 * data that need to be updated will be sent to package module
                 * at once. This is important if we have package in more than 
                 * one architecture. If we would update one after another we 
                 * may end up with the ones doesn't matching default 
                 * architecture being removed. */
                BufferAppendF(install_buffer, 
                              "Name=%s\nVersion=%s\nArchitecture=%s\n",
                              package_info->name,
                              update_package->version,
                              update_package->arch);
                
                /* Here we are adding latest_versions elements to different 
                 * seq. Make sure to not free those and not free latest_versions
                 * before we are done with packages_to_install. 
                 * This is needed for later verification if package was 
                 * installed correctly. */
                SeqAppend(packages_to_install, update_package);
            }
        }
        
        char *install_formated_list = BufferClose(install_buffer);
        
        if (install_formated_list)
        {
            /* If we have some packages to install. */
            if (strlen(install_formated_list) > 0)
            {
                Log(LOG_LEVEL_DEBUG, "Formatted list of packages to be send to "
                    "package module: %s", install_formated_list);
                res = InstallPackageGeneric(policy_data->package_options,
                                      PACKAGE_TYPE_REPO,
                                      install_formated_list, wrapper);
                
                for (int i = 0; i < SeqLength(packages_to_install); i++)
                {
                    PackageInfo *to_verify = SeqAt(packages_to_install, i);
                    PromiseResult validate = 
                            ValidateChangedPackage(policy_data, wrapper,
                                                   to_verify,
                                                   NEW_PACKAGE_ACTION_PRESENT);
                    Log(LOG_LEVEL_DEBUG, "Validating package %s:%s:%s "
                        "installation result: %d", to_verify->name,
                            to_verify->version, to_verify->arch, validate);
                    res = PromiseResultUpdate(res, validate);
                    *verified = true;
                }
            }
            free(install_formated_list);
        }
        
        SeqDestroy(packages_to_install);
        SeqDestroy(latest_versions);
        return res;
    }
    /* No version or explicit version specified. */
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Package '%s' already installed",
            package_info->name);
            
        return PROMISE_RESULT_NOOP;
    }
    /* Just to keep compiler happy; we shouldn't reach this point. */
    return PROMISE_RESULT_FAIL;
}

PromiseResult RepoInstallPackage(EvalContext *ctx, 
                                 PackageInfo *package_info,
                                 const NewPackages *policy_data,
                                 const PackageModuleWrapper *wrapper,
                                 int is_in_cache,
                                 enum cfopaction action)
{
    bool verified = false;
    PromiseResult res = RepoInstall(ctx, package_info, policy_data, wrapper,
                                    is_in_cache, action, &verified);
    
    if (res == PROMISE_RESULT_CHANGE && !verified)
    {
        return ValidateChangedPackage(policy_data, wrapper, package_info,
                                      NEW_PACKAGE_ACTION_PRESENT);
    }
    return res;
}

static bool CheckPolicyAndPackageInfoMatch(const NewPackages *packages_policy,
                                           const PackageInfo *info)
{
    /* Check if file we are having matches what we want in policy. */
    if (info->arch && packages_policy->package_architecture && 
            !StringSafeEqual(info->arch, packages_policy->package_architecture))
    {
        Log(LOG_LEVEL_VERBOSE, 
            "package arch and one specified in policy doesn't match: %s -> %s",
            info->arch, packages_policy->package_architecture);
        return false;
    }
    if (info->version)
    {
        if (StringSafeEqual(packages_policy->package_version, "latest"))
        {
            Log(LOG_LEVEL_VERBOSE, "unsupported 'latest' version for package "
                    "promise of type file.");
            return false;
        }
        if (packages_policy->package_version && 
            !StringSafeEqual(info->version, packages_policy->package_version))
        {
            Log(LOG_LEVEL_VERBOSE,
                "package version and one specified in policy doesn't "
                "match: %s -> %s",
                info->version, packages_policy->package_version);
            return false;
        }
    }
    return true;
}

PromiseResult HandlePresentPromiseAction(EvalContext *ctx, 
                                         const char *package_name,
                                         const NewPackages *policy_data,
                                         const PackageModuleWrapper *wrapper,
                                         enum cfopaction action)
{
    Log(LOG_LEVEL_DEBUG, "Starting evaluating present action promise.");
    
    /* Figure out what kind of package we are having. */
    PackageInfo *package_info = GetPackageData(package_name,
                                               policy_data->package_options,
                                               wrapper);
    
    PromiseResult result = PROMISE_RESULT_FAIL;
    if (package_info)
    {
        /* Check if data in policy matches returned by wrapper (files only). */
        if (package_info->type == PACKAGE_TYPE_FILE)
        {
            if (!CheckPolicyAndPackageInfoMatch(policy_data, package_info))
            {
                Log(LOG_LEVEL_ERR, "package data and policy doesn't match");
                FreePackageInfo(package_info);
                return PROMISE_RESULT_FAIL;
            }
        }
        
        /* Fill missing data in package_info from policy. This will allow
         * to match cache against all known package details we are 
         * interested in */
        if (!package_info->arch && policy_data->package_architecture)
        {
            package_info->arch =
                    SafeStringDuplicate(policy_data->package_architecture);
        }
        if (!package_info->version && policy_data->package_version)
        {
            package_info->version =
                    SafeStringDuplicate(policy_data->package_version);
        }
        
        /* Check if package exists in cache */
        int is_in_cache = IsPackageInCache(ctx, wrapper,
                                           package_info->name,
                                           package_info->version,
                                           package_info->arch);
        
        if (is_in_cache == -1)
        {
            Log(LOG_LEVEL_INFO, "Some error occurred while looking for package "
                    "'%s' in cache.", package_name);
            return PROMISE_RESULT_FAIL;
        }
        
        switch (package_info->type)
        {
            case PACKAGE_TYPE_FILE:
                result = FileInstallPackage(package_name, package_info,
                                            policy_data,
                                            wrapper,
                                            is_in_cache,
                                            action);
                break;
            case PACKAGE_TYPE_REPO:
                result = RepoInstallPackage(ctx, package_info, policy_data,
                                            wrapper,
                                            is_in_cache, action);
                break;
            default:
                /* We shouldn't end up here. If we are having unsupported 
                 package type this should be detected and handled
                 in ParseAndCheckPackageDataReply(). */
                assert(0 && "unsupported package type");
        }
        
        FreePackageInfo(package_info);
    }
    else
    {
        Log(LOG_LEVEL_INFO, "Can not obtain package data for promise: %s",
            package_name);
    }
        
    
    Log(LOG_LEVEL_DEBUG, "Evaluating present action promise status: %c", result);
    return result;
}


PromiseResult HandleAbsentPromiseAction(EvalContext *ctx,
                                        char *package_name,
                                        const NewPackages *policy_data, 
                                        const PackageModuleWrapper *wrapper,
                                        enum cfopaction action)
{
    /* Check if we are not having 'latest' version. */
    if (policy_data->package_version &&
            StringSafeEqual(policy_data->package_version, "latest"))
    {
        Log(LOG_LEVEL_ERR, "Package version 'latest' not supported for"
                "absent package promise");
        return PROMISE_RESULT_FAIL;
    }
    
    /* Check if package exists in cache */
    int is_in_cache = IsPackageInCache(ctx, wrapper, package_name,
                                       policy_data->package_version,
                                       policy_data->package_architecture);
    if (is_in_cache == 1)
    {
        /* Remove package(s) */
        PromiseResult res;
        
        if (action == cfa_warn || DONTDO)
        {
            Log(LOG_LEVEL_NOTICE, "Need to remove package: %s", package_name);
            res = PROMISE_RESULT_FAIL;
        }
        else
        {
            res = RemovePackage(package_name,
                    policy_data->package_options, policy_data->package_version,
                    policy_data->package_architecture, wrapper);

            if (res == PROMISE_RESULT_CHANGE)
            {
                /* Check if package was removed. */
                return ValidateChangedPackage(policy_data, wrapper, 
                        &((PackageInfo){.name = package_name, 
                                        .version = policy_data->package_version, 
                                        .arch = policy_data->package_architecture}),
                        NEW_PACKAGE_ACTION_ABSENT);
            }
        }
        
        return res;
    }
    else if (is_in_cache == -1)
    {
        Log(LOG_LEVEL_INFO, "Error occurred while checking package '%s' "
            "existence in cache.", package_name);
        return PROMISE_RESULT_FAIL;
    }
    else
    {
        /* Package is not in cache which means it is already removed. */
        Log(LOG_LEVEL_DEBUG, "Package '%s' not installed. Skipping removing.",
                package_name);
        return PROMISE_RESULT_NOOP;
    }
}


/* IMPORTANT: This must be called under protection of 
 * GLOBAL_PACKAGE_PROMISE_LOCK_NAME lock! */
bool UpdateSinglePackageModuleCache(EvalContext *ctx,
                                    const PackageModuleWrapper *module_wrapper,
                                    UpdateType type, bool force_update)
{
    assert(module_wrapper->package_module->name);
    
    if (!force_update)
    {
        if (module_wrapper->package_module->installed_ifelapsed == CF_NOINT ||
            module_wrapper->package_module->updates_ifelapsed == CF_NOINT)
        {
            Log(LOG_LEVEL_ERR, "Package module body constraints error: %s %d %d",
                module_wrapper->package_module->name, 
                module_wrapper->package_module->installed_ifelapsed,
                module_wrapper->package_module->updates_ifelapsed);
            return false;
        }
    }
    
    Bundle bundle = {.name = "package_cache"};
    PromiseType promie_type = {.name = "package_cache",
                               .parent_bundle = &bundle};
    Promise pp = {.promiser = "package_cache",
                  .parent_promise_type = &promie_type};

    CfLock cache_updates_lock;
    char cache_updates_lock_name[CF_BUFSIZE];
    int ifelapsed_time = -1;

    dbid dbid_val;

    if (type == UPDATE_TYPE_INSTALLED)
    {
        dbid_val = dbid_packages_installed;
        snprintf(cache_updates_lock_name, CF_BUFSIZE - 1,
                 "package-cache-installed-%s", module_wrapper->package_module->name);
        ifelapsed_time = module_wrapper->package_module->installed_ifelapsed;
    }
    else
    {
        dbid_val = dbid_packages_updates;
        snprintf(cache_updates_lock_name, CF_BUFSIZE - 1,
                "package-cache-updates-%s", module_wrapper->package_module->name);
        ifelapsed_time = module_wrapper->package_module->updates_ifelapsed;
    }

    char *db_name = DBIdToSubPath(dbid_val, module_wrapper->name);
    struct stat statbuf;
    if (!force_update && stat(db_name, &statbuf) == -1 && errno == ENOENT)
    {
        /* Force update if database file doesn't exist. Not strictly necessary
         * with the locks we have, but good to have for tests that delete the
         * database. */
        Log(LOG_LEVEL_VERBOSE, "Forcing package list update due to "
            "missing database");
        force_update = true;
    }
    free(db_name);

    if (!force_update)
    {
        cache_updates_lock =
            AcquireLock(ctx, cache_updates_lock_name, VUQNAME, CFSTARTTIME,
                (TransactionContext) 
                {.ifelapsed = ifelapsed_time,
                 .expireafter = VEXPIREAFTER},
                &pp, false);
    }

    bool ret = true;
    
    if (force_update || cache_updates_lock.lock != NULL)
    {
        
        /* Update available updates cache. */
        if (!UpdateCache(module_wrapper->package_module->options, module_wrapper, type))
        {
            Log(LOG_LEVEL_INFO,
                "Some error occurred while updating available updates cache.");
            ret = false;
        }
        if (!force_update)
        {
            YieldCurrentLock(cache_updates_lock);
        }
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Skipping %s package cache update.",
            type == UPDATE_TYPE_INSTALLED ? 
                    "installed packages" : "available updates");
    }
    return ret;
}
