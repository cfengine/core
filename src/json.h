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

void JsonArrayAppendString(JsonArray **parent, char *value);

void JsonStringPrint(FILE *out, const char *value);
void JsonArrayPrint(FILE* out, JsonArray *value);
void JsonObjectPrint(FILE* out, JsonObject *value, int indent_level);


#endif
