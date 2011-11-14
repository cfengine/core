#include "test.h"
#include "json.h"

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

fprintf(expected, "{\n"
      "  \"first\": \"one\",\n"
      "  \"second\": \"two\"\n"
      "}");

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

fprintf(expected, "{\n"
      "  \"first\": [\n"
      "    \"one\",\n"
      "    \"two\"\n"
      "  ]\n"
      "}");

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

fprintf(expected, "[\n"
      "  \"snookie\",\n"
      "  \"sitch\"\n"
      "]");

JsonArray *array = NULL;
JsonArrayAppendString(&array, "snookie");
JsonArrayAppendString(&array, "sitch");

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

fprintf(expected, "[\n"
      "  {\n"
      "    \"first\": \"one\"\n"
      "  }\n"
      "]");

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
   };

return run_tests(tests);
}

