#include "json.h"

#include "writer.h"

#define JSON_OBJECT_TYPE 'o'
#define JSON_ARRAY_TYPE 'a'

static const int SPACES_PER_INDENT = 2;

static void JsonElementDelete(Rlist *element)
{
Rlist *rp = NULL, *next = NULL;
for (rp = element; rp != NULL; rp = next)
   {
   next = rp->next;
   switch (rp->type)
      {
      case CF_ASSOC:
         {
         CfAssoc *assoc = rp->item;
         switch (assoc->rval.rtype)
            {
            case JSON_OBJECT_TYPE:
            case JSON_ARRAY_TYPE:
            JsonElementDelete(assoc->rval.item);
            break;

	    default:
	       free(assoc->rval.item);
	       break;
	    }
	 free(assoc->lval);
	 free(assoc);
	 }
	 break;

      default:
	 free(rp->item);
	 break;
      }
   free(rp);
   }
}

void JsonObjectDelete(JsonObject *object)
{
JsonElementDelete(object);
}

void JsonArrayDelete(JsonArray *array)
{
JsonElementDelete(array);
}

void JsonObjectAppendObject(JsonObject **parent, const char *key, JsonObject *value)
{
CfAssoc *ap = NULL;

if (value == NULL)
   {
   return;
   }

ap = AssocNewReference(key, (Rval) { value, JSON_OBJECT_TYPE }, cf_notype);
RlistAppendReference(parent, ap, CF_ASSOC);
}

void JsonObjectAppendArray(JsonObject **parent, const char *key, JsonArray *value)
{
CfAssoc *ap = NULL;

if (value == NULL)
   {
   return;
   }

ap = AssocNewReference(key, (Rval) { value, JSON_ARRAY_TYPE }, cf_notype);
RlistAppendReference(parent, ap, CF_ASSOC);
}

void JsonObjectAppendString(JsonObject **parent, const char *key, const char *value)
{
CfAssoc *ap = NULL;

if (value == NULL)
   {
   return;
   }

ap = AssocNewReference(key, (Rval) { xstrdup(value), CF_SCALAR }, cf_str);
RlistAppendReference(parent, ap, CF_ASSOC);
}

void JsonObjectAppendInteger(JsonObject **parent, const char *key, int value)
{
char *buffer = xcalloc(32, sizeof(char));
snprintf(buffer, 32, "%d", value);

CfAssoc *ap = AssocNewReference(key, (Rval) { buffer, CF_SCALAR }, cf_int);
RlistAppendReference(parent, ap, CF_ASSOC);
}

void JsonObjectAppendReal(JsonObject **parent, const char *key, double value)
{
char *buffer = xcalloc(32, sizeof(char));
snprintf(buffer, 32, "%.4f", value);

CfAssoc *ap = AssocNewReference(key, (Rval) { buffer, CF_SCALAR }, cf_real);
RlistAppendReference(parent, ap, CF_ASSOC);
}

void JsonArrayAppendObject(JsonArray **parent, JsonObject *value)
{
if (value == NULL)
   {
   return;
   }

RlistAppendReference(parent, value, JSON_OBJECT_TYPE);
}

void JsonArrayAppendArray(JsonArray **parent, JsonArray *value)
{
if (value == NULL)
   {
   return;
   }

RlistAppendReference(parent, value, JSON_ARRAY_TYPE);
}

void JsonArrayAppendString(JsonArray **parent, const char *value)
{
if (value == NULL)
   {
   return;
   }

RlistAppendReference(parent, xstrdup(value), CF_SCALAR);
}

size_t JsonArrayLength(JsonArray *array)
{
return RlistLen(array);
}

static void ShowIndent(Writer *writer, int num)
{
int i = 0;
for (i = 0; i < num * SPACES_PER_INDENT; i++)
   {
   WriterWriteChar(writer, ' ');
   }
}

void JsonStringPrint(Writer *writer, const char *value, int indent_level)
{
ShowIndent(writer, indent_level);
WriterWriteF(writer, "\"%s\"", value);
}

void JsonNumberPrint(Writer *writer, const char *value, int indent_level)
{
ShowIndent(writer, indent_level);
WriterWrite(writer, value);
}

void JsonArrayPrint(Writer *writer, JsonArray *value, int indent_level)
{
Rlist *rp = NULL;

if (JsonArrayLength(value) == 0)
   {
   WriterWrite(writer, "[]");
   return;
   }

WriterWrite(writer, "[\n");
for (rp = value; rp != NULL; rp = rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
         JsonStringPrint(writer, (const char*)rp->item, indent_level + 1);
         break;

      case JSON_OBJECT_TYPE:
         ShowIndent(writer, indent_level + 1);
         JsonObjectPrint(writer, (JsonObject*)rp->item, indent_level + 1);
         break;

      default:
	 /* TODO: not implemented, how to deal? */
         JsonStringPrint(writer, "", indent_level + 1);
         break;
      }

      if (rp->next != NULL)
	 {
         WriterWrite(writer, ",\n");
	 }
      else
	 {
         WriterWrite(writer, "\n");
	 }
   }

ShowIndent(writer, indent_level);
WriterWriteChar(writer, ']');
}

void JsonObjectPrint(Writer *writer, JsonObject *value, int indent_level)
{
Rlist *rp = NULL;

WriterWrite(writer, "{\n");

for (rp = value; rp != NULL; rp = rp->next)
   {
   CfAssoc *entry = NULL;

   ShowIndent(writer, indent_level + 1);
   entry = (CfAssoc *)rp->item;

   WriterWriteF(writer, "\"%s\": ", entry->lval);
   switch (entry->rval.rtype)
      {
      case CF_SCALAR:
         switch (entry->dtype)
            {
            case cf_str:
               JsonStringPrint(writer, (const char*)entry->rval.item, 0);
               break;

            case cf_int:
	    case cf_real:
               JsonNumberPrint(writer, (const char*)entry->rval.item, 0);
               break;

            default:
               /* TODO: not implemented, how to deal? */
               JsonStringPrint(writer, "", indent_level + 1);
               break;
            }
            break;

      case JSON_OBJECT_TYPE:
         JsonObjectPrint(writer, entry->rval.item, indent_level + 1);
         break;

      case JSON_ARRAY_TYPE:
         JsonArrayPrint(writer, entry->rval.item, indent_level + 1);
         break;

      default:
	 /* TODO: internal error, how to deal? */
	 break;
      }
      if (rp->next != NULL)
         {
         WriterWriteChar(writer, ',');
         }
      WriterWrite(writer, "\n");
   }

ShowIndent(writer, indent_level);
WriterWriteChar(writer, '}');
}

const char *JsonObjectGetAsString(JsonObject *object, const char *key)
{
for (Rlist *rp = (Rlist *)object; rp != NULL; rp = rp->next)
   {
   CfAssoc *entry = rp->item;
   if (strcmp(entry->lval, key) == 0)
      {
      return entry->rval.item;
      }
   }

return NULL;
}

JsonObject *JsonObjectGetAsObject(JsonObject *object, const char *key)
{
for (Rlist *rp = (Rlist *)object; rp != NULL; rp = rp->next)
   {
   CfAssoc *entry = rp->item;
   if (strcmp(entry->lval, key) == 0)
      {
      return entry->rval.item;
      }
   }

return NULL;
}

const char *JsonArrayGetAsString(JsonArray *array, size_t index)
{
Rlist *rp = RlistAt(array, index);
if (rp != NULL)
   {
   return rp->item;
   }

return NULL;
}

JsonObject *JsonArrayGetAsObject(JsonArray *array, size_t index)
{
Rlist *rp = RlistAt(array, index);
if (rp != NULL)
   {
   return rp->item;
   }

return NULL;
}

bool JsonIsWhitespace(char ch)
{
return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

const char *JsonParseAsString(const char **data)
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

JsonArray *JsonParseAsArray(const char **data)
{
if (**data != '[')
   {
   CfDebug("Unable to parse json data as array, did not start with '[': %s", *data);
   return NULL;
   }

JsonArray *array = NULL;

for (*data = *data + 1; **data != '\0'; *data = *data + 1)
   {
   if (JsonIsWhitespace(**data))
      {
      continue;
      }

   switch (**data)
      {
      case '"':
         JsonArrayAppendString(&array, JsonParseAsString(data));
         break;

      case '[':
         JsonArrayAppendArray(&array, JsonParseAsArray(data));
         break;

      case '{':
         JsonArrayAppendObject(&array, JsonParseAsObject(data));
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

JsonObject *JsonParseAsObject(const char **data)
{
if (**data != '{')
   {
   CfDebug("Unable to parse json data as object, did not start with '{': %s", *data);
   return NULL;
   }

JsonObject *object = NULL;
const char *lval = NULL;

for (*data = *data + 1; **data != '\0'; *data = *data + 1)
   {
   if (JsonIsWhitespace(**data))
      {
      continue;
      }

   switch (**data)
      {
      case '"':
         if (lval != NULL)
            {
            JsonObjectAppendString(&object, lval, JsonParseAsString(data));
            lval = NULL;
            }
         else
            {
            lval = JsonParseAsString(data);
            }
         break;

      case ':':
         if (lval == NULL)
            {
            CfDebug("Unable to parse json data as object, ':' seen without having specified an l-value: %s", *data);
            return NULL;
            }
         break;

      case ',':
         if (lval != NULL)
            {
            CfDebug("Unable to parse json data as object, ',' seen without having specified an r-value: %s", *data);
            return NULL;
            }
         break;

      case '[':
         if (lval != NULL)
            {
            JsonObjectAppendArray(&object, lval, JsonParseAsArray(data));
            lval = NULL;
            }
         else
            {
            CfDebug("Unable to parse json data as object, array not allowed as l-value: %s", *data);
            return NULL;
            }
         break;

      case '{':
         if (lval != NULL)
            {
            JsonObjectAppendObject(&object, lval, JsonParseAsObject(data));
            lval = NULL;
            }
         else
            {
            CfDebug("Unable to parse json data as object, object not allowed as l-value: %s", *data);
            return NULL;
            }
         break;

      case '}':
         if (lval != NULL)
            {
            CfDebug("Unable to parse json data as object, tried to close object having opened an l-value: %s", *data);
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
