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

#include <mod_custom.h>

#include <syntax.h>
#include <string_lib.h>      // StringStartsWith()
#include <string_sequence.h> // SeqStrginFromString()
#include <policy.h>          // Promise
#include <eval_context.h>    // cfPS(), EvalContextVariableGet()
#include <attributes.h>      // GetClassContextAttributes(), IsClassesBodyConstraint()
#include <expand.h>          // ExpandScalar()
#include <var_expressions.h> // StringContainsUnresolved(), StringIsBareNonScalarRef()
#include <map.h>             // Map*

static Map *custom_modules = NULL;

static const ConstraintSyntax promise_constraints[] = {
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewString(
        "path", "", "Path to promise module", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString(
        "interpreter", "", "Path to interpreter", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()};

const BodySyntax CUSTOM_PROMISE_BLOCK_SYNTAX =
    BodySyntaxNew("promise", promise_constraints, NULL, SYNTAX_STATUS_NORMAL);

Body *FindCustomPromiseType(const Promise *promise)
{
    assert(promise != NULL);

    const char *const promise_type = PromiseGetPromiseType(promise);
    const Policy *const policy =
        promise->parent_section->parent_bundle->parent_policy;
    Seq *custom_promise_types = policy->custom_promise_types;
    const size_t length = SeqLength(custom_promise_types);
    for (size_t i = 0; i < length; ++i)
    {
        Body *current = SeqAt(custom_promise_types, i);
        if (StringEqual(current->name, promise_type))
        {
            return current;
        }
    }
    return NULL;
}

static bool GetInterpreterAndPath(
    EvalContext *ctx,
    Body *promise_block,
    char **interpreter_out,
    char **path_out)
{
    assert(promise_block != NULL);
    assert(interpreter_out != NULL);
    assert(path_out != NULL);

    char *interpreter = NULL;
    char *path = NULL;

    const char *promise_type = promise_block->name;
    Seq *promise_block_attributes = promise_block->conlist;
    const size_t length = SeqLength(promise_block_attributes);

    for (size_t i = 0; i < length; ++i)
    {
        Constraint *attribute = SeqAt(promise_block_attributes, i);
        const char *name = attribute->lval;
        const char *value = RvalScalarValue(attribute->rval);

        if (StringEqual("interpreter", name))
        {
            free(interpreter);
            interpreter = ExpandScalar(ctx, NULL, NULL, value, NULL);
        }
        else if (StringEqual("path", name))
        {
            free(path);
            path = ExpandScalar(ctx, NULL, NULL, value, NULL);
        }
        else
        {
            debug_abort_if_reached();
        }
    }

    if (path == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Custom promise type '%s' missing path",
            promise_type);
        free(interpreter);
        free(path);
        return false;
    }

    *interpreter_out = interpreter;
    *path_out = path;
    return true;
}

static inline bool PromiseModule_LogJson(JsonElement *object, const Promise *pp)
{
    const char *level_string = JsonObjectGetAsString(object, "level");
    const char *message = JsonObjectGetAsString(object, "message");

    assert(level_string != NULL && message != NULL);
    const LogLevel level = LogLevelFromString(level_string);
    assert(level != LOG_LEVEL_NOTHING);

    if (pp != NULL)
    {
        /* Check if there is a log level specified for the particular promise. */
        const char *value = PromiseGetConstraintAsRval(pp, "log_level", RVAL_TYPE_SCALAR);
        if (value != NULL)
        {
            LogLevel specific = ActionAttributeLogLevelFromString(value);
            if (specific < level)
            {
                /* Do not log messages that have a higher log level than the log
                 * level specified for the promise (e.g. 'info' messages when
                 * 'error' was requested for the promise). */
                return false;
            }
        }
    }

    Log(level, "%s", message);

    // We want to keep track of whether the module logged an error,
    // it must log errors for not kept promises and validation errors.
    const bool logged_error =
        (level == LOG_LEVEL_ERR || level == LOG_LEVEL_CRIT);
    return logged_error;
}

static inline JsonElement *PromiseModule_ParseResultClasses(char *value)
{
    JsonElement *result_classes = JsonArrayCreate(1);
    char *delim = strchr(value, ',');
    while (delim != NULL)
    {
        *delim = '\0';
        JsonArrayAppendString(result_classes, value);
        value = delim + 1;
        delim = strchr(value, ',');
    }
    JsonArrayAppendString(result_classes, value);
    return result_classes;
}

static JsonElement *PromiseModule_Receive(PromiseModule *module, const Promise *pp)
{
    assert(module != NULL);

    bool line_based = !(module->json);

    char *line = NULL;
    size_t size = 0;
    bool empty_line = false;
    JsonElement *log_array = JsonArrayCreate(10);
    JsonElement *response = NULL;

    if (line_based)
    {
        response = JsonObjectCreate(10);
    }

    bool logged_error = false;

    ssize_t bytes;
    while (!empty_line
           && ((bytes = getline(&line, &size, module->output)) > 0))
    {
        assert(bytes > 0);
        assert(line != NULL);

        assert(line[bytes] == '\0');
        assert(line[bytes - 1] == '\n');
        line[bytes - 1] = '\0';

        // Log only non-empty lines:
        if (bytes > 1)
        {
            Log(LOG_LEVEL_DEBUG, "Received line from module: '%s'", line);
        }

        if (line[0] == '\0')
        {
            empty_line = true;
        }
        else if (StringStartsWith(line, "log_"))
        {
            const char *const equal_sign = strchr(line, '=');
            assert(equal_sign != NULL);
            if (equal_sign == NULL)
            {
                Log(LOG_LEVEL_ERR,
                    "Promise module sent invalid log line: '%s'",
                    line);
                // Skip this line but keep parsing
                FREE_AND_NULL(line);
                size = 0;
                continue;
            }
            const char *const message = equal_sign + 1;
            const char *const level_start = line + strlen("log_");
            const size_t level_length = equal_sign - level_start;
            char *const level = xstrndup(level_start, level_length);
            assert(strlen(level) == level_length);

            JsonElement *log_message = JsonObjectCreate(2);
            JsonObjectAppendString(log_message, "level", level);
            JsonObjectAppendString(log_message, "message", message);
            logged_error |= PromiseModule_LogJson(log_message, pp);
            JsonArrayAppendObject(log_array, log_message);

            free(level);
        }
        else if (line_based)
        {
            const char *const equal_sign = strchr(line, '=');
            assert(equal_sign != NULL);
            if (equal_sign == NULL)
            {
                Log(LOG_LEVEL_ERR,
                    "Promise module sent invalid line: '%s'",
                    line);
            }
            else
            {
                const char *const value = equal_sign + 1;
                const size_t key_length = equal_sign - line;
                char *const key = xstrndup(line, key_length);
                assert(strlen(key) == key_length);
                if (StringEqual(key, "result_classes"))
                {
                    char *result_classes_str = xstrdup(value);
                    JsonElement *result_classes = PromiseModule_ParseResultClasses(result_classes_str);
                    JsonObjectAppendArray(response, key, result_classes);
                    free(result_classes_str);
                }
                else
                {
                    JsonObjectAppendString(response, key, value);
                }
                free(key);
            }
        }
        else // JSON protocol:
        {
            assert(strlen(line) > 0);
            assert(response == NULL); // Should be first and only line
            const char *data = line;  // JsonParse() moves this while parsing
            JsonParseError err = JsonParse(&data, &response);
            if (err != JSON_PARSE_OK)
            {
                assert(response == NULL);
                Log(LOG_LEVEL_ERR,
                    "Promise module '%s' sent invalid JSON",
                    module->path);
                free(line);
                return NULL;
            }
            assert(response != NULL);
        }

        FREE_AND_NULL(line);
        size = 0;
    }

    if (response == NULL)
    {
        // This can happen if using the JSON protocol, and the module sends
        // nothing (newlines) or only log= lines.
        assert(!line_based);
        Log(LOG_LEVEL_ERR,
            "The '%s' promise module sent an invalid/incomplete response with JSON based protocol",
            module->path);
        return NULL;
    }

    if (line_based)
    {
        JsonObjectAppendArray(response, "log", log_array);
        log_array = NULL;
    }
    else
    {
        JsonElement *json_log_messages = JsonObjectGet(response, "log");

        // Log messages inside JSON data haven't been printed yet,
        // do it now:
        if (json_log_messages != NULL)
        {
            size_t length = JsonLength(json_log_messages);
            for (size_t i = 0; i < length; ++i)
            {
                logged_error |= PromiseModule_LogJson(
                    JsonArrayGet(json_log_messages, i), pp);
            }
        }

        JsonElement *merged = NULL;
        bool had_log_lines = (log_array != NULL && JsonLength(log_array) > 0);
        if (json_log_messages == NULL && !had_log_lines)
        {
            // No log messages at all, no need to add anything to JSON
        }
        else if (!had_log_lines)
        {
            // No separate log lines before JSON data, leave JSON as is
        }
        else if (had_log_lines && (json_log_messages == NULL))
        {
            // Separate log lines, but no log messages in JSON data
            JsonObjectAppendArray(response, "log", log_array);
            log_array = NULL;
        }
        else
        {
            // both log messages as separate lines and in JSON, merge:
            merged = JsonMerge(log_array, json_log_messages);
            JsonObjectAppendArray(response, "log", merged);
            // json_log_messages will be destroyed since we append over it
        }
    }
    JsonDestroy(log_array);

    assert(response != NULL);
    JsonObjectAppendBool(response, "_logged_error", logged_error);
    return response;
}

static void PromiseModule_SendMessage(PromiseModule *module, Seq *message)
{
    assert(module != NULL);

    const size_t length = SeqLength(message);
    for (size_t i = 0; i < length; ++i)
    {
        const char *line = SeqAt(message, i);
        const size_t line_length = strlen(line);
        assert(line_length > 0 && memchr(line, '\n', line_length) == NULL);
        fprintf(module->input, "%s\n", line);
    }
    fprintf(module->input, "\n");
    fflush(module->input);
}

static Seq *PromiseModule_ReceiveHeader(PromiseModule *module)
{
    assert(module != NULL);

    // Read header:
    char *line = NULL;
    size_t size = 0;
    ssize_t bytes = getline(&line, &size, module->output);
    if (bytes <= 0)
    {
        Log(LOG_LEVEL_ERR,
            "Did not receive header from promise module '%s'",
            module->path);
        free(line);
        return NULL;
    }
    if (line[bytes - 1] != '\n')
    {
        Log(LOG_LEVEL_ERR,
            "Promise module '%s %s' sent an invalid header with no newline: '%s'",
            module->interpreter,
            module->path,
            line);
        free(line);
        return NULL;
    }
    line[bytes - 1] = '\0';

    Log(LOG_LEVEL_DEBUG, "Received header from promise module: '%s'", line);

    Seq *header = SeqStringFromString(line, ' ');

    FREE_AND_NULL(line);
    size = 0;

    // Read empty line:
    bytes = getline(&line, &size, module->output);
    if (bytes != 1 || line[0] != '\n')
    {
        Log(LOG_LEVEL_ERR,
            "Promise module '%s %s' failed to send empty line after header: '%s'",
            module->interpreter,
            module->path,
            line);
        SeqDestroy(header);
        free(line);
        return NULL;
    }

    free(line);
    return header;
}

// Internal function, use PromiseModule_Terminate instead
static void PromiseModule_DestroyInternal(PromiseModule *module)
{
    assert(module != NULL);

    free(module->path);
    free(module->interpreter);

    cf_pclose_full_duplex(&(module->fds));
    free(module);
}

static PromiseModule *PromiseModule_Start(char *interpreter, char *path)
{
    assert(path != NULL);

    if ((interpreter != NULL) && (access(interpreter, X_OK) != 0))
    {
        Log(LOG_LEVEL_ERR,
            "Promise module interpreter '%s' is not an executable file",
            interpreter);
        return NULL;
    }

    if ((interpreter == NULL) && (access(path, X_OK) != 0))
    {
        Log(LOG_LEVEL_ERR,
            "Promise module path '%s' is not an executable file",
            path);
        return NULL;
    }

    if (access(path, F_OK) != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Promise module '%s' does not exist",
            path);
        return NULL;
    }

    PromiseModule *module = xcalloc(1, sizeof(PromiseModule));

    module->interpreter = interpreter;
    module->path = path;

    char command[CF_BUFSIZE];
    if (interpreter == NULL)
    {
        snprintf(command, CF_BUFSIZE, "%s", path);
    }
    else
    {
        snprintf(command, CF_BUFSIZE, "%s %s", interpreter, path);
    }

    Log(LOG_LEVEL_VERBOSE, "Starting custom promise module '%s' with command '%s'",
        path, command);
    module->fds = cf_popen_full_duplex_streams(command, false, true);
    module->output = module->fds.read_stream;
    module->input = module->fds.write_stream;
    module->message = NULL;

    fprintf(module->input, "cf-agent %s v1\n\n", Version());
    fflush(module->input);

    Seq *header = PromiseModule_ReceiveHeader(module);

    if (header == NULL)
    {
        // error logged in PromiseModule_ReceiveHeader()

        /* Make sure 'path' and 'interpreter' are not free'd twice (the calling
         * code frees them if it gets NULL). */
        module->path = NULL;
        module->interpreter = NULL;
        PromiseModule_DestroyInternal(module);
        return NULL;
    }

    assert(SeqLength(header) >= 3);
    Seq *flags = SeqSplit(header, 3);
    const size_t flags_length = SeqLength(flags);
    for (size_t i = 0; i < flags_length; ++i)
    {
        const char *const flag = SeqAt(flags, i);
        if (StringEqual(flag, "json_based"))
        {
            module->json = true;
        }
        else if (StringEqual(flag, "line_based"))
        {
            module->json = false;
        }
    }
    SeqDestroy(flags);

    SeqDestroy(header);

    return module;
}

static void PromiseModule_AppendString(
    PromiseModule *module, const char *key, const char *value)
{
    assert(module != NULL);

    if (module->message == NULL)
    {
        module->message = JsonObjectCreate(10);
    }
    JsonObjectAppendString(module->message, key, value);
}

static void PromiseModule_AppendInteger(
    PromiseModule *module, const char *key, int64_t value)
{
    assert(module != NULL);

    if (module->message == NULL)
    {
        module->message = JsonObjectCreate(10);
    }
    JsonObjectAppendInteger64(module->message, key, value);
}

static void PromiseModule_AppendAttribute(
    PromiseModule *module, const char *key, JsonElement *value)
{
    assert(module != NULL);

    if (module->message == NULL)
    {
        module->message = JsonObjectCreate(10);
    }

    JsonElement *attributes = JsonObjectGet(module->message, "attributes");
    if (attributes == NULL)
    {
        attributes = JsonObjectCreate(10);
        JsonObjectAppendObject(module->message, "attributes", attributes);
    }

    JsonObjectAppendElement(attributes, key, value);
}

static void PromiseModule_Send(PromiseModule *module)
{
    assert(module != NULL);

    if (module->json)
    {
        Writer *w = FileWriter(module->input);
        JsonWriteCompact(w, module->message);
        FileWriterDetach(w);
        DESTROY_AND_NULL(JsonDestroy, module->message);
        fprintf(module->input, "\n\n");
        fflush(module->input);
        return;
    }

    Seq *message = SeqNew(10, free);

    JsonIterator iter = JsonIteratorInit(module->message);
    const char *key;
    while ((key = JsonIteratorNextKey(&iter)) != NULL)
    {
        if (StringEqual("attributes", key))
        {
            JsonElement *attributes = JsonIteratorCurrentValue(&iter);
            JsonIterator attr_iter = JsonIteratorInit(attributes);

            const char *attr_name;
            while ((attr_name = JsonIteratorNextKey(&attr_iter)) != NULL)
            {
                const char *attr_val = JsonPrimitiveGetAsString(
                    JsonIteratorCurrentValue(&attr_iter));
                char *attr_line = NULL;
                xasprintf(&attr_line, "attribute_%s=%s", attr_name, attr_val);
                SeqAppend(message, attr_line);
            }
        }
        else
        {
            const char *value =
                JsonPrimitiveGetAsString(JsonIteratorCurrentValue(&iter));
            char *line = NULL;
            xasprintf(&line, "%s=%s", key, value);
            SeqAppend(message, line);
        }
    }

    PromiseModule_SendMessage(module, message);
    SeqDestroy(message);
    DESTROY_AND_NULL(JsonDestroy, module->message);
}

static inline bool TryToGetContainerFromScalarRef(const EvalContext *ctx, const char *scalar, JsonElement **out)
{
    if (StringIsBareNonScalarRef(scalar))
    {
        /* Resolve a potential 'data' variable reference. */
        const size_t scalar_len = strlen(scalar);
        char *var_ref_str = xstrndup(scalar + 2, scalar_len - 3);
        VarRef *ref = VarRefParse(var_ref_str);

        DataType type = CF_DATA_TYPE_NONE;
        const void *val = EvalContextVariableGet(ctx, ref, &type);
        free(var_ref_str);
        VarRefDestroy(ref);

        if ((val != NULL) && (type == CF_DATA_TYPE_CONTAINER))
        {
            if (out != NULL)
            {
                *out = JsonCopy(val);
            }
            return true;
        }
    }
    return false;
}

static void PromiseModule_AppendAllAttributes(
    PromiseModule *module, const EvalContext *ctx, const Promise *pp)
{
    assert(module != NULL);
    assert(pp != NULL);

    const size_t attributes = SeqLength(pp->conlist);
    for (size_t i = 0; i < attributes; i++)
    {
        const Constraint *attribute = SeqAt(pp->conlist, i);
        const char *const name = attribute->lval;
        assert(!StringEqual(name, "ifvarclass")); // Not allowed by validation
        if (IsClassesBodyConstraint(name)
            || StringEqual(name, "if")
            || StringEqual(name, "ifvarclass")
            || StringEqual(name, "unless")
            || StringEqual(name, "depends_on"))
        {
            // Evaluated by agent and not sent to module, skip
            continue;
        }

        if (StringEqual(attribute->lval, "log_level"))
        {
            /* Passed to the module as 'log_level' request field, not as an attribute. */
            continue;
        }

        JsonElement *value = NULL;
        if (attribute->rval.type == RVAL_TYPE_SCALAR)
        {
            /* Could be a '@(container)' reference. */
            if (!TryToGetContainerFromScalarRef(ctx, RvalScalarValue(attribute->rval), &value))
            {
                /* Didn't resolve to a container value, let's just use the
                 * scalar value as-is. */
                value = RvalToJson(attribute->rval);
            }
        }
        if ((attribute->rval.type == RVAL_TYPE_LIST) ||
            (attribute->rval.type == RVAL_TYPE_CONTAINER))
        {
            value = RvalToJson(attribute->rval);
        }

        if (value != NULL)
        {
            PromiseModule_AppendAttribute(module, name, value);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE,
                "Unsupported type of the '%s' attribute (%c), cannot be sent to custom promise module",
                name, attribute->rval.type);
        }
    }
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

static inline bool CustomPromise_IsFullyResolved(const EvalContext *ctx, const Promise *pp, bool nonscalars_allowed)
{
    assert(pp != NULL);

    if (StringContainsUnresolved(pp->promiser))
    {
        return false;
    }
    const size_t attributes = SeqLength(pp->conlist);
    for (size_t i = 0; i < attributes; i++)
    {
        const Constraint *attribute = SeqAt(pp->conlist, i);
        if (IsClassesBodyConstraint(attribute->lval))
        {
            /* Not passed to the modules, handled on the agent side. */
            continue;
        }
        if (StringEqual(attribute->lval, "log_level"))
        {
            /* Passed to the module as 'log_level' request field, not as an attribute. */
            continue;
        }
        if (StringEqual(attribute->lval, "unless"))
        {
            /* unless can actually have unresolved variables here,
               it defaults to evaluate in case of unresolved variables,
               to be the true opposite of if. (if would skip).*/
            continue;
        }
        if ((attribute->rval.type == RVAL_TYPE_FNCALL) ||
            (!nonscalars_allowed && (attribute->rval.type != RVAL_TYPE_SCALAR)))
        {
            return false;
        }
        if (attribute->rval.type == RVAL_TYPE_SCALAR)
        {
            const char *const value = RvalScalarValue(attribute->rval);
            if (StringContainsUnresolved(value) && !TryToGetContainerFromScalarRef(ctx, value, NULL))
            {
                return false;
            }
        }
        else if (attribute->rval.type == RVAL_TYPE_LIST)
        {
            assert(nonscalars_allowed);
            for (Rlist *rl = RvalRlistValue(attribute->rval); rl != NULL; rl = rl->next)
            {
                assert(rl->val.type == RVAL_TYPE_SCALAR);
                const char *const value = RvalScalarValue(rl->val);
                if (StringContainsUnresolved(value))
                {
                    return false;
                }
            }
        }
        else
        {
            assert(nonscalars_allowed);
            assert(attribute->rval.type == RVAL_TYPE_CONTAINER);
            JsonElement *attr_data = RvalContainerValue(attribute->rval);
            return JsonWalk(attr_data, CheckObjectForUnexpandedVars, NULL,
                            CheckPrimitiveForUnexpandedVars, NULL);
        }
    }
    return true;
}


static inline bool HasResultAndResultIsValid(JsonElement *response)
{
    const char *const result = JsonObjectGetAsString(response, "result");
    return ((result != NULL) && StringEqual(result, "valid"));
}

static inline const char *LogLevelToRequestFromModule(const Promise *pp)
{
    LogLevel log_level = LogGetGlobalLevel();

    /* Check if there is a log level specified for the particular promise. */
    const char *value = PromiseGetConstraintAsRval(pp, "log_level", RVAL_TYPE_SCALAR);
    if (value != NULL)
    {
        LogLevel specific = ActionAttributeLogLevelFromString(value);

        /* Promise-specific log level cannot go above the global log level
         * (e.g. no 'info' messages for a particular promise if the global level
         * is 'error'). */
        log_level = MIN(log_level, specific);
    }

    // We will never request LOG_LEVEL_NOTHING or LOG_LEVEL_CRIT from the
    // module:
    if (log_level < LOG_LEVEL_ERR)
    {
        assert((log_level == LOG_LEVEL_NOTHING) || (log_level == LOG_LEVEL_CRIT));
        return LogLevelToString(LOG_LEVEL_ERR);
    }
    return LogLevelToString(log_level);
}

static bool PromiseModule_Validate(PromiseModule *module, const EvalContext *ctx, const Promise *pp)
{
    assert(module != NULL);
    assert(pp != NULL);

    const char *const promise_type = PromiseGetPromiseType(pp);
    const char *const promiser = pp->promiser;

    PromiseModule_AppendString(module, "operation", "validate_promise");
    PromiseModule_AppendString(module, "log_level", LogLevelToRequestFromModule(pp));
    PromiseModule_AppendString(module, "promise_type", promise_type);
    PromiseModule_AppendString(module, "promiser", promiser);
    PromiseModule_AppendInteger(module, "line_number", pp->offset.line);
    PromiseModule_AppendString(module, "filename", PromiseGetBundle(pp)->source_path);
    PromiseModule_AppendAllAttributes(module, ctx, pp);
    PromiseModule_Send(module);

    // Prints errors / log messages from module:
    JsonElement *response = PromiseModule_Receive(module, pp);

    if (response == NULL)
    {
        // Error already printed in PromiseModule_Receive()
        return false;
    }

    // TODO: const bool logged_error = JsonObjectGetAsBool(response, "_logged_error");
    const bool logged_error = JsonPrimitiveGetAsBool(
        JsonObjectGet(response, "_logged_error"));
    const bool valid = HasResultAndResultIsValid(response);

    JsonDestroy(response);

    if (!valid)
    {
        // Detailed error messages from module should already have been printed
        const char *const filename =
            pp->parent_section->parent_bundle->source_path;
        const size_t line = pp->offset.line;
        Log(LOG_LEVEL_VERBOSE,
            "%s promise with promiser '%s' failed validation (%s:%zu)",
            promise_type,
            promiser,
            filename,
            line);

        if (!logged_error)
        {
            Log(LOG_LEVEL_CRIT,
                "Bug in promise module - No error(s) logged for invalid %s promise with promiser '%s' (%s:%zu)",
                promise_type,
                promiser,
                filename,
                line);
        }
    }

    return valid;
}

static PromiseResult PromiseModule_Evaluate(
    PromiseModule *module, EvalContext *ctx, const Promise *pp)
{
    assert(module != NULL);
    assert(pp != NULL);

    const char *const promise_type = PromiseGetPromiseType(pp);
    const char *const promiser = pp->promiser;

    PromiseModule_AppendString(module, "operation", "evaluate_promise");
    PromiseModule_AppendString(
        module, "log_level", LogLevelToRequestFromModule(pp));
    PromiseModule_AppendString(module, "promise_type", promise_type);
    PromiseModule_AppendString(module, "promiser", promiser);
    PromiseModule_AppendInteger(module, "line_number", pp->offset.line);
    PromiseModule_AppendString(module, "filename", PromiseGetBundle(pp)->source_path);

    PromiseModule_AppendAllAttributes(module, ctx, pp);
    PromiseModule_Send(module);

    JsonElement *response = PromiseModule_Receive(module, pp);
    if (response == NULL)
    {
        // Log from PromiseModule_Receive
        return PROMISE_RESULT_FAIL;
    }

    JsonElement *result_classes = JsonObjectGetAsArray(response, "result_classes");
    if (result_classes != NULL)
    {
        const size_t n_classes = JsonLength(result_classes);
        for (size_t i = 0; i < n_classes; i++)
        {
            const char *class_name = JsonArrayGetAsString(result_classes, i);
            assert(class_name != NULL);
            EvalContextClassPutSoft(ctx, class_name, CONTEXT_SCOPE_BUNDLE, "source=promise-module");
        }
    }

    PromiseResult result;
    const char *const result_str = JsonObjectGetAsString(response, "result");
    // TODO: const bool logged_error = JsonObjectGetAsBool(response, "_logged_error");
    const bool logged_error = JsonPrimitiveGetAsBool(
        JsonObjectGet(response, "_logged_error"));

    /* Attributes needed for setting outcome classes etc. */
    Attributes a = GetClassContextAttributes(ctx, pp);

    if (result_str == NULL)
    {
        result = PROMISE_RESULT_FAIL;
        cfPS(
            ctx,
            LOG_LEVEL_ERR,
            result,
            pp,
            &a,
            "Promise module did not return a result for promise evaluation (%s promise, promiser: '%s' module: '%s')",
            promise_type,
            promiser,
            module->path);
    }
    else if (StringEqual(result_str, "kept"))
    {
        result = PROMISE_RESULT_NOOP;
        cfPS(
            ctx,
            LOG_LEVEL_VERBOSE,
            result,
            pp,
            &a,
            "Promise with promiser '%s' was kept by promise module '%s'",
            promiser,
            module->path);
    }
    else if (StringEqual(result_str, "not_kept"))
    {
        result = PROMISE_RESULT_FAIL;
        cfPS(
            ctx,
            LOG_LEVEL_VERBOSE,
            result,
            pp,
            &a,
            "Promise with promiser '%s' was not kept by promise module '%s'",
            promiser,
            module->path);
        if (!logged_error)
        {
            const char *const filename =
                pp->parent_section->parent_bundle->source_path;
            const size_t line = pp->offset.line;
            Log(LOG_LEVEL_CRIT,
                "Bug in promise module - Failed to log errors for not kept %s promise with promiser '%s' (%s:%zu)",
                promise_type,
                promiser,
                filename,
                line);
        }
    }
    else if (StringEqual(result_str, "repaired"))
    {
        result = PROMISE_RESULT_CHANGE;
        cfPS(
            ctx,
            LOG_LEVEL_VERBOSE,
            result,
            pp,
            &a,
            "Promise with promiser '%s' was repaired by promise module '%s'",
            promiser,
            module->path);
    }
    else if (StringEqual(result_str, "error"))
    {
        result = PROMISE_RESULT_FAIL;
        cfPS(
            ctx,
            LOG_LEVEL_ERR,
            result,
            pp,
            &a,
            "An unexpected error occured in promise module (%s promise, promiser: '%s' module: '%s')",
            promise_type,
            promiser,
            module->path);
    }
    else
    {
        result = PROMISE_RESULT_FAIL;
        cfPS(
            ctx,
            LOG_LEVEL_ERR,
            result,
            pp,
            &a,
            "Promise module returned unacceptable result: '%s' (%s promise, promiser: '%s' module: '%s')",
            result_str,
            promise_type,
            promiser,
            module->path);
    }

    JsonDestroy(response);
    return result;
}

static void PromiseModule_Terminate(PromiseModule *module, const Promise *pp)
{
    if (module != NULL)
    {
        PromiseModule_AppendString(module, "operation", "terminate");
        PromiseModule_Send(module);

        JsonElement *response = PromiseModule_Receive(module, pp);
        JsonDestroy(response);

        PromiseModule_DestroyInternal(module);
    }
}

static void PromiseModule_Terminate_untyped(void *data)
{
    PromiseModule *module = data;
    PromiseModule_Terminate(module, NULL);
}

bool InitializeCustomPromises()
{
    /* module_path -> PromiseModule map */
    custom_modules = MapNew(StringHash_untyped,
                            StringEqual_untyped,
                            free,
                            PromiseModule_Terminate_untyped);
    assert(custom_modules != NULL);

    return (custom_modules != NULL);
}

void FinalizeCustomPromises()
{
    MapDestroy(custom_modules);
}

PromiseResult EvaluateCustomPromise(EvalContext *ctx, const Promise *pp)
{
    assert(ctx != NULL);
    assert(pp != NULL);

    Body *promise_block = FindCustomPromiseType(pp);
    if (promise_block == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Undefined promise type '%s'",
            PromiseGetPromiseType(pp));
        return PROMISE_RESULT_FAIL;
    }

    /* Attributes needed for setting outcome classes etc. */
    Attributes a = GetClassContextAttributes(ctx, pp);

    char *interpreter = NULL;
    char *path = NULL;

    bool success = GetInterpreterAndPath(ctx, promise_block, &interpreter, &path);

    if (!success)
    {
        assert(interpreter == NULL && path == NULL);
        /* Details logged in GetInterpreterAndPath() */
        cfPS(ctx, LOG_LEVEL_NOTHING, PROMISE_RESULT_FAIL, pp, &a, NULL);
        return PROMISE_RESULT_FAIL;
    }

    PromiseModule *module = MapGet(custom_modules, path);
    if (module == NULL)
    {
        module = PromiseModule_Start(interpreter, path);
        if (module != NULL)
        {
            MapInsert(custom_modules, xstrdup(path), module);
        }
        else
        {
            free(interpreter);
            free(path);
            // Error logged in PromiseModule_Start()
            cfPS(ctx, LOG_LEVEL_NOTHING, PROMISE_RESULT_FAIL, pp, &a, NULL);
            return PROMISE_RESULT_FAIL;
        }
    }
    else
    {
        if (!StringEqual(interpreter, module->interpreter))
        {
            Log(LOG_LEVEL_ERR, "Conflicting interpreter specifications for custom promise module '%s'"
                " (started with '%s' and '%s' requested for promise '%s' of type '%s')",
                path, module->interpreter, interpreter, pp->promiser, PromiseGetPromiseType(pp));
            free(interpreter);
            free(path);
            cfPS(ctx, LOG_LEVEL_NOTHING, PROMISE_RESULT_FAIL, pp, &a, NULL);
            return PROMISE_RESULT_FAIL;
        }
        free(interpreter);
        free(path);
    }

    // TODO: Do validation earlier (cf-promises --full-check)
    bool valid = PromiseModule_Validate(module, ctx, pp);

    if (valid)
    {
        valid = CustomPromise_IsFullyResolved(ctx, pp, module->json);
        if ((!valid) && (EvalContextGetPass(ctx) == CF_DONEPASSES - 1))
        {
            Log(LOG_LEVEL_ERR,
                "%s promise with promiser '%s' has unresolved/unexpanded variables",
                PromiseGetPromiseType(pp),
                pp->promiser);
        }
    }

    PromiseResult result;
    if (valid)
    {
        result = PromiseModule_Evaluate(module, ctx, pp);
    }
    else
    {
        // PromiseModule_Validate() already printed an error
        Log(LOG_LEVEL_VERBOSE,
            "%s promise with promiser '%s' will be skipped because it failed validation",
            PromiseGetPromiseType(pp),
            pp->promiser);
        cfPS(ctx, LOG_LEVEL_NOTHING, PROMISE_RESULT_FAIL, pp, &a, NULL);
        result = PROMISE_RESULT_FAIL; // TODO: Investigate if DENIED is more
                                      // appropriate
    }

    return result;
}
