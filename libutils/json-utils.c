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

#include <platform.h>
#include <json-utils.h>
#include <logging.h>    // Log()
#include <file_lib.h>   // safe_fopen()
#include <string_lib.h> // TrimWhitespace()
#include <csv_parser.h>
#include <json-yaml.h>  // JsonParseYamlFile()
#include <alloc.h>
#define ENV_BYTE_LIMIT 4096

/**
 * @brief Filters a string, according to env file format
 *
 * This is used to parse the part after the equal sign in an env file
 * Leading and trailing whitespace should already be removed
 * Nonescaped single or double quotes must either be at src[0] to start a
 * quoted string or inside a quoted string of the other type.
 * Will terminate string after closing quote, even when there's more
 * Closing quote is optional (implied at null terminator)
 *
 * Supported escape characters: \\ \" \' \n
 * Anything else will just add the character directly, ex: \x -> x
 *
 * @param src Copy from pointer, can be same as dst
 * @param dst Copy to pointer, can be same as src
 * @return beginning of processed string, either dst or dst + 1
 */
static char *filtered_copy(const char *src, char *dst)
{
    assert(src);
    assert(dst);
    char *beginning = dst;
    char opening_quote = '\0';
    // Check for opening quote, must be at src[0]
    if (*src == '\"' || *src == '\'')
    {
        opening_quote = *src;
        ++src;
    }
    // Loop until null terminator or quote matching opening_quote:
    while (*src != '\0' && (*src != opening_quote))
    {
        if (opening_quote == '\0' && (*src == '\"' || *src == '\''))
        {
            // Return NULL when encountering an unmatched, unescaped quote
            // Invalid input: AB"CD
            // Correct ways:  AB\"CD or 'AB"CD'
            return NULL;
        }
        if (*src == '\\')
        {
            // Backslash escape char
            ++src;
            // Special case for \n newline:
            if (*src == 'n')
            {
                *dst = '\n';
                ++src;
                ++dst;
                continue;
            }
            // Otherwise: copy next char directly
        }
        *dst = *src;
        ++src;
        ++dst;
    }
    *dst = '\0';
    return beginning;
}

/**
 * @brief Splits a line of en env file into key and value string
 *
 * See filtered_copy for details on how value in (key-value pair) is parsed
 * Empty lines are skipped
 * Lines with first nonspace symbol '#' are skipped (comments)
 * To preserve whitespace use quotes: WHITESPACE="   "
 *
 * @param raw_line input from CfReadLine. Will be edited!
 * @param key_out   Where to store pointer to key in raw_line
 * @param value_out Where to store pointer to value in raw_line
 * @param filename_for_log Optional name for logging purposes
 */
 void ParseEnvLine(char *raw_line, char **key_out, char **value_out, const char *filename_for_log, int linenumber)
 {
    assert(raw_line);
    assert(key_out);
    assert(value_out);
    char *key = NULL;
    char *value = NULL;
    *key_out = NULL;
    *value_out = NULL;

    char *line = TrimWhitespace(raw_line);
    if (NULL_OR_EMPTY(line))
    {
        return;
    }

    const char *myname = "ParseEnvLine";
    size_t line_length = strlen(line);

    if (line[0] == '#' || line_length == 0)
    {
        return;
    }
    char *next_equal = strchr(line, '=');
    if (next_equal == NULL)
    {
        Log(LOG_LEVEL_DEBUG, "%s: Line %d in ENV file '%s' isn't empty, but was skipped because it's missing an equal sign",
            myname, linenumber, filename_for_log);
        return;
    }
    long equal_index = next_equal - line;
    if (equal_index == 0)
    {
        Log(LOG_LEVEL_DEBUG, "%s: Line %d in ENV file '%s' was skipped because it's missing a key",
            myname, linenumber, filename_for_log);
        return;
    }
    *next_equal = '\0';
    key = TrimWhitespace(line);
    value = TrimWhitespace(next_equal + 1);

    // Copy from value to value (dest=src) and return new starting pointer
    // new_start = filtered_copy(src,dst)
    // Modifies the string in place, removing enclosing quotes and
    // obeys backslash escape characters
    value = filtered_copy(value, value);

    if (value != NULL && key != NULL)
    {
        // Succeeded in finding both key and value, copy to output
        *key_out = key;
        *value_out = value;
    }
    else if (value != NULL || key != NULL)
    {
        // Parsing failed for either key or value, print log message and skip
        Log(LOG_LEVEL_DEBUG, "%s: Line %d in ENV file '%s' was skipped because it has invalid syntax",
            myname, linenumber, filename_for_log);
    }
}

/**
 * @brief Parses an env file and creates a key-value pair json element
 *
 * Creates JSON element where all keys and values are strings
 *
 * @param input_path file to read from ex: "/etc/os-release"
 * @param size_max   Maximum size of env file (in bytes)
 * @param json_out   Where to save pointer to new JsonElement, must destroy
 * @return true for success, false for failure
 */
bool JsonParseEnvFile(const char *input_path, size_t size_max, JsonElement **json_out)
{
    assert(json_out);
    *json_out = JsonObjectCreate(10);
    const char *myname = "JsonParseEnvFile";
    size_t line_size = ENV_BYTE_LIMIT;
    char *raw_line = xmalloc(line_size);
    char *key, *value;
    int linenumber = 0;
    size_t byte_count = 0;

    assert(input_path);
    assert(myname);
    FILE *fin = safe_fopen(input_path, "r");
    if (fin == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "%s cannot open the ENV file '%s' (fopen: %s)",
            myname, input_path, GetErrorStr());
        return false;
    }

    while (CfReadLine(&raw_line, &line_size, fin) != -1)
    {
        ++linenumber;
        byte_count += strlen(raw_line);
        if (byte_count > size_max)
        {
            Log(LOG_LEVEL_VERBOSE, "%s: ENV file '%s' exceeded byte limit %zu at line %d",
                myname, input_path, size_max, linenumber);
            Log(LOG_LEVEL_VERBOSE, "Done with ENV file, the rest will not be parsed");
            break;
        }

        ParseEnvLine(raw_line, &key, &value, input_path, linenumber);
        if (key != NULL && value != NULL)
        {
            JsonObjectAppendString(*json_out, key, value);
        }
    }

    bool reached_eof = feof(fin);
    fclose(fin);

    if (!reached_eof && byte_count <= size_max)
    {
        Log(LOG_LEVEL_ERR,
            "%s: failed to read ENV file '%s'. (fread: %s)",
            myname, input_path, GetErrorStr());
        JsonDestroy(*json_out);
        free(raw_line);
        return false;
    }

    free(raw_line);
    return true;
}

bool JsonParseCsvFile(const char *input_path, size_t size_max, JsonElement **json_out)
{
    assert(json_out);
    *json_out = JsonArrayCreate(50);
    const char *myname = "JsonParseCsvFile";
    char *line;
    size_t byte_count = 0;

    int linenumber = 0;

    FILE *fin = safe_fopen(input_path, "r");
    if (fin == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "%s cannot open the csv file '%s' (fopen: %s)",
            myname, input_path, GetErrorStr());
        return false;
    }

    while ((line = GetCsvLineNext(fin)) != NULL)
    {
        ++linenumber;

        byte_count += strlen(line);
        if (byte_count > size_max)
        {
            Log(LOG_LEVEL_VERBOSE, "%s: CSV file '%s' exceeded byte limit %zu at line %d",
                myname, input_path, size_max, linenumber);
            Log(LOG_LEVEL_VERBOSE, "Done with CSV file, the rest will not be parsed");
            free(line);
            break;
        }

        Seq *list = SeqParseCsvString(line);
        free(line);

        if (list != NULL)
        {
            JsonElement *line_arr = JsonArrayCreate(SeqLength(list));

            for (size_t i = 0; i < SeqLength(list); i++)
            {
                JsonArrayAppendString(line_arr, SeqAt(list, i));
            }

            SeqDestroy(list);
            JsonArrayAppendArray(*json_out, line_arr);
        }
    }

    bool reached_eof = feof(fin);
    fclose(fin);

    if (!reached_eof && byte_count <= size_max)
    {
        Log(LOG_LEVEL_ERR,
            "%s: unable to read line from CSV file '%s'. (fread: %s)",
            myname, input_path, GetErrorStr());
        JsonDestroy(*json_out);
        return false;
    }

    return true;
}

JsonElement *JsonReadDataFile(const char *log_identifier, const char *input_path,
                          const char *requested_mode, size_t size_max)
{
    const char *myname = log_identifier ? log_identifier : "JsonReadDataFile";

    bool env_mode = (strcmp("ENV", requested_mode) == 0);
    bool csv_mode = (strcmp("CSV", requested_mode) == 0);
    //size_t size_max = 50 * (1024 * 1024);

    if (env_mode || csv_mode)
    {
        JsonElement *json = NULL;
        bool success;
        if (env_mode)
        {
            success = JsonParseEnvFile(input_path, size_max, &json);
        }
        else
        {
            success = JsonParseCsvFile(input_path, size_max, &json);
        }
        if (success == false)
        {
            return NULL;
        }
        return json;
    }

    bool yaml_mode = (strcmp(requested_mode, "YAML") == 0);
    const char *data_type = requested_mode;

    JsonElement *json = NULL;
    JsonParseError res;
    if (yaml_mode)
    {
        res = JsonParseYamlFile(input_path, size_max, &json);
    }
    else
    {
        res = JsonParseFile(input_path, size_max, &json);
    }

    // the NO_DATA errors often happen when the file hasn't been created yet
    if (res == JSON_PARSE_ERROR_NO_DATA)
    {
        Log(LOG_LEVEL_ERR, "%s: data error parsing %s file '%s': %s",
            myname, data_type, input_path, JsonParseErrorToString(res));
    }
    else if (res != JSON_PARSE_OK)
    {
        Log(LOG_LEVEL_ERR, "%s: error parsing %s file '%s': %s",
            myname, data_type, input_path, JsonParseErrorToString(res));
    }
    else if (JsonGetElementType(json) == JSON_ELEMENT_TYPE_PRIMITIVE)
    {
        Log(LOG_LEVEL_ERR, "%s: non-container from parsing %s file '%s'",myname, data_type, input_path);
        JsonDestroy(json);
    }
    else
    {
        return json;
    }

    return NULL;
}
