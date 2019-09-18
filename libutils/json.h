/*
   Copyright 2018 Northern.tech AS

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

#ifndef CFENGINE_JSON_H
#define CFENGINE_JSON_H

#include <regex.h>

/**
  @brief JSON data-structure.

  This is a JSON Document Object Model (DOM). Clients deal only with the opaque JsonElement, which may be either a container or
  a primitive (client should probably not deal much with primitive elements). A JSON container may be either an object or an array.
  The JSON DOM currently supports copy semantics for primitive values, but not for container types. In practice, this means that
  clients always just free the parent element, but an element should just have a single parent, or none.

  JSON primitives as JsonElement are currently not well supported.

  JSON DOM is currently built upon Sequence.
  The JSON specification may be found at @link http://www.json.org @endlink.

  @see Sequence
*/

typedef enum
{
    JSON_ELEMENT_TYPE_CONTAINER,
    JSON_ELEMENT_TYPE_PRIMITIVE
} JsonElementType;

typedef enum
{
    JSON_CONTAINER_TYPE_OBJECT,
    JSON_CONTAINER_TYPE_ARRAY
} JsonContainerType;

typedef enum
{
    JSON_PRIMITIVE_TYPE_STRING,
    JSON_PRIMITIVE_TYPE_INTEGER,
    JSON_PRIMITIVE_TYPE_REAL,
    JSON_PRIMITIVE_TYPE_BOOL,
    JSON_PRIMITIVE_TYPE_NULL
} JsonPrimitiveType;

typedef enum
{
    JSON_PARSE_OK = 0,

    JSON_PARSE_ERROR_STRING_NO_DOUBLEQUOTE_START,
    JSON_PARSE_ERROR_STRING_NO_DOUBLEQUOTE_END,

    JSON_PARSE_ERROR_NUMBER_EXPONENT_NEGATIVE,
    JSON_PARSE_ERROR_NUMBER_EXPONENT_POSITIVE,
    JSON_PARSE_ERROR_NUMBER_DUPLICATE_ZERO,
    JSON_PARSE_ERROR_NUMBER_NO_DIGIT,
    JSON_PARSE_ERROR_NUMBER_EXPONENT_DUPLICATE,
    JSON_PARSE_ERROR_NUMBER_EXPONENT_DIGIT,
    JSON_PARSE_ERROR_NUMBER_EXPONENT_FOLLOW_LEADING_ZERO,
    JSON_PARSE_ERROR_NUMBER_BAD_SYMBOL,
    JSON_PARSE_ERROR_NUMBER_DIGIT_END,

    JSON_PARSE_ERROR_ARRAY_START,
    JSON_PARSE_ERROR_ARRAY_END,
    JSON_PARSE_ERROR_ARRAY_COMMA,

    JSON_PARSE_ERROR_OBJECT_BAD_SYMBOL,
    JSON_PARSE_ERROR_OBJECT_START,
    JSON_PARSE_ERROR_OBJECT_END,
    JSON_PARSE_ERROR_OBJECT_COLON,
    JSON_PARSE_ERROR_OBJECT_COMMA,
    JSON_PARSE_ERROR_OBJECT_ARRAY_LVAL,
    JSON_PARSE_ERROR_OBJECT_OBJECT_LVAL,
    JSON_PARSE_ERROR_OBJECT_OPEN_LVAL,

    JSON_PARSE_ERROR_INVALID_START,
    JSON_PARSE_ERROR_NO_LIBYAML,
    JSON_PARSE_ERROR_LIBYAML_FAILURE,
    JSON_PARSE_ERROR_NO_DATA,
    JSON_PARSE_ERROR_TRUNCATED,

    JSON_PARSE_ERROR_MAX
} JsonParseError;

typedef struct JsonElement_ JsonElement;

#include <writer.h>

typedef struct
{
    const JsonElement *container;
    size_t index;
} JsonIterator;


/**
  @brief Create a new JSON object
  @param initial_capacity [in] The number of fields to preallocate space for.
  @returns A pointer to the created object.
  */
JsonElement *JsonObjectCreate(size_t initial_capacity);

/**
  @brief Create a new JSON array
  @param initial_capacity [in] The number of fields to preallocate space for.
  @returns The pointer to the created array.
  */
JsonElement *JsonArrayCreate(size_t initialCapacity);

/**
  @brief Create a new JSON string primitive.

  @param value [in] The string to base the primitive on. Will be copied.
  @returns The pointer to the created string primitive element.
  */
JsonElement *JsonStringCreate(const char *value);

JsonElement *JsonIntegerCreate(int value);
JsonElement *JsonRealCreate(double value);
JsonElement *JsonBoolCreate(bool value);
JsonElement *JsonNullCreate();

JsonElement *JsonCopy(const JsonElement *json);
int JsonCompare(const JsonElement *a, const JsonElement *b);
JsonElement *JsonMerge(const JsonElement *a, const JsonElement *b);

/**
  @brief Destroy a JSON element
  @param element [in] The JSON element to destroy.
  */
void JsonDestroy(JsonElement *element);

/**
  @brief Destroy a JSON element if needed
  @param element [in] The JSON element to destroy.
  @param allocated [in] Whether the element was allocated and needs to be destroyed.
  */
void JsonDestroyMaybe(JsonElement *element, bool allocated);

/**
  @brief Get the length of a JsonElement. This is the number of elements or fields in an array or object respectively.
  @param element [in] The JSON element.
  */
size_t JsonLength(const JsonElement *element);

JsonElementType JsonGetElementType(const JsonElement *element);
const char* JsonElementGetPropertyName(const JsonElement *element);

JsonContainerType JsonGetContainerType(const JsonElement *container);

JsonPrimitiveType JsonGetPrimitiveType(const JsonElement *primitive);
const char *JsonPrimitiveGetAsString(const JsonElement *primitive);
char* JsonPrimitiveToString(const JsonElement *primitive);
bool JsonPrimitiveGetAsBool(const JsonElement *primitive);
long JsonPrimitiveGetAsInteger(const JsonElement *primitive);
double JsonPrimitiveGetAsReal(const JsonElement *primitive);
const char *JsonGetPropertyAsString(const JsonElement *element);

/**
  @brief Pretty-print a JsonElement recursively into a Writer.  If it's a JsonObject, its children will be sorted to produce canonical JSON output, but the object's contents are not modified so it's still a const.
  @see Writer
  @param writer [in] The Writer object to use as a buffer.
  @param element [in] The JSON element to print.
  @param indent_level [in] The nesting level with which the printing should be done. This is mainly to allow the
  function to be called recursively. Clients will normally want to set this to 0.
  */
void JsonWrite(Writer *writer, const JsonElement *element, size_t indent_level);

void JsonWriteCompact(Writer *w, const JsonElement *element);

/**
  @brief Append a string field to an object.
  @param object [in] The JSON object parent.
  @param key [in] the key of the field.
  @param value [in] The value of the field.
  */
void JsonObjectAppendString(JsonElement *object, const char *key, const char *value);

/**
  @brief Append an integer field to an object.
  @param object [in] The JSON object parent.
  @param key [in] the key of the field.
  @param value [in] The value of the field.
  */
void JsonObjectAppendInteger(JsonElement *object, const char *key, int value);

/**
  @brief Append an real number field to an object.
  @param object [in] The JSON object parent.
  @param key [in] the key of the field.
  @param value [in] The value of the field.
  */
void JsonObjectAppendReal(JsonElement *object, const char *key, double value);

/**
  @param object [in] The JSON object parent.
  @param key [in] the key of the field.
  @param value [in] The value of the field.
  */
void JsonObjectAppendBool(JsonElement *object, const char *key, _Bool value);

/**
  @brief Append null field to an object.
  @param object [in] The JSON object parent.
  @param key [in] the key of the field.
  */
void JsonObjectAppendNull(JsonElement *object, const char *key);

/**
  @brief Append an array field to an object.
  @param object [in] The JSON object parent.
  @param key [in] the key of the field.
  @param value [in] The value of the field.
  */
void JsonObjectAppendArray(JsonElement *object, const char *key, JsonElement *array);

/**
  @brief Append an object field to an object.
  @param object [in] The JSON object parent.
  @param key [in] the key of the field.
  @param value [in] The value of the field.
  */
void JsonObjectAppendObject(JsonElement *object, const char *key, JsonElement *childObject);

/**
  @brief Append any JSON element to an object.
  @param object [in] The JSON object parent.
  @param key [in] the key of the field.
  @param element [in] The element to append
*/
void JsonObjectAppendElement(JsonElement *object, const char *key, JsonElement *element);

/**
  @brief Get the value of a field in an object, as a string.
  @param object [in] The JSON object parent.
  @param key [in] the key of the field.
  @returns A pointer to the string value, or NULL if non-existent.
  */
const char *JsonObjectGetAsString(const JsonElement *object, const char *key);

/**
  @brief Get the value of a field in an object, as an object.
  @param object [in] The JSON object parent.
  @param key [in] the key of the field.
  @returns A pointer to the object value, or NULL if non-existent.
  */
JsonElement *JsonObjectGetAsObject(JsonElement *object, const char *key);

/**
  @brief Get the value of a field in an object, as an array.
  @param object [in] The JSON object parent.
  @param key [in] the key of the field.
  @returns A pointer to the array value, or NULL if non-existent.
  */
JsonElement *JsonObjectGetAsArray(JsonElement *object, const char *key);

JsonElement *JsonObjectGet(const JsonElement *object, const char *key);

/**
  @brief Append a string to an array.
  @param array [in] The JSON array parent.
  @param value [in] The string value to append.
  */
void JsonArrayAppendString(JsonElement *array, const char *value);

void JsonArrayAppendBool(JsonElement *array, bool value);

/**
  @brief Append an integer to an array.
  @param array [in] The JSON array parent.
  @param value [in] The integer value to append.
  */
void JsonArrayAppendInteger(JsonElement *array, int value);

/**
  @brief Append an real to an array.
  @param array [in] The JSON array parent.
  @param value [in] The real value to append.
  */
void JsonArrayAppendReal(JsonElement *array, double value);

/**
  @brief Append null to an array.
  @param array [in] The JSON array parent.
  */
void JsonArrayAppendNull(JsonElement *array);

/**
  @brief Append an array to an array.
  @param array [in] The JSON array parent.
  @param child_array [in] The array value to append.
  */
void JsonArrayAppendArray(JsonElement *array, JsonElement *child_array);

/**
  @brief Append an object to an array.
  @param array [in] The JSON array parent.
  @param object [in] The object value to append.
  */
void JsonArrayAppendObject(JsonElement *array, JsonElement *object);

/**
  @brief Append any JSON element to an array.
  @param array [in] The JSON array parent.
  @param element [in] The object to append.
  */
void JsonArrayAppendElement(JsonElement *array, JsonElement *element);

/**
  @brief Remove an inclusive range from a JSON array.
  @see SequenceRemoveRange
  @param array [in] The JSON array parent.
  @param start [in] Index of the first element to remove.
  @param end [in] Index of the last element to remove.
  */
void JsonArrayRemoveRange(JsonElement *array, size_t start, size_t end);

void JsonContainerReverse(JsonElement *array);

/**
  @brief Get a string value from an array
  @param array [in] The JSON array parent
  @param index [in] Position of the value to get
  @returns A pointer to the string value, or NULL if non-existent.
  */
const char *JsonArrayGetAsString(JsonElement *array, size_t index);

/**
  @brief Get an object value from an array
  @param array [in] The JSON array parent
  @param index [in] Position of the value to get
  @returns A pointer to the object value, or NULL if non-existent.
  */
JsonElement *JsonArrayGetAsObject(JsonElement *array, size_t index);

JsonElement *JsonArrayGet(const JsonElement *array, size_t index);

/**
  @brief Check if an array contains only primitives
  @param array [in] The JSON array parent
  @returns true if the array contains only primitives, false otherwise
  */
bool JsonArrayContainsOnlyPrimitives(JsonElement *array);

typedef JsonElement *JsonLookup(void *ctx, const char **data);

/**
  @brief Parse a string to create a JsonElement
  @param data [in] Pointer to the string to parse
  @param json_out Resulting JSON object
  @returns See JsonParseError and JsonParseErrorToString
  */
JsonParseError JsonParse(const char **data, JsonElement **json_out);

/**
  @brief Parse a string to create a JsonElement
  @param lookup_data [in] Evaluation context for variable lookups
  @param lookup_function [in] Callback function for variable lookups
  @param data [in] Pointer to the string to parse
  @param json_out Resulting JSON object
  @returns See JsonParseError and JsonParseErrorToString

  The lookup_context type is void so we don't have to include
  eval_context.h from libpromises into libutil
  */
JsonParseError JsonParseWithLookup(void *lookup_data, JsonLookup *lookup_function, const char **data, JsonElement **json_out);

/**
 * @brief Convenience function to parse JSON from a file
 * @param path Path to the file
 * @param size_max Maximum size to read in memory
 * @param json_out Resulting JSON object
 * @return See JsonParseError and JsonParseErrorToString
 */
JsonParseError JsonParseFile(const char *path, size_t size_max, JsonElement **json_out);

const char* JsonParseErrorToString(JsonParseError error);

/**
  @brief Remove key from the object
  @param object containing the key property
  @param property name to be removed
  @return True if key was removed
  */
bool JsonObjectRemoveKey(JsonElement *object, const char *key);

/**
  @brief Detach json element ownership from parent object;
  @param object containing the key property
  @param property name to be detached
  */
JsonElement *JsonObjectDetachKey(JsonElement *object, const char *key);

typedef int JsonComparator(const JsonElement *, const JsonElement *, void *user_data);

void JsonSort(JsonElement *container, JsonComparator *Compare, void *user_data);
JsonElement *JsonAt(const JsonElement *container, size_t index);
JsonElement *JsonSelect(JsonElement *element, size_t num_indices, char **indices);

const char *JsonPrimitiveTypeToString(JsonPrimitiveType type);


JsonIterator JsonIteratorInit(const JsonElement *container);
const char *JsonIteratorNextKey(JsonIterator *iter);
const JsonElement *JsonIteratorNextValue(JsonIterator *iter);
const JsonElement *JsonIteratorNextValueByType(JsonIterator *iter, JsonElementType type, bool skip_null);
const char *JsonIteratorCurrentKey(const JsonIterator *iter);
const JsonElement *JsonIteratorCurrentValue(const JsonIterator *iter);
JsonElementType JsonIteratorCurrentElementType(const JsonIterator *iter);
JsonContainerType JsonIteratorCurrentContainerType(const JsonIterator *iter);
JsonPrimitiveType JsonIteratorCurrentPrimitiveType(const JsonIterator *iter);
bool JsonIteratorHasMore(const JsonIterator *iter);

JsonElement* StringCaptureData(pcre *pattern, const char* regex, const char* data);
char *JsonDecodeString(const char *encoded_string);

typedef struct _Slice
{
    // Slice is used to represent a section of memory which may or may not
    // contain NUL bytes. This is useful for storing the unescaped versions of
    // JSON(5) strings (which may have NUL bytes).

    void *data;  // Binary data here, not just ascii plain text
    size_t size; // Allocated size in bytes (or shorter if you shrink later)
} Slice;

char *Json5EscapeData(Slice unescaped_data);

#endif
