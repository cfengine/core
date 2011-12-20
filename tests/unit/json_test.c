#include "test.h"
#include "json.h"

static const char *OBJECT_ARRAY = "{\n"
      "  \"first\": [\n"
      "    \"one\",\n"
      "    \"two\"\n"
      "  ]\n"
      "}";

static const char *OBJECT_COMPOUND = "{\n"
      "  \"first\": \"one\",\n"
      "  \"second\": {\n"
      "    \"third\": \"three\"\n"
      "  },\n"
      "  \"fourth\": {\n"
      "    \"fifth\": \"five\"\n"
      "  }\n"
      "}";

static const char *OBJECT_SIMPLE = "{\n"
      "  \"first\": \"one\",\n"
      "  \"second\": \"two\"\n"
      "}";

static const char *ARRAY_SIMPLE = "[\n"
      "  \"one\",\n"
      "  \"two\"\n"
      "]";

static const char *ARRAY_OBJECT = "[\n"
      "  {\n"
      "    \"first\": \"one\"\n"
      "  }\n"
      "]";

static void test_new_delete(void **state)
{
JsonObject *json = NULL;
JsonObjectAppendString(&json, "first", "one");
JsonObjectDelete(json);
}

static void test_show_object_simple(void **state)
{
JsonObject *json = NULL;
FILE *expected = tmpfile();
FILE *actual = tmpfile();

fprintf(expected, "%s", OBJECT_SIMPLE);

JsonObjectAppendString(&json, "first", "one");
JsonObjectAppendString(&json, "second", "two");

JsonObjectPrint(actual, json, 0);

assert_file_equal(expected, actual);
fclose(expected);
fclose(actual);

JsonObjectDelete(json);
}

static void test_show_object_compound(void **state)
{
JsonObject *json = NULL;
FILE *expected = tmpfile();
FILE *actual = tmpfile();

fprintf(expected, "%s", OBJECT_COMPOUND);

JsonObjectAppendString(&json, "first", "one");
   {
   JsonObject *inner = NULL;
   JsonObjectAppendString(&inner, "third", "three");

   JsonObjectAppendObject(&json, "second", inner);
   }
   {
   JsonObject *inner = NULL;
   JsonObjectAppendString(&inner, "fifth", "five");

   JsonObjectAppendObject(&json, "fourth", inner);
   }

JsonObjectPrint(actual, json, 0);

assert_file_equal(expected, actual);
fclose(expected);
fclose(actual);

JsonObjectDelete(json);
}

static void test_show_object_array(void **state)
{
JsonObject *json = NULL;
FILE *expected = tmpfile();
FILE *actual = tmpfile();

fprintf(expected, "%s", OBJECT_ARRAY);

   {
   JsonArray *array = NULL;
   JsonArrayAppendString(&array, "one");
   JsonArrayAppendString(&array, "two");

   JsonObjectAppendArray(&json, "first", array);
   }

JsonObjectPrint(actual, json, 0);

assert_file_equal(expected, actual);
fclose(expected);
fclose(actual);

JsonObjectDelete(json);
}

static void test_show_array(void **state)
{
FILE *expected = tmpfile();
FILE *actual = tmpfile();

fprintf(expected, "%s", ARRAY_SIMPLE);

JsonArray *array = NULL;
JsonArrayAppendString(&array, "one");
JsonArrayAppendString(&array, "two");

JsonArrayPrint(actual, array, 0);

assert_file_equal(expected, actual);
fclose(expected);
fclose(actual);

JsonArrayDelete(array);
}

static void test_show_array_object(void **state)
{
FILE *expected = tmpfile();
FILE *actual = tmpfile();

fprintf(expected, "%s", ARRAY_OBJECT);

JsonArray *array = NULL;
JsonObject *object = NULL;

JsonObjectAppendString(&object, "first", "one");

JsonArrayAppendObject(&array, object);

JsonArrayPrint(actual, array, 0);

assert_file_equal(expected, actual);
fclose(expected);
fclose(actual);

JsonArrayDelete(array);
}

static void test_show_array_empty(void **state)
{
FILE *expected = tmpfile();
FILE *actual = tmpfile();

fprintf(expected, "[]");

JsonArray *array = NULL;

JsonArrayPrint(actual, array, 0);

assert_file_equal(expected, actual);
fclose(expected);
fclose(actual);

JsonArrayDelete(array);
}

static void test_object_get_string(void **state)
{
JsonObject *obj = NULL;
JsonObjectAppendString(&obj, "first", "one");
JsonObjectAppendString(&obj, "second", "two");

assert_string_equal(JsonObjectGetAsString(obj, "second"), "two");
assert_string_equal(JsonObjectGetAsString(obj, "first"), "one");
assert_int_equal(JsonObjectGetAsString(obj, "third"), NULL);
}

static void test_array_get_string(void **state)
{
JsonArray *arr = NULL;
JsonArrayAppendString(&arr, "first");
JsonArrayAppendString(&arr, "second");

assert_string_equal(JsonArrayGetAsString(arr, 1), "second");
assert_string_equal(JsonArrayGetAsString(arr, 0), "first");
assert_int_equal(JsonArrayGetAsString(arr, 2), NULL);
}

static void test_parse_string(void **state)
{
const char *data = "\"snookie\"";
assert_string_equal("snookie", JsonParseAsString(&data));
}

static void test_parse_object_simple(void **state)
{
const char *data = OBJECT_SIMPLE;
JsonObject *obj = JsonParseAsObject(&data);
assert_string_equal(JsonObjectGetAsString(obj, "second"), "two");
assert_string_equal(JsonObjectGetAsString(obj, "first"), "one");
assert_int_equal(JsonObjectGetAsString(obj, "third"), NULL);
}

static void test_parse_array_simple(void **state)
{
const char *data = ARRAY_SIMPLE;
JsonArray *arr = JsonParseAsArray(&data);

assert_string_equal(JsonArrayGetAsString(arr, 1), "two");
assert_string_equal(JsonArrayGetAsString(arr, 0), "one");
assert_int_equal(JsonArrayGetAsString(arr, 2), NULL);
}

static void test_parse_object_compound(void **state)
{
const char *data = OBJECT_COMPOUND;
JsonObject *obj = JsonParseAsObject(&data);

assert_string_equal(JsonObjectGetAsString(obj, "first"), "one");

JsonObject *second = JsonObjectGetAsObject(obj, "second");
assert_string_equal(JsonObjectGetAsString(second, "third"), "three");

JsonObject *fourth = JsonObjectGetAsObject(obj, "fourth");
assert_string_equal(JsonObjectGetAsString(fourth, "fifth"), "five");
}

static void test_parse_array_object(void **state)
{
const char *data = ARRAY_OBJECT;
JsonArray *arr = JsonParseAsArray(&data);

JsonObject *first = JsonArrayGetAsObject(arr, 0);
assert_string_equal(JsonObjectGetAsString(first, "first"), "one");
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_new_delete),
   unit_test(test_show_object_simple),
   unit_test(test_show_object_compound),
   unit_test(test_show_object_array),
   unit_test(test_show_array),
   unit_test(test_show_array_object),
   unit_test(test_show_array_empty),
   unit_test(test_object_get_string),
   unit_test(test_array_get_string),
   unit_test(test_parse_string),
   unit_test(test_parse_object_simple),
   unit_test(test_parse_array_simple),
   unit_test(test_parse_object_compound),
   unit_test(test_parse_array_object),
   };

return run_tests(tests);
}

