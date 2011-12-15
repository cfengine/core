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

#include "cf3.defs.h"

typedef struct Rlist JsonObject;
typedef struct Rlist JsonArray;

void JsonObjectDelete(JsonObject *object);
void JsonArrayDelete(JsonArray *array);

void JsonObjectAppendString(JsonObject **parent, const char *key, const char *value);
void JsonObjectAppendArray(JsonObject **parent, const char *key, JsonArray *value);
void JsonObjectAppendObject(JsonObject **parent, const char *key, JsonObject *value);

void JsonArrayAppendObject(JsonArray **parent, JsonObject *value);
void JsonArrayAppendString(JsonArray **parent, const char *value);

size_t JsonArrayLength(JsonArray *array);

void JsonStringPrint(FILE *out, const char *value, int indent_level);
void JsonArrayPrint(FILE* out, JsonArray *value, int indent_level);
void JsonObjectPrint(FILE* out, JsonObject *value, int indent_level);


#endif
