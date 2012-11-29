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
        SequenceDestroy(element->container.children);
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

JsonContainerType JsonIteratorCurrentContrainerType(JsonIterator *iter)
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

    SequenceSort(container->container.children, (SequenceItemComparator)Compare, user_data);
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

static void _JsonObjectAppendPrimitive(JsonElement *object, const char *key, JsonElement *child_primitive)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);

    assert(child_primitive);
    assert(child_primitive->type == JSON_ELEMENT_TYPE_PRIMITIVE);

    JsonElementSetPropertyName(child_primitive, key);

    SequenceAppend(object->container.children, child_primitive);
}

void JsonObjectAppendString(JsonElement *object, const char *key, const char *value)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);
    assert(value);

    JsonElement *child = JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_STRING, EscapeJsonString(value));
    _JsonObjectAppendPrimitive(object, key, child);
}

void JsonObjectAppendInteger(JsonElement *object, const char *key, int value)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    JsonElement *child = JsonIntegerCreate(value);
    _JsonObjectAppendPrimitive(object, key, child);
}

void JsonObjectAppendBool(JsonElement *object, const char *key, _Bool value)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    JsonElement *child = JsonBoolCreate(value);
    _JsonObjectAppendPrimitive(object, key, child);
}

void JsonObjectAppendReal(JsonElement *object, const char *key, double value)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    JsonElement *child = JsonRealCreate(value);
    _JsonObjectAppendPrimitive(object, key, child);
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

static int JsonElementHasProperty(const void *propertyName, const void *jsonElement, void *_user_data)
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

static int CompareKeyToPropertyName(const void *a, const void *b, void *_user_data)
{
    return StringSafeCompare((char*)a, ((JsonElement*)b)->propertyName);
}

static size_t JsonElementIndexInParentObject(JsonElement *parent, const char* key)
{
    assert(parent);
    assert(parent->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(parent->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    return SequenceIndexOf(parent->container.children, key, CompareKeyToPropertyName);
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
        SequenceRemove(object->container.children, index);
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
        detached = SequenceLookup(object->container.children, key, JsonElementHasProperty);
        SequenceSoftRemove(object->container.children, index);
    }

    return detached;
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

JsonElement *JsonObjectGet(JsonElement *object, const char *key)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    return SequenceLookup(object->container.children, key, JsonElementHasProperty);
}

// *******************************************************************************************
// JsonArray Functions
// *******************************************************************************************

JsonElement *JsonArrayCreate(size_t initialCapacity)
{
    return JsonElementCreateContainer(JSON_CONTAINER_TYPE_ARRAY, NULL, initialCapacity);
}

static void _JsonArrayAppendPrimitive(JsonElement *array, JsonElement *child_primitive)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    assert(child_primitive);
    assert(child_primitive->type == JSON_ELEMENT_TYPE_PRIMITIVE);

    SequenceAppend(array->container.children, child_primitive);
}

void JsonArrayAppendString(JsonElement *array, const char *value)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);
    assert(value);

    JsonElement *child = JsonStringCreate(value);
    _JsonArrayAppendPrimitive(array, child);
}

void JsonArrayAppendBool(JsonElement *array, bool value)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    JsonElement *child = JsonBoolCreate(value);
    _JsonArrayAppendPrimitive(array, child);
}

void JsonArrayAppendInteger(JsonElement *array, int value)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    JsonElement *child = JsonIntegerCreate(value);
    _JsonArrayAppendPrimitive(array, child);
}

void JsonArrayAppendReal(JsonElement *array, double value)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    JsonElement *child = JsonRealCreate(value);
    _JsonArrayAppendPrimitive(array, child);
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

void JsonContainerReverse(JsonElement *array)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    SequenceReverse(array->container.children);
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
        CfDebug("Attempted to add NaN or inifinite value to JSON");
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

static JsonElement *JsonParseAsObject(const char **data);

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

static char *JsonParseAsString(const char **data)
{
    if (**data != '"')
    {
        CfDebug("Unable to parse json data as string, did not start with doublequote: %s", *data);
        return NULL;
    }

    Writer *writer = StringWriter();

    for (*data = *data + 1; **data != '\0'; *data = *data + 1)
    {
        if (**data == '"' && *(*data - 1) != '\\')
        {
            return StringWriterClose(writer);
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

    CfDebug("Unable to parse json data as string, did not end with doublequote: %s", *data);
    WriterClose(writer);
    return NULL;
}

static JsonElement *JsonParseAsNumber(const char **data)
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
                CfDebug("Unable to parse json data as number, - not at the start or not after exponent, %s", *data);
                WriterClose(writer);
                return NULL;
            }
            break;

        case '+':
            if (prev_char != 'e' && prev_char != 'E')
            {
                CfDebug("Unable to parse json data as number, + without preceding exponent, %s", *data);
                WriterClose(writer);
                return NULL;
            }
            break;

        case '0':
            if (zero_started && !seen_dot && !seen_exponent)
            {
                CfDebug("Unable to parse json data as number, started with 0 before dot or exponent, duplicate 0 seen, %s", *data);
                WriterClose(writer);
                return NULL;
            }
            if (prev_char == 0)
            {
                zero_started = true;
            }
            break;

        case '.':
            if (prev_char != '0' && !IsDigit(prev_char))
            {
                CfDebug("Unable to parse json data as number, dot not preceded by digit, %s", *data);
                WriterClose(writer);
                return NULL;
            }
            seen_dot = true;
            break;

        case 'e':
        case 'E':
            if (seen_exponent)
            {
                CfDebug("Unable to parse json data as number, duplicate exponent, %s", *data);
                WriterClose(writer);
                return NULL;
            }
            else if (!IsDigit(prev_char) && prev_char != '0')
            {
                CfDebug("Unable to parse json data as number, exponent without preceding digit, %s", *data);
                WriterClose(writer);
                return NULL;
            }
            seen_exponent = true;
            break;

        default:
            if (zero_started && !seen_dot && !seen_exponent)
            {
                CfDebug("Unable to parse json data as number, dot or exponent must follow leading 0: %s", *data);
                WriterClose(writer);
                return NULL;
            }

            if (!IsDigit(**data))
            {
                CfDebug("Unable to parse json data as number, invalid symbol, %s", *data);
                WriterClose(writer);
                return NULL;
            }
            break;
        }

        WriterWriteChar(writer, **data);
    }

    if (prev_char != '0' && !IsDigit(prev_char))
    {
        CfDebug("Unable to parse json data as string, did not end with digit: %s", *data);
        WriterClose(writer);
        return NULL;
    }

    // rewind 1 char so caller will see separator next
    *data = *data - 1;

    if (seen_dot)
    {
        return JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_REAL, StringWriterClose(writer));
    }
    else
    {
        return JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_INTEGER, StringWriterClose(writer));
    }
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
            {
                char *value = JsonParseAsString(data);
                JsonArrayAppendString(array, value);
                free(value);
            }
            break;

        case '[':
            {
                JsonElement *child_array = JsonParseAsArray(data);
                if (!child_array)
                {
                    JsonElementDestroy(array);
                    return NULL;
                }

                JsonArrayAppendArray(array, child_array);
            }
            break;

        case '{':
            {
                JsonElement *child_object = JsonParseAsObject(data);
                if (!child_object)
                {
                    JsonElementDestroy(array);
                    return NULL;
                }

                JsonArrayAppendObject(array, child_object);
            }
            break;

        case ',':
            break;

        case ']':
            return array;

        default:
            if (**data == '-' || **data == '0' || IsDigit(**data))
            {
                JsonElement *child = JsonParseAsNumber(data);
                if (!child)
                {
                    JsonElementDestroy(array);
                    return NULL;
                }
                _JsonArrayAppendPrimitive(array, child);
                break;
            }

            JsonElement *child_bool = JsonParseAsBoolean(data);
            if (child_bool)
            {
                _JsonArrayAppendPrimitive(array, child_bool);
                break;
            }

            JsonElement *child_null = JsonParseAsNull(data);
            if (child_null)
            {
                _JsonArrayAppendPrimitive(array, child_null);
                break;
            }

            CfDebug("Unable to parse json data as object, unrecognized token beginning entry: %s", *data);
            JsonElementDestroy(array);
            return NULL;
        }
    }

    CfDebug("Unable to parse json data as array, did not end with ']': %s", *data);
    JsonElementDestroy(array);
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
                char *property_value = JsonParseAsString(data);
                if (!property_value)
                {
                    free(property_name);
                    JsonElementDestroy(object);
                    return NULL;
                }

                JsonObjectAppendString(object, property_name, property_value);
                free(property_value);
                free(property_name);
                property_name = NULL;
            }
            else
            {
                property_name = JsonParseAsString(data);
                if (!property_name)
                {
                    JsonElementDestroy(object);
                    return NULL;
                }
            }
            break;

        case ':':
            if (property_name == NULL)
            {
                CfDebug("Unable to parse json data as object, ':' seen without having specified an l-value: %s", *data);
                free(property_name);
                JsonElementDestroy(object);
                return NULL;
            }
            break;

        case ',':
            if (property_name != NULL)
            {
                CfDebug("Unable to parse json data as object, ',' seen without having specified an r-value: %s", *data);
                free(property_name);
                JsonElementDestroy(object);
                return NULL;
            }
            break;

        case '[':
            if (property_name != NULL)
            {
                JsonElement *child_array = JsonParseAsArray(data);
                if (!child_array)
                {
                    free(property_name);
                    JsonElementDestroy(object);
                    return NULL;
                }

                JsonObjectAppendArray(object, property_name, child_array);
                free(property_name);
                property_name = NULL;
            }
            else
            {
                CfDebug("Unable to parse json data as object, array not allowed as l-value: %s", *data);
                free(property_name);
                JsonElementDestroy(object);
                return NULL;
            }
            break;

        case '{':
            if (property_name != NULL)
            {
                JsonElement *child_object = JsonParseAsObject(data);
                if (!child_object)
                {
                    free(property_name);
                    JsonElementDestroy(object);
                    return NULL;
                }

                JsonObjectAppendObject(object, property_name, child_object);
                free(property_name);
                property_name = NULL;
            }
            else
            {
                CfDebug("Unable to parse json data as object, object not allowed as l-value: %s", *data);
                free(property_name);
                JsonElementDestroy(object);
                return NULL;
            }
            break;

        case '}':
            if (property_name != NULL)
            {
                CfDebug("Unable to parse json data as object, tried to close object having opened an l-value: %s",
                        *data);
                free(property_name);
                JsonElementDestroy(object);
                return NULL;
            }
            free(property_name);
            return object;

        default:
            if (property_name)
            {
                if (**data == '-' || **data == '0' || IsDigit(**data))
                {
                    JsonElement *child = JsonParseAsNumber(data);
                    if (!child)
                    {
                        free(property_name);
                        JsonElementDestroy(object);
                        return NULL;
                    }
                    _JsonObjectAppendPrimitive(object, property_name, child);
                    free(property_name);
                    property_name = NULL;
                    break;
                }

                JsonElement *child_bool = JsonParseAsBoolean(data);
                if (child_bool)
                {
                    _JsonObjectAppendPrimitive(object, property_name, child_bool);
                    free(property_name);
                    property_name = NULL;
                    break;
                }

                JsonElement *child_null = JsonParseAsNull(data);
                if (child_null)
                {
                    _JsonObjectAppendPrimitive(object, property_name, child_null);
                    free(property_name);
                    property_name = NULL;
                    break;
                }
            }

            CfDebug("Unable to parse json data as object, unrecognized token beginning entry: %s", *data);
            free(property_name);
            JsonElementDestroy(object);
            return NULL;
        }
    }

    CfDebug("Unable to parse json data as string, did not end with '}': %s", *data);
    free(property_name);
    JsonElementDestroy(object);
    return NULL;
}

JsonElement *JsonParse(const char **data)
{
    assert(data && *data);
    if (data == NULL || *data == NULL)
    {
        return NULL;
    }

    while (**data)
    {
        if (**data == '{')
        {
            return JsonParseAsObject(data);
        }
        else if (**data == '[')
        {
            return JsonParseAsArray(data);
        }
        else if (IsWhitespace(**data))
        {
            (*data)++;
        }
        else
        {
            CfDebug("Unwilling to parse json data starting with invalid character: %c", **data);
            return NULL;
        }
    }

    return NULL;
}
