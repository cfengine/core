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
#include <set.h>                /* StringSet */
#include <string_lib.h>
#include <known_dirs.h>         /* GetDataDir() */
#include <var_expressions.h>    /* VarRef, StringContainsUnresolved() */
#include <eval_context.h>       /* EvalContext*() */
#include <files_names.h>        /* JoinPaths() */

#include <cmdb.h>

#define HOST_SPECIFIC_DATA_FILE "host_specific.json"
#define HOST_SPECIFIC_DATA_MAX_SIZE (5 * 1024 * 1024) /* maximum size of the host-specific.json file */

#define CMDB_NAMESPACE "data"
#define CMDB_VARIABLES_TAGS "tags"
#define CMDB_VARIABLES_DATA "value"
#define CMDB_CLASSES_TAGS "tags"
#define CMDB_CLASSES_CLASS_EXPRESSIONS "class_expressions"
#define CMDB_CLASSES_REGULAR_EXPRESSIONS "regular_expressions"
#define CMDB_COMMENT_KEY "comment"

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

static VarRef *GetCMDBVariableRef(const char *key)
{
    VarRef *ref = VarRefParse(key);
    if (ref->ns == NULL)
    {
        ref->ns = xstrdup(CMDB_NAMESPACE);
    }
    else
    {
        if (ref->scope == NULL)
        {
            Log(LOG_LEVEL_ERR, "Invalid variable specification in CMDB data: '%s'"
                " (bundle name has to be specified if namespace is specified)", key);
            VarRefDestroy(ref);
            return NULL;
        }
    }

    if (ref->scope == NULL)
    {
        ref->scope = xstrdup("variables");
    }
    return ref;
}

static bool AddCMDBVariable(EvalContext *ctx, const char *key, const VarRef *ref,
                            JsonElement *data, StringSet *tags, const char *comment)
{
    assert(ctx != NULL);
    assert(key != NULL);
    assert(ref != NULL);
    assert(data != NULL);
    assert(tags != NULL);

    bool ret;
    if (JsonGetElementType(data) == JSON_ELEMENT_TYPE_PRIMITIVE)
    {
        char *value = JsonPrimitiveToString(data);
        Log(LOG_LEVEL_VERBOSE, "Installing CMDB variable '%s:%s.%s=%s'",
            ref->ns, ref->scope, key, value);
        ret = EvalContextVariablePutTagsSetWithComment(ctx, ref, value, CF_DATA_TYPE_STRING,
                                                       tags, comment);
        free(value);
    }
    else if ((JsonGetType(data) == JSON_TYPE_ARRAY) &&
             JsonArrayContainsOnlyPrimitives(data))
    {
        // map to slist if the data only has primitives
        Log(LOG_LEVEL_VERBOSE, "Installing CMDB slist variable '%s:%s.%s'",
            ref->ns, ref->scope, key);
        Rlist *data_rlist = RlistFromContainer(data);
        ret = EvalContextVariablePutTagsSetWithComment(ctx, ref,
                                                       data_rlist, CF_DATA_TYPE_STRING_LIST,
                                                       tags, comment);
        RlistDestroy(data_rlist);
    }
    else
    {
        // install as a data container
        Log(LOG_LEVEL_VERBOSE, "Installing CMDB data container variable '%s:%s.%s'",
            ref->ns, ref->scope, key);
        ret = EvalContextVariablePutTagsSetWithComment(ctx, ref,
                                                       data, CF_DATA_TYPE_CONTAINER,
                                                       tags, comment);
    }
    if (!ret)
    {
        /* On success, EvalContextVariablePutTagsSet() consumes the tags set,
         * otherwise, we shall destroy it. */
        StringSetDestroy(tags);
    }
    return ret;
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

        VarRef *ref = GetCMDBVariableRef(key);
        if (ref == NULL)
        {
            continue;
        }

        StringSet *tags = StringSetNew();
        StringSetAdd(tags, xstrdup(CMDB_SOURCE_TAG));
        bool ret = AddCMDBVariable(ctx, key, ref, data, tags, NULL);
        VarRefDestroy(ref);
        if (!ret)
        {
            /* Details should have been logged already. */
            Log(LOG_LEVEL_ERR, "Failed to add CMDB variable '%s'", key);
        }
    }
    return true;
}

static StringSet *GetTagsFromJsonTags(const char *item_type,
                                      const char *key,
                                      const JsonElement *json_tags,
                                      const char *default_tag)
{
    StringSet *tags = NULL;
    if (JSON_NOT_NULL(json_tags))
    {
        if ((JsonGetType(json_tags) != JSON_TYPE_ARRAY) ||
            (!JsonArrayContainsOnlyPrimitives((JsonElement*) json_tags)))
        {
            Log(LOG_LEVEL_ERR,
                "Invalid json_tags information for %s '%s' in CMDB data:"
                " must be a JSON array of strings",
                item_type, key);
        }
        else
        {
            tags = JsonArrayToStringSet(json_tags);
            if (tags == NULL)
            {
                Log(LOG_LEVEL_ERR,
                    "Invalid json_tags information %s '%s' in CMDB data:"
                    " must be a JSON array of strings",
                    item_type, key);
            }
        }
    }
    if (tags == NULL)
    {
        tags = StringSetNew();
    }
    StringSetAdd(tags, xstrdup(default_tag));

    return tags;
}

static inline const char *GetCMDBComment(const char *item_type, const char *identifier,
                                         const JsonElement *json_object)
{
    assert(JsonGetType(json_object) == JSON_TYPE_OBJECT);

    JsonElement *json_comment = JsonObjectGet(json_object, CMDB_COMMENT_KEY);
    if (NULL_JSON(json_comment))
    {
        return NULL;
    }

    if (JsonGetType(json_comment) != JSON_TYPE_STRING)
    {
        Log(LOG_LEVEL_ERR,
            "Invalid type of the 'comment' field for the '%s' %s in CMDB data, must be a string",
            identifier, item_type);
        return NULL;
    }

    return JsonPrimitiveGetAsString(json_comment);
}

/** Uses the new format allowing metadata (CFE-3633) */
static bool ReadCMDBVariables(EvalContext *ctx, JsonElement *variables)
{
    assert(variables != NULL);

    if (JsonGetType(variables) != JSON_TYPE_OBJECT)
    {
        Log(LOG_LEVEL_ERR, "Invalid 'variables' CMDB data, must be a JSON object");
        return false;
    }

    if (!JsonWalk(variables, CheckObjectForUnexpandedVars, NULL, CheckPrimitiveForUnexpandedVars, NULL))
    {
        Log(LOG_LEVEL_ERR, "Invalid 'variables' CMDB data, cannot contain variable references");
        return false;
    }

    JsonIterator iter = JsonIteratorInit(variables);
    while (JsonIteratorHasMore(&iter))
    {
        const char *key = JsonIteratorNextKey(&iter);

        VarRef *ref = GetCMDBVariableRef(key);
        if (ref == NULL)
        {
            continue;
        }

        JsonElement *const var_info = JsonObjectGet(variables, key);

        JsonElement *data;
        StringSet *tags;
        const char *comment = NULL;

        if (JsonGetType(var_info) == JSON_TYPE_OBJECT)
        {
            data = JsonObjectGet(var_info, CMDB_VARIABLES_DATA);

            if (data == NULL)
            {
                Log(LOG_LEVEL_ERR, "Missing value in '%s' variable specification in CMDB data (value field is required)", key);
                VarRefDestroy(ref);
                continue;
            }

            JsonElement *json_tags = JsonObjectGet(var_info, CMDB_VARIABLES_TAGS);
            tags = GetTagsFromJsonTags("variable", key, json_tags, CMDB_SOURCE_TAG);
            comment = GetCMDBComment("variable", key, var_info);
        }
        else
        {
            // Just a bare value, like in "vars", no metadata
            data = var_info;
            tags = GetTagsFromJsonTags("variable", key, NULL, CMDB_SOURCE_TAG);
        }

        assert(tags != NULL);
        assert(data != NULL);

        bool ret = AddCMDBVariable(ctx, key, ref, data, tags, comment);
        VarRefDestroy(ref);
        if (!ret)
        {
            /* Details should have been logged already. */
            Log(LOG_LEVEL_ERR, "Failed to add CMDB variable '%s'", key);
        }
    }
    return true;
}

static bool AddCMDBClass(EvalContext *ctx, const char *key, StringSet *tags, const char *comment)
{
    assert(ctx != NULL);
    assert(key != NULL);
    assert(tags != NULL);

    bool ret;
    Log(LOG_LEVEL_VERBOSE, "Installing CMDB class '%s'", key);

    if (strchr(key, ':') != NULL)
    {
        char *ns_class_name = xstrdup(key);
        char *sep = strchr(ns_class_name, ':');
        *sep = '\0';
        key = sep + 1;
        ret = EvalContextClassPutSoftNSTagsSetWithComment(ctx, ns_class_name, key,
                                                          CONTEXT_SCOPE_NAMESPACE, tags, comment);
        free(ns_class_name);
    }
    else
    {
        ret = EvalContextClassPutSoftNSTagsSetWithComment(ctx, CMDB_NAMESPACE, key,
                                                          CONTEXT_SCOPE_NAMESPACE, tags, comment);
    }
    if (!ret)
    {
        /* On success, EvalContextClassPutSoftNSTagsSetWithComment() consumes
         * the tags set, otherwise, we shall destroy it. */
        StringSetDestroy(tags);
    }

    return ret;
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

            StringSet *default_tags = StringSetNew();
            StringSetAdd(default_tags, xstrdup(CMDB_SOURCE_TAG));
            bool ret = AddCMDBClass(ctx, key, default_tags, NULL);
            if (!ret)
            {
                /* Details should have been logged already. */
                Log(LOG_LEVEL_ERR, "Failed to add CMDB class '%s'", key);
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
            StringSet *default_tags = StringSetNew();
            StringSetAdd(default_tags, xstrdup(CMDB_SOURCE_TAG));
            bool ret = AddCMDBClass(ctx, key, default_tags, NULL);
            if (!ret)
            {
                /* Details should have been logged already. */
                Log(LOG_LEVEL_ERR, "Failed to add CMDB class '%s'", key);
            }
        }
        else if (JsonGetContainerType(data) == JSON_CONTAINER_TYPE_OBJECT)
        {
            const JsonElement *class_exprs = JsonObjectGet(data, CMDB_CLASSES_CLASS_EXPRESSIONS);
            const JsonElement *reg_exprs = JsonObjectGet(data, CMDB_CLASSES_REGULAR_EXPRESSIONS);
            const JsonElement *json_tags = JsonObjectGet(data, CMDB_CLASSES_TAGS);

            if (JSON_NOT_NULL(class_exprs) &&
                (JsonGetType(class_exprs) != JSON_TYPE_ARRAY ||
                 JsonLength(class_exprs) > 1 ||
                 (JsonLength(class_exprs) == 1 &&
                  !StringEqual(JsonPrimitiveGetAsString(JsonArrayGet(class_exprs, 0)), "any::"))))
            {
                Log(LOG_LEVEL_ERR,
                    "Invalid class expression rules for class '%s' in CMDB data,"
                    " only '[]' or '[\"any::\"]' allowed", key);
                continue;
            }
            if (JSON_NOT_NULL(reg_exprs) &&
                (JsonGetType(reg_exprs) != JSON_TYPE_ARRAY ||
                 JsonLength(reg_exprs) > 1 ||
                 (JsonLength(reg_exprs) == 1 &&
                  !StringEqual(JsonPrimitiveGetAsString(JsonArrayGet(reg_exprs, 0)), "any"))))
            {
                Log(LOG_LEVEL_ERR,
                    "Invalid regular expression rules for class '%s' in CMDB data,"
                    " only '[]' or '[\"any\"]' allowed", key);
                continue;
            }

            StringSet *tags = GetTagsFromJsonTags("class", key, json_tags, CMDB_SOURCE_TAG);
            const char *comment = GetCMDBComment("class", key, data);
            bool ret = AddCMDBClass(ctx, key, tags, comment);
            if (!ret)
            {
                /* Details should have been logged already. */
                Log(LOG_LEVEL_ERR, "Failed to add CMDB class '%s'", key);
            }
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Invalid CMDB class data for class '%s'", key);
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
        if (!IsStrIn(key, (const char*[4]){"vars", "classes", "variables", NULL}))
        {
            Log(LOG_LEVEL_WARNING, "Invalid key '%s' in the CMDB data file '%s', skipping it",
                key, file_path);
        }
    }

    bool success = true;
    JsonElement *vars = JsonObjectGet(data, "vars");
    if (JSON_NOT_NULL(vars) && !ReadCMDBVars(ctx, vars))
    {
        success = false;
    }
    /* Uses the new format allowing metadata (CFE-3633) */
    JsonElement *variables = JsonObjectGet(data, "variables");
    if (JSON_NOT_NULL(variables) && !ReadCMDBVariables(ctx, variables))
    {
        success = false;
    }
    JsonElement *classes = JsonObjectGet(data, "classes");
    if (JSON_NOT_NULL(classes) && !ReadCMDBClasses(ctx, classes))
    {
        success = false;
    }

    JsonDestroy(data);
    return success;
}
