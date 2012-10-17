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

#include "sequence.h"

#include <assert.h>

static const int SPACES_PER_INDENT = 2;
static const int DEFAULT_CONTAINER_CAPACITY = 64;

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
    JSON_PRIMITIVE_TYPE_REAL
} JsonPrimitiveType;

struct JsonElement_
{
    JsonElementType type;
    char *propertyName;         // to avoid having a struct JsonProperty
    union
    {
        struct JsonContainer
        {
            JsonContainerType type;
            Sequence *children;
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
    element->container.children = SequenceCreate(initialCapacity, JsonElementDestroy);

    return element;
}

static JsonElement *JsonElementCreatePrimitive(JsonPrimitiveType primitiveType, const char *propertyName,
                                               const char *value)
{
    JsonElement *element = xcalloc(1, sizeof(JsonElement));

    element->type = JSON_ELEMENT_TYPE_PRIMITIVE;

    JsonElementSetPropertyName(element, propertyName);

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
        SequenceDestroy(element->container.children);
        element->container.children = NULL;
        break;

    case JSON_ELEMENT_TYPE_PRIMITIVE:
        assert(element->primitive.value);
        free((void *) element->primitive.value);
        element->primitive.value = NULL;
        break;
    }

    if (element->propertyName)
    {
        free(element->propertyName);
    }

    free(element);
}

size_t JsonElementLength(JsonElement *element)
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
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);
    assert(value);

    JsonElement *child = JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_STRING, key, EscapeJsonString(value));

    SequenceAppend(object->container.children, child);
}

void JsonObjectAppendInteger(JsonElement *object, const char *key, int value)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    char *buffer = xcalloc(32, sizeof(char));

    snprintf(buffer, 32, "%d", value);

    JsonElement *child = JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_INTEGER, key, buffer);

    SequenceAppend(object->container.children, child);
}

void JsonObjectAppendReal(JsonElement *object, const char *key, double value)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    if (isnan(value) || !isfinite(value))
    {
        CfDebug("Attempted to add NaN or inifinite value to JSON");
        value = 0.0;
    }

    char *buffer = xcalloc(32, sizeof(char));

    snprintf(buffer, 32, "%.4f", value);

    JsonElement *child = JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_REAL, key, buffer);

    SequenceAppend(object->container.children, child);
}

void JsonObjectAppendArray(JsonElement *object, const char *key, JsonElement *array)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    JsonElementSetPropertyName(array, key);
    SequenceAppend(object->container.children, array);
}

void JsonObjectAppendObject(JsonElement *object, const char *key, JsonElement *childObject)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);
    assert(childObject);
    assert(childObject->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(childObject->container.type == JSON_CONTAINER_TYPE_OBJECT);

    JsonElementSetPropertyName(childObject, key);
    SequenceAppend(object->container.children, childObject);
}

static int JsonElementHasProperty(const void *propertyName, const void *jsonElement)
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

const char *JsonObjectGetAsString(JsonElement *object, const char *key)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    JsonElement *childPrimitive = SequenceLookup(object->container.children, key, JsonElementHasProperty);

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

    JsonElement *childPrimitive = SequenceLookup(object->container.children, key, JsonElementHasProperty);

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

    JsonElement *childPrimitive = SequenceLookup(object->container.children, key, JsonElementHasProperty);

    if (childPrimitive)
    {
        assert(childPrimitive->type == JSON_ELEMENT_TYPE_CONTAINER);
        assert(childPrimitive->container.type == JSON_CONTAINER_TYPE_ARRAY);
        return childPrimitive;
    }

    return NULL;
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
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);
    assert(value);

    JsonElement *child = JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_STRING, NULL, EscapeJsonString(value));

    SequenceAppend(array->container.children, child);
}

void JsonArrayAppendInteger(JsonElement *object, int value)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_ARRAY);

    char *buffer = xcalloc(32, sizeof(char));

    snprintf(buffer, 32, "%d", value);

    JsonElement *child = JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_INTEGER, NULL, buffer);

    SequenceAppend(object->container.children, child);
}

void JsonArrayAppendArray(JsonElement *array, JsonElement *childArray)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);
    assert(childArray);
    assert(childArray->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(childArray->container.type == JSON_CONTAINER_TYPE_ARRAY);

    SequenceAppend(array->container.children, childArray);
}

void JsonArrayAppendObject(JsonElement *array, JsonElement *object)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);

    SequenceAppend(array->container.children, object);
}

void JsonArrayRemoveRange(JsonElement *array, size_t start, size_t end)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);
    assert(end < array->container.children->length);
    assert(start <= end);

    SequenceRemoveRange(array->container.children, start, end);
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

// *******************************************************************************************
// Primitive Functions
// *******************************************************************************************

JsonElement *JsonStringCreate(const char *value)
{
    assert(value);
    return JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_STRING, NULL, EscapeJsonString(value));
}

// *******************************************************************************************
// Printing
// *******************************************************************************************

static void JsonContainerPrint(Writer *writer, JsonElement *containerElement, size_t indent_level);

static bool IsWhitespace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
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

    case JSON_PRIMITIVE_TYPE_INTEGER:
    case JSON_PRIMITIVE_TYPE_REAL:
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

static JsonElement *JsonParseAsObject(const char **data);

static const char *JsonParseAsString(const char **data)
{
    if (**data != '"')
    {
        CfDebug("Unable to parse json data as string, did not start with doublequote: %s", *data);
        return NULL;
    }

    Writer *writer = StringWriter();

    for (*data = *data + 1; **data != '\0'; *data = *data + 1)
    {
        if (**data == '"')
        {
            return StringWriterClose(writer);
        }

        WriterWriteChar(writer, **data);
    }

    CfDebug("Unable to parse json data as string, did not end with doublequote: %s", *data);
    return NULL;
}

static JsonElement *JsonParseAsArray(const char **data)
{
    if (**data != '[')
    {
        CfDebug("Unable to parse json data as array, did not start with '[': %s", *data);
        return NULL;
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
            JsonArrayAppendString(array, JsonParseAsString(data));
            break;

        case '[':
            JsonArrayAppendArray(array, JsonParseAsArray(data));
            break;

        case '{':
            JsonArrayAppendObject(array, JsonParseAsObject(data));
            break;

        case ',':
            break;

        case ']':
            return array;

        default:
            CfDebug("Unable to parse json data as object, unrecognized token beginning entry: %s", *data);
            return NULL;
        }
    }

    CfDebug("Unable to parse json data as array, did not end with ']': %s", *data);
    return NULL;
}

static JsonElement *JsonParseAsObject(const char **data)
{
    if (**data != '{')
    {
        CfDebug("Unable to parse json data as object, did not start with '{': %s", *data);
        return NULL;
    }

    JsonElement *object = JsonObjectCreate(DEFAULT_CONTAINER_CAPACITY);
    const char *propertyName = NULL;

    for (*data = *data + 1; **data != '\0'; *data = *data + 1)
    {
        if (IsWhitespace(**data))
        {
            continue;
        }

        switch (**data)
        {
        case '"':
            if (propertyName != NULL)
            {
                JsonObjectAppendString(object, propertyName, JsonParseAsString(data));
                propertyName = NULL;
            }
            else
            {
                propertyName = JsonParseAsString(data);
            }
            break;

        case ':':
            if (propertyName == NULL)
            {
                CfDebug("Unable to parse json data as object, ':' seen without having specified an l-value: %s", *data);
                return NULL;
            }
            break;

        case ',':
            if (propertyName != NULL)
            {
                CfDebug("Unable to parse json data as object, ',' seen without having specified an r-value: %s", *data);
                return NULL;
            }
            break;

        case '[':
            if (propertyName != NULL)
            {
                JsonObjectAppendArray(object, propertyName, JsonParseAsArray(data));
                propertyName = NULL;
            }
            else
            {
                CfDebug("Unable to parse json data as object, array not allowed as l-value: %s", *data);
                return NULL;
            }
            break;

        case '{':
            if (propertyName != NULL)
            {
                JsonObjectAppendObject(object, propertyName, JsonParseAsObject(data));
                propertyName = NULL;
            }
            else
            {
                CfDebug("Unable to parse json data as object, object not allowed as l-value: %s", *data);
                return NULL;
            }
            break;

        case '}':
            if (propertyName != NULL)
            {
                CfDebug("Unable to parse json data as object, tried to close object having opened an l-value: %s",
                        *data);
                return NULL;
            }
            return object;

        default:
            CfDebug("Unable to parse json data as object, unrecognized token beginning entry: %s", *data);
            return NULL;
        }
    }

    CfDebug("Unable to parse json data as string, did not end with '}': %s", *data);
    return NULL;
}

JsonElement *JsonParse(const char **data)
{
    assert(**data && "Cannot parse NULL data");

    if (**data == '{')
    {
        return JsonParseAsObject(data);
    }
    else if (**data == '[')
    {
        return JsonParseAsArray(data);
    }
    else if (**data == '"')
    {
        return JsonParseAsObject(data);
    }
    else
    {
        CfDebug("Don't know how to parse JSON input: %s", *data);
        return NULL;
    }
}
