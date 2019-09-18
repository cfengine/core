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

#include <logging.h>
#include <json.h>
#include <json-priv.h>
#include <json-yaml.h>

#include <alloc.h>
#include <sequence.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <file_lib.h>
#include <printsize.h>
#include <regex.h>
#include <buffer.h>

static const int SPACES_PER_INDENT = 2;
const int DEFAULT_CONTAINER_CAPACITY = 64;

static const char *const JSON_TRUE = "true";
static const char *const JSON_FALSE = "false";
static const char *const JSON_NULL = "null";

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

const char *JsonPrimitiveTypeToString(JsonPrimitiveType type)
{
    switch (type)
    {
    case JSON_PRIMITIVE_TYPE_STRING:
        return "string";
    case JSON_PRIMITIVE_TYPE_REAL:
    case JSON_PRIMITIVE_TYPE_INTEGER:
        return "number";
    case JSON_PRIMITIVE_TYPE_BOOL:
        return "boolean";
    default:
        UnexpectedError("Unknown JSON primitive type: %d", type);
        return "(null)";
    }
}

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

const char* JsonElementGetPropertyName(const JsonElement *element)
{
    assert(element);

    return element->propertyName;
}

static JsonElement *JsonElementCreateContainer(JsonContainerType containerType, const char *propertyName,
                                               size_t initialCapacity)
{
    JsonElement *element = xcalloc(1, sizeof(JsonElement));

    element->type = JSON_ELEMENT_TYPE_CONTAINER;

    JsonElementSetPropertyName(element, propertyName);

    element->container.type = containerType;
    element->container.children = SeqNew(initialCapacity, JsonDestroy);

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

static JsonElement *JsonArrayCopy(const JsonElement *array)
{
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    JsonElement *copy = JsonArrayCreate(JsonLength(array));

    JsonIterator iter = JsonIteratorInit(array);
    const JsonElement *child = NULL;
    while ((child = JsonIteratorNextValue(&iter)))
    {
        JsonArrayAppendElement(copy, JsonCopy(child));
    }

    return copy;
}

static JsonElement *JsonObjectCopy(const JsonElement *object)
{
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);

    JsonElement *copy = JsonObjectCreate(JsonLength(object));

    JsonIterator iter = JsonIteratorInit(object);
    const JsonElement *child = NULL;
    while ((child = JsonIteratorNextValue(&iter)))
    {
        JsonObjectAppendElement(copy, JsonIteratorCurrentKey(&iter), JsonCopy(child));
    }

    return copy;
}


static JsonElement *JsonContainerCopy(const JsonElement *container)
{
    assert(container->type == JSON_ELEMENT_TYPE_CONTAINER);
    switch (container->container.type)
    {
    case JSON_CONTAINER_TYPE_ARRAY:
        return JsonArrayCopy(container);

    case JSON_CONTAINER_TYPE_OBJECT:
        return JsonObjectCopy(container);
    }

    UnexpectedError("Unknown JSON container type: %d",
                    container->container.type);
    return NULL;
}

static JsonElement *JsonPrimitiveCopy(const JsonElement *primitive)
{
    assert(primitive->type == JSON_ELEMENT_TYPE_PRIMITIVE);
    switch (primitive->primitive.type)
    {
    case JSON_PRIMITIVE_TYPE_BOOL:
        return JsonBoolCreate(JsonPrimitiveGetAsBool(primitive));

    case JSON_PRIMITIVE_TYPE_INTEGER:
        return JsonIntegerCreate(JsonPrimitiveGetAsInteger(primitive));

    case JSON_PRIMITIVE_TYPE_NULL:
        return JsonNullCreate();

    case JSON_PRIMITIVE_TYPE_REAL:
        return JsonRealCreate(JsonPrimitiveGetAsReal(primitive));

    case JSON_PRIMITIVE_TYPE_STRING:
        return JsonStringCreate(JsonPrimitiveGetAsString(primitive));
    }

    UnexpectedError("Unknown JSON primitive type: %d",
                    primitive->primitive.type);
    return NULL;
}

JsonElement *JsonCopy(const JsonElement *element)
{
    switch (element->type)
    {
    case JSON_ELEMENT_TYPE_CONTAINER:
        return JsonContainerCopy(element);
    case JSON_ELEMENT_TYPE_PRIMITIVE:
        return JsonPrimitiveCopy(element);
    }

    UnexpectedError("Unknown JSON element type: %d",
                    element->type);
    return NULL;
}

static int JsonArrayCompare(const JsonElement *a, const JsonElement *b)
{
    assert(a->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(a->container.type == JSON_CONTAINER_TYPE_ARRAY);
    assert(b->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(b->container.type == JSON_CONTAINER_TYPE_ARRAY);

    int ret = JsonLength(a) - JsonLength(b);
    if (ret != 0)
    {
        return ret;
    }

    JsonIterator iter_a = JsonIteratorInit(a);
    JsonIterator iter_b = JsonIteratorInit(a);

    for (size_t i = 0; i < JsonLength(a); i++)
    {
        const JsonElement *child_a = JsonIteratorNextValue(&iter_a);
        const JsonElement *child_b = JsonIteratorNextValue(&iter_b);

        ret = JsonCompare(child_a, child_b);
        if (ret != 0)
        {
            return ret;
        }
    }

    return ret;
}

static int JsonObjectCompare(const JsonElement *a, const JsonElement *b)
{
    assert(a->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(a->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(b->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(b->container.type == JSON_CONTAINER_TYPE_OBJECT);

    int ret = JsonLength(a) - JsonLength(b);
    if (ret != 0)
    {
        return ret;
    }

    JsonIterator iter_a = JsonIteratorInit(a);
    JsonIterator iter_b = JsonIteratorInit(a);

    for (size_t i = 0; i < JsonLength(a); i++)
    {
        const JsonElement *child_a = JsonIteratorNextValue(&iter_a);
        const JsonElement *child_b = JsonIteratorNextValue(&iter_b);

        ret = strcmp(JsonIteratorCurrentKey(&iter_a), JsonIteratorCurrentKey(&iter_b));
        if (ret != 0)
        {
            return ret;
        }

        ret = JsonCompare(child_a, child_b);
        if (ret != 0)
        {
            return ret;
        }
    }

    return ret;
}


static int JsonContainerCompare(const JsonElement *a, const JsonElement *b)
{
    assert(a->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(b->type == JSON_ELEMENT_TYPE_CONTAINER);

    if (a->container.type != b->container.type)
    {
        return a->container.type - b->container.type;
    }

    switch (a->container.type)
    {
    case JSON_CONTAINER_TYPE_ARRAY:
        return JsonArrayCompare(a, b);

    case JSON_CONTAINER_TYPE_OBJECT:
        return JsonObjectCompare(a, b);
    }

    UnexpectedError("Unknown JSON container type: %d",
                    a->container.type);
    return -1;
}

int JsonCompare(const JsonElement *a, const JsonElement *b)
{
    if (a->type != b->type)
    {
        return a->type - b->type;
    }

    switch (a->type)
    {
    case JSON_ELEMENT_TYPE_CONTAINER:
        return JsonContainerCompare(a, b);
    case JSON_ELEMENT_TYPE_PRIMITIVE:
        return strcmp(a->primitive.value, b->primitive.value);
    }

    UnexpectedError("Unknown JSON element type: %d",
                    a->type);
    return -1;
}


void JsonDestroy(JsonElement *element)
{
    if (element)
    {
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

        default:
            UnexpectedError("Unknown JSON element type: %d",
                            element->type);
        }

        if (element->propertyName)
        {
            free(element->propertyName);
        }

        free(element);
    }
}

void JsonDestroyMaybe(JsonElement *element, bool allocated)
{
    if (allocated)
    {
        JsonDestroy(element);
    }
}

JsonElement *JsonArrayMergeArray(const JsonElement *a, const JsonElement *b)
{
    assert(JsonGetElementType(a) == JsonGetElementType(b));
    assert(JsonGetElementType(a) == JSON_ELEMENT_TYPE_CONTAINER);
    assert(JsonGetContainerType(a) == JsonGetContainerType(b));
    assert(JsonGetContainerType(a) == JSON_CONTAINER_TYPE_ARRAY);

    JsonElement *result = JsonArrayCreate(JsonLength(a) + JsonLength(b));
    for (size_t i = 0; i < JsonLength(a); i++)
    {
        JsonArrayAppendElement(result, JsonCopy(JsonAt(a, i)));
    }

    for (size_t i = 0; i < JsonLength(b); i++)
    {
        JsonArrayAppendElement(result, JsonCopy(JsonAt(b, i)));
    }

    return result;
}

JsonElement *JsonObjectMergeArray(const JsonElement *a, const JsonElement *b)
{
    assert(JsonGetElementType(a) == JsonGetElementType(b));
    assert(JsonGetElementType(a) == JSON_ELEMENT_TYPE_CONTAINER);
    assert(JsonGetContainerType(a) == JSON_CONTAINER_TYPE_OBJECT);
    assert(JsonGetContainerType(b) == JSON_CONTAINER_TYPE_ARRAY);

    JsonElement *result = JsonObjectCopy(a);
    for (size_t i = 0; i < JsonLength(b); i++)
    {
        char *key = StringFromLong(i);
        JsonObjectAppendElement(result, key, JsonCopy(JsonAt(b, i)));
        free(key);
    }

    return result;
}

JsonElement *JsonObjectMergeObject(const JsonElement *a, const JsonElement *b)
{
    assert(JsonGetElementType(a) == JsonGetElementType(b));
    assert(JsonGetElementType(a) == JSON_ELEMENT_TYPE_CONTAINER);
    assert(JsonGetContainerType(a) == JSON_CONTAINER_TYPE_OBJECT);
    assert(JsonGetContainerType(b) == JSON_CONTAINER_TYPE_OBJECT);

    JsonElement *result = JsonObjectCopy(a);
    JsonIterator iter = JsonIteratorInit(b);
    const char *key;
    while ((key = JsonIteratorNextKey(&iter)))
    {
        JsonObjectAppendElement(result, key, JsonCopy(JsonIteratorCurrentValue(&iter)));
    }

    return result;
}

JsonElement *JsonMerge(const JsonElement *a, const JsonElement *b)
{
    assert(JsonGetElementType(a) == JsonGetElementType(b));
    assert(JsonGetElementType(a) == JSON_ELEMENT_TYPE_CONTAINER);

    switch (JsonGetContainerType(a))
    {
    case JSON_CONTAINER_TYPE_ARRAY:
        switch (JsonGetContainerType(b))
        {
        case JSON_CONTAINER_TYPE_OBJECT:
            return JsonObjectMergeArray(b, a);
        case JSON_CONTAINER_TYPE_ARRAY:
            return JsonArrayMergeArray(a, b);
        }
        UnexpectedError("Unknown JSON container type: %d",
                        JsonGetContainerType(b));
        break;

    case JSON_CONTAINER_TYPE_OBJECT:
        switch (JsonGetContainerType(b))
        {
        case JSON_CONTAINER_TYPE_OBJECT:
            return JsonObjectMergeObject(a, b);
        case JSON_CONTAINER_TYPE_ARRAY:
            return JsonObjectMergeArray(a, b);
        }
        UnexpectedError("Unknown JSON container type: %d",
                        JsonGetContainerType(b));
        break;

    default:
        UnexpectedError("Unknown JSON container type: %d",
                        JsonGetContainerType(a));
    }

    return NULL;
}


size_t JsonLength(const JsonElement *element)
{
    assert(element);

    switch (element->type)
    {
    case JSON_ELEMENT_TYPE_CONTAINER:
        return element->container.children->length;

    case JSON_ELEMENT_TYPE_PRIMITIVE:
        return strlen(element->primitive.value);

    default:
        UnexpectedError("Unknown JSON element type: %d",
                        element->type);
    }

    return (size_t) -1;                  // appease gcc
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

    if (iter->index >= JsonLength(iter->container))
    {
        return NULL;
    }

    return iter->container->container.children->data[iter->index++];
}

const JsonElement *JsonIteratorNextValueByType(JsonIterator *iter, JsonElementType type, bool skip_null)
{
    const JsonElement *e = NULL;
    while ((e = JsonIteratorNextValue(iter)))
    {
        if (skip_null
            && JsonGetElementType(e) == JSON_ELEMENT_TYPE_PRIMITIVE
            && JsonGetPrimitiveType(e) == JSON_PRIMITIVE_TYPE_NULL)
        {
            continue;
        }

        if (e->type == type)
        {
            return e;
        }
    }

    return NULL;
}

const JsonElement *JsonIteratorCurrentValue(const JsonIterator *iter)
{
    assert(iter);
    assert(iter->container->type == JSON_ELEMENT_TYPE_CONTAINER);

    if (iter->index == 0 || iter->index > JsonLength(iter->container))
    {
        return NULL;
    }

    return iter->container->container.children->data[(iter->index) - 1];
}

const char *JsonIteratorCurrentKey(const JsonIterator *iter)
{
    assert(iter);
    assert(iter->container->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(iter->container->container.type == JSON_CONTAINER_TYPE_OBJECT);

    const JsonElement *child = JsonIteratorCurrentValue(iter);

    return child ? child->propertyName : NULL;
}

JsonElementType JsonIteratorCurrentElementType(const JsonIterator *iter)
{
    assert(iter);

    const JsonElement *child = JsonIteratorCurrentValue(iter);
    return child->type;
}

JsonContainerType JsonIteratorCurrentContainerType(const JsonIterator *iter)
{
    assert(iter);

    const JsonElement *child = JsonIteratorCurrentValue(iter);
    assert(child->type == JSON_ELEMENT_TYPE_CONTAINER);

    return child->container.type;
}

JsonPrimitiveType JsonIteratorCurrentPrimitiveType(const JsonIterator *iter)
{
    assert(iter);

    const JsonElement *child = JsonIteratorCurrentValue(iter);
    assert(child->type == JSON_ELEMENT_TYPE_PRIMITIVE);

    return child->primitive.type;
}

bool JsonIteratorHasMore(const JsonIterator *iter)
{
    assert(iter);

    return iter->index < JsonLength(iter->container);
}

JsonElementType JsonGetElementType(const JsonElement *element)
{
    assert(element);
    return element->type;
}

JsonContainerType JsonGetContainerType(const JsonElement *container)
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

char* JsonPrimitiveToString(const JsonElement *primitive)
{
    if (JsonGetElementType(primitive) != JSON_ELEMENT_TYPE_PRIMITIVE)
    {
        return NULL;
    }

    switch (JsonGetPrimitiveType(primitive))
    {
    case JSON_PRIMITIVE_TYPE_BOOL:
        return xstrdup(JsonPrimitiveGetAsBool(primitive) ? "true" : "false");
        break;

    case JSON_PRIMITIVE_TYPE_INTEGER:
        return StringFromLong(JsonPrimitiveGetAsInteger(primitive));
        break;

    case JSON_PRIMITIVE_TYPE_REAL:
        return StringFromDouble(JsonPrimitiveGetAsReal(primitive));
        break;

    case JSON_PRIMITIVE_TYPE_STRING:
        return xstrdup(JsonPrimitiveGetAsString(primitive));
        break;

    case JSON_PRIMITIVE_TYPE_NULL: // redundant
        break;
    }

    return NULL;
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

    return StringToLongExitOnError(primitive->primitive.value);
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
    assert(index < JsonLength(container));

    return container->container.children->data[index];
}

JsonElement *JsonSelect(JsonElement *element, size_t num_indices, char **indices)
{
    if (num_indices == 0)
    {
        return element;
    }
    else
    {
        if (JsonGetElementType(element) != JSON_ELEMENT_TYPE_CONTAINER)
        {
            return NULL;
        }

        const char *index = indices[0];

        switch (JsonGetContainerType(element))
        {
        case JSON_CONTAINER_TYPE_OBJECT:
            {
                JsonElement *child = JsonObjectGet(element, index);
                if (child)
                {
                    return JsonSelect(child, num_indices - 1, indices + 1);
                }
            }
            return NULL;

        case JSON_CONTAINER_TYPE_ARRAY:
            if (StringIsNumeric(index))
            {
                size_t i = StringToLongExitOnError(index);
                if (i < JsonLength(element))
                {
                    JsonElement *child = JsonArrayGet(element, i);
                    if (child)
                    {
                        return JsonSelect(child, num_indices - 1, indices + 1);
                    }
                }
            }
            break;

        default:
            UnexpectedError("Unknown JSON container type: %d",
                            JsonGetContainerType(element));
        }
        return NULL;
    }
}

// *******************************************************************************************
// JsonObject Functions
// *******************************************************************************************

JsonElement *JsonObjectCreate(size_t initialCapacity)
{
    return JsonElementCreateContainer(JSON_CONTAINER_TYPE_OBJECT, NULL, initialCapacity);
}

static char *JsonEncodeString(const char *unescaped_string)
{
    assert(unescaped_string);

    Writer *writer = StringWriter();

    for (const char *c = unescaped_string; *c != '\0'; c++)
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

char *JsonDecodeString(const char *encoded_string)
{
    assert(encoded_string);

    Writer *w = StringWriter();

    for (const char *c = encoded_string; *c != '\0'; c++)
    {
        switch (*c)
        {
        case '\\':
            switch (c[1])
            {
            case '\"':
            case '\\':
                WriterWriteChar(w, c[1]);
                c++;
                break;
            case 'b':
                WriterWriteChar(w, '\b');
                c++;
                break;
            case 'f':
                WriterWriteChar(w, '\f');
                c++;
                break;
            case 'n':
                WriterWriteChar(w, '\n');
                c++;
                break;
            case 'r':
                WriterWriteChar(w, '\r');
                c++;
                break;
            case 't':
                WriterWriteChar(w, '\t');
                c++;
                break;
            default:
                WriterWriteChar(w, *c);
                break;
            }
            break;
        default:
            WriterWriteChar(w, *c);
            break;
        }
    }

    return StringWriterClose(w);
}

void Json5EscapeDataWriter(const Slice unescaped_data, Writer *const writer)
{
    // See: https://spec.json5.org/#strings

    const char *const data = unescaped_data.data;
    assert(data != NULL);

    const size_t size = unescaped_data.size;

    for (size_t index = 0; index < size; index++)
    {
        const char byte = data[index];
        switch (byte)
        {
        case '\0':
            WriterWrite(writer, "\\0");
            break;
        case '\"':
        case '\\':
            WriterWriteChar(writer, '\\');
            WriterWriteChar(writer, byte);
            break;
        case '\b':
            WriterWrite(writer, "\\b");
            break;
        case '\f':
            WriterWrite(writer, "\\f");
            break;
        case '\n':
            WriterWrite(writer, "\\n");
            break;
        case '\r':
            WriterWrite(writer, "\\r");
            break;
        case '\t':
            WriterWrite(writer, "\\t");
            break;
        default:
        {
            if (CharIsPrintableAscii(byte))
            {
                WriterWriteChar(writer, byte);
            }
            else
            {
                // unsigned char behaves better when implicitly cast to int:
                WriterWriteF(writer, "\\x%2.2X", (unsigned char) byte);
            }
            break;
        }
        }
    }
}

char *Json5EscapeData(Slice unescaped_data)
{
    Writer *writer = StringWriter();

    Json5EscapeDataWriter(unescaped_data, writer);

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

    JsonObjectRemoveKey(object, key);

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

static ssize_t JsonElementIndexInParentObject(JsonElement *parent, const char* key)
{
    assert(parent);
    assert(parent->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(parent->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    return SeqIndexOf(parent->container.children, key, CompareKeyToPropertyName);
}

bool JsonObjectRemoveKey(JsonElement *object, const char *key)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    ssize_t index = JsonElementIndexInParentObject(object, key);
    if (index != -1)
    {
        SeqRemove(object->container.children, index);
        return true;
    }
    return false;
}

JsonElement *JsonObjectDetachKey(JsonElement *object, const char *key)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    JsonElement *detached = NULL;

    ssize_t index = JsonElementIndexInParentObject(object, key);
    if (index != -1)
    {
        detached = SeqLookup(object->container.children, key, JsonElementHasProperty);
        SeqSoftRemove(object->container.children, index);
    }

    return detached;
}

const char *JsonObjectGetAsString(const JsonElement *object, const char *key)
{
    assert(object);
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);
    assert(key);

    JsonElement *childPrimitive = SeqLookup(object->container.children, key, JsonElementHasProperty);

    if (childPrimitive)
    {
        assert(childPrimitive->type == JSON_ELEMENT_TYPE_PRIMITIVE);
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

JsonElement *JsonObjectGet(const JsonElement *object, const char *key)
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

JsonElement *JsonArrayGet(const JsonElement *array, size_t index)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    return JsonAt(array, index);
}

bool JsonArrayContainsOnlyPrimitives(JsonElement *array)
{
    assert(array);
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);


    for (size_t i = 0; i < JsonLength(array); i++)
    {
        JsonElement *child = JsonArrayGet(array, i);

        if (child->type != JSON_ELEMENT_TYPE_PRIMITIVE)
        {
            return false;
        }
    }

    return true;
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
    return JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_STRING, xstrdup(value));
}

JsonElement *JsonIntegerCreate(int value)
{
    char *buffer;
    xasprintf(&buffer, "%d", value);

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

static void JsonContainerWrite(Writer *writer, const JsonElement *containerElement, size_t indent_level);
static void JsonContainerWriteCompact(Writer *writer, const JsonElement *containerElement);

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

static void JsonPrimitiveWrite(Writer *writer, const JsonElement *primitiveElement, size_t indent_level)
{
    assert(primitiveElement->type == JSON_ELEMENT_TYPE_PRIMITIVE);

    switch (primitiveElement->primitive.type)
    {
    case JSON_PRIMITIVE_TYPE_STRING:
        PrintIndent(writer, indent_level);
        {
            char *encoded = JsonEncodeString(primitiveElement->primitive.value);
            WriterWriteF(writer, "\"%s\"", encoded);
            free(encoded);
        }
        break;

    default:
        PrintIndent(writer, indent_level);
        WriterWrite(writer, primitiveElement->primitive.value);
        break;
    }
}

static void JsonArrayWrite(Writer *writer, const JsonElement *array, size_t indent_level)
{
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    if (JsonLength(array) == 0)
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
            JsonPrimitiveWrite(writer, child, indent_level + 1);
            break;

        case JSON_ELEMENT_TYPE_CONTAINER:
            PrintIndent(writer, indent_level + 1);
            JsonContainerWrite(writer, child, indent_level + 1);
            break;

        default:
            UnexpectedError("Unknown JSON element type: %d",
                            child->type);
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

int JsonElementPropertyCompare(const void *e1, const void *e2, ARG_UNUSED void *user_data)
{
    return strcmp( ((JsonElement*)e1)->propertyName,
                   ((JsonElement*)e2)->propertyName);
}

void JsonObjectWrite(Writer *writer, const JsonElement *object, size_t indent_level)
{
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);

    WriterWrite(writer, "{\n");

    for (size_t i = 0; i < object->container.children->length; i++)
    {
        assert(((JsonElement*)object->container.children->data[i])->propertyName);
    }

    // sort the children Seq so the output is canonical (keys are sorted)
    // we've already asserted that the children have a valid propertyName
    JsonSort((JsonElement*)object, (JsonComparator*)JsonElementPropertyCompare, NULL);

    for (size_t i = 0; i < object->container.children->length; i++)
    {
        JsonElement *child = object->container.children->data[i];

        PrintIndent(writer, indent_level + 1);

        assert(child->propertyName);
        WriterWriteF(writer, "\"%s\": ", child->propertyName);

        switch (child->type)
        {
        case JSON_ELEMENT_TYPE_PRIMITIVE:
            JsonPrimitiveWrite(writer, child, 0);
            break;

        case JSON_ELEMENT_TYPE_CONTAINER:
            JsonContainerWrite(writer, child, indent_level + 1);
            break;

        default:
            UnexpectedError("Unknown JSON element type: %d",
                            child->type);
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

static void JsonContainerWrite(Writer *writer, const JsonElement *container, size_t indent_level)
{
    assert(container->type == JSON_ELEMENT_TYPE_CONTAINER);

    switch (container->container.type)
    {
    case JSON_CONTAINER_TYPE_OBJECT:
        JsonObjectWrite(writer, container, indent_level);
        break;

    case JSON_CONTAINER_TYPE_ARRAY:
        JsonArrayWrite(writer, container, indent_level);
    }
}

void JsonWrite(Writer *writer, const JsonElement *element, size_t indent_level)
{
    assert(writer);
    assert(element);

    switch (element->type)
    {
    case JSON_ELEMENT_TYPE_CONTAINER:
        JsonContainerWrite(writer, element, indent_level);
        break;

    case JSON_ELEMENT_TYPE_PRIMITIVE:
        JsonPrimitiveWrite(writer, element, indent_level);
        break;

    default:
        UnexpectedError("Unknown JSON element type: %d",
                        element->type);
    }
}

static void JsonArrayWriteCompact(Writer *writer, const JsonElement *array)
{
    assert(array->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(array->container.type == JSON_CONTAINER_TYPE_ARRAY);

    if (JsonLength(array) == 0)
    {
        WriterWrite(writer, "[]");
        return;
    }

    WriterWrite(writer, "[");
    for (size_t i = 0; i < array->container.children->length; i++)
    {
        JsonElement *child = array->container.children->data[i];

        switch (child->type)
        {
        case JSON_ELEMENT_TYPE_PRIMITIVE:
            JsonPrimitiveWrite(writer, child, 0);
            break;

        case JSON_ELEMENT_TYPE_CONTAINER:
            JsonContainerWriteCompact(writer, child);
            break;

        default:
            UnexpectedError("Unknown JSON element type: %d",
                            child->type);
        }

        if (i < array->container.children->length - 1)
        {
            WriterWrite(writer, ",");
        }
    }

    WriterWriteChar(writer, ']');
}

void JsonObjectWriteCompact(Writer *writer, const JsonElement *object)
{
    assert(object->type == JSON_ELEMENT_TYPE_CONTAINER);
    assert(object->container.type == JSON_CONTAINER_TYPE_OBJECT);

    WriterWrite(writer, "{");

    for (size_t i = 0; i < object->container.children->length; i++)
    {
        assert(((JsonElement*)object->container.children->data[i])->propertyName);
    }

    // sort the children Seq so the output is canonical (keys are sorted)
    // we've already asserted that the children have a valid propertyName
    JsonSort((JsonElement*)object, (JsonComparator*)JsonElementPropertyCompare, NULL);

    for (size_t i = 0; i < object->container.children->length; i++)
    {
        JsonElement *child = object->container.children->data[i];

        WriterWriteF(writer, "\"%s\":", child->propertyName);

        switch (child->type)
        {
        case JSON_ELEMENT_TYPE_PRIMITIVE:
            JsonPrimitiveWrite(writer, child, 0);
            break;

        case JSON_ELEMENT_TYPE_CONTAINER:
            JsonContainerWriteCompact(writer, child);
            break;

        default:
            UnexpectedError("Unknown JSON element type: %d",
                            child->type);
        }

        if (i < object->container.children->length - 1)
        {
            WriterWriteChar(writer, ',');
        }
    }

    WriterWriteChar(writer, '}');
}

static void JsonContainerWriteCompact(Writer *writer, const JsonElement *container)
{
    assert(container->type == JSON_ELEMENT_TYPE_CONTAINER);

    switch (container->container.type)
    {
    case JSON_CONTAINER_TYPE_OBJECT:
        JsonObjectWriteCompact(writer, container);
        break;

    case JSON_CONTAINER_TYPE_ARRAY:
        JsonArrayWriteCompact(writer, container);
    }
}

void JsonWriteCompact(Writer *w, const JsonElement *element)
{
    assert(w);
    assert(element);

    switch (element->type)
    {
    case JSON_ELEMENT_TYPE_CONTAINER:
        JsonContainerWriteCompact(w, element);
        break;

    case JSON_ELEMENT_TYPE_PRIMITIVE:
        JsonPrimitiveWrite(w, element, 0);
        break;

    default:
        UnexpectedError("Unknown JSON element type: %d",
                        element->type);
    }
}

// *******************************************************************************************
// Parsing
// *******************************************************************************************

static JsonParseError JsonParseAsObject(void *lookup_context, JsonLookup *lookup_function, const char **data, JsonElement **json_out);

static JsonElement *JsonParseAsBoolean(const char **data)
{
    if (StringMatch("^true", *data, NULL, NULL))
    {
        char next = *(*data + 4);
        if (IsSeparator(next) || next == '\0')
        {
            *data += 3;
            return JsonBoolCreate(true);
        }
    }
    else if (StringMatch("^false", *data, NULL, NULL))
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
    if (StringMatch("^null", *data, NULL, NULL))
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
    static const char *const parse_errors[JSON_PARSE_ERROR_MAX] =
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
        [JSON_PARSE_ERROR_ARRAY_COMMA] = "Unable to parse json data as array, extraneous commas",

        [JSON_PARSE_ERROR_OBJECT_BAD_SYMBOL] = "Unable to parse json data as object, unrecognized token beginning entry",
        [JSON_PARSE_ERROR_OBJECT_START] = "Unable to parse json data as object, did not start with '{'",
        [JSON_PARSE_ERROR_OBJECT_END] = "Unable to parse json data as string, did not end with '}'",
        [JSON_PARSE_ERROR_OBJECT_COLON] = "Unable to parse json data as object, ':' seen without having specified an l-value",
        [JSON_PARSE_ERROR_OBJECT_COMMA] = "Unable to parse json data as object, ',' seen without having specified an r-value",
        [JSON_PARSE_ERROR_OBJECT_ARRAY_LVAL] = "Unable to parse json data as object, array not allowed as l-value",
        [JSON_PARSE_ERROR_OBJECT_OBJECT_LVAL] = "Unable to parse json data as object, object not allowed as l-value",
        [JSON_PARSE_ERROR_OBJECT_OPEN_LVAL] = "Unable to parse json data as object, tried to close object having opened an l-value",

        [JSON_PARSE_ERROR_INVALID_START] = "Unwilling to parse json data starting with invalid character",
        [JSON_PARSE_ERROR_TRUNCATED] = "Unable to parse JSON without truncating",
        [JSON_PARSE_ERROR_NO_LIBYAML] = "CFEngine was not built with libyaml support",
        [JSON_PARSE_ERROR_LIBYAML_FAILURE] = "libyaml internal failure",
        [JSON_PARSE_ERROR_NO_DATA] = "No data"
    };

    assert(error < JSON_PARSE_ERROR_MAX);
    return parse_errors[error];
}

static JsonParseError JsonParseAsString(const char **data, char **str_out)
{
    /* NB: although JavaScript supports both single and double quotes
     * as string delimiters, JSON only supports double quotes. */
    if (**data != '"')
    {
        *str_out = NULL;
        return JSON_PARSE_ERROR_STRING_NO_DOUBLEQUOTE_START;
    }

    Writer *writer = StringWriter();

    for (*data = *data + 1; **data != '\0'; *data = *data + 1)
    {
        switch (**data)
        {
        case '"':
            *str_out = StringWriterClose(writer);
            return JSON_PARSE_OK;

        case '\\':
            *data = *data + 1;
            switch (**data)
            {
            case '\\':
            case '"':
            case '/':
                break;

            case 'b':
                WriterWriteChar(writer, '\b');
                continue;
            case 'f':
                WriterWriteChar(writer, '\f');
                continue;
            case 'n':
                WriterWriteChar(writer, '\n');
                continue;
            case 'r':
                WriterWriteChar(writer, '\r');
                continue;
            case 't':
                WriterWriteChar(writer, '\t');
                continue;

            default:
                /* Unrecognised escape sequence.
                 *
                 * For example, we fail to handle Unicode escapes -
                 * \u{hex digits} - we have no way to represent the
                 * character they denote.  So keep them verbatim, for
                 * want of any other way to handle them; but warn. */
                Log(LOG_LEVEL_DEBUG,
                    "Keeping verbatim unrecognised JSON escape '%.6s'",
                    *data - 1); /* i.e. include the \ in the displayed escape */
                WriterWriteChar(writer, '\\');
                break;
            }
            /* Deliberate fall-through */
        default:
            WriterWriteChar(writer, **data);
            break;
        }
    }

    WriterClose(writer);
    *str_out = NULL;
    return JSON_PARSE_ERROR_STRING_NO_DOUBLEQUOTE_END;
}

JsonParseError JsonParseAsNumber(const char **data, JsonElement **json_out)
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

static JsonParseError JsonParseAsPrimitive(const char **data, JsonElement **json_out)
{
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
            *json_out = JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_STRING, JsonDecodeString(value));
            free(value);
        }
        return JSON_PARSE_OK;

    default:
        if (**data == '-' || **data == '0' || IsDigit(**data))
        {
            JsonParseError err = JsonParseAsNumber(data, json_out);
            if (err != JSON_PARSE_OK)
            {
                return err;
            }
            return JSON_PARSE_OK;
        }

        JsonElement *child_bool = JsonParseAsBoolean(data);
        if (child_bool)
        {
            *json_out = child_bool;
            return JSON_PARSE_OK;
        }

        JsonElement *child_null = JsonParseAsNull(data);
        if (child_null)
        {
            *json_out = child_null;
            return JSON_PARSE_OK;
        }

        *json_out = NULL;
        return JSON_PARSE_ERROR_OBJECT_BAD_SYMBOL;
    }
}

static JsonParseError JsonParseAsArray(void *lookup_context, JsonLookup *lookup_function, const char **data, JsonElement **json_out)
{
    if (**data != '[')
    {
        *json_out = NULL;
        return JSON_PARSE_ERROR_ARRAY_START;
    }

    JsonElement *array = JsonArrayCreate(DEFAULT_CONTAINER_CAPACITY);
    char prev_char = '[';

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
                JsonArrayAppendElement(array, JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_STRING, JsonDecodeString(value)));
                free(value);
            }
            break;

        case '[':
            {
                if (prev_char != '[' && prev_char != ',')
                {
                    JsonDestroy(array);
                    return JSON_PARSE_ERROR_ARRAY_START;
                }
                JsonElement *child_array = NULL;
                JsonParseError err = JsonParseAsArray(lookup_context, lookup_function, data, &child_array);
                if (err != JSON_PARSE_OK)
                {
                    JsonDestroy(array);
                    return err;
                }
                assert(child_array);

                JsonArrayAppendArray(array, child_array);
            }
            break;

        case '{':
            {
                if (prev_char != '[' && prev_char != ',')
                {
                    JsonDestroy(array);
                    return JSON_PARSE_ERROR_ARRAY_START;
                }
                JsonElement *child_object = NULL;
                JsonParseError err = JsonParseAsObject(lookup_context, lookup_function, data, &child_object);
                if (err != JSON_PARSE_OK)
                {
                    JsonDestroy(array);
                    return err;
                }
                assert(child_object);

                JsonArrayAppendObject(array, child_object);
            }
            break;

        case ',':
            if (prev_char == ',' || prev_char == '[')
            {
                JsonDestroy(array);
                return JSON_PARSE_ERROR_ARRAY_COMMA;
            }
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
                    JsonDestroy(array);
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

            if (lookup_function)
            {
                JsonElement *child_ref = (*lookup_function)(lookup_context, data);
                if (child_ref)
                {
                    JsonArrayAppendElement(array, child_ref);
                    break;
                }
            }

            *json_out = NULL;
            JsonDestroy(array);
            return JSON_PARSE_ERROR_OBJECT_BAD_SYMBOL;
        }

        prev_char = **data;
    }

    *json_out = NULL;
    JsonDestroy(array);
    return JSON_PARSE_ERROR_ARRAY_END;
}

static JsonParseError JsonParseAsObject(void *lookup_context, JsonLookup *lookup_function, const char **data, JsonElement **json_out)
{
    if (**data != '{')
    {
        *json_out = NULL;
        return JSON_PARSE_ERROR_ARRAY_START;
    }

    JsonElement *object = JsonObjectCreate(DEFAULT_CONTAINER_CAPACITY);
    char *property_name = NULL;
    char prev_char = '{';

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
                    JsonDestroy(object);
                    return err;
                }
                assert(property_value);

                JsonObjectAppendElement(object, property_name, JsonElementCreatePrimitive(JSON_PRIMITIVE_TYPE_STRING, JsonDecodeString(property_value)));
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
                    JsonDestroy(object);
                    return err;
                }
                assert(property_name);
            }
            break;

        case ':':
            if (property_name == NULL || prev_char == ':' || prev_char == ',')
            {
                json_out = NULL;
                free(property_name);
                JsonDestroy(object);
                return JSON_PARSE_ERROR_OBJECT_COLON;
            }
            break;

        case ',':
            if (property_name != NULL || prev_char == ':' || prev_char == ',')
            {
                free(property_name);
                JsonDestroy(object);
                return JSON_PARSE_ERROR_OBJECT_COMMA;
            }
            break;

        case '[':
            if (property_name != NULL)
            {
                JsonElement *child_array = NULL;
                JsonParseError err = JsonParseAsArray(lookup_context, lookup_function, data, &child_array);
                if (err != JSON_PARSE_OK)
                {
                    free(property_name);
                    JsonDestroy(object);
                    return err;
                }

                JsonObjectAppendArray(object, property_name, child_array);
                free(property_name);
                property_name = NULL;
            }
            else
            {
                free(property_name);
                JsonDestroy(object);
                return JSON_PARSE_ERROR_OBJECT_ARRAY_LVAL;
            }
            break;

        case '{':
            if (property_name != NULL)
            {
                JsonElement *child_object = NULL;
                JsonParseError err = JsonParseAsObject(lookup_context, lookup_function, data, &child_object);
                if (err != JSON_PARSE_OK)
                {
                    free(property_name);
                    JsonDestroy(object);
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
                JsonDestroy(object);
                return JSON_PARSE_ERROR_OBJECT_OBJECT_LVAL;
            }
            break;

        case '}':
            if (property_name != NULL)
            {
                *json_out = NULL;
                free(property_name);
                JsonDestroy(object);
                return JSON_PARSE_ERROR_OBJECT_OPEN_LVAL;
            }
            free(property_name);
            *json_out = object;
            return JSON_PARSE_OK;

        default:
            // Note the character class excludes ':'.
            // This will match the key from { foo : 2 } but not { -foo: 2 }
            if (property_name == NULL &&
                StringMatch("^\\w[-\\w]*\\s*:", *data, NULL, NULL))
            {
                char *colon = strchr(*data, ':');

                // Step backwards until we are on the last whitespace.

                // Note that this is safe because the above regex guarantees
                // we will find at least one non-whitespace character as we
                // go backwards.
                char *ws = colon;
                while (IsWhitespace(*(ws-1)))
                {
                    ws -= 1;
                }

                property_name = xstrndup(*data, ws - *data);
                *data = colon;

                break;
            }
            else if (property_name)
            {
                if (**data == '-' || **data == '0' || IsDigit(**data))
                {
                    JsonElement *child = NULL;
                    JsonParseError err = JsonParseAsNumber(data, &child);
                    if (err != JSON_PARSE_OK)
                    {
                        free(property_name);
                        JsonDestroy(object);
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

                if (lookup_function)
                {
                    JsonElement *child_ref = (*lookup_function)(lookup_context, data);
                    if (child_ref)
                    {
                        JsonObjectAppendElement(object, property_name, child_ref);
                        free(property_name);
                        property_name = NULL;
                        break;
                    }
                }

            }

            *json_out = NULL;
            free(property_name);
            JsonDestroy(object);
            return JSON_PARSE_ERROR_OBJECT_BAD_SYMBOL;
        }

        prev_char = **data;
    }

    *json_out = NULL;
    free(property_name);
    JsonDestroy(object);
    return JSON_PARSE_ERROR_OBJECT_END;
}

JsonParseError JsonParse(const char **data, JsonElement **json_out)
{
    return JsonParseWithLookup(NULL, NULL, data, json_out);
}

JsonParseError JsonParseWithLookup(void *lookup_context, JsonLookup *lookup_function, const char **data, JsonElement **json_out)
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
            return JsonParseAsObject(lookup_context, lookup_function, data, json_out);
        }
        else if (**data == '[')
        {
            return JsonParseAsArray(lookup_context, lookup_function, data, json_out);
        }
        else if (IsWhitespace(**data))
        {
            (*data)++;
        }
        else
        {
            return JsonParseAsPrimitive(data, json_out);
        }
    }

    return JSON_PARSE_ERROR_NO_DATA;
}

JsonParseError JsonParseAnyFile(const char *path, size_t size_max, JsonElement **json_out, const bool yaml_format)
{
    bool truncated = false;
    Writer *contents = FileRead(path, size_max, &truncated);
    if (!contents)
    {
        return JSON_PARSE_ERROR_NO_DATA;
    }
    else if (truncated)
    {
        return JSON_PARSE_ERROR_TRUNCATED;
    }
    assert(json_out);
    *json_out = NULL;
    const char *data = StringWriterData(contents);
    JsonParseError err;

    if (yaml_format)
    {
        err = JsonParseYamlString(&data, json_out);
    }
    else
    {
        err = JsonParse(&data, json_out);
    }

    WriterClose(contents);
    return err;
}

JsonParseError JsonParseFile(const char *path, size_t size_max, JsonElement **json_out)
{
    return JsonParseAnyFile(path, size_max, json_out, false);
}

/*******************************************************************/

// returns NULL on any failure
// takes either a pre-compiled pattern OR a regex (one of the two shouldn't be NULL)
JsonElement* StringCaptureData(pcre *pattern, const char* regex, const char* data)
{
    assert(regex || pattern);
    assert(data);

    Seq *s;

    if (pattern != NULL)
    {
        s = StringMatchCapturesWithPrecompiledRegex(pattern, data, true);
    }
    else
    {
        s = StringMatchCaptures(regex, data, true);
    }

    if (!s || SeqLength(s) == 0)
    {
        SeqDestroy(s);
        return NULL;
    }

    JsonElement *json = JsonObjectCreate(SeqLength(s)/2);

    for (int i = 1; i < SeqLength(s); i+=2)
    {
        Buffer *key = SeqAt(s, i-1);
        Buffer *value = SeqAt(s, i);

        JsonObjectAppendString(json, BufferData(key), BufferData(value));
    }

    SeqDestroy(s);

    JsonObjectRemoveKey(json, "0");
    return json;
}
