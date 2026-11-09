/*
  Copyright 2024 Northern.tech AS

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

/*
 * Loading of augments files (def.json).
 *
 * LoadAugments() is called by the agents after the CMDB (host-specific)
 * data is loaded and before the policy is parsed. It loads
 * def_preferred.json (unless --ignore-preferred-augments is given) or
 * def.json from the policy input directory:
 *
 *   LoadAugments()               pick def_preferred.json or def.json
 *     LoadAugmentsFiles()        expand the filename and parse the JSON file
 *       LoadAugmentsData()       handle each top-level key of the JSON object
 *         ReadAugmentVars()      "vars"
 *         ReadAugmentVariables() "variables"
 *         ReadAugmentClasses()   "classes"
 *         ReadAugmentInputs()    "inputs"
 *         ReadAugmentAugments()  "augments", calls LoadAugmentsFiles() for
 *                                each further file
 *
 * Variables and classes already defined from host-specific data are not
 * overridden (see CanSetVariable() and CanSetClass()), except for classes
 * whose expression is "true".
 */

#include <platform.h>

#include <augments.h>

#include <json.h>
#include <logging.h>
#include <set.h>                /* StringSet */
#include <string_lib.h>         /* StringEqual(), StringEndsWith(), StringFormat() */
#include <file_lib.h>           /* FileCanOpen() */
#include <eval_context.h>       /* EvalContext*(), ClassesMatching(), JsonExpandElement() */
#include <var_expressions.h>    /* VarRef, VarRefParse() */
#include <expand.h>             /* ExpandScalar() */
#include <rlist.h>              /* RlistFromContainer() */
#include <cmdb.h>               /* ReadJsonFile(), CMDB_SOURCE_TAG */

#define AUGMENTS_VARIABLES_TAGS "tags"
#define AUGMENTS_VARIABLES_DATA "value"
#define AUGMENTS_CLASSES_TAGS "tags"
#define AUGMENTS_CLASSES_CLASS_EXPRESSIONS "class_expressions"
#define AUGMENTS_CLASSES_REGULAR_EXPRESSIONS "regular_expressions"
#define AUGMENTS_COMMENT_KEY "comment"

static bool LoadAugmentsFiles(EvalContext *ctx, const char *filename);

static bool CheckContextClassmatch(EvalContext *ctx, const char *class_str)
{
    if (StringEndsWith(class_str, "::")) // Treat as class expression, not regex
    {
        const size_t length = strlen(class_str);
        if (length <= 2)
        {
            assert(length == 2); // True because StringEndsWith
            Log(LOG_LEVEL_ERR,
                "Invalid class expression in augments: '%s'",
                class_str);
            return false;
        }

        char *const tmp_class_str = xstrdup(class_str);
        assert(strlen(tmp_class_str) == length);

        tmp_class_str[length - 2] = '\0'; // 2 = strlen("::")
        const bool found = IsDefinedClass(ctx, tmp_class_str);

        free(tmp_class_str);
        return found;
    }

    ClassTableIterator *iter = EvalContextClassTableIteratorNewGlobal(ctx, NULL, true, true);
    StringSet *global_matches = ClassesMatching(ctx, iter, class_str, NULL, true); // returns early

    const bool found = (StringSetSize(global_matches) > 0);

    StringSetDestroy(global_matches);
    ClassTableIteratorDestroy(iter);

    return found;
}

static StringSet *GetTagsFromAugmentsTags(const char *item_type,
                                          const char *key,
                                          const JsonElement *json_tags,
                                          const char *default_tag,
                                          const char *filename)
{
    StringSet *tags = NULL;
    if (JSON_NOT_NULL(json_tags))
    {
        if ((JsonGetType(json_tags) != JSON_TYPE_ARRAY) ||
            (!JsonArrayContainsOnlyPrimitives((JsonElement*) json_tags)))
        {
            Log(LOG_LEVEL_ERR,
                "Invalid tags information for %s '%s' in augments file '%s':"
                " must be a JSON array of strings",
                item_type, key, filename);
        }
        else
        {
            tags = JsonArrayToStringSet(json_tags);
            if (tags == NULL)
            {
                Log(LOG_LEVEL_ERR,
                    "Invalid meta information %s '%s' in augments file '%s':"
                    " must be a JSON array of strings",
                    item_type, key, filename);
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

static inline bool CanSetVariable(const EvalContext *ctx, VarRef *var_ref)
{
    assert(var_ref != NULL);

    bool null_ns = false;
    if (var_ref->ns == NULL)
    {
        null_ns = true;
        var_ref->ns = "default";
    }
    StringSet *tags = EvalContextVariableTags(ctx, var_ref);
    bool can_set = ((tags == NULL) || !StringSetContains(tags, CMDB_SOURCE_TAG));
    if (!can_set)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Cannot set variable %s:%s.%s from augments, already defined from host-specific data",
            var_ref->ns, var_ref->scope, var_ref->lval);
    }
    if (null_ns)
    {
        var_ref->ns = NULL;
    }

    return can_set;
}

static inline bool CanSetClass(const EvalContext *ctx, const char *class_spec)
{
    char *ns = NULL;
    const char *ns_delim = strchr(class_spec, ':');
    if (ns_delim != NULL)
    {
        ns = xstrndup(class_spec, ns_delim - class_spec);
        class_spec = ns_delim + 1;
    }

    StringSet *tags = EvalContextClassTags(ctx, ns, class_spec);
    bool can_set = ((tags == NULL) || !StringSetContains(tags, CMDB_SOURCE_TAG));
    if (!can_set)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Cannot set class %s:%s from augments, already defined from host-specific data",
            ns, class_spec);
    }

    return can_set;
}

static inline const char *GetAugmentsComment(const char *item_type, const char *identifier,
                                             const char *file_name, const JsonElement *json_object)
{
    assert(JsonGetType(json_object) == JSON_TYPE_OBJECT);

    JsonElement *json_comment = JsonObjectGet(json_object, AUGMENTS_COMMENT_KEY);
    if (NULL_JSON(json_comment))
    {
        return NULL;
    }

    if (JsonGetType(json_comment) != JSON_TYPE_STRING)
    {
        Log(LOG_LEVEL_ERR,
            "Invalid type of the 'comment' field for the '%s' %s in augments data in '%s', must be a string",
            identifier, item_type, file_name);
        return NULL;
    }

    return JsonPrimitiveGetAsString(json_comment);
}

/** Loads the "vars" key of augments data into the context */
static bool ReadAugmentVars(EvalContext *ctx, const char *filename, JsonElement *element)
{
    JsonElement* vars = JsonExpandElement(ctx, element);

    if (vars == NULL ||
        JsonGetElementType(vars) != JSON_ELEMENT_TYPE_CONTAINER ||
        JsonGetContainerType(vars) != JSON_CONTAINER_TYPE_OBJECT)
    {
        Log(LOG_LEVEL_ERR, "Invalid augments vars in '%s', must be a JSON object", filename);
        JsonDestroy(vars);
        return false;
    }

    JsonIterator iter = JsonIteratorInit(vars);
    const char *vkey;
    while ((vkey = JsonIteratorNextKey(&iter)))
    {
        VarRef *ref = VarRefParse(vkey);
        if (ref->ns != NULL)
        {
            if (ref->scope == NULL)
            {
                Log(LOG_LEVEL_ERR, "Invalid variable specification in augments data in '%s': '%s'"
                    " (bundle name has to be specified if namespace is specified)", filename, vkey);
                VarRefDestroy(ref);
                continue;
            }
        }
        if (ref->scope == NULL)
        {
            ref->scope = xstrdup("def");
        }

        JsonElement *data = JsonObjectGet(vars, vkey);
        if (JsonGetElementType(data) == JSON_ELEMENT_TYPE_PRIMITIVE)
        {
            char *value = JsonPrimitiveToString(data);
            if ((ref->ns == NULL) && (ref->scope == NULL))
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments variable '%s.%s=%s' from file '%s'",
                    SpecialScopeToString(SPECIAL_SCOPE_DEF), vkey, value, filename);
                if (CanSetVariable(ctx, ref))
                {
                    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_DEF, vkey, value, CF_DATA_TYPE_STRING, "source=augments_file");
                }
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments variable '%s=%s' from file '%s'",
                    vkey, value, filename);
                if (CanSetVariable(ctx, ref))
                {
                    EvalContextVariablePut(ctx, ref, value, CF_DATA_TYPE_STRING, "source=augments_file");
                }
            }
            free(value);
        }
        else if (JsonGetElementType(data) == JSON_ELEMENT_TYPE_CONTAINER &&
                    JsonGetContainerType(data) == JSON_CONTAINER_TYPE_ARRAY &&
                    JsonArrayContainsOnlyPrimitives(data))
        {
            // map to slist if the data only has primitives
            Rlist *data_as_rlist = RlistFromContainer(data);
            if ((ref->ns == NULL) && (ref->scope == NULL))
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments slist variable '%s.%s' from file '%s'",
                    SpecialScopeToString(SPECIAL_SCOPE_DEF), vkey, filename);
                if (CanSetVariable(ctx, ref))
                {
                    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_DEF,
                                                    vkey, data_as_rlist,
                                                    CF_DATA_TYPE_STRING_LIST,
                                                    "source=augments_file");
                }
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments slist variable '%s' from file '%s'",
                    vkey, filename);
                if (CanSetVariable(ctx, ref))
                {
                    EvalContextVariablePut(ctx, ref, data_as_rlist, CF_DATA_TYPE_STRING_LIST,
                                            "source=augments_file");
                }
            }

            RlistDestroy(data_as_rlist);
        }
        else // install as a data container
        {
            if ((ref->ns == NULL) && (ref->scope == NULL))
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments data container variable '%s.%s' from file '%s'",
                    SpecialScopeToString(SPECIAL_SCOPE_DEF), vkey, filename);
                if (CanSetVariable(ctx, ref))
                {
                    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_DEF,
                                                    vkey, data,
                                                    CF_DATA_TYPE_CONTAINER,
                                                    "source=augments_file");
                }
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments data container variable '%s' from file '%s'",
                    vkey, filename);
                if (CanSetVariable(ctx, ref))
                {
                    EvalContextVariablePut(ctx, ref, data,
                                            CF_DATA_TYPE_CONTAINER,
                                            "source=augments_file");
                }
            }
        }
        VarRefDestroy(ref);
    }

    JsonDestroy(vars);
    return true;
}

/** Loads the "variables" key of augments data (uses the new format allowing metadata, CFE-3633) */
static bool ReadAugmentVariables(EvalContext *ctx, const char *filename, JsonElement *element)
{
    JsonElement* variables = JsonExpandElement(ctx, element);

    if (variables == NULL || JsonGetType(variables) != JSON_TYPE_OBJECT)
    {
        Log(LOG_LEVEL_ERR, "Invalid augments variables in '%s', must be a JSON object", filename);
        JsonDestroy(variables);
        return false;
    }

    JsonIterator variables_iter = JsonIteratorInit(variables);
    const char *vkey;
    while ((vkey = JsonIteratorNextKey(&variables_iter)))
    {
        VarRef *ref = VarRefParse(vkey);
        if (ref->ns != NULL)
        {
            if (ref->scope == NULL)
            {
                Log(LOG_LEVEL_ERR, "Invalid variable specification in augments data in '%s': '%s'"
                    " (bundle name has to be specified if namespace is specified)", filename, vkey);
                VarRefDestroy(ref);
                continue;
            }
        }
        if (ref->scope == NULL)
        {
            ref->scope = xstrdup("def");
        }

        const JsonElement *const var_info = JsonObjectGet(variables, vkey);

        const JsonElement *data;
        StringSet *tags;
        const char *comment = NULL;

        if (JsonGetType(var_info) == JSON_TYPE_OBJECT)
        {
            data = JsonObjectGet(var_info, AUGMENTS_VARIABLES_DATA);

            if (NULL_JSON(data))
            {
                Log(LOG_LEVEL_ERR, "Missing value for the augments variable '%s' in '%s' (value field is required)",
                    vkey, filename);
                VarRefDestroy(ref);
                continue;
            }

            const JsonElement *json_tags = JsonObjectGet(var_info, AUGMENTS_VARIABLES_TAGS);
            tags = GetTagsFromAugmentsTags("variable", vkey, json_tags, "source=augments_file", filename);
            comment = GetAugmentsComment("variable", vkey, filename, var_info);
        }
        else
        {
            // Just a bare value, like in "vars", no metadata
            data = var_info;
            tags = GetTagsFromAugmentsTags("variable", vkey, NULL, "source=augments_file", filename);
        }

        assert(tags != NULL);
        assert(data != NULL);

        bool installed = false;
        if (JsonGetElementType(data) == JSON_ELEMENT_TYPE_PRIMITIVE)
        {
            char *value = JsonPrimitiveToString(data);
            if ((ref->ns == NULL) && (ref->scope == NULL))
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments variable '%s.%s=%s' from file '%s'",
                    SpecialScopeToString(SPECIAL_SCOPE_DEF), vkey, value, filename);
                if (CanSetVariable(ctx, ref))
                {
                    installed = EvalContextVariablePutSpecialTagsSetWithComment(ctx, SPECIAL_SCOPE_DEF, vkey, value,
                                                                                CF_DATA_TYPE_STRING, tags, comment);
                }
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments variable '%s=%s' from file '%s'",
                    vkey, value, filename);
                if (CanSetVariable(ctx, ref))
                {
                    installed = EvalContextVariablePutTagsSetWithComment(ctx, ref, value, CF_DATA_TYPE_STRING,
                                                                            tags, comment);
                }
            }
            free(value);
        }
        else if (JsonGetElementType(data) == JSON_ELEMENT_TYPE_CONTAINER &&
                    JsonGetContainerType(data) == JSON_CONTAINER_TYPE_ARRAY &&
                    JsonArrayContainsOnlyPrimitives((JsonElement *) data))
        {
            // map to slist if the data only has primitives
            Rlist *data_as_rlist = RlistFromContainer(data);
            if ((ref->ns == NULL) && (ref->scope == NULL))
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments slist variable '%s.%s' from file '%s'",
                    SpecialScopeToString(SPECIAL_SCOPE_DEF), vkey, filename);
                if (CanSetVariable(ctx, ref))
                {
                    installed = EvalContextVariablePutSpecialTagsSetWithComment(ctx, SPECIAL_SCOPE_DEF,
                                                                                vkey, data_as_rlist,
                                                                                CF_DATA_TYPE_STRING_LIST,
                                                                                tags, comment);
                }
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments slist variable '%s' from file '%s'",
                    vkey, filename);
                if (CanSetVariable(ctx, ref))
                {
                    installed = EvalContextVariablePutTagsSetWithComment(ctx, ref, data_as_rlist,
                                                                            CF_DATA_TYPE_STRING_LIST,
                                                                            tags, comment);
                }
            }

            RlistDestroy(data_as_rlist);
        }
        else // install as a data container
        {
            if ((ref->ns == NULL) && (ref->scope == NULL))
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments data container variable '%s.%s' from file '%s'",
                    SpecialScopeToString(SPECIAL_SCOPE_DEF), vkey, filename);
                if (CanSetVariable(ctx, ref))
                {
                    installed = EvalContextVariablePutSpecialTagsSetWithComment(ctx, SPECIAL_SCOPE_DEF,
                                                                                vkey, data,
                                                                                CF_DATA_TYPE_CONTAINER,
                                                                                tags, comment);
                }
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments data container variable '%s' from file '%s'",
                    vkey, filename);
                if (CanSetVariable(ctx, ref))
                {
                    installed = EvalContextVariablePutTagsSetWithComment(ctx, ref, data,
                                                                            CF_DATA_TYPE_CONTAINER,
                                                                            tags, comment);
                }
            }
        }
        VarRefDestroy(ref);
        if (!installed)
        {
            /* EvalContextVariablePutTagsSetWithComment() and
                * EvalContextVariablePutSpecialTagsSetWithComment() take
                * over tags in case of success. Otherwise we have to
                * destroy the set. */
            StringSetDestroy(tags);
        }
    }

    JsonDestroy(variables);
    return true;
}

/** Loads the "classes" key of augments data into the context */
static bool ReadAugmentClasses(EvalContext *ctx, const char *filename, JsonElement *element)
{
    JsonElement* classes = JsonExpandElement(ctx, element);

    if (JsonGetElementType(classes) != JSON_ELEMENT_TYPE_CONTAINER ||
        JsonGetContainerType(classes) != JSON_CONTAINER_TYPE_OBJECT)
    {
        Log(LOG_LEVEL_ERR, "Invalid augments classes in '%s', must be a JSON object", filename);
        JsonDestroy(classes);
        return false;
    }

    const char default_tags[] = "source=augments_file";
    JsonIterator iter = JsonIteratorInit(classes);
    const char *ckey;
    while ((ckey = JsonIteratorNextKey(&iter)))
    {
        JsonElement *data = JsonObjectGet(classes, ckey);
        if (JsonGetElementType(data) == JSON_ELEMENT_TYPE_PRIMITIVE)
        {
            char *check = JsonPrimitiveToString(data);
            // check if class is true
            if (CheckContextClassmatch(ctx, check))
            {
                Log(LOG_LEVEL_VERBOSE, "Installing augments class '%s' (checked '%s') from file '%s'",
                    ckey, check, filename);
                if (CanSetClass(ctx, ckey))
                {
                    EvalContextClassPutSoft(ctx, ckey, CONTEXT_SCOPE_NAMESPACE, default_tags);
                }
            }
            else if (StringEqual(check, "true"))
            {
                EvalContextClassPutSoft(ctx, ckey, CONTEXT_SCOPE_NAMESPACE, default_tags);
            }
            free(check);
        }
        else if (JsonGetElementType(data) == JSON_ELEMENT_TYPE_CONTAINER &&
                    JsonGetContainerType(data) == JSON_CONTAINER_TYPE_ARRAY &&
                    JsonArrayContainsOnlyPrimitives(data))
        {
            // check if each class is true
            JsonIterator data_iter = JsonIteratorInit(data);
            const JsonElement *el;
            while ((el = JsonIteratorNextValueByType(&data_iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
            {
                char *check = JsonPrimitiveToString(el);
                if (CheckContextClassmatch(ctx, check))
                {
                    Log(LOG_LEVEL_VERBOSE, "Installing augments class '%s' (checked array entry '%s') from file '%s'",
                        ckey, check, filename);
                    if (CanSetClass(ctx, ckey))
                    {
                        EvalContextClassPutSoft(ctx, ckey, CONTEXT_SCOPE_NAMESPACE, default_tags);
                    }
                    free(check);
                    break;
                }
                else if (StringEqual(check, "true"))
                {
                    EvalContextClassPutSoft(ctx, ckey, CONTEXT_SCOPE_NAMESPACE, default_tags);
                }

                free(check);
            }
        }
        else if (JsonGetType(data) == JSON_TYPE_OBJECT)
        {
            const JsonElement *class_exprs = JsonObjectGet(data, AUGMENTS_CLASSES_CLASS_EXPRESSIONS);
            const JsonElement *reg_exprs = JsonObjectGet(data, AUGMENTS_CLASSES_REGULAR_EXPRESSIONS);
            const JsonElement *json_tags = JsonObjectGet(data, AUGMENTS_CLASSES_TAGS);

            if ((JSON_NOT_NULL(class_exprs) && JSON_NOT_NULL(reg_exprs)) ||
                (NULL_JSON(class_exprs) && NULL_JSON(reg_exprs)))
            {
                Log(LOG_LEVEL_ERR, "Invalid augments class data for class '%s' in '%s':"
                    " either \"class_expressions\" or \"regular_expressions\" need to be specified",
                    ckey, filename);
                continue;
            }

            StringSet *tags = GetTagsFromAugmentsTags("class", ckey, json_tags,
                                                        "source=augments_file", filename);
            const char *comment = GetAugmentsComment("class", ckey, filename, data);
            bool installed = false;
            JsonIterator exprs_iter = JsonIteratorInit(class_exprs ? class_exprs : reg_exprs);
            const JsonElement *el;
            while ((el = JsonIteratorNextValueByType(&exprs_iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
            {
                char *check = JsonPrimitiveToString(el);
                if (CheckContextClassmatch(ctx, check))
                {
                    Log(LOG_LEVEL_VERBOSE, "Installing augments class '%s' (checked array entry '%s') from file '%s'",
                        ckey, check, filename);
                    if (CanSetClass(ctx, ckey))
                    {
                        installed = EvalContextClassPutSoftTagsSetWithComment(ctx, ckey, CONTEXT_SCOPE_NAMESPACE,
                                                                                tags, comment);
                    }
                    else if (StringEqual(check, "true"))
                    {
                        installed = EvalContextClassPutSoftTagsSetWithComment(ctx, ckey, CONTEXT_SCOPE_NAMESPACE,
                                                                                tags, comment);
                    }
                    free(check);
                    break;
                }

                free(check);
            }
            if (!installed)
            {
                /* EvalContextClassPutSoftTagsSetWithComment() takes over tags in
                    * case of success. Otherwise we have to destroy the set. */
                StringSetDestroy(tags);
            }
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Invalid augments class data for class '%s' in '%s'",
                ckey, filename);
        }
    }

    JsonDestroy(classes);
    return true;
}

/** Loads the "inputs" key of augments data into def.augments_inputs */
static bool ReadAugmentInputs(EvalContext *ctx, const char *filename, JsonElement *element)
{
    JsonElement* inputs = JsonExpandElement(ctx, element);

    if (JsonGetElementType(inputs) != JSON_ELEMENT_TYPE_CONTAINER ||
        JsonGetContainerType(inputs) != JSON_CONTAINER_TYPE_ARRAY ||
        !JsonArrayContainsOnlyPrimitives(inputs))
    {
        Log(LOG_LEVEL_ERR, "Trying to augment inputs in '%s' but the value was not a list of strings",
            filename);
        JsonDestroy(inputs);
        return false;
    }

    Log(LOG_LEVEL_VERBOSE, "Installing augments def.augments_inputs from file '%s'",
        filename);
    Rlist *rlist = RlistFromContainer(inputs);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_DEF,
                                  "augments_inputs", rlist,
                                  CF_DATA_TYPE_STRING_LIST,
                                  "source=augments_file");
    RlistDestroy(rlist);

    JsonDestroy(inputs);
    return true;
}

/** Loads further augments files listed in the "augments" key */
static bool ReadAugmentAugments(EvalContext *ctx, const char *filename, JsonElement *further_augments)
{
    assert(further_augments != NULL);

    if (JsonGetElementType(further_augments) != JSON_ELEMENT_TYPE_CONTAINER ||
        JsonGetContainerType(further_augments) != JSON_CONTAINER_TYPE_ARRAY ||
        !JsonArrayContainsOnlyPrimitives(further_augments))
    {
        Log(LOG_LEVEL_ERR, "Trying to augment inputs in '%s' but the value was not a list of strings",
            filename);
        return false;
    }

    JsonIterator iter = JsonIteratorInit(further_augments);
    const JsonElement *el;
    while ((el = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)) != NULL)
    {
        char *nested_filename = JsonPrimitiveToString(el);
        bool further_loaded = LoadAugmentsFiles(ctx, nested_filename);
        if (further_loaded)
        {
            Log(LOG_LEVEL_VERBOSE, "Completed augmenting from file '%s'", nested_filename);
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Could not load requested further augments from file '%s'", nested_filename);
        }
        free(nested_filename);
    }

    return true;
}

/**
 * Installs the contents of one augments file into the evaluation context.
 *
 * The data is a JSON object and each supported top-level key is handled by its
 * own ReadAugment*() function, in this order (the same structure as
 * LoadCMDBData() in cmdb.c):
 *
 *   "vars"       ReadAugmentVars()       variables (old format)
 *   "variables"  ReadAugmentVariables()  variables with tags and comments
 *   "classes"    ReadAugmentClasses()    classes
 *   "inputs"     ReadAugmentInputs()     def.augments_inputs
 *   "augments"   ReadAugmentAugments()   further augments files to load
 *
 * Unknown keys are logged and skipped. An invalid key does not stop the
 * processing of the other keys.
 *
 * @return %false if the data is not a JSON object or any key is invalid
 */
static bool LoadAugmentsData(EvalContext *ctx, const char *filename, const JsonElement* augment)
{
    if (JsonGetElementType(augment) != JSON_ELEMENT_TYPE_CONTAINER ||
        JsonGetContainerType(augment) != JSON_CONTAINER_TYPE_OBJECT)
    {
        Log(LOG_LEVEL_ERR, "Invalid augments file contents in '%s', must be a JSON object", filename);
        return false;
    }

    Log(LOG_LEVEL_VERBOSE, "Loaded augments file '%s', installing contents", filename);

    JsonIterator iter = JsonIteratorInit(augment);
    const char *key;
    while ((key = JsonIteratorNextKey(&iter)))
    {
        if (!(StringEqual(key, "vars") ||
                StringEqual(key, "classes") ||
                StringEqual(key, "inputs") ||
                StringEqual(key, "augments") ||
                StringEqual(key, "variables")))
        {
            Log(LOG_LEVEL_VERBOSE, "Unknown augments key '%s' in file '%s', skipping it",
                key, filename);
        }
    }

    bool success = true;
    JsonElement *vars = JsonObjectGet(augment, "vars");
    if (JSON_NOT_NULL(vars) && !ReadAugmentVars(ctx, filename, vars))
    {
        success = false;
    }

    /* Uses the new format allowing metadata (CFE-3633) */
    JsonElement *variables = JsonObjectGet(augment, "variables");
    if (JSON_NOT_NULL(variables) && !ReadAugmentVariables(ctx, filename, variables))
    {
        success = false;
    }

    JsonElement *classes = JsonObjectGet(augment, "classes");
    if (JSON_NOT_NULL(classes) && !ReadAugmentClasses(ctx, filename, classes))
    {
        success = false;
    }

    JsonElement *inputs = JsonObjectGet(augment, "inputs");
    if (JSON_NOT_NULL(inputs) && !ReadAugmentInputs(ctx, filename, inputs))
    {
        success = false;
    }

    /* load further def.json files (if any) */
    JsonElement *augments = JsonObjectGet(augment, "augments");
    if (JSON_NOT_NULL(augments) && !ReadAugmentAugments(ctx, filename, augments))
    {
        success = false;
    }

    return success;
}

static bool LoadAugmentsFiles(EvalContext *ctx, const char *unexpanded_filename)
{
    bool loaded = false;

    char *filename = ExpandScalar(ctx, NULL, "this", unexpanded_filename, NULL);

    if (strstr(filename, "/.json"))
    {
        Log(LOG_LEVEL_DEBUG,
            "Skipping augments file '%s' because it failed to expand the base filename, resulting in '%s'",
            unexpanded_filename, filename);
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "Searching for augments file '%s' (original '%s')",
            filename, unexpanded_filename);
        if (FileCanOpen(filename, "r"))
        {
            // 5 MB should be enough for most reasonable def.json data
            JsonElement* augment = ReadJsonFile(filename, LOG_LEVEL_ERR, 5 * 1024 * 1024);
            if (augment != NULL)
            {
                loaded = LoadAugmentsData(ctx, filename, augment);
                JsonDestroy(augment);
            }
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "could not load JSON augments from '%s'", filename);
        }
    }

    free(filename);
    return loaded;
}

static bool IsFile(const char *const filename)
{
    struct stat buffer;
    if (stat(filename, &buffer) != 0)
    {
        return false;
    }
    if (S_ISREG(buffer.st_mode) != 0)
    {
        return true;
    }
    return false;
}

void LoadAugments(EvalContext *ctx, GenericAgentConfig *config)
{
    assert(config != NULL);

    char* def_json = NULL;
    // --ignore-preferred-augments command line option:
    if (config->ignore_preferred_augments)
    {
        EvalContextClassPutHard(ctx, "ignore_preferred_augments", "source=command_line_option");
        // def_json is NULL so it will be assigned below
    }
    else
    {
        def_json = StringFormat("%s%c%s", config->input_dir, FILE_SEPARATOR, "def_preferred.json");
        if (!IsFile(def_json))
        {
            // def_preferred.json does not exist or we cannot read it
            FREE_AND_NULL(def_json);
        }
    }

    if (def_json == NULL)
    {
        // No def_preferred.json, either because the feature is disabled
        // or we could not read the file.
        // Fall back to old / default behavior, using def.json:
        def_json = StringFormat("%s%c%s", config->input_dir, FILE_SEPARATOR, "def.json");
    }
    Log(LOG_LEVEL_VERBOSE, "Loading JSON augments from '%s' (input dir '%s', input file '%s'", def_json, config->input_dir, config->input_file);
    LoadAugmentsFiles(ctx, def_json);
    free(def_json);
}
