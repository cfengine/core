#include "json.h"

enum json_dtype
   {
   json_dtype_object,
   json_dtype_array
   };

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
	    case CF_LIST:
	       JsonElementDelete(assoc->rval);
	       break;

	    default:
	       /* don't free leaves */
	       break;
	    }
	 free(assoc->lval);
	 free(assoc);
	 }
	 break;

      default:
	 /* don't free leaves */
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
struct CfAssoc *ap = AssocNewReference(key, value, CF_LIST, (enum cfdatatype)json_dtype_object);
RlistAppendReference(parent, ap, CF_ASSOC);
}

void JsonObjectAppendString(JsonObject **parent, const char *key, const char *value)
{
struct CfAssoc *ap = AssocNewReference(key, value, CF_SCALAR, cf_str);
RlistAppendReference(parent, ap, CF_ASSOC);
}

void JsonObjectAppendArray(JsonObject **parent, const char *key, JsonArray *value)
{
struct CfAssoc *ap = AssocNewReference(key, value, CF_LIST, (enum cfdatatype)json_dtype_array);
RlistAppendReference(parent, ap, CF_ASSOC);
}

void JsonArrayAppendString(JsonArray **parent, char *value)
{
RlistAppendReference(parent, value, CF_SCALAR);
}

static void ShowIndent(FILE *out, int num)
{ int i;
for (i = 0; i < num * SPACES_PER_INDENT; i++)
   {
   fputc(' ', out);
   }
}

void JsonStringPrint(FILE *out, const char *value)
{
fprintf(out, "\"%s\"", value);
}

void JsonArrayPrint(FILE *out, JsonArray *value)
{
struct Rlist *rp = NULL;

fprintf(out, "[");
for (rp = value; rp != NULL; rp = rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
	 JsonStringPrint(out, (const char*)rp->item);
	 break;

      default:
	 /* TODO: not implemented, how to deal? */
	 JsonStringPrint(out, "");
	 break;
      }

      if (rp->next != NULL)
	 {
	 fprintf(out, ", ");
	 }
   }
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
	       JsonStringPrint(out, (const char*)entry->rval);
	       break;

	    default:
	       /* TODO: not implemented, how to deal? */
	       JsonStringPrint(out, "");
	       break;
	    }
	 break;

      case CF_LIST:
	 switch ((enum json_dtype) entry->dtype)
	    {
	    case json_dtype_object:
	       JsonObjectPrint(out, entry->rval, indent_level + 1);
	       break;

	    case json_dtype_array:
	       JsonArrayPrint(out, entry->rval);
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


