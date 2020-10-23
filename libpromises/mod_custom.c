/*
  Copyright 2020 Northern.tech AS

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
#include <eval_context.h>    // cfPS()
#include <attributes.h>      // GetClassContextAttributes()
#include <expand.h>          // ExpandScalar()

static const ConstraintSyntax promise_constraints[] = {
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewString(
        "path", "", "Path to promise module", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString(
        "interpreter", "", "Path to interpreter", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()};

const BodySyntax CUSTOM_PROMISE_BLOCK_SYNTAX =
    BodySyntaxNew("promise", promise_constraints, NULL, SYNTAX_STATUS_NORMAL);

bool IsCustomPromiseType(const Promise *pp)
{
    assert(pp != NULL);

    Policy *policy = pp->parent_section->parent_bundle->parent_policy;

    return PolicyHasCustomPromiseType(
        policy, pp->parent_section->promise_type);
}

static Body *FindCustomPromiseType(const Promise *promise)
{
    assert(promise != NULL);

    const char *const promise_type = promise->parent_section->promise_type;
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

    if ((interpreter == NULL) || (path == NULL))
    {
        Log(LOG_LEVEL_ERR,
            "Custom promise type '%s' missing interpreter or path",
            promise_type);
        free(interpreter);
        free(path);
        return false;
    }

    *interpreter_out = interpreter;
    *path_out = path;
    return true;
}

static inline void PromiseModule_LogJson(JsonElement *object)
{
    const char *level_string = JsonObjectGetAsString(object, "level");
    const char *message = JsonObjectGetAsString(object, "message");

    assert(level_string != NULL && message != NULL);
    const LogLevel level = LogLevelFromString(level_string);
    assert(level != LOG_LEVEL_NOTHING);
    Log(level, "%s", message);
}

static JsonElement *PromiseModule_Receive(PromiseModule *module)
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

    ssize_t bytes;
    while (!empty_line
           && ((bytes = getline(&line, &size, module->output)) > 0))
    {
        assert(bytes > 0);
        assert(line != NULL);

        assert(line[bytes] == '\0');
        assert(line[bytes - 1] == '\n');
        line[bytes - 1] = '\0';

        Log(LOG_LEVEL_DEBUG, "Received line from module: '%s'", line);

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
            }
            const char *const message = equal_sign + 1;
            const char *const level_start = line + strlen("log_");
            const size_t level_length = equal_sign - level_start;
            char *const level = xstrndup(level_start, level_length);
            assert(strlen(level) == level_length);

            JsonElement *log_message = JsonObjectCreate(2);
            JsonObjectAppendString(log_message, "level", level);
            JsonObjectAppendString(log_message, "message", message);
            PromiseModule_LogJson(log_message);
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
                JsonObjectAppendString(response, key, value);
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
        }

        FREE_AND_NULL(line);
        size = 0;
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
                PromiseModule_LogJson(JsonArrayGet(json_log_messages, i));
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
    size_t bytes = getline(&line, &size, module->output);
    assert(bytes > 1);
    line[bytes - 1] = '\0';

    Seq *header = SeqStringFromString(line, ' ');

    FREE_AND_NULL(line);
    size = 0;

    // Read empty line:
    bytes = getline(&line, &size, module->output);
    assert(bytes == 1 && line[0] == '\n');

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
    PromiseModule *module = xcalloc(1, sizeof(PromiseModule));

    module->interpreter = interpreter;
    module->path = path;

    char command[CF_BUFSIZE];
    snprintf(command, CF_BUFSIZE, "%s %s", interpreter, path);

    module->fds = cf_popen_full_duplex_streams(command, false, true);
    module->output = module->fds.read_stream;
    module->input = module->fds.write_stream;
    module->message = NULL;

    fprintf(module->input, "CFEngine 3.16.0 v1\n\n");
    fflush(module->input);

    Seq *header = PromiseModule_ReceiveHeader(module);

    if (header == NULL)
    {
        // error logged in PromiseModule_ReceiveHeader()
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

static void PromiseModule_AppendAttribute(
    PromiseModule *module, const char *key, const char *value)
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

    JsonObjectAppendString(attributes, key, value);
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

static void PromiseModule_AppendAllAttributes(
    PromiseModule *module, const Promise *pp)
{
    assert(module != NULL);
    assert(pp != NULL);

    const size_t attributes = SeqLength(pp->conlist);
    for (size_t i = 0; i < attributes; i++)
    {
        const Constraint *attribute = SeqAt(pp->conlist, i);
        const char *const name = attribute->lval;
        assert(!StringEqual(name, "ifvarclass")); // Not allowed by validation
        if (StringEqual(name, "if") || StringEqual(name, "ifvarclass"))
        {
            // Evaluated by agent and not sent to module, skip
            continue;
        }
        char *const value = RvalToString(attribute->rval);
        PromiseModule_AppendAttribute(module, name, value);
        free(value);
    }
}

static inline bool StringContainsUnresolved(const char *str)
{
    // clang-format off
    return (StringContains(str, "$(")
            || StringContains(str, "${")
            || StringContains(str, "@{")
            || StringContains(str, "@("));
    // clang-format on
}

static inline bool CustomPromise_IsFullyResolved(const Promise *pp)
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
        if (attribute->rval.type != RVAL_TYPE_SCALAR)
        {
            // Most likely container or unresolved function call
            // Custom promises only support scalars currently
            // TODO - Implement slists for custom promises:
            // https://tracker.mender.io/browse/CFE-3444
            return false;
        }
        const char *const value = RvalScalarValue(attribute->rval);
        if (StringContainsUnresolved(value))
        {
            return false;
        }
    }
    return true;
}


static inline bool HasResultAndResultIsValid(JsonElement *response)
{
    const char *const result = JsonObjectGetAsString(response, "result");
    return ((result != NULL) && StringEqual(result, "valid"));
}

static bool PromiseModule_Validate(PromiseModule *module, const Promise *pp)
{
    assert(module != NULL);
    assert(pp != NULL);

    PromiseModule_AppendString(module, "operation", "validate_promise");
    PromiseModule_AppendString(module, "log_level", "info");
    PromiseModule_AppendString(module, "promiser", pp->promiser);
    PromiseModule_AppendAllAttributes(module, pp);
    PromiseModule_Send(module);

    // Prints errors / log messages from module:
    JsonElement *response = PromiseModule_Receive(module);

    const bool valid = HasResultAndResultIsValid(response);

    JsonDestroy(response);

    if (!valid)
    {
        // Detailed error messages from module should already have been printed
        const char *const promise_type = pp->parent_section->promise_type;
        const char *const promiser = pp->promiser;
        const char *const filename =
            pp->parent_section->parent_bundle->source_path;
        const size_t line = pp->offset.line;
        Log(LOG_LEVEL_ERR,
            "%s:%zu: %s promise with promiser '%s' failed validation",
            filename,
            line,
            promise_type,
            promiser);
    }

    return valid;
}

static inline const char *LogLevelToRequestFromModule()
{
    // We will never request LOG_LEVEL_NOTHING or LOG_LEVEL_CRIT from the
    // module:
    const LogLevel global = LogGetGlobalLevel();
    if (global < LOG_LEVEL_ERR)
    {
        assert((global == LOG_LEVEL_NOTHING) || (global == LOG_LEVEL_CRIT));
        return LogLevelToString(LOG_LEVEL_ERR);
    }
    return LogLevelToString(global);
}

static PromiseResult PromiseModule_Evaluate(
    PromiseModule *module, EvalContext *ctx, const Promise *pp)
{
    assert(module != NULL);
    assert(pp != NULL);

    PromiseModule_AppendString(module, "operation", "evaluate_promise");
    PromiseModule_AppendString(
        module, "log_level", LogLevelToRequestFromModule());
    PromiseModule_AppendString(module, "promiser", pp->promiser);

    PromiseModule_AppendAllAttributes(module, pp);
    PromiseModule_Send(module);

    JsonElement *response = PromiseModule_Receive(module);
    PromiseResult result;
    const char *const result_str = JsonObjectGetAsString(response, "result");

    Attributes a = GetClassContextAttributes(ctx, pp); // TODO: WTF

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
            pp->parent_section->promise_type,
            pp->promiser,
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
            pp->promiser,
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
            pp->promiser,
            module->path);
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
            pp->promiser,
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
            pp->parent_section->promise_type,
            pp->promiser,
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
            pp->parent_section->promise_type,
            pp->promiser,
            module->path);
    }

    JsonDestroy(response);
    return result;
}

static void PromiseModule_Terminate(PromiseModule *module)
{
    if (module != NULL)
    {
        PromiseModule_AppendString(module, "operation", "terminate");
        PromiseModule_Send(module);

        JsonElement *response = PromiseModule_Receive(module);
        JsonDestroy(response);

        PromiseModule_DestroyInternal(module);
    }
}

PromiseResult EvaluateCustomPromise(EvalContext *ctx, const Promise *pp)
{
    assert(ctx != NULL);
    assert(pp != NULL);

    Body *promise_block = FindCustomPromiseType(pp);
    char *interpreter = NULL;
    char *path = NULL;

    bool success = GetInterpreterAndPath(ctx, promise_block, &interpreter, &path);

    if (!success)
    {
        assert(interpreter == NULL && path == NULL);
        return PROMISE_RESULT_FAIL;
    }

    // TODO: Store promise modules and avoid starting one per promise
    // evaluation
    PromiseModule *module = PromiseModule_Start(interpreter, path);
    PromiseResult result;

    // TODO: Do validation earlier (cf-promises --full-check)
    bool valid = PromiseModule_Validate(module, pp);

    if (valid)
    {
        valid = CustomPromise_IsFullyResolved(pp);
        if ((!valid) && (EvalContextGetPass(ctx) == CF_DONEPASSES - 1))
        {
            Log(LOG_LEVEL_ERR,
                "%s promise with promiser '%s' has unresolved/unexpanded variables",
                pp->parent_section->promise_type,
                pp->promiser);
        }
    }

    if (valid)
    {
        result = PromiseModule_Evaluate(module, ctx, pp);
    }
    else
    {
        // PromiseModule_Validate() already printed an error
        Log(LOG_LEVEL_VERBOSE,
            "%s promise with promiser '%s' will be skipped because it failed validation",
            pp->parent_section->promise_type,
            pp->promiser);
        result = PROMISE_RESULT_FAIL; // TODO: Investigate if DENIED is more
                                      // appropriate
    }

    PromiseModule_Terminate(module);

    return result;
}
