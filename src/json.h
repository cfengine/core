#ifndef CFENGINE_JSON_H
#define CFENGINE_JSON_H

#include "cf3.defs.h"

typedef struct Rlist JsonObject;
typedef struct Rlist JsonArray;

void DeleteJsonObject(JsonObject *object);
void DeleteJsonArray(JsonArray *array);

void JsonObjectAppendString(JsonObject **parent, const char *key, const char *value);
void JsonObjectAppendArray(JsonObject **parent, const char *key, JsonArray *value);
void JsonObjectAppendObject(JsonObject **parent, const char *key, JsonObject *value);

void JsonArrayAppendString(JsonArray **parent, char *value);

void ShowJsonString(FILE *out, const char *value);
void ShowJsonArray(FILE* out, JsonArray *value);
void ShowJsonObject(FILE* out, JsonObject *value, int indent_level);


#endif
