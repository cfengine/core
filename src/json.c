#include "json.h"

enum json_dtype
   {
   json_dtype_object,
   json_dtype_array
   };

static const int SPACES_PER_INDENT = 2;

void DeleteJsonObject(JsonObject *object)
{
DeleteRlist(object);
}

void DeleteJsonArray(JsonArray *array)
{
DeleteRlist(array);
}

void JsonObjectAppendObject(JsonObject **parent, const char *key, JsonObject *value)
{
struct CfAssoc *ap = NewAssoc(key, value, CF_LIST, (enum cfdatatype)json_dtype_object);
AppendRlist(parent, ap, CF_ASSOC);
DeleteAssoc(ap);
}

void JsonObjectAppendString(JsonObject **parent, const char *key, const char *value)
{
struct CfAssoc *ap = NewAssoc(key, value, CF_SCALAR, cf_str);
AppendRlist(parent, ap, CF_ASSOC);
DeleteAssoc(ap);
}

void JsonObjectAppendArray(JsonObject **parent, const char *key, JsonArray *value)
{
struct CfAssoc *ap = NewAssoc(key, value, CF_LIST, (enum cfdatatype)json_dtype_array);
AppendRlist(parent, ap, CF_ASSOC);
DeleteAssoc(ap);
}

void JsonArrayAppendString(JsonArray **parent, char *value)
{
AppendRlist(parent, value, CF_SCALAR);
}

static void ShowIndent(FILE *out, int num)
{ int i;
for (i = 0; i < num * SPACES_PER_INDENT; i++)
   {
   fputc(' ', out);
   }
}

void ShowJsonString(FILE *out, const char *value)
{
fprintf(out, "\"%s\"", value);
}

void ShowJsonArray(FILE *out, JsonArray *value)
{
struct Rlist *rp = NULL;

fprintf(out, "[");
for (rp = value; rp != NULL; rp = rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
	 ShowJsonString(out, (const char*)rp->item);
	 break;

      default:
	 /* TODO: not implemented, how to deal? */
	 ShowJsonString(out, "");
	 break;
      }

      if (rp->next != NULL)
	 {
	 fprintf(out, ", ");
	 }
   }
fprintf(out, "]");

}

void ShowJsonObject(FILE *out, JsonObject *value, int indent_level)
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
	       ShowJsonString(out, (const char*)entry->rval);
	       break;

	    default:
	       /* TODO: not implemented, how to deal? */
	       ShowJsonString(out, "");
	       break;
	    }
	 break;

      case CF_LIST:
	 switch ((enum json_dtype) entry->dtype)
	    {
	    case json_dtype_object:
	       ShowJsonObject(out, entry->rval, indent_level + 1);
	       break;

	    case json_dtype_array:
	       ShowJsonArray(out, entry->rval);
	       break;

	    default:
	       /* TODO: internal error, how to deal? */
	       break;
	    }
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


