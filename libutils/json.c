/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "json.h"

#include "alloc.h"
#include "sequence.h"
#include "string_lib.h"

#include <assert.h>

static const int SPACES_PER_INDENT = 2;
static const int DEFAULT_CONTAINER_CAPACITY = 64;

static const char *JSON_TRUE = "true";
static const char *JSON_FALSE = "false";
static const char *JSON_NULL = "null";

struct JsonElement_
{
    JsonElementType type;
    char *propertyName;         // to avoid having a struct JsonProperty
    union
    {
        struct JsonContainer
        {
            JsonContainerType type;
            Seq *children;
        } container;
        struct JsonPrimitive
        {
            JsonPrimitiveType type;
            const char *value;
        } primitive;
    };
};

// *******************************************************************************************
// JsonElement Functions
// *******************************************************************************************

static void JsonElementSetPropertyName(JsonElement *element, const char *propertyName)
{
    assert(element);

    if (element->propertyName)
    {
        free(element->propertyName);
        element->propertyName = NULL;
    }

    if (propertyName)
    {
        element->propertyName = xstrdup(propertyName);
    }
}

static JsonElement *JsonElementCreateContainer(JsonContainerType containerType, const char *propertyName,
                                               size_t initialCapacity)
{
    JsonElement *element = xcalloc(1, sizeof(JsonElement));

    element->type = JSON_ELEMENT_TYPE_CONTAINER;

    JsonElementSetPropertyName(element, propertyName);

    element->container.type = containerType;
    element->container.children = SeqNew(initialCapacity, JsonElementDestroy);

    return element;
}

static JsonElement *JsonElementCreatePrimitive(JsonPrimitiveType primitiveType,
                                               const char *value)
{
    JsonElement *element = xcalloc(1, sizeof(JsonElement));

    element->type = JSON_ELEMENT_TYPE_PRIMITIVE;

    element->primitive.type = primitiveType;
    element->primitive.value = value;

    return element;
}

void JsonElementDestroy(JsonElement *element)
{
    assert(element);

    switch (element->type)
    {
    case JSON_ELEMENT_TYPE_CONTAINER:
        assert(element->container.children);
        SeqDestroy(element->container.children);
        element->container.children = NULL;
        break;

    case JSON_ELEMENT_TYPE_PRIMITIVE:
        assert(element->primitive.value);

        if (element->primitive.type != JSON_PRIMITIVE_TYPE_NULL &&
            element->primitive.type != JSON_PRIMITIVE_TYPE_BOOL)
        {
            free((void *) element->primitive.value);
        }
        element->primitive.value = NULL;
        break;
    }

    if (element->propertyName)
    {
        free(element->propertyName);
    }

    free(element);
}

size_t JsonElementLength(const JsonElement *element)
{
    assert(element);

    switch (element->type)
    {
    case JSON_ELEMENT_TYPE_CONTAINER:
        return element->container.children->length;

    case JSON_ELEMENT_TYPE_PRIMITIVE:
        return strlen(element->primitive.value);
    }

    return -1;                  // appease gcc
}

JsonIterator JsonIteratorInit(const JsonElement *container)
{
    assert(container);
    assert(container->type == JSON_ELEMENT_TYPE_CONTAINER);

    return (JsonIterator) { container, 0 };
}

const char *JsonIteratorNextKey(JsonIterator *iter)
{
    assert(iter);
    assert(iter->container->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(iter->container->container.type == JSON_CONTAINER_TYPE_OBJECT);

    const JsonElement *child = JsonIteratorNextValue(iter);
    return child ? child->propertyName : NULL;
}

const JsonElement *JsonIteratorNextValue(JsonIterator *iter)
{
    assert(iter);
    assert(iter->container->type == JSON_ELEMENT_TYPE_CONTAINER);

    if (iter->index >= JsonElementLength(iter->container))
    {
        return NULL;
    }

    return iter->container->container.children->data[iter->index++];
}

const JsonElement *JsonIteratorCurrentValue(JsonIterator *iter)
{
    assert(iter);
    assert(iter->container->type == JSON_ELEMENT_TYPE_CONTAINER);

    if (iter->index > JsonElementLength(iter->container))
    {
        return NULL;
    }

    return iter->container->container.children->data[(iter->index) - 1];
}

const char *JsonIteratorCurrentKey(JsonIterator *iter)
{
    assert(iter);
    assert(iter->container->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(iter->container->container.type == JSON_CONTAINER_TYPE_OBJECT);

    const JsonElement *child = JsonIteratorCurrentValue(iter);

    return child ? child->propertyName : NULL;
}

JsonElementType JsonIteratorCurrentElementType(JsonIterator *iter)
{
    assert(iter);

    const JsonElement *child = JsonIteratorCurrentValue(iter);
    return child->type;
}

JsonContainerType JsonIteratorCurrentContainerType(JsonIterator *iter)
{
    assert(iter);

    const JsonElement *child = JsonIteratorCurrentValue(iter);
    assert(child->type == JSON_ELEMENT_TYPE_CONTAINER);

    return child->container.type;
}

JsonPrimitiveType JsonIteratorCurrentPrimitiveType(JsonIterator *iter)
{
    assert(iter);

    const JsonElement *child = JsonIteratorCurrentValue(iter);
    assert(child->type == JSON_ELEMENT_TYPE_PRIMITIVE);

    return child->primitive.type;
}

JsonElementType JsonGetElementType(const JsonElement *element)
{
    assert(element);
    return element->type;
}

JsonContainerType JsonGetContrainerType(const JsonElement *container)
{
    assert(container);
    assert(container->type == JSON_ELEMENT_TYPE_CONTAINER);

    return container->container.type;
}

JsonPrimitiveType JsonGetPrimitiveType(const JsonElement *primitive)
{
    assert(primitive);
    assert(primitive->type == JSON_ELEMENT_TYPE_PRIMITIVE);

    return primitive->primitive.type;
}

const char *JsonPrimitiveGetAsString(const JsonElement *primitive)
{
    assert(primitive);
    assert(primitive->type == JSON_ELEMENT_TYPE_PRIMITIVE);

    return primitive->primitive.value;
}

bool JsonPrimitiveGetAsBool(const JsonElement *primitive)
{
    assert(primitive);
    assert(primitive->type == JSON_ELEMENT_TYPE_PRIMITIVE);
    assert(primitive->primitive.type == JSON_PRIMITIVE_TYPE_BOOL);

    return StringSafeEqual(JSON_TRUE, primitive->primitive.value);
}

long JsonPrimitiveGetAsInteger(const JsonElement *primitive)
{
    assert(primitive);
    assert(primitive->type == JSON_ELEMENT_TYPE_PRIMITIVE);
    assert(primitive->primitive.type == JSON_PRIMITIVE_TYPE_INTEGER);

    return StringToLong(primitive->primitive.value);
}

double JsonPrimitiveGetAsReal(const JsonElement *primitive)
{
    assert(primitive);
    assert(primitive->type == JSON_ELEMENT_TYPE_PRIMITIVE);
    assert(primitive->primitive.type == JSON_PRIMITIVE_TYPE_REAL);

    return StringToDouble(primitive->primitive.value);
}

const char *JsonGetPropertyAsString(const JsonElement *element)
{
    assert(element);

    return element->propertyName;
}

void JsonSort(JsonElement *container, JsonComparator *Compare, void *user_data)
{
    assert(container);
    assert(container->type == JSON_ELEMENT_TYPE_CONTAINER);

    SeqSort(container->container.children, (SeqItemComparator)Compare, user_data);
}

JsonElement *JsonAt(const JsonElement *container, size_t index)
{
    assert(container);
    assert(container->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(index < JsonElementLength(container));

    return container->container.children->data[index];
}

// *******************************************************************************************
// JsonObject Functions
// *******************************************************************************************

JsonElement *JsonObjectCreate(size_t initialCapacity)
{
    return JsonElementCreateContainer(JSON_CONTAINER_TYPE_OBJECT, NULL, initialCapacity);
}

static const char *EscapeJsonString(const char *unescapedString)
{
    assert(unescapedString);

    Writer *writer = StringWriter();

    for (const char *c = unescapedString; *c != '\0'; c++)
    {
        switch (*c)
        {
            case '\"':
            case '\\':
                WriterWriteChar(writer, '\\');
                WriterWriteChar(writer, *c);
                break;
            case '\b':
                WriterWriteChar(writer, '\\');
                WriterWriteChar(writer, 'b');
                break;
            case '\f':
                WriterWriteChar(writer, '\\');
                WriterWriteChar(writer, 'f');
                break;
            case '\n':
                WriterWriteChar(writer, '\\');
                WriterWriteChar(writer, 'n');
                break;
            case '\r':
                WriterWriteChar(writer, '\\');
                WriterWriteChar(writer, 'r');
                break;
            case '\t':
                WriterWriteChar(writer, '\\');
                WriterWriteChar(writer, 't');
                break;
            default:
                WriterWriteChar(writer, *c);
        }
    }

    return StringWriterClose(writer);
}

void JsonObjectAppendString(JsonElement *object, const char *key, const char *value)
{
    JsonElement *child = JsonStringCreate(value);
    JsonObjectAppendElement(object, key, child);
}

void JsonObjectAppendInteger(JsonElement *object, const char *key, int value)
{
    JsonElement *child = JsonIntegerCreate(value);
    JsonObjectAppendElement(object, key, child);
}

void JsonObjectAppendBool(JsonElement *object, const char *key, _Bool value)
{
    JsonElement *child = JsonBoolCreate(value);
    JsonObjectAppendElement(object, key, child);
}

void JsonObjectAppendReal(JsonElement *object, const char *key, double value)
{
    JsonElement *child = JsonRealCreate(value);
    JsonObjectAppendElement(object, key, child);
}

void JsonObjectAppendNull(JsonElement *object, const char *key)
{
    JsonElement *child = JsonNullCreate();
    JsonObjectAppendElement(object, key, child);
}

void JsonObjectAppendArray(JsonElement *object, const char *key, JsonElement *array)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    JsonObjectAppendElement(object, key, array);
}

void JsonObjectAppendObject(JsonElement *object, const char *key, JsonElement *childObject)
{
    assert(childObject);
    assert(childObject->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(childObject->container.type == JSON_CONTAINER_TYPE_OBJECT);

    JsonObjectAppendElement(object, key, childObject);
}

void JsonObjectAppendElement(JsonElement *object, const char *key, JsonElement *element)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);
    assert(element);

    JsonElementSetPropertyName(element, key);
    SeqAppend(object->container.children, element);
}

static int JsonElementHasProperty(const void *propertyName, const void *jsonElement, ARG_UNUSED void *user_data)
{
    assert(propertyName);

    const JsonElement *element = jsonElement;

    assert(element->propertyName);

    if (strcmp(propertyName, element->propertyName) == 0)
    {
        return 0;
    }
    return -1;
}

static int CompareKeyToPropertyName(const void *a, const void *b, ARG_UNUSED void *user_data)
{
    return StringSafeCompare((char*)a, ((JsonElement*)b)->propertyName);
}

static size_t JsonElementIndexInParentObject(JsonElement *parent, const char* key)
{
    assert(parent);
    assert(parent->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(parent->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    return SeqIndexOf(parent->container.children, key, CompareKeyToPropertyName);
}

void JsonObjectRemoveKey(JsonElement *object, const char *key)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    size_t index = JsonElementIndexInParentObject(object, key);
    if (index != -1)
    {
        SeqRemove(object->container.children, index);
    }
}

JsonElement *JsonObjectDetachKey(JsonElement *object, const char *key)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    JsonElement *detached = NULL;

    size_t index = JsonElementIndexInParentObject(object, key);
    if (index != -1)
    {
        detached = SeqLookup(object->container.children, key, JsonElementHasProperty);
        SeqSoftRemove(object->container.children, index);
    }

    return detached;
}

const char *JsonObjectGetAsString(JsonElement *object, const char *key)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    JsonElement *childPrimitive = SeqLookup(object->container.children, key, JsonElementHasProperty);

    if (childPrimitive)
    {
        assert(childPrimitive->type == JSON_ELEMENT_TYPE_PRIMITIVE);
        assert(childPrimitive->primitive.type == JSON_PRIMITIVE_TYPE_STRING);
        return childPrimitive->primitive.value;
    }

    return NULL;
}

JsonElement *JsonObjectGetAsObject(JsonElement *object, const char *key)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    JsonElement *childPrimitive = SeqLookup(object->container.children, key, JsonElementHasProperty);

    if (childPrimitive)
    {
        assert(childPrimitive->type == JSON_ELEMENT_TYPE_CONTAINER);
        assert(childPrimitive->container.type == JSON_CONTAINER_TYPE_OBJECT);
        return childPrimitive;
    }

    return NULL;
}

JsonElement *JsonObjectGetAsArray(JsonElement *object, const char *key)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    JsonElement *childPrimitive = SeqLookup(object->container.children, key, JsonElementHasProperty);

    if (childPrimitive)
    {
        assert(childPrimitive->type == JSON_ELEMENT_TYPE_CONTAINER);
        assert(childPrimitive->container.type == JSON_CONTAINER_TYPE_ARRAY);
        return childPrimitive;
    }

    return NULL;
}

JsonElement *JsonObjectGet(JsonElement *object, const char *key)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    return SeqLookup(object->container.children, key, JsonElementHasProperty);
}

// *******************************************************************************************
// JsonArray Functions
// *******************************************************************************************

JsonElement *JsonArrayCreate(size_t initialCapacity)
{
    return JsonElementCreateContainer(JSON_CONTAINER_TYPE_ARRAY, NULL, initialCapacity);
}

void JsonArrayAppendString(JsonElement *array, const char *value)
{
    JsonElement *child = JsonStringCreate(value);
    JsonArrayAppendElement(array, child);
}

void JsonArrayAppendBool(JsonElement *array, bool value)
{
    JsonElement *child = JsonBoolCreate(value);
    JsonArrayAppendElement(array, child);
}

void JsonArrayAppendInteger(JsonElement *array, int value)
{
    JsonElement *child = JsonIntegerCreate(value);
    JsonArrayAppendElement(array, child);
}

void JsonArrayAppendReal(JsonElement *array, double value)
{
    JsonElement *child = JsonRealCreate(value);
    JsonArrayAppendElement(array, child);
}

void JsonArrayAppendNull(JsonElement *array)
{
    JsonElement *child = JsonNullCreate();
    JsonArrayAppendElement(array, child);
}

void JsonArrayAppendArray(JsonElement *array, JsonElement *childArray)
{
    assert(childArray);
    assert(childArray->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(childArray->container.type == JSON_CONTAINER_TYPE_ARRAY);

    JsonArrayAppendElement(array, childArray);
}

void JsonArrayAppendObject(JsonElement *array, JsonElement *object)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);

    JsonArrayAppendElement(array, object);
}

void JsonArrayAppendElement(JsonElement *array, JsonElement *element)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);
    assert(element);

    SeqAppend(array->container.children, element);
}

void JsonArrayRemoveRange(JsonElement *array, size_t start, size_t end)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);
    assert(end < array->container.children->length);
    assert(start <= end);

    SeqRemoveRange(array->container.children, start, end);
}

const char *JsonArrayGetAsString(JsonElement *array, size_t index)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);
    assert(index < array->container.children->length);

    JsonElement *childPrimitive = array->container.children->data[index];

    if (childPrimitive)
    {
        assert(childPrimitive->type == JSON_ELEMENT_TYPE_PRIMITIVE);
        assert(childPrimitive->primitive.type == JSON_PRIMITIVE_TYPE_STRING);
        return childPrimitive->primitive.value;
    }

    return NULL;
}

JsonElement *JsonArrayGetAsObject(JsonElement *array, size_t index)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);
    assert(index < array->container.children->length);

    JsonElement *child = array->container.children->data[index];

    if (child)
    {
        assert(child->type == JSON_ELEMENT_TYPE_CONTAINER);
        assert(child->container.type == JSON_CONTAINER_TYPE_OBJECT);
        return child;
    }

    return NULL;
}

void JsonContainerReverse(JsonElement *array)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    SeqReverse(array->container.children);
}

// *******************************************************************************************
// Primitive Functions
// *******************************************************************************************

JsonElement *JsonStringCreate(const char *value)
{
    assert(value);
    return JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_STRING, EscapeJsonString(value));
}

JsonElement *JsonIntegerCreate(int value)
{
    char *buffer = xcalloc(32, sizeof(char));
    snprintf(buffer, 32, "%d", value);

    return JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_INTEGER, buffer);
}

JsonElement *JsonRealCreate(double value)
{
    if (isnan(value) || !isfinite(value))
    {
        value = 0.0;
    }

    char *buffer = xcalloc(32, sizeof(char));
    snprintf(buffer, 32, "%.4f", value);

    return  JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_REAL, buffer);
}

JsonElement *JsonBoolCreate(bool value)
{
    return  JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_BOOL, value ? JSON_TRUE : JSON_FALSE);
}

JsonElement *JsonNullCreate()
{
    return JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_NULL, JSON_NULL);
}

// *******************************************************************************************
// Printing
// *******************************************************************************************

static void JsonContainerPrint(Writer *writer, JsonElement *containerElement, size_t indent_level);

static bool IsWhitespace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static bool IsSeparator(char ch)
{
    return IsWhitespace(ch) || ch == ',' || ch == ']' || ch == '}';
}

static bool IsDigit(char ch)
{
    // [1,9]
    return ch >= 49 && ch <= 57;
}

static void PrintIndent(Writer *writer, int num)
{
    int i = 0;

    for (i = 0; i < num * SPACES_PER_INDENT; i++)
    {
        WriterWriteChar(writer, ' ');
    }
}

static void JsonPrimitivePrint(Writer *writer, JsonElement *primitiveElement, size_t indent_level)
{
    assert(primitiveElement->type == JSON_ELEMENT_TYPE_PRIMITIVE);

    switch (primitiveElement->primitive.type)
    {
    case JSON_PRIMITIVE_TYPE_STRING:
        PrintIndent(writer, indent_level);
        WriterWriteF(writer, "\"%s\"", primitiveElement->primitive.value);
        break;

    default:
        PrintIndent(writer, indent_level);
        WriterWrite(writer, primitiveElement->primitive.value);
        break;        
    }
}

static void JsonArrayPrint(Writer *writer, JsonElement *array, size_t indent_level)
{
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    if (JsonElementLength(array) == 0)
    {
        WriterWrite(writer, "[]");
        return;
    }

    WriterWrite(writer, "[\n");
    for (size_t i = 0; i < array->container.children->length; i++)
    {
        JsonElement *child = array->container.children->data[i];

        switch (child->type)
        {
        case JSON_ELEMENT_TYPE_PRIMITIVE:
            JsonPrimitivePrint(writer, child, indent_level + 1);
            break;

        case JSON_ELEMENT_TYPE_CONTAINER:
            PrintIndent(writer, indent_level + 1);
            JsonContainerPrint(writer, child, indent_level + 1);
            break;
        }

        if (i < array->container.children->length - 1)
        {
            WriterWrite(writer, ",\n");
        }
        else
        {
            WriterWrite(writer, "\n");
        }
    }

    PrintIndent(writer, indent_level);
    WriterWriteChar(writer, ']');
}

void JsonObjectPrint(Writer *writer, JsonElement *object, size_t indent_level)
{
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);

    WriterWrite(writer, "{\n");

    for (size_t i = 0; i < object->container.children->length; i++)
    {
        JsonElement *child = object->container.children->data[i];

        PrintIndent(writer, indent_level + 1);

        assert(child->propertyName);
        WriterWriteF(writer, "\"%s\": ", child->propertyName);

        switch (child->type)
        {
        case JSON_ELEMENT_TYPE_PRIMITIVE:
            JsonPrimitivePrint(writer, child, 0);
            break;

        case JSON_ELEMENT_TYPE_CONTAINER:
            JsonContainerPrint(writer, child, indent_level + 1);
            break;
        }

        if (i < object->container.children->length - 1)
        {
            WriterWriteChar(writer, ',');
        }
        WriterWrite(writer, "\n");
    }

    PrintIndent(writer, indent_level);
    WriterWriteChar(writer, '}');
}

static void JsonContainerPrint(Writer *writer, JsonElement *container, size_t indent_level)
{
    assert(container->type == JSON_ELEMENT_TYPE_CONTAINER);

    switch (container->container.type)
    {
    case JSON_CONTAINER_TYPE_OBJECT:
        JsonObjectPrint(writer, container, indent_level);
        break;

    case JSON_CONTAINER_TYPE_ARRAY:
        JsonArrayPrint(writer, container, indent_level);
    }
}

void JsonElementPrint(Writer *writer, JsonElement *element, size_t indent_level)
{
    assert(writer);
    assert(element);

    switch (element->type)
    {
    case JSON_ELEMENT_TYPE_CONTAINER:
        JsonContainerPrint(writer, element, indent_level);
        break;

    case JSON_ELEMENT_TYPE_PRIMITIVE:
        JsonPrimitivePrint(writer, element, indent_level);
        break;
    }
}

// *******************************************************************************************
// Parsing
// *******************************************************************************************

static JsonParseError JsonParseAsObject(const char **data, JsonElement **json_out);

static JsonElement *JsonParseAsBoolean(const char **data)
{
    if (StringMatch("^true", *data))
    {
        char next = *(*data + 4);
        if (IsSeparator(next) || next == '\0')
        {
            *data += 3;
            return JsonBoolCreate(true);
        }
    }
    else if (StringMatch("^false", *data))
    {
        char next = *(*data + 5);
        if (IsSeparator(next) || next == '\0')
        {
            *data += 4;
            return JsonBoolCreate(false);
        }
    }

    return NULL;
}

static JsonElement *JsonParseAsNull(const char **data)
{
    if (StringMatch("^null", *data))
    {
        char next = *(*data + 4);
        if (IsSeparator(next) || next == '\0')
        {
            *data += 3;
            return JsonNullCreate();
        }
    }

    return NULL;
}

const char* JsonParseErrorToString(JsonParseError error)
{
    static const char *parse_errors[JSON_PARSE_ERROR_MAX] =
    {
        [JSON_PARSE_OK] = "Success",

        [JSON_PARSE_ERROR_STRING_NO_DOUBLEQUOTE_START] = "Unable to parse json data as string, did not start with doublequote",
        [JSON_PARSE_ERROR_STRING_NO_DOUBLEQUOTE_END] = "Unable to parse json data as string, did not end with doublequote",

        [JSON_PARSE_ERROR_NUMBER_EXPONENT_NEGATIVE] = "Unable to parse json data as number, - not at the start or not after exponent",
        [JSON_PARSE_ERROR_NUMBER_EXPONENT_POSITIVE] = "Unable to parse json data as number, + without preceding exponent",
        [JSON_PARSE_ERROR_NUMBER_DUPLICATE_ZERO] = "Unable to parse json data as number, started with 0 before dot or exponent, duplicate 0 seen",
        [JSON_PARSE_ERROR_NUMBER_NO_DIGIT] = "Unable to parse json data as number, dot not preceded by digit",
        [JSON_PARSE_ERROR_NUMBER_EXPONENT_DUPLICATE] = "Unable to parse json data as number, duplicate exponent",
        [JSON_PARSE_ERROR_NUMBER_EXPONENT_DIGIT] = "Unable to parse json data as number, exponent without preceding digit",
        [JSON_PARSE_ERROR_NUMBER_EXPONENT_FOLLOW_LEADING_ZERO] = "Unable to parse json data as number, dot or exponent must follow leading 0",
        [JSON_PARSE_ERROR_NUMBER_BAD_SYMBOL] = "Unable to parse json data as number, invalid symbol",
        [JSON_PARSE_ERROR_NUMBER_DIGIT_END] = "Unable to parse json data as string, did not end with digit",

        [JSON_PARSE_ERROR_ARRAY_START] = "Unable to parse json data as array, did not start with '['",
        [JSON_PARSE_ERROR_ARRAY_END] = "Unable to parse json data as array, did not end with ']'",

        [JSON_PARSE_ERROR_OBJECT_BAD_SYMBOL] = "Unable to parse json data as object, unrecognized token beginning entry",
        [JSON_PARSE_ERROR_OBJECT_START] = "Unable to parse json data as object, did not start with '{'",
        [JSON_PARSE_ERROR_OBJECT_END] = "Unable to parse json data as string, did not end with '}'",
        [JSON_PARSE_ERROR_OBJECT_COLON] = "Unable to parse json data as object, ':' seen without having specified an l-value",
        [JSON_PARSE_ERROR_OBJECT_COMMA] = "Unable to parse json data as object, ',' seen without having specified an r-value",
        [JSON_PARSE_ERROR_OBJECT_ARRAY_LVAL] = "Unable to parse json data as object, array not allowed as l-value",
        [JSON_PARSE_ERROR_OBJECT_OBJECT_LVAL] = "Unable to parse json data as object, object not allowed as l-value",
        [JSON_PARSE_ERROR_OBJECT_OPEN_LVAL] = "Unable to parse json data as object, tried to close object having opened an l-value",

        [JSON_PARSE_ERROR_INVALID_START] = "Unwilling to parse json data starting with invalid character",
        [JSON_PARSE_ERROR_NO_DATA] = "No data"
    };

    assert(error < JSON_PARSE_ERROR_MAX);
    return parse_errors[error];
}

static JsonParseError JsonParseAsString(const char **data, char **str_out)
{
    if (**data != '"')
    {
        *str_out = NULL;
        return JSON_PARSE_ERROR_STRING_NO_DOUBLEQUOTE_START;
    }

    Writer *writer = StringWriter();

    for (*data = *data + 1; **data != '\0'; *data = *data + 1)
    {
        if (**data == '"' && *(*data - 1) != '\\')
        {
            *str_out = StringWriterClose(writer);
            return JSON_PARSE_OK;
        }

        /* unescaping input strings */
        if (**data == '\\' &&
                (*(*data + 1) == '\"' ||
                 *(*data + 1) == '\\' ||
                 *(*data + 1) == '\b' ||
                 *(*data + 1) == '\f' ||
                 *(*data + 1) == '\n' ||
                 *(*data + 1) == '\r' ||
                 *(*data + 1) == '\t'))
        {
            continue;
        }

        WriterWriteChar(writer, **data);
    }

    WriterClose(writer);
    *str_out = NULL;
    return JSON_PARSE_ERROR_STRING_NO_DOUBLEQUOTE_END;
}

static JsonParseError JsonParseAsNumber(const char **data, JsonElement **json_out)
{
    Writer *writer = StringWriter();

    bool zero_started = false;
    bool seen_dot = false;
    bool seen_exponent = false;

    char prev_char = 0;

    for (*data = *data; **data != '\0' && !IsSeparator(**data); prev_char = **data, *data = *data + 1)
    {
        switch (**data)
        {
        case '-':
            if (prev_char != 0 && prev_char != 'e' && prev_char != 'E')
            {
                *json_out = NULL;
                WriterClose(writer);
                return JSON_PARSE_ERROR_NUMBER_EXPONENT_NEGATIVE;
            }
            break;

        case '+':
            if (prev_char != 'e' && prev_char != 'E')
            {
                *json_out = NULL;
                WriterClose(writer);
                return JSON_PARSE_ERROR_NUMBER_EXPONENT_POSITIVE;
            }
            break;

        case '0':
            if (zero_started && !seen_dot && !seen_exponent)
            {
                *json_out = NULL;
                WriterClose(writer);
                return JSON_PARSE_ERROR_NUMBER_DUPLICATE_ZERO;
            }
            if (prev_char == 0)
            {
                zero_started = true;
            }
            break;

        case '.':
            if (prev_char != '0' && !IsDigit(prev_char))
            {
                *json_out = NULL;
                WriterClose(writer);
                return JSON_PARSE_ERROR_NUMBER_NO_DIGIT;
            }
            seen_dot = true;
            break;

        case 'e':
        case 'E':
            if (seen_exponent)
            {
                *json_out = NULL;
                WriterClose(writer);
                return JSON_PARSE_ERROR_NUMBER_EXPONENT_DUPLICATE;
            }
            else if (!IsDigit(prev_char) && prev_char != '0')
            {
                *json_out = NULL;
                WriterClose(writer);
                return JSON_PARSE_ERROR_NUMBER_EXPONENT_DIGIT;
            }
            seen_exponent = true;
            break;

        default:
            if (zero_started && !seen_dot && !seen_exponent)
            {
                *json_out = NULL;
                WriterClose(writer);
                return JSON_PARSE_ERROR_NUMBER_EXPONENT_FOLLOW_LEADING_ZERO;
            }

            if (!IsDigit(**data))
            {
                *json_out = NULL;
                WriterClose(writer);
                return JSON_PARSE_ERROR_NUMBER_BAD_SYMBOL;
            }
            break;
        }

        WriterWriteChar(writer, **data);
    }

    if (prev_char != '0' && !IsDigit(prev_char))
    {
        *json_out = NULL;
        WriterClose(writer);
        return JSON_PARSE_ERROR_NUMBER_DIGIT_END;
    }

    // rewind 1 char so caller will see separator next
    *data = *data - 1;

    if (seen_dot)
    {
        *json_out = JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_REAL, StringWriterClose(writer));
        return JSON_PARSE_OK;
    }
    else
    {
        *json_out = JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_INTEGER, StringWriterClose(writer));
        return JSON_PARSE_OK;
    }
}

static JsonParseError JsonParseAsArray(const char **data, JsonElement **json_out)
{
    if (**data != '[')
    {
        *json_out = NULL;
        return JSON_PARSE_ERROR_ARRAY_START;
    }

    JsonElement *array = JsonArrayCreate(DEFAULT_CONTAINER_CAPACITY);

    for (*data = *data + 1; **data != '\0'; *data = *data + 1)
    {
        if (IsWhitespace(**data))
        {
            continue;
        }

        switch (**data)
        {
        case '"':
            {
                char *value = NULL;
                JsonParseError err = JsonParseAsString(data, &value);
                if (err != JSON_PARSE_OK)
                {
                    return err;
                }
                JsonArrayAppendString(array, value);
                free(value);
            }
            break;

        case '[':
            {
                JsonElement *child_array = NULL;
                JsonParseError err = JsonParseAsArray(data, &child_array);
                if (err != JSON_PARSE_OK)
                {
                    JsonElementDestroy(array);
                    return err;
                }
                assert(child_array);

                JsonArrayAppendArray(array, child_array);
            }
            break;

        case '{':
            {
                JsonElement *child_object = NULL;
                JsonParseError err = JsonParseAsObject(data, &child_object);
                if (err != JSON_PARSE_OK)
                {
                    JsonElementDestroy(array);
                    return err;
                }
                assert(child_object);

                JsonArrayAppendObject(array, child_object);
            }
            break;

        case ',':
            break;

        case ']':
            *json_out = array;
            return JSON_PARSE_OK;

        default:
            if (**data == '-' || **data == '0' || IsDigit(**data))
            {
                JsonElement *child = NULL;
                JsonParseError err = JsonParseAsNumber(data, &child);
                if (err != JSON_PARSE_OK)
                {
                    JsonElementDestroy(array);
                    return err;
                }
                assert(child);

                JsonArrayAppendElement(array, child);
                break;
            }

            JsonElement *child_bool = JsonParseAsBoolean(data);
            if (child_bool)
            {
                JsonArrayAppendElement(array, child_bool);
                break;
            }

            JsonElement *child_null = JsonParseAsNull(data);
            if (child_null)
            {
                JsonArrayAppendElement(array, child_null);
                break;
            }

            *json_out = NULL;
            JsonElementDestroy(array);
            return JSON_PARSE_ERROR_OBJECT_BAD_SYMBOL;
        }
    }

    *json_out = NULL;
    JsonElementDestroy(array);
    return JSON_PARSE_ERROR_ARRAY_END;
}

static JsonParseError JsonParseAsObject(const char **data, JsonElement **json_out)
{
    if (**data != '{')
    {
        *json_out = NULL;
        return JSON_PARSE_ERROR_ARRAY_START;
    }

    JsonElement *object = JsonObjectCreate(DEFAULT_CONTAINER_CAPACITY);
    char *property_name = NULL;

    for (*data = *data + 1; **data != '\0'; *data = *data + 1)
    {
        if (IsWhitespace(**data))
        {
            continue;
        }

        switch (**data)
        {
        case '"':
            if (property_name != NULL)
            {
                char *property_value = NULL;
                JsonParseError err = JsonParseAsString(data, &property_value);
                if (err != JSON_PARSE_OK)
                {
                    free(property_name);
                    JsonElementDestroy(object);
                    return err;
                }
                assert(property_value);

                JsonObjectAppendString(object, property_name, property_value);
                free(property_value);
                free(property_name);
                property_name = NULL;
            }
            else
            {
                property_name = NULL;
                JsonParseError err = JsonParseAsString(data, &property_name);
                if (err != JSON_PARSE_OK)
                {
                    JsonElementDestroy(object);
                    return err;
                }
                assert(property_name);
            }
            break;

        case ':':
            if (property_name == NULL)
            {
                json_out = NULL;
                free(property_name);
                JsonElementDestroy(object);
                return JSON_PARSE_ERROR_OBJECT_COLON;
            }
            break;

        case ',':
            if (property_name != NULL)
            {
                free(property_name);
                JsonElementDestroy(object);
                return JSON_PARSE_ERROR_OBJECT_COMMA;
            }
            break;

        case '[':
            if (property_name != NULL)
            {
                JsonElement *child_array = NULL;
                JsonParseError err = JsonParseAsArray(data, &child_array);
                if (err != JSON_PARSE_OK)
                {
                    free(property_name);
                    JsonElementDestroy(object);
                    return err;
                }

                JsonObjectAppendArray(object, property_name, child_array);
                free(property_name);
                property_name = NULL;
            }
            else
            {
                free(property_name);
                JsonElementDestroy(object);
                return JSON_PARSE_ERROR_OBJECT_ARRAY_LVAL;
            }
            break;

        case '{':
            if (property_name != NULL)
            {
                JsonElement *child_object = NULL;
                JsonParseError err = JsonParseAsObject(data, &child_object);
                if (err != JSON_PARSE_OK)
                {
                    free(property_name);
                    JsonElementDestroy(object);
                    return err;
                }

                JsonObjectAppendObject(object, property_name, child_object);
                free(property_name);
                property_name = NULL;
            }
            else
            {
                *json_out = NULL;
                free(property_name);
                JsonElementDestroy(object);
                return JSON_PARSE_ERROR_OBJECT_OBJECT_LVAL;
            }
            break;

        case '}':
            if (property_name != NULL)
            {
                *json_out = NULL;
                free(property_name);
                JsonElementDestroy(object);
                return JSON_PARSE_ERROR_OBJECT_OPEN_LVAL;
            }
            free(property_name);
            *json_out = object;
            return JSON_PARSE_OK;

        default:
            if (property_name)
            {
                if (**data == '-' || **data == '0' || IsDigit(**data))
                {
                    JsonElement *child = NULL;
                    JsonParseError err = JsonParseAsNumber(data, &child);
                    if (err != JSON_PARSE_OK)
                    {
                        free(property_name);
                        JsonElementDestroy(object);
                        return err;
                    }
                    JsonObjectAppendElement(object, property_name, child);
                    free(property_name);
                    property_name = NULL;
                    break;
                }

                JsonElement *child_bool = JsonParseAsBoolean(data);
                if (child_bool)
                {
                    JsonObjectAppendElement(object, property_name, child_bool);
                    free(property_name);
                    property_name = NULL;
                    break;
                }

                JsonElement *child_null = JsonParseAsNull(data);
                if (child_null)
                {
                    JsonObjectAppendElement(object, property_name, child_null);
                    free(property_name);
                    property_name = NULL;
                    break;
                }
            }

            *json_out = NULL;
            free(property_name);
            JsonElementDestroy(object);
            return JSON_PARSE_ERROR_OBJECT_BAD_SYMBOL;
        }
    }

    *json_out = NULL;
    free(property_name);
    JsonElementDestroy(object);
    return JSON_PARSE_ERROR_OBJECT_END;
}

JsonParseError JsonParse(const char **data, JsonElement **json_out)
{
    assert(data && *data);
    if (data == NULL || *data == NULL)
    {
        return JSON_PARSE_ERROR_NO_DATA;
    }

    while (**data)
    {
        if (**data == '{')
        {
            return JsonParseAsObject(data, json_out);
        }
        else if (**data == '[')
        {
            return JsonParseAsArray(data, json_out);
        }
        else if (IsWhitespace(**data))
        {
            (*data)++;
        }
        else
        {
            *json_out = NULL;
            return JSON_PARSE_ERROR_INVALID_START;
        }
    }

    return JSON_PARSE_ERROR_NO_DATA;
}
