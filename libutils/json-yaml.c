/*
   Copyright 2017 Northern.tech AS

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

#include <alloc.h>
#include <logging.h>
#include <misc_lib.h>

#include <json-yaml.h>
#include <json-priv.h>

JsonParseError JsonParseYamlFile(const char *path, size_t size_max, JsonElement **json_out)
{
    return JsonParseAnyFile(path, size_max, json_out, true);
}

#ifdef HAVE_LIBYAML

// macros and some of the parse guesses follow https://php-yaml.googlecode.com/svn-history/trunk/parse.c
#define JSON_YAML_SCALAR_TAG_IS(tag, name) \
  !strcmp((const char *)tag, name)

#define JSON_YAML_IS_NOT_IMPLICIT_AND_TAG_IS(event, tag, name)           \
  (!event->data.scalar.quoted_implicit && !event->data.scalar.plain_implicit && JSON_YAML_SCALAR_TAG_IS(tag, name))

#define JSON_YAML_IS_NOT_QUOTED_OR_TAG_IS(event, tag, name)              \
  (!event->data.scalar.quoted_implicit && (event->data.scalar.plain_implicit || JSON_YAML_SCALAR_TAG_IS(tag, name)))

static JsonElement* JsonParseYamlScalarValue(yaml_event_t *event)
{
    assert(event);
    assert(event->type == YAML_SCALAR_EVENT);

    const char *tag = (const char *) event->data.scalar.tag;
    const char *value = (const char *) event->data.scalar.value;
    size_t length = event->data.scalar.length;

    if (NULL == tag)
    {
        tag = YAML_DEFAULT_SCALAR_TAG;
    }

    if (JSON_YAML_SCALAR_TAG_IS(tag, YAML_NULL_TAG) ||
        event->data.scalar.plain_implicit)
    {
        if ((length == 1 && *value == '~') || length == 0 ||
            !strcmp("NULL", value) || !strcmp("Null", value) ||
            !strcmp("null", value))
        {
            return JsonNullCreate();
        }
    }

    if (JSON_YAML_IS_NOT_QUOTED_OR_TAG_IS(event, tag, YAML_BOOL_TAG))
    {
        // see http://yaml.org/type/bool.html
        if ((length == 1 && (*value == 'Y' || *value == 'y')) ||
            !strcmp("YES", value) || !strcmp("Yes", value) ||
            !strcmp("yes", value) || !strcmp("TRUE", value) ||
            !strcmp("True", value) || !strcmp("true", value) ||
            !strcmp("ON", value) || !strcmp("On", value) || !strcmp("on", value))
        {
            return JsonBoolCreate(true);
        }

        if ((length == 1 && (*value == 'N' || *value == 'n')) ||
            !strcmp("NO", value) || !strcmp("No", value) || !strcmp("no", value) ||
            !strcmp("FALSE", value) || !strcmp("False", value) ||
            !strcmp("false", value) || !strcmp("OFF", value) ||
            !strcmp("Off", value) || !strcmp("off", value))
        {
            return JsonBoolCreate(false);
        }
    }
    else if (JSON_YAML_IS_NOT_IMPLICIT_AND_TAG_IS(event, tag, YAML_BOOL_TAG))
    {
        if (length == 0 || (length == 1 && *value == '0'))
        {
            return JsonBoolCreate(false);
        }
        else
        {
            return JsonBoolCreate(true);
        }
    }

    /* check for numeric (int or float) */
    if (!event->data.scalar.quoted_implicit &&
        (event->data.scalar.plain_implicit ||
         JSON_YAML_SCALAR_TAG_IS(tag, YAML_INT_TAG) ||
         JSON_YAML_SCALAR_TAG_IS(tag, YAML_FLOAT_TAG)))
    {
        JsonElement *tobuild;
        if (JSON_PARSE_OK == JsonParseAsNumber(&value, &tobuild))
        {
            return tobuild;
        }
    }

    if (0 == strcmp(tag, YAML_TIMESTAMP_TAG))
    {
        // what else could we do, return a epoch time?
        Log(LOG_LEVEL_VERBOSE, "YAML parse: treating timestamp value '%s' as a string", value);
        return JsonStringCreate(value);
    }

    if (JSON_YAML_SCALAR_TAG_IS(tag, YAML_STR_TAG))
    {
        return JsonStringCreate(value);
    }

    Log(LOG_LEVEL_VERBOSE, "YAML parse: unhandled scalar tag %s, returning as string", tag);
    return JsonStringCreate(value);
}

static void JsonParseYamlData(yaml_parser_t *parser, JsonElement *element, const int depth)
{
    yaml_event_t event;
    char* key = NULL;

    Log(LOG_LEVEL_DEBUG, "YAML parse: entering JsonParseYamlStore");

    while (1)
    {
        yaml_parser_parse(parser, &event);
        Log(LOG_LEVEL_DEBUG, "YAML parse: event of type %d arrived with depth %d, key %s",
            event.type,
            depth,
            key == NULL ? "[NULL]" : key);

        // Parse value either as a new leaf in the mapping
        //  or as a leaf value (one of them, in case it's a sequence)
        if (event.type == YAML_SCALAR_EVENT)
        {
            Log(LOG_LEVEL_DEBUG, "YAML parse: scalar event, value '%s'", event.data.scalar.value);
            if (JsonGetElementType(element) == JSON_ELEMENT_TYPE_CONTAINER)
            {
                if (JsonGetContainerType(element) == JSON_CONTAINER_TYPE_OBJECT)
                {
                    if (NULL == key)
                    {
                        // save key
                        key = xstrdup(event.data.scalar.value);
                    }
                    else
                    {
                        JsonObjectAppendElement(element, key, JsonParseYamlScalarValue(&event));
                        // clear key
                        key = NULL;
                    }
                }
                else if (JsonGetContainerType(element) == JSON_CONTAINER_TYPE_ARRAY)
                {
                    JsonArrayAppendElement(element, JsonParseYamlScalarValue(&event));
                    // clear key
                    key = NULL;
                }
                else
                {
                    ProgrammingError("YAML Parse: scalar event received inside an unknown JSON container type");
                }
            }
            else
            {
                ProgrammingError("YAML Parse: scalar event received inside a non-container JSON element");
            }
        }
        else if (event.type == YAML_SEQUENCE_START_EVENT)
        {
            Log(LOG_LEVEL_DEBUG, "YAML parse: starting sequence");
            JsonElement *arr = JsonArrayCreate(DEFAULT_CONTAINER_CAPACITY);

            if (JsonGetElementType(element) == JSON_ELEMENT_TYPE_CONTAINER)
            {
                if (JsonGetContainerType(element) == JSON_CONTAINER_TYPE_OBJECT)
                {
                    if (key)
                    {
                        JsonObjectAppendElement(element, key, arr);
                        JsonParseYamlData(parser, arr, depth+1);
                        // clear key
                        key = NULL;
                    }
                    else
                    {
                        ProgrammingError("YAML Parse: Unexpected sequence start event inside a container without a key");
                    }
                }
                else if (JsonGetContainerType(element) == JSON_CONTAINER_TYPE_ARRAY)
                {
                    JsonArrayAppendArray(element, arr);
                    JsonParseYamlData(parser, arr, depth+1);
                    // clear key
                    key = NULL;
                }
                else
                {
                    ProgrammingError("YAML Parse: Unexpected sequence start event inside a non-container");
                }
            }
        }
        else if (event.type == YAML_SEQUENCE_END_EVENT)
        {
            Log(LOG_LEVEL_DEBUG, "YAML parse: ending sequence");
            if (JsonGetElementType(element) == JSON_ELEMENT_TYPE_CONTAINER)
            {
                if (JsonGetContainerType(element) == JSON_CONTAINER_TYPE_ARRAY)
                {
                    // finished with this array, return up
                    break;
                }
                else
                {
                    ProgrammingError("YAML Parse: Unexpected sequence end event inside a non-array container");
                }
            }
            else
            {
                ProgrammingError("YAML Parse: Unexpected sequence end event inside a non-container");
            }
        }
        else if (event.type == YAML_MAPPING_START_EVENT)
        {
            Log(LOG_LEVEL_DEBUG, "YAML parse: starting mapping");
            JsonElement *obj = JsonObjectCreate(DEFAULT_CONTAINER_CAPACITY);

            if (JsonGetElementType(element) == JSON_ELEMENT_TYPE_CONTAINER)
            {
                if (JsonGetContainerType(element) == JSON_CONTAINER_TYPE_OBJECT)
                {
                    if (key)
                    {
                        JsonObjectAppendElement(element, key, obj);
                        JsonParseYamlData(parser, obj, depth+1);
                        // clear key
                        key = NULL;
                    }
                    else
                    {
                        ProgrammingError("YAML Parse: Unexpected mapping start event inside a container without a key");
                    }
                }
                else if (JsonGetContainerType(element) == JSON_CONTAINER_TYPE_ARRAY)
                {
                    JsonArrayAppendObject(element, obj);
                    JsonParseYamlData(parser, obj, depth+1);
                    // clear key
                    key = NULL;
                }
                else
                {
                    ProgrammingError("YAML Parse: Unexpected mapping start event inside a non-container");
                }
            }
        }
        else if (event.type == YAML_MAPPING_END_EVENT)
        {
            Log(LOG_LEVEL_DEBUG, "YAML parse: ending mapping");
            if (JsonGetElementType(element) == JSON_ELEMENT_TYPE_CONTAINER)
            {
                if (JsonGetContainerType(element) == JSON_CONTAINER_TYPE_OBJECT)
                {
                    // finished with this object, return up
                    break;
                }
                else
                {
                    ProgrammingError("YAML Parse: Unexpected mapping end event inside a non-object container");
                }
            }
            else
            {
                ProgrammingError("YAML Parse: Unexpected mapping end event inside a non-container");
            }
        }
        else if (event.type == YAML_STREAM_END_EVENT)
        {
            Log(LOG_LEVEL_DEBUG, "YAML parse: ending stream");
            break;
        }
        else if (event.type == YAML_NO_EVENT)
        {
            Log(LOG_LEVEL_DEBUG, "YAML parse: NO_EVENT");
            break; // NO_EVENT doesn't need to be deleted
        }
        else
        {
            // ignore other events
        }

        yaml_event_delete(&event);

        Log(LOG_LEVEL_DEBUG, "YAML parse: running inner loop");
    }

    if (key)
    {
        free(key);
    }

    Log(LOG_LEVEL_DEBUG, "YAML parse: exiting JsonParseYamlData");
}

JsonParseError JsonParseYamlString(const char **data, JsonElement **json_out)
{
    assert(data && *data);
    if (data == NULL || *data == NULL)
    {
        return JSON_PARSE_ERROR_NO_DATA;
    }

    yaml_parser_t parser;

    if(!yaml_parser_initialize(&parser))
    {
        return JSON_PARSE_ERROR_LIBYAML_FAILURE;
    }

    yaml_parser_set_input_string(&parser, *data, strlen(*data));

    JsonElement *holder = JsonArrayCreate(1);
    JsonParseYamlData(&parser, holder, 0);
    *json_out = JsonCopy(JsonAt(holder, 0));
    JsonDestroy(holder);

    yaml_parser_delete(&parser);

    return JSON_PARSE_OK;
}

#else // !HAVE_LIBYAML

JsonParseError JsonParseYamlString(ARG_UNUSED const char **data,
                                   ARG_UNUSED JsonElement **json_out)
{
    return JSON_PARSE_ERROR_NO_LIBYAML;
}
#endif
