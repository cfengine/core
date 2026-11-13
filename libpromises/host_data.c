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

#include <platform.h>
#include <json.h>
#include <logging.h>
#include <set.h>                /* StringSet, JsonArrayToStringSet() */
#include <var_expressions.h>    /* VarRef, VarRefParse() */

#include <host_data.h>

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

VarRef *GetVariableRefFromDataKey(const char *key, const char *default_ns,
                                  const char *default_scope, const char *source)
{
    assert(key != NULL);
    assert(default_scope != NULL);
    assert(source != NULL);

    VarRef *ref = VarRefParse(key);
    if ((ref->ns != NULL) && (ref->scope == NULL))
    {
        Log(LOG_LEVEL_ERR, "Invalid variable specification in %s: '%s'"
            " (bundle name has to be specified if namespace is specified)", source, key);
        VarRefDestroy(ref);
        return NULL;
    }

    if ((ref->ns == NULL) && (default_ns != NULL))
    {
        ref->ns = xstrdup(default_ns);
    }
    if (ref->scope == NULL)
    {
        ref->scope = xstrdup(default_scope);
    }
    return ref;
}

StringSet *GetTagsFromJsonTags(const char *item_type,
                               const char *key,
                               const JsonElement *json_tags,
                               const char *default_tag,
                               const char *source)
{
    StringSet *tags = NULL;
    if (JSON_NOT_NULL(json_tags))
    {
        if ((JsonGetType(json_tags) != JSON_TYPE_ARRAY) ||
            (!JsonArrayContainsOnlyPrimitives((JsonElement*) json_tags)))
        {
            Log(LOG_LEVEL_ERR,
                "Invalid tags information for %s '%s' in %s:"
                " must be a JSON array of strings",
                item_type, key, source);
        }
        else
        {
            tags = JsonArrayToStringSet(json_tags);
            if (tags == NULL)
            {
                Log(LOG_LEVEL_ERR,
                    "Invalid tags information for %s '%s' in %s:"
                    " must be a JSON array of strings",
                    item_type, key, source);
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

const char *GetCommentFromJsonData(const char *item_type, const char *identifier,
                                   const JsonElement *json_object, const char *source)
{
    assert(JsonGetType(json_object) == JSON_TYPE_OBJECT);

    JsonElement *json_comment = JsonObjectGet(json_object, JSON_DATA_KEY_COMMENT);
    if (NULL_JSON(json_comment))
    {
        return NULL;
    }

    if (JsonGetType(json_comment) != JSON_TYPE_STRING)
    {
        Log(LOG_LEVEL_ERR,
            "Invalid type of the 'comment' field for the '%s' %s in %s, must be a string",
            identifier, item_type, source);
        return NULL;
    }

    return JsonPrimitiveGetAsString(json_comment);
}
