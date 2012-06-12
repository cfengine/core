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

#ifndef CFENGINE_JSON_H
#define CFENGINE_JSON_H

typedef struct JsonElement_ JsonElement;

#include "cf3.defs.h"
#include "writer.h"

JsonElement *JsonObjectCreate(size_t initialCapacity);
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

/**
  @brief Destroy a JSON element
  @param element [in] The JSON element to destroy.
  */
void JsonElementDestroy(JsonElement *element);

size_t JsonElementLength(JsonElement *element);
void JsonElementPrint(Writer *writer, JsonElement *element, size_t indent_level);

void JsonObjectAppendString(JsonElement *object, const char *key, const char *value);
void JsonObjectAppendInteger(JsonElement *object, const char *key, int value);
void JsonObjectAppendReal(JsonElement *object, const char *key, double value);
void JsonObjectAppendArray(JsonElement *object, const char *key, JsonElement *array);
void JsonObjectAppendObject(JsonElement *object, const char *key, JsonElement *childObject);
const char *JsonObjectGetAsString(JsonElement *object, const char *key);
JsonElement *JsonObjectGetAsObject(JsonElement *object, const char *key);
JsonElement *JsonObjectGetAsArray(JsonElement *object, const char *key);

void JsonArrayAppendString(JsonElement *array, const char *value);

/**
  @brief Append an integer to an array.
  @param array [in] The JSON array parent.
  @param value [in] The integer value to append.
  */
void JsonArrayAppendInteger(JsonElement *array, int value);

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
void JsonArrayRemoveRange(JsonElement *array, size_t start, size_t end);
const char *JsonArrayGetAsString(JsonElement *array, size_t index);
JsonElement *JsonArrayGetAsObject(JsonElement *array, size_t index);

// do not use parsing in production code.
JsonElement *JsonParse(const char **data);

#endif
