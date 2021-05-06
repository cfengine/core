/*
  Copyright 2021 Northern.tech AS

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
#include <unistd.h>
#include <json.h>
#include <string_lib.h>
#include <known_dirs.h>         /* GetDataDir() */
#include <var_expressions.h>    /* VarRef, StringContainsUnresolved() */
#include <eval_context.h>       /* EvalContext*() */
#include <files_names.h>        /* JoinPaths() */

#include <cmdb.h>

#define HOST_SPECIFIC_DATA_FILE "host_specific.json"
#define HOST_SPECIFIC_DATA_MAX_SIZE (5 * 1024 * 1024) /* maximum size of the host-specific.json file */

JsonElement *ReadJsonFile(const char *filename, LogLevel log_level, size_t size_max)
{
    assert(filename != NULL);

    JsonElement *doc = NULL;
    JsonParseError err = JsonParseFile(filename, size_max, &doc);

    if (err == JSON_PARSE_ERROR_NO_SUCH_FILE)
    {
        Log(log_level, "Could not open JSON file %s", filename);
        return NULL;
    }

    if (err != JSON_PARSE_OK ||
        doc == NULL)
    {
        Log(log_level, "Could not parse JSON file %s: %s", filename, JsonParseErrorToString(err));
    }

    return doc;
}

static bool CheckPrimitiveForUnexpandedVars(JsonElement *primitive, ARG_UNUSED void *data)
{
    assert(JsonGetElementType(primitive) == JSON_ELEMENT_TYPE_PRIMITIVE);

    /* Stop the iteration if a variable expression is found. */
    return (!StringContainsUnresolved(JsonPrimitiveGetAsString(primitive)));
}

static bool CheckObjectForUnexpandedVars(JsonElement *object, ARG_UNUSED void *data)
{
    assert(JsonGetType(object) == JSON_TYPE_OBJECT);

    /* Stop the iteration if a variable expression is found among children
     * keys. (elements inside the object are checked separately) */
    JsonIterator iter = JsonIteratorInit(object);
    while (JsonIteratorHasMore(&iter))
    {
        const char *key = JsonIteratorNextKey(&iter);
        if (StringContainsUnresolved(key))
        {
            return false;
        }
    }
    return true;
}

static bool ReadCMDBVars(EvalContext *ctx, JsonElement *vars)
{
    assert(vars != NULL);

    if (JsonGetType(vars) != JSON_TYPE_OBJECT)
    {
        Log(LOG_LEVEL_ERR, "Invalid 'vars' CMDB data, must be a JSON object");
        return false;
    }

    if (!JsonWalk(vars, CheckObjectForUnexpandedVars, NULL, CheckPrimitiveForUnexpandedVars, NULL))
    {
        Log(LOG_LEVEL_ERR, "Invalid 'vars' CMDB data, cannot contain variable references");
        return false;
    }

    JsonIterator iter = JsonIteratorInit(vars);
    while (JsonIteratorHasMore(&iter))
    {
        const char *key = JsonIteratorNextKey(&iter);
        JsonElement *data = JsonObjectGet(vars, key);

        VarRef *ref = VarRefParse(key);
        if (ref->ns == NULL)
        {
            ref->ns = xstrdup("cmdb");
        }
        else
        {
            if (ref->scope == NULL)
            {
                Log(LOG_LEVEL_ERR, "Invalid variable specification in CMDB data: '%s'"
                    " (bundle name has to be specified if namespace is specified)", key);
                VarRefDestroy(ref);
                continue;
            }
        }

        if (ref->scope == NULL)
        {
            ref->scope = xstrdup("variables");
        }

        if (JsonGetElementType(data) == JSON_ELEMENT_TYPE_PRIMITIVE)
        {
            char *value = JsonPrimitiveToString(data);
            Log(LOG_LEVEL_VERBOSE, "Installing CMDB variable '%s:%s.%s=%s'",
                ref->ns, ref->scope, key, value);
            EvalContextVariablePut(ctx, ref, value, CF_DATA_TYPE_STRING, "source=cmdb");
            free(value);
        }
        else if ((JsonGetType(data) == JSON_TYPE_ARRAY) &&
                 JsonArrayContainsOnlyPrimitives(data))
        {
            // map to slist if the data only has primitives
            Log(LOG_LEVEL_VERBOSE, "Installing CMDB slist variable '%s:%s.%s'",
                ref->ns, ref->scope, key);
            Rlist *data_rlist = RlistFromContainer(data);
            EvalContextVariablePut(ctx, ref, data_rlist, CF_DATA_TYPE_STRING_LIST, "source=cmdb");
            RlistDestroy(data_rlist);
        }
        else // install as a data container
        {
            Log(LOG_LEVEL_VERBOSE, "Installing CMDB data container variable '%s:%s.%s'",
                ref->ns, ref->scope, key);
            EvalContextVariablePut(ctx, ref, data, CF_DATA_TYPE_CONTAINER, "source=cmdb");
        }
        VarRefDestroy(ref);
    }
    return true;
}

static bool ReadCMDBClasses(EvalContext *ctx, JsonElement *classes)
{
    assert(classes != NULL);

    if (JsonGetType(classes) != JSON_TYPE_OBJECT)
    {
        Log(LOG_LEVEL_ERR, "Invalid 'classes' CMDB data, must be a JSON object");
        return false;
    }

    if (!JsonWalk(classes, CheckObjectForUnexpandedVars, NULL, CheckPrimitiveForUnexpandedVars, NULL))
    {
        Log(LOG_LEVEL_ERR, "Invalid 'classes' CMDB data, cannot contain variable references");
        return false;
    }

    JsonIterator iter = JsonIteratorInit(classes);
    while (JsonIteratorHasMore(&iter))
    {
        const char *key = JsonIteratorNextKey(&iter);
        JsonElement *data = JsonObjectGet(classes, key);
        if (JsonGetElementType(data) == JSON_ELEMENT_TYPE_PRIMITIVE)
        {
            const char *expr = JsonPrimitiveGetAsString(data);
            if (!StringEqual(expr, "any::"))
            {
                Log(LOG_LEVEL_ERR,
                    "Invalid class specification '%s' in CMDB data, only \"any::\" allowed", expr);
                continue;
            }
            Log(LOG_LEVEL_VERBOSE, "Installing CMDB class '%s'", key);
            if (strchr(key, ':') != NULL)
            {
                char *ns_class_name = xstrdup(key);
                char *sep = strchr(ns_class_name, ':');
                *sep = '\0';
                key = sep + 1;
                EvalContextClassPutSoftNS(ctx, ns_class_name, key, CONTEXT_SCOPE_NAMESPACE, "source=cmdb");
                free(ns_class_name);
            }
            else
            {
                EvalContextClassPutSoftNS(ctx, "cmdb", key, CONTEXT_SCOPE_NAMESPACE, "source=cmdb");
            }
        }
        else if (JsonGetContainerType(data) == JSON_CONTAINER_TYPE_ARRAY &&
                 JsonArrayContainsOnlyPrimitives(data))
        {
            if ((JsonLength(data) != 1) ||
                (!StringEqual(JsonPrimitiveGetAsString(JsonArrayGet(data, 0)), "any::")))
            {
                Log(LOG_LEVEL_ERR,
                    "Invalid class specification '%s' in CMDB data, only '[\"any::\"]' allowed",
                    JsonPrimitiveGetAsString(JsonArrayGet(data, 0)));
                continue;
            }
            Log(LOG_LEVEL_VERBOSE, "Installing CMDB class '%s'", key);
            if (strchr(key, ':') != NULL)
            {
                char *ns_class_name = xstrdup(key);
                char *sep = strchr(ns_class_name, ':');
                *sep = '\0';
                key = sep + 1;
                EvalContextClassPutSoftNS(ctx, ns_class_name, key, CONTEXT_SCOPE_NAMESPACE, "source=cmdb");
                free(ns_class_name);
            }
            else
            {
                EvalContextClassPutSoftNS(ctx, "cmdb", key, CONTEXT_SCOPE_NAMESPACE, "source=cmdb");
            }
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Invalid CMDB class data for class '%s', must be a JSON object", key);
        }
    }
    return true;
}

bool LoadCMDBData(EvalContext *ctx)
{
    char file_path[PATH_MAX] = {0};
    strncpy(file_path, GetDataDir(), sizeof(file_path));
    JoinPaths(file_path, sizeof(file_path), HOST_SPECIFIC_DATA_FILE);
    if (access(file_path, F_OK) != 0)
    {
        Log(LOG_LEVEL_VERBOSE, "No host-specific JSON data available at '%s'", file_path);
        return true;            /* not an error */
    }
    if (access(file_path, R_OK) != 0)
    {
        Log(LOG_LEVEL_ERR, "Cannot read host-spefic JSON data from '%s'",
            file_path);
        return false;
    }

    JsonElement *data = ReadJsonFile(file_path, LOG_LEVEL_ERR, HOST_SPECIFIC_DATA_MAX_SIZE);
    if (data == NULL)
    {
        /* Details are logged by ReadJsonFile() */
        return false;
    }
    if (JsonGetType(data) != JSON_TYPE_OBJECT)
    {
        Log(LOG_LEVEL_ERR, "Invalid CMDB contents in '%s', must be a JSON object", file_path);
        JsonDestroy(data);
        return false;
    }

    Log(LOG_LEVEL_VERBOSE, "Loaded CMDB data file '%s', installing contents", file_path);

    JsonIterator iter = JsonIteratorInit(data);
    while (JsonIteratorHasMore(&iter))
    {
        const char *key = JsonIteratorNextKey(&iter);
        /* Only vars and classes allowed in CMDB data */
        if (!StringEqual(key, "vars") && !StringEqual(key, "classes"))
        {
            Log(LOG_LEVEL_WARNING, "Invalid key '%s' in the CMDB data file '%s', skipping it",
                key, file_path);
        }
    }

    bool success = true;
    JsonElement *vars = JsonObjectGet(data, "vars");
    if ((vars != NULL) && !ReadCMDBVars(ctx, vars))
    {
        success = false;
    }
    JsonElement *classes = JsonObjectGet(data, "classes");
    if ((classes != NULL) && !ReadCMDBClasses(ctx, classes))
    {
        success = false;
    }

    JsonDestroy(data);
    return success;
}
