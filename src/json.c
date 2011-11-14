#include "json.h"

#define JSON_OBJECT_TYPE 'o'
#define JSON_ARRAY_TYPE 'a'

static const int SPACES_PER_INDENT = 2;

static void JsonElementDelete(struct Rlist *element)
{
struct Rlist *rp = NULL, *next = NULL;
for (rp = element; rp != NULL; rp = next)
   {
   next = rp->next;
   switch (rp->type)
      {
      case CF_ASSOC:
	 {
	 struct CfAssoc *assoc = rp->item;
	 switch (assoc->rtype)
	    {
	    case JSON_OBJECT_TYPE:
	    case JSON_ARRAY_TYPE:
	       JsonElementDelete(assoc->rval);
	       break;

	    default:
	       free(assoc->rval);
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
struct CfAssoc *ap = NULL;

if (value == NULL)
   {
   return;
   }

ap = AssocNewReference(key, value, JSON_OBJECT_TYPE, cf_notype);
RlistAppendReference(parent, ap, CF_ASSOC);
}

void JsonObjectAppendArray(JsonObject **parent, const char *key, JsonArray *value)
{
struct CfAssoc *ap = NULL;

if (value == NULL)
   {
   return;
   }

ap = AssocNewReference(key, value, JSON_ARRAY_TYPE, cf_notype);
RlistAppendReference(parent, ap, CF_ASSOC);
}

void JsonObjectAppendString(JsonObject **parent, const char *key, const char *value)
{
struct CfAssoc *ap = NULL;

if (value == NULL)
   {
   return;
   }

ap = AssocNewReference(key, xstrdup(value), CF_SCALAR, cf_str);
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

static void ShowIndent(FILE *out, int num)
{
int i = 0;
for (i = 0; i < num * SPACES_PER_INDENT; i++)
   {
   fputc(' ', out);
   }
}

void JsonStringPrint(FILE *out, const char *value, int indent_level)
{
ShowIndent(out, indent_level);
fprintf(out, "\"%s\"", value);
}

void JsonArrayPrint(FILE *out, JsonArray *value, int indent_level)
{
struct Rlist *rp = NULL;

if (JsonArrayLength(value) == 0)
   {
   fprintf(out, "[]");
   return;
   }

fprintf(out, "[\n");
for (rp = value; rp != NULL; rp = rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
	 JsonStringPrint(out, (const char*)rp->item, indent_level + 1);
	 break;

      case JSON_OBJECT_TYPE:
	 ShowIndent(out, indent_level + 1);
	 JsonObjectPrint(out, (JsonObject*)rp->item, indent_level + 1);
	 break;

      default:
	 /* TODO: not implemented, how to deal? */
	 JsonStringPrint(out, "", indent_level + 1);
	 break;
      }

      if (rp->next != NULL)
	 {
	 fprintf(out, ",\n");
	 }
      else
	 {
	 fprintf(out, "\n");
	 }
   }

ShowIndent(out, indent_level);
fprintf(out, "]");
}

void JsonObjectPrint(FILE *out, JsonObject *value, int indent_level)
{
struct Rlist *rp = NULL;

fprintf(out, "{\n");

for (rp = value; rp != NULL; rp = rp->next)
   {
   struct CfAssoc *entry = NULL;

   ShowIndent(out, indent_level + 1);
   entry = (struct CfAssoc *)rp->item;

   fprintf(out, "\"%s\": ", entry->lval);
   switch (entry->rtype)
      {
      case CF_SCALAR:
	 switch (entry->dtype)
	    {
	    case cf_str:
	       JsonStringPrint(out, (const char*)entry->rval, 0);
	       break;

	    default:
	       /* TODO: not implemented, how to deal? */
	       JsonStringPrint(out, "", indent_level + 1);
	       break;
	    }
	 break;

      case JSON_OBJECT_TYPE:
	 JsonObjectPrint(out, entry->rval, indent_level + 1);
	 break;

      case JSON_ARRAY_TYPE:
	 JsonArrayPrint(out, entry->rval, indent_level + 1);
	 break;

      default:
	 /* TODO: internal error, how to deal? */
	 break;
      }
      if (rp->next != NULL)
	 {
	 fprintf(out, ",");
	 }
      fprintf(out, "\n");
   }

ShowIndent(out, indent_level);
fprintf(out, "}");
}
