#include <test.h>

#include <json.h>
#include <files_lib.h>
#include <misc_lib.h>                                          /* xsnprintf */

#include <float.h>


static const char *OBJECT_ARRAY = "{\n" "  \"first\": [\n" "    \"one\",\n" "    \"two\"\n" "  ]\n" "}";

static const char *OBJECT_COMPOUND = "{\n"
    "  \"first\": \"one\",\n"
    "  \"second\": {\n"
    "    \"third\": \"three\"\n" "  },\n" "  \"fourth\": {\n" "    \"fifth\": \"five\"\n" "  }\n" "}";

static const char *OBJECT_SIMPLE = "{\n" "  \"first\": \"one\",\n" "  \"second\": \"two\"\n" "}";

static const char *OBJECT_NUMERIC = "{\n" "  \"real\": 1234.5678,\n" "  \"int\": -1234567890\n" "}";

static const char *OBJECT_BOOLEAN = "{\n" "  \"bool_value\": true\n" "}";

static const char *OBJECT_ESCAPED = "{\n" "  \"escaped\": \"quote\\\"stuff \\t \\n\\n\"\n" "}";

static const char *ARRAY_SIMPLE = "[\n" "  \"one\",\n" "  \"two\"\n" "]";

static const char *ARRAY_NUMERIC = "[\n" "  123,\n" "  123.1234\n" "]";

static const char *ARRAY_OBJECT = "[\n" "  {\n" "    \"first\": \"one\"\n" "  }\n" "]";

static JsonElement *LoadTestFile(const char *filename)
{
    char path[PATH_MAX];
    xsnprintf(path, sizeof(path), "%s/%s", TESTDATADIR, filename);

    Writer *w = FileRead(path, SIZE_MAX, NULL);
    if (!w)
    {
        return NULL;
    }
    JsonElement *json = NULL;
    const char *data = StringWriterData(w);
    if (JsonParse(&data, &json) != JSON_PARSE_OK)
    {
        WriterClose(w);
        return NULL;
    }

    WriterClose(w);
    return json;
}

static void test_new_delete(void)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendString(json, "first", "one");
    JsonDestroy(json);
}

static void test_object_duplicate_key(void)
{
    JsonElement *a = JsonObjectCreate(1);

    JsonObjectAppendString(a, "a", "a");
    JsonObjectAppendString(a, "a", "a");

    assert_int_equal(1, JsonLength(a));

    JsonDestroy(a);
}

static void test_show_string(void)
{
    JsonElement *str = JsonStringCreate("snookie");

    Writer *writer = StringWriter();

    JsonWrite(writer, str, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal("\"snookie\"", output);

    JsonDestroy(str);
    free(output);
}

static void test_show_object_simple(void)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendString(json, "first", "one");
    JsonObjectAppendString(json, "second", "two");

    Writer *writer = StringWriter();

    JsonWrite(writer, json, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_SIMPLE, output);

    JsonDestroy(json);
    free(output);
}

static void test_show_object_escaped(void)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendString(json, "escaped", "quote\"stuff \t \n\n");

    Writer *writer = StringWriter();

    JsonWrite(writer, json, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_ESCAPED, output);

    JsonDestroy(json);
    free(output);
}

static void test_show_object_numeric(void)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendReal(json, "real", 1234.5678);
    JsonObjectAppendInteger(json, "int", -1234567890);

    Writer *writer = StringWriter();

    JsonWrite(writer, json, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_NUMERIC, output);

    JsonDestroy(json);
    free(output);
}

static void test_show_object_boolean(void)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendBool(json, "bool_value", true);

    Writer *writer = StringWriter();

    JsonWrite(writer, json, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_BOOLEAN, output);

    JsonDestroy(json);
    free(output);
}

static void test_show_object_compound(void)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendString(json, "first", "one");
    {
        JsonElement *inner = JsonObjectCreate(10);

        JsonObjectAppendString(inner, "third", "three");

        JsonObjectAppendObject(json, "second", inner);
    }
    {
        JsonElement *inner = JsonObjectCreate(10);

        JsonObjectAppendString(inner, "fifth", "five");

        JsonObjectAppendObject(json, "fourth", inner);
    }

    Writer *writer = StringWriter();

    JsonWrite(writer, json, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_COMPOUND, output);

    JsonDestroy(json);
    free(output);
}

static void test_show_object_compound_compact(void)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendString(json, "first", "one");
    {
        JsonElement *inner = JsonObjectCreate(10);

        JsonObjectAppendString(inner, "third", "three");

        JsonObjectAppendObject(json, "second", inner);
    }
    {
        JsonElement *inner = JsonObjectCreate(10);

        JsonObjectAppendString(inner, "fifth", "five");

        JsonObjectAppendObject(json, "fourth", inner);
    }

    Writer *writer = StringWriter();

    JsonWriteCompact(writer, json);
    char *output = StringWriterClose(writer);

    assert_string_equal("{\"first\":\"one\",\"second\":{\"third\":\"three\"},\"fourth\":{\"fifth\":\"five\"}}", output);

    JsonDestroy(json);
    free(output);
}

static void test_show_object_array(void)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonElement *array = JsonArrayCreate(10);

    JsonArrayAppendString(array, "one");
    JsonArrayAppendString(array, "two");

    JsonObjectAppendArray(json, "first", array);

    Writer *writer = StringWriter();

    JsonWrite(writer, json, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_ARRAY, output);

    JsonDestroy(json);
    free(output);
}

static void test_show_array(void)
{
    JsonElement *array = JsonArrayCreate(10);

    JsonArrayAppendString(array, "one");
    JsonArrayAppendString(array, "two");

    Writer *writer = StringWriter();

    JsonWrite(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(ARRAY_SIMPLE, output);

    JsonDestroy(array);
    free(output);
}

static void test_show_array_compact(void)
{
    JsonElement *array = JsonArrayCreate(10);

    JsonArrayAppendString(array, "one");
    JsonArrayAppendString(array, "two");

    Writer *writer = StringWriter();

    JsonWriteCompact(writer, array);
    char *output = StringWriterClose(writer);

    assert_string_equal("[\"one\",\"two\"]", output);

    JsonDestroy(array);
    free(output);
}

static void test_show_array_boolean(void)
{
    JsonElement *array = JsonArrayCreate(10);

    JsonArrayAppendBool(array, true);
    JsonArrayAppendBool(array, false);

    Writer *writer = StringWriter();

    JsonWrite(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal("[\n" "  true,\n" "  false\n" "]", output);

    JsonDestroy(array);
    free(output);
}

static void test_show_array_numeric(void)
{
    JsonElement *array = JsonArrayCreate(10);

    JsonArrayAppendInteger(array, 123);
    JsonArrayAppendReal(array, 123.1234);

    Writer *writer = StringWriter();

    JsonWrite(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(ARRAY_NUMERIC, output);

    JsonDestroy(array);
    free(output);
}

static void test_show_array_object(void)
{
    JsonElement *array = JsonArrayCreate(10);
    JsonElement *object = JsonObjectCreate(10);

    JsonObjectAppendString(object, "first", "one");

    JsonArrayAppendObject(array, object);

    Writer *writer = StringWriter();

    JsonWrite(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(ARRAY_OBJECT, output);

    JsonDestroy(array);
    free(output);
}

static void test_show_array_empty(void)
{
    JsonElement *array = JsonArrayCreate(10);

    Writer *writer = StringWriter();

    JsonWrite(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal("[]", output);

    JsonDestroy(array);
    free(output);
}

static void test_show_array_nan(void)
{
    JsonElement *array = JsonArrayCreate(10);
    JsonArrayAppendReal(array, sqrt(-1));

    Writer *writer = StringWriter();

    JsonWrite(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal("[\n  0.0000\n]", output);

    JsonDestroy(array);
    free(output);
}

#ifndef INFINITY
  #define INFINITY (1.0/0.0)
#endif

static void test_show_array_infinity(void)
{
    JsonElement *array = JsonArrayCreate(10);
    JsonArrayAppendReal(array, INFINITY);

    Writer *writer = StringWriter();

    JsonWrite(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal("[\n  0.0000\n]", output);

    JsonDestroy(array);
    free(output);
}

static void test_object_get_string(void)
{
    JsonElement *obj = JsonObjectCreate(10);

    JsonObjectAppendString(obj, "first", "one");
    JsonObjectAppendString(obj, "second", "two");

    assert_string_equal(JsonObjectGetAsString(obj, "second"), "two");
    assert_string_equal(JsonObjectGetAsString(obj, "first"), "one");

    JsonDestroy(obj);
}

static void test_object_get_array(void)
{
    JsonElement *arr = JsonArrayCreate(10);

    JsonArrayAppendString(arr, "one");
    JsonArrayAppendString(arr, "two");

    JsonElement *obj = JsonObjectCreate(10);

    JsonObjectAppendArray(obj, "array", arr);

    JsonElement *arr2 = JsonObjectGetAsArray(obj, "array");

    assert_string_equal(JsonArrayGetAsString(arr2, 1), "two");

    JsonDestroy(obj);
}

static void test_object_iterator(void)
{
    JsonElement *obj = JsonObjectCreate(10);

    JsonObjectAppendString(obj, "first", "one");
    JsonObjectAppendString(obj, "second", "two");
    JsonObjectAppendInteger(obj, "third", 3);
    JsonObjectAppendBool(obj, "fourth", true);
    JsonObjectAppendBool(obj, "fifth", false);


    {
        JsonIterator it = JsonIteratorInit(obj);

        assert_true(JsonIteratorHasMore(&it));
        assert_string_equal("first", JsonIteratorNextKey(&it));
        assert_string_equal("second", JsonIteratorNextKey(&it));
        assert_string_equal("third", JsonIteratorNextKey(&it));
        assert_string_equal("fourth", JsonIteratorNextKey(&it));
        assert_true(JsonIteratorHasMore(&it));
        assert_string_equal("fifth", JsonIteratorNextKey(&it));
        assert_false(JsonIteratorHasMore(&it));
        assert_false(JsonIteratorNextKey(&it));
    }

    {
        JsonIterator it = JsonIteratorInit(obj);

        assert_true(JsonIteratorHasMore(&it));
        assert_string_equal("one", JsonPrimitiveGetAsString(JsonIteratorNextValue(&it)));
        assert_string_equal("two", JsonPrimitiveGetAsString(JsonIteratorNextValue(&it)));
        assert_int_equal(3, JsonPrimitiveGetAsInteger(JsonIteratorNextValue(&it)));
        assert_true(JsonPrimitiveGetAsBool(JsonIteratorNextValue(&it)));
        assert_true(JsonIteratorHasMore(&it));
        assert_false(JsonPrimitiveGetAsBool(JsonIteratorNextValue(&it)));
        assert_false(JsonIteratorHasMore(&it));
        assert_false(JsonIteratorNextValue(&it));
    }

    JsonDestroy(obj);
}

static void test_array_get_string(void)
{
    JsonElement *arr = JsonArrayCreate(10);

    JsonArrayAppendString(arr, "first");
    JsonArrayAppendString(arr, "second");

    assert_string_equal(JsonArrayGetAsString(arr, 1), "second");
    assert_string_equal(JsonArrayGetAsString(arr, 0), "first");

    JsonDestroy(arr);
}

static void test_array_iterator(void)
{
    JsonElement *arr = JsonArrayCreate(10);

    JsonArrayAppendString(arr, "first");
    JsonArrayAppendString(arr, "second");

    {
        JsonIterator it = JsonIteratorInit(arr);

        assert_true(JsonIteratorHasMore(&it));
        assert_string_equal("first", JsonPrimitiveGetAsString(JsonIteratorNextValue(&it)));
        assert_true(JsonIteratorHasMore(&it));
        assert_string_equal("second", JsonPrimitiveGetAsString(JsonIteratorNextValue(&it)));
        assert_false(JsonIteratorHasMore(&it));
        assert_false(JsonIteratorNextValue(&it));
    }

    JsonDestroy(arr);
}

static void test_copy_compare(void)
{
    JsonElement *bench = LoadTestFile("benchmark.json");
    assert_true(bench != NULL);

    JsonElement *copy = JsonCopy(bench);
    assert_true(copy != NULL);

    assert_int_equal(0, JsonCompare(copy, bench));

    JsonDestroy(bench);
    JsonDestroy(copy);
}

static void test_select(void)
{
    const char *data = OBJECT_ARRAY;
    JsonElement *obj = NULL;
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &obj));

    assert_true(obj == JsonSelect(obj, 0, NULL));

    {
        char *indices[] = { "first" };
        assert_int_equal(JSON_CONTAINER_TYPE_ARRAY, JsonGetContainerType(JsonSelect(obj, 1, indices)));
    }
    {
        char *indices[] = { "first", "0" };
        assert_string_equal("one", JsonPrimitiveGetAsString(JsonSelect(obj, 2, indices)));
    }
    {
        char *indices[] = { "first", "1" };
        assert_string_equal("two", JsonPrimitiveGetAsString(JsonSelect(obj, 2, indices)));
    }
    {
        char *indices[] = { "first", "2" };
        assert_true(JsonSelect(obj, 2, indices) == NULL);
    }
    {
        char *indices[] = { "first", "x" };
        assert_true(JsonSelect(obj, 2, indices) == NULL);
    }

    {
        char *indices[] = { "first", "0", "x" };
        assert_true(JsonSelect(obj, 3, indices) == NULL);
    }

    {
        char *indices[] = { "second" };
        assert_true(JsonSelect(obj, 1, indices) == NULL);
    }

    JsonDestroy(obj);
}

static void test_merge_array(void)
{
    JsonElement *a = JsonArrayCreate(2);
    JsonArrayAppendString(a, "a");
    JsonArrayAppendString(a, "b");

    JsonElement *b = JsonArrayCreate(2);
    JsonArrayAppendString(b, "c");
    JsonArrayAppendString(b, "d");

    JsonElement *c = JsonMerge(a, b);

    assert_int_equal(2, JsonLength(a));
    assert_int_equal(2, JsonLength(b));
    assert_int_equal(4, JsonLength(c));

    assert_string_equal("a", JsonArrayGetAsString(c, 0));
    assert_string_equal("b", JsonArrayGetAsString(c, 1));
    assert_string_equal("c", JsonArrayGetAsString(c, 2));
    assert_string_equal("d", JsonArrayGetAsString(c, 3));

    JsonDestroy(a);
    JsonDestroy(b);
    JsonDestroy(c);
}

static void test_merge_object(void)
{
    JsonElement *a = JsonObjectCreate(2);
    JsonObjectAppendString(a, "a", "a");
    JsonObjectAppendString(a, "b", "b");

    JsonElement *b = JsonObjectCreate(2);
    JsonObjectAppendString(b, "b", "b");
    JsonObjectAppendString(b, "c", "c");

    JsonElement *c = JsonMerge(a, b);

    assert_int_equal(2, JsonLength(a));
    assert_int_equal(2, JsonLength(b));
    assert_int_equal(3, JsonLength(c));

    assert_string_equal("a", JsonObjectGetAsString(c, "a"));
    assert_string_equal("b", JsonObjectGetAsString(c, "b"));
    assert_string_equal("c", JsonObjectGetAsString(c, "c"));

    JsonDestroy(a);
    JsonDestroy(b);
    JsonDestroy(c);
}

static void test_parse_empty_containers(void)
{
    {
        const char *data = "{}";
        JsonElement *obj = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &obj));
        assert_true(obj != NULL);
        assert_int_equal(JSON_ELEMENT_TYPE_CONTAINER, JsonGetElementType(obj));
        assert_int_equal(JSON_CONTAINER_TYPE_OBJECT, JsonGetContainerType(obj));
        assert_int_equal(0, JsonLength(obj));
        JsonDestroy(obj);
    }

    {
        const char *data = "[]";
        JsonElement *arr = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &arr));
        assert_true(arr != NULL);
        assert_int_equal(JSON_ELEMENT_TYPE_CONTAINER, JsonGetElementType(arr));
        assert_int_equal(JSON_CONTAINER_TYPE_ARRAY, JsonGetContainerType(arr));
        assert_int_equal(0, JsonLength(arr));
        JsonDestroy(arr);
    }
}

static void test_parse_object_simple(void)
{
    const char *data = OBJECT_SIMPLE;
    JsonElement *obj = NULL;
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &obj));

    assert_string_equal(JsonObjectGetAsString(obj, "second"), "two");
    assert_string_equal(JsonObjectGetAsString(obj, "first"), "one");
    assert_int_equal(JsonObjectGetAsString(obj, "third"), NULL);

    JsonDestroy(obj);
}

static void test_parse_object_escaped(void)
{
    const char *decoded = "\"/var/cfenigne/bin/cf-know\" ";

    const char *json_string =  "{\n  \"key\": \"\\\"/var/cfenigne/bin/cf-know\\\" \"\n}";

    JsonElement *obj = NULL;
    const char *data = json_string;
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &obj));

    assert_int_not_equal(obj, NULL);
    assert_string_equal(JsonObjectGetAsString(obj, "key"), decoded);

    {
        Writer *w = StringWriter();
        JsonWrite(w, obj, 0);

        assert_string_equal(json_string, StringWriterData(w));

        WriterClose(w);
    }

    JsonDestroy(obj);
}

static void test_parse_tzz_evil_key(void)
{
    const char *data = "{ \"third key! can? be$ anything&\": [ \"a\", \"b\", \"c\" ]}";
    JsonElement *obj = NULL;
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &obj));

    assert_string_equal("b", JsonArrayGetAsString(JsonObjectGetAsArray(obj, "third key! can? be$ anything&"), 1));

    JsonDestroy(obj);
}

static void test_parse_primitives(void)
{
    JsonElement *pri = NULL;

    const char *data = "\"foo\"";
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &pri));
    assert_string_equal("foo", JsonPrimitiveGetAsString(pri));
    JsonDestroy(pri);

    data = "-123";
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &pri));
    assert_true(-123 == JsonPrimitiveGetAsInteger(pri));
    JsonDestroy(pri);

    data = "1.23";
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &pri));
    assert_double_close(1.23, JsonPrimitiveGetAsReal(pri));
    JsonDestroy(pri);

    data = "true";
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &pri));
    assert_true(JsonPrimitiveGetAsBool(pri));
    JsonDestroy(pri);
}

static void test_parse_array_simple(void)
{
    const char *data = ARRAY_SIMPLE;
    JsonElement *arr = NULL;
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &arr));

    assert_string_equal(JsonArrayGetAsString(arr, 1), "two");
    assert_string_equal(JsonArrayGetAsString(arr, 0), "one");

    JsonDestroy(arr);
}

static void test_parse_object_compound(void)
{
    const char *data = OBJECT_COMPOUND;
    JsonElement *obj = NULL;
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &obj));

    assert_string_equal(JsonObjectGetAsString(obj, "first"), "one");

    JsonElement *second = JsonObjectGetAsObject(obj, "second");

    assert_string_equal(JsonObjectGetAsString(second, "third"), "three");

    JsonElement *fourth = JsonObjectGetAsObject(obj, "fourth");

    assert_string_equal(JsonObjectGetAsString(fourth, "fifth"), "five");

    JsonDestroy(obj);
}

static void test_parse_object_diverse(void)
{
    {
        const char *data = "{ \"a\": 1, \"b\": \"snookie\", \"c\": 1.0, \"d\": {}, \"e\": [], \"f\": true, \"g\": false, \"h\": null }";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "{\"a\":1,\"b\":\"snookie\",\"c\":1.0,\"d\":{},\"e\":[],\"f\":true,\"g\":false,\"h\":null}";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }
}

static void test_parse_array_object(void)
{
    const char *data = ARRAY_OBJECT;
    JsonElement *arr = NULL;
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &arr));

    JsonElement *first = JsonArrayGetAsObject(arr, 0);

    assert_string_equal(JsonObjectGetAsString(first, "first"), "one");

    JsonDestroy(arr);
}

static void test_iterator_current(void)
{
    const char *data = ARRAY_SIMPLE;
    JsonElement *arr = NULL;
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &arr));

    JsonElement *json = JsonObjectCreate(1);
    JsonObjectAppendArray(json, "array", arr);

    JsonIterator it = JsonIteratorInit(json);
    while (JsonIteratorNextValue(&it) != NULL)
    {
        assert_int_equal((int)JsonIteratorCurrentElementType(&it),
                         (int)JSON_ELEMENT_TYPE_CONTAINER);
        assert_int_equal((int)JsonIteratorCurrentContainerType(&it),
                         (int)JSON_CONTAINER_TYPE_ARRAY);
        assert_string_equal(JsonIteratorCurrentKey(&it), "array");
    }

    JsonDestroy(json);
}

static void test_parse_empty_string(void)
{
    const char *data = "";
    JsonElement *json = NULL;
    assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));

    assert_false(json);

    data = "\"\"";
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));

    assert_string_equal("", JsonPrimitiveGetAsString(json));

    JsonDestroy(json);
}

static char *JsonToString(const JsonElement *json)
{
    Writer *w = StringWriter();
    JsonWriteCompact(w, json);
    return StringWriterClose(w);
}

static void test_parse_escaped_string(void)
{
    {
        const char *data = "\"\\\\\"";
        const char *original = data;
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));

        assert_string_equal("\\", JsonPrimitiveGetAsString(json));

        char *out = JsonToString(json);
        assert_string_equal(original, out);
        free(out);

        JsonDestroy(json);
    }

    {
        // included by paranoia from Redmine #5773
        const char *data = "\"/\\\\//\\\\/\\\\\"";
        const char *original = data;
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));

        assert_string_equal("/\\//\\/\\", JsonPrimitiveGetAsString(json));

        char *out = JsonToString(json);
        assert_string_equal(original, out);
        free(out);

        JsonDestroy(json);
    }

    {
        const char *data = "\"x\\tx\"";
        const char *original = data;
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));

        assert_string_equal("x\tx", JsonPrimitiveGetAsString(json));

        char *out = JsonToString(json);
        assert_string_equal(original, out);
        free(out);

        JsonDestroy(json);
    }
}

static void test_parse_good_numbers(void)
{
    {
        const char *data = "[0.1]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "[0.1234567890123456789]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "[0.1234e10]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "[0.1234e+10]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "[0.1234e-10]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "[1203e10]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "[1203e+10]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "[123e-10]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "[0e-10]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "[0.0e-10]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "[-0.0e-10]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }
}

static void test_parse_bad_numbers(void)
{
    {
        const char *data = "[01]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
    }

    {
        const char *data = "[01.1]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
    }

    {
        const char *data = "[1.]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
    }

    {
        const char *data = "[e10]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
    }

    {
        const char *data = "[-e10]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
    }

    {
        const char *data = "[+2]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
    }

    {
        const char *data = "[1e]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
    }

    {
        const char *data = "[e10]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
    }
}

static void test_parse_trim(void)
{
    const char *data = "           []    ";
    JsonElement *json = NULL;
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));

    assert_true(json);

    JsonDestroy(json);
}

static void test_parse_array_extra_closing(void)
{
    const char *data = "  []]";
    JsonElement *json = NULL;
    assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));

    assert_true(json);

    JsonDestroy(json);
}

static void test_parse_array_diverse(void)
{
    {
        const char *data = "[1, \"snookie\", 1.0, {}, [], true, false, null ]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }

    {
        const char *data = "[1,\"snookie\",1.0,{},[],true,false,null]";
        JsonElement *json = NULL;
        assert_int_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_true(json);
        JsonDestroy(json);
    }
}

static void test_parse_bad_apple2(void)
{
    const char *data = "][";
    JsonElement *json = NULL;
    assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));

    assert_false(json);
}

static void test_parse_object_garbage(void)
{
    {
        const char *data = "{ \"first\": 1, garbage \"second\": 2 }";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }

    {
        const char *data = "{ \"first\": 1 garbage \"second\": 2 }";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }

    {
        const char *data = "{ \"first\": garbage, \"second\": 2 }";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }

    {
        const char *data = "{ \"first\": garbage \"second\": 2 }";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }
}

static void test_parse_object_nested_garbage(void)
{
    {
        const char *data = "{ \"first\": { garbage } }";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }

    {
        const char *data = "{ \"first\": [ garbage ] }";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }
}

static void test_parse_array_garbage(void)
{
    {
        const char *data = "[1, garbage]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }

    {
        const char *data = "[1 garbage]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }

    {
        const char *data = "[garbage]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }

    {
        const char *data = "[garbage, 1]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }
}

static void test_parse_array_nested_garbage(void)
{
    {
        const char *data = "[1, [garbage]]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }

    {
        const char *data = "[1, { garbage }]";
        JsonElement *json = NULL;
        assert_int_not_equal(JSON_PARSE_OK, JsonParse(&data, &json));
        assert_false(json);
    }
}

static void test_array_remove_range(void)
{
    {
        // remove whole
        JsonElement *arr = JsonArrayCreate(5);

        JsonArrayAppendString(arr, "one");
        JsonArrayAppendString(arr, "two");
        JsonArrayAppendString(arr, "three");
        JsonArrayRemoveRange(arr, 0, 2);

        assert_int_equal(JsonLength(arr), 0);

        JsonDestroy(arr);
    }

    {
        // remove middle
        JsonElement *arr = JsonArrayCreate(5);

        JsonArrayAppendString(arr, "one");
        JsonArrayAppendString(arr, "two");
        JsonArrayAppendString(arr, "three");
        JsonArrayRemoveRange(arr, 1, 1);

        assert_int_equal(JsonLength(arr), 2);
        assert_string_equal(JsonArrayGetAsString(arr, 0), "one");
        assert_string_equal(JsonArrayGetAsString(arr, 1), "three");

        JsonDestroy(arr);
    }

    {
        // remove rest
        JsonElement *arr = JsonArrayCreate(5);

        JsonArrayAppendString(arr, "one");
        JsonArrayAppendString(arr, "two");
        JsonArrayAppendString(arr, "three");
        JsonArrayRemoveRange(arr, 1, 2);

        assert_int_equal(JsonLength(arr), 1);
        assert_string_equal(JsonArrayGetAsString(arr, 0), "one");

        JsonDestroy(arr);
    }

    {
        // remove but last
        JsonElement *arr = JsonArrayCreate(5);

        JsonArrayAppendString(arr, "one");
        JsonArrayAppendString(arr, "two");
        JsonArrayAppendString(arr, "three");
        JsonArrayRemoveRange(arr, 0, 1);

        assert_int_equal(JsonLength(arr), 1);
        assert_string_equal(JsonArrayGetAsString(arr, 0), "three");

        JsonDestroy(arr);
    }
}

static void test_remove_key_from_object(void)
{
    JsonElement *object = JsonObjectCreate(3);

    JsonObjectAppendInteger(object, "one", 1);
    JsonObjectAppendInteger(object, "two", 2);
    JsonObjectAppendInteger(object, "three", 3);

    JsonObjectRemoveKey(object, "two");

    assert_int_equal(2, JsonLength(object));

    JsonDestroy(object);
}

static void test_detach_key_from_object(void)
{
    JsonElement *object = JsonObjectCreate(3);

    JsonObjectAppendInteger(object, "one", 1);
    JsonObjectAppendInteger(object, "two", 2);
    JsonObjectAppendInteger(object, "three", 3);

    JsonElement *detached = JsonObjectDetachKey(object, "two");

    assert_int_equal(2, JsonLength(object));
    JsonDestroy(object);

    assert_int_equal(1, JsonLength(detached));
    JsonDestroy(detached);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_new_delete),
        unit_test(test_object_duplicate_key),
        unit_test(test_show_string),
        unit_test(test_show_object_simple),
        unit_test(test_show_object_escaped),
        unit_test(test_show_object_numeric),
        unit_test(test_show_object_boolean),
        unit_test(test_show_object_compound),
        unit_test(test_show_object_compound_compact),
        unit_test(test_show_object_array),
        unit_test(test_show_array),
        unit_test(test_show_array_compact),
        unit_test(test_show_array_boolean),
        unit_test(test_show_array_numeric),
        unit_test(test_show_array_object),
        unit_test(test_show_array_empty),
        unit_test(test_show_array_nan),
        unit_test(test_show_array_infinity),
        unit_test(test_object_get_string),
        unit_test(test_object_get_array),
        unit_test(test_object_iterator),
        unit_test(test_iterator_current),
        unit_test(test_array_get_string),
        unit_test(test_array_iterator),
        unit_test(test_copy_compare),
        unit_test(test_select),
        unit_test(test_merge_array),
        unit_test(test_merge_object),
        unit_test(test_parse_empty_string),
        unit_test(test_parse_escaped_string),
        unit_test(test_parse_empty_containers),
        unit_test(test_parse_object_simple),
        unit_test(test_parse_array_simple),
        unit_test(test_parse_object_compound),
        unit_test(test_parse_object_diverse),
        unit_test(test_parse_array_object),
        unit_test(test_parse_good_numbers),
        unit_test(test_parse_bad_numbers),
        unit_test(test_parse_trim),
        unit_test(test_parse_array_extra_closing),
        unit_test(test_parse_array_diverse),
        unit_test(test_parse_bad_apple2),
        unit_test(test_parse_object_garbage),
        unit_test(test_parse_object_nested_garbage),
        unit_test(test_parse_array_garbage),
        unit_test(test_parse_array_nested_garbage),
        unit_test(test_parse_object_escaped),
        unit_test(test_parse_tzz_evil_key),
        unit_test(test_parse_primitives),
        unit_test(test_array_remove_range),
        unit_test(test_remove_key_from_object),
        unit_test(test_detach_key_from_object),
    };

    return run_tests(tests);
}
