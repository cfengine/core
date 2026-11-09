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

#ifndef CFENGINE_HOST_DATA_H
#define CFENGINE_HOST_DATA_H

/* Helpers for the JSON data defining variables and classes, shared by the
 * host-specific data (CMDB, host_specific.json) and augments (def.json). */

#include <json.h>
#include <logging.h>            /* LogLevel */
#include <set.h>                /* StringSet */
#include <var_expressions.h>    /* VarRef */

/* Keys used in the JSON data for variables and classes */
#define JSON_DATA_KEY_VALUE "value"
#define JSON_DATA_KEY_TAGS "tags"
#define JSON_DATA_KEY_COMMENT "comment"
#define JSON_DATA_KEY_CLASS_EXPRESSIONS "class_expressions"
#define JSON_DATA_KEY_REGULAR_EXPRESSIONS "regular_expressions"

JsonElement *ReadJsonFile(const char *filename, LogLevel log_level, size_t size_max);

/* Helpers for parsing the variables and classes JSON data.
 *
 * @source describes the data in log messages, e.g. "CMDB data" or
 * "augments data in '/path/to/def.json'".
 */

/**
 * Parses the name of a variable from the JSON data.
 *
 * @param default_ns     namespace to use if not specified (or %NULL)
 * @param default_scope  bundle to use if not specified
 * @return The variable reference or %NULL (after logging an error) if a
 *         namespace is specified without a bundle.
 */
VarRef *GetVariableRefFromDataKey(const char *key, const char *default_ns,
                                  const char *default_scope, const char *source);

/**
 * Gets the set of tags from the 'tags' JSON array of a variable or class
 * (@json_tags may be %NULL) and adds @default_tag to it.
 *
 * @return A new set of tags, never %NULL. Invalid tags are logged and ignored.
 */
StringSet *GetTagsFromJsonTags(const char *item_type,
                               const char *key,
                               const JsonElement *json_tags,
                               const char *default_tag,
                               const char *source);

/**
 * Gets the 'comment' field of a variable or class JSON object.
 *
 * @return The comment (owned by @json_object) or %NULL if not specified or
 *         invalid (logged).
 */
const char *GetCommentFromJsonData(const char *item_type, const char *identifier,
                                   const JsonElement *json_object, const char *source);

#endif  /* CFENGINE_HOST_DATA_H */
