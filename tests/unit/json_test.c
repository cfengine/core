#include "test.h"
#include "json.h"

static void test_new_delete(void **state)
{
JsonObject *json = NULL;
JsonObjectAppendString(&json, "first", "one");
DeleteJsonObject(json);
}

static void test_show_object_simple(void **state)
{
JsonObject *json = NULL;
FILE *expected = tmpfile();
FILE *actual = tmpfile();

fprintf(expected, "{\n"
      "  \"first\": \"one\",\n"
      "  \"second\": \"two\"\n"
      "}");

JsonObjectAppendString(&json, "first", "one");
JsonObjectAppendString(&json, "second", "two");

ShowJsonObject(actual, json, 0);

assert_file_equal(expected, actual);
fclose(expected);
fclose(actual);

DeleteJsonObject(json);
}

static void test_show_object_compound(void **state)
{
JsonObject *json = NULL;
FILE *expected = tmpfile();
FILE *actual = tmpfile();

fprintf(expected, "{\n"
      "  \"first\": \"one\",\n"
      "  \"second\": {\n"
      "    \"third\": \"three\"\n"
      "  },\n"
      "  \"fourth\": {\n"
      "    \"fifth\": \"five\"\n"
      "  }\n"
      "}");

JsonObjectAppendString(&json, "first", "one");
   {
   JsonObject *inner = NULL;
   JsonObjectAppendString(&inner, "third", "three");

   JsonObjectAppendObject(&json, "second", inner);
   DeleteJsonObject(inner);
   }
   {
   JsonObject *inner = NULL;
   JsonObjectAppendString(&inner, "fifth", "five");

   JsonObjectAppendObject(&json, "fourth", inner);
   DeleteJsonObject(inner);
   }

ShowJsonObject(actual, json, 0);

assert_file_equal(expected, actual);
fclose(expected);
fclose(actual);

DeleteJsonObject(json);
}

static void test_show_object_array(void **state)
{
JsonObject *json = NULL;
FILE *expected = tmpfile();
FILE *actual = tmpfile();

fprintf(expected, "{\n"
      "  \"first\": [\"one\", \"two\"]\n"
      "}");

   {
   JsonArray *array = NULL;
   JsonArrayAppendString(&array, "one");
   JsonArrayAppendString(&array, "two");

   JsonObjectAppendArray(&json, "first", array);
   DeleteJsonArray(array);
   }

ShowJsonObject(actual, json, 0);

assert_file_equal(expected, actual);
fclose(expected);
fclose(actual);

DeleteJsonObject(json);
}

static void test_show_array(void **state)
{
FILE *expected = tmpfile();
FILE *actual = tmpfile();

fprintf(expected, "[\"snookie\", \"sitch\"]");

JsonArray *array = NULL;
JsonArrayAppendString(&array, "snookie");
JsonArrayAppendString(&array, "sitch");

ShowJsonArray(actual, array);

assert_file_equal(expected, actual);
fclose(expected);
fclose(actual);

DeleteJsonArray(array);
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_new_delete),
   unit_test(test_show_object_simple),
   unit_test(test_show_object_compound),
   unit_test(test_show_object_array),
   unit_test(test_show_array)
   };

return run_tests(tests);
}

