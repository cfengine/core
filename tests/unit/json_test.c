#include "test.h"
#include "json.h"

static const char *OBJECT_ARRAY = "{\n" "  \"first\": [\n" "    \"one\",\n" "    \"two\"\n" "  ]\n" "}";

static const char *OBJECT_COMPOUND = "{\n"
    "  \"first\": \"one\",\n"
    "  \"second\": {\n"
    "    \"third\": \"three\"\n" "  },\n" "  \"fourth\": {\n" "    \"fifth\": \"five\"\n" "  }\n" "}";

static const char *OBJECT_SIMPLE = "{\n" "  \"first\": \"one\",\n" "  \"second\": \"two\"\n" "}";

static const char *OBJECT_NUMERIC = "{\n" "  \"real\": 1234.5678,\n" "  \"int\": -1234567890\n" "}";

static const char *OBJECT_ESCAPED = "{\n" "  \"escaped\": \"quote\\\"stuff \\t \\n\\n\"\n" "}";

static const char *ARRAY_SIMPLE = "[\n" "  \"one\",\n" "  \"two\"\n" "]";

static const char *ARRAY_OBJECT = "[\n" "  {\n" "    \"first\": \"one\"\n" "  }\n" "]";

static void test_new_delete(void **state)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendString(json, "first", "one");
    JsonElementDestroy(json);
}

static void test_show_string(void **state)
{
    JsonElement *str = JsonStringCreate("snookie");

    Writer *writer = StringWriter();

    JsonElementPrint(writer, str, 0);
    assert_string_equal("\"snookie\"", StringWriterClose(writer));

    JsonElementDestroy(str);
}

static void test_show_object_simple(void **state)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendString(json, "first", "one");
    JsonObjectAppendString(json, "second", "two");

    Writer *writer = StringWriter();

    JsonElementPrint(writer, json, 0);

    assert_string_equal(OBJECT_SIMPLE, StringWriterData(writer));

    JsonElementDestroy(json);
}

static void test_show_object_escaped(void **state)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendString(json, "escaped", "quote\"stuff \t \n\n");

    Writer *writer = StringWriter();

    JsonElementPrint(writer, json, 0);

    assert_string_equal(OBJECT_ESCAPED, StringWriterData(writer));

    JsonElementDestroy(json);
}

static void test_show_object_numeric(void **state)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendReal(json, "real", 1234.5678);
    JsonObjectAppendInteger(json, "int", -1234567890);

    Writer *writer = StringWriter();

    JsonElementPrint(writer, json, 0);

    assert_string_equal(OBJECT_NUMERIC, StringWriterData(writer));

    JsonElementDestroy(json);
}

static void test_show_object_compound(void **state)
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

    JsonElementPrint(writer, json, 0);

    assert_string_equal(OBJECT_COMPOUND, StringWriterData(writer));

    JsonElementDestroy(json);
}

static void test_show_object_array(void **state)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonElement *array = JsonArrayCreate(10);

    JsonArrayAppendString(array, "one");
    JsonArrayAppendString(array, "two");

    JsonObjectAppendArray(json, "first", array);

    Writer *writer = StringWriter();

    JsonElementPrint(writer, json, 0);

    assert_string_equal(OBJECT_ARRAY, StringWriterData(writer));

    JsonElementDestroy(json);
}

static void test_show_array(void **state)
{
    JsonElement *array = JsonArrayCreate(10);

    JsonArrayAppendString(array, "one");
    JsonArrayAppendString(array, "two");

    Writer *writer = StringWriter();

    JsonElementPrint(writer, array, 0);

    assert_string_equal(ARRAY_SIMPLE, StringWriterData(writer));

    JsonElementDestroy(array);
}

static void test_show_array_boolean(void **state)
{
    JsonElement *array = JsonArrayCreate(10);

    JsonArrayAppendBool(array, true);
    JsonArrayAppendBool(array, false);
    Writer *writer = StringWriter();

    JsonElementPrint(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal("[\n" "  true,\n" "  false\n" "]", output);

    JsonElementDestroy(array);
    free(output);
}

static void test_show_array_object(void **state)
{
    JsonElement *array = JsonArrayCreate(10);
    JsonElement *object = JsonObjectCreate(10);

    JsonObjectAppendString(object, "first", "one");

    JsonArrayAppendObject(array, object);

    Writer *writer = StringWriter();

    JsonElementPrint(writer, array, 0);

    assert_string_equal(ARRAY_OBJECT, StringWriterData(writer));

    JsonElementDestroy(array);
}

static void test_show_array_empty(void **state)
{
    JsonElement *array = JsonArrayCreate(10);

    Writer *writer = StringWriter();

    JsonElementPrint(writer, array, 0);

    assert_string_equal("[]", StringWriterData(writer));

    JsonElementDestroy(array);
}

static void test_object_get_string(void **state)
{
    JsonElement *obj = JsonObjectCreate(10);

    JsonObjectAppendString(obj, "first", "one");
    JsonObjectAppendString(obj, "second", "two");

    assert_string_equal(JsonObjectGetAsString(obj, "second"), "two");
    assert_string_equal(JsonObjectGetAsString(obj, "first"), "one");

    JsonElementDestroy(obj);
}

static void test_object_get_array(void **state)
{
    JsonElement *arr = JsonArrayCreate(10);

    JsonArrayAppendString(arr, "one");
    JsonArrayAppendString(arr, "two");

    JsonElement *obj = JsonObjectCreate(10);

    JsonObjectAppendArray(obj, "array", arr);

    JsonElement *arr2 = JsonObjectGetAsArray(obj, "array");

    assert_string_equal(JsonArrayGetAsString(arr2, 1), "two");

    JsonElementDestroy(arr);
}

static void test_object_iterator(void **state)
{
    JsonElement *obj = JsonObjectCreate(10);

    JsonObjectAppendString(obj, "first", "one");
    JsonObjectAppendString(obj, "second", "two");

    {
        JsonIterator it = JsonIteratorInit(obj);

        assert_string_equal("first", JsonIteratorNextKey(&it));
        assert_string_equal("second", JsonIteratorNextKey(&it));
        assert_false(JsonIteratorNextKey(&it));
    }

    {
        JsonIterator it = JsonIteratorInit(obj);

        assert_string_equal("one", JsonPrimitiveGetAsString(JsonIteratorNextValue(&it)));
        assert_string_equal("two", JsonPrimitiveGetAsString(JsonIteratorNextValue(&it)));
        assert_false(JsonIteratorNextValue(&it));
    }

    JsonElementDestroy(obj);
}

static void test_array_get_string(void **state)
{
    JsonElement *arr = JsonArrayCreate(10);

    JsonArrayAppendString(arr, "first");
    JsonArrayAppendString(arr, "second");

    assert_string_equal(JsonArrayGetAsString(arr, 1), "second");
    assert_string_equal(JsonArrayGetAsString(arr, 0), "first");

    JsonElementDestroy(arr);
}

static void test_array_iterator(void **state)
{
    JsonElement *arr = JsonArrayCreate(10);

    JsonArrayAppendString(arr, "first");
    JsonArrayAppendString(arr, "second");

    {
        JsonIterator it = JsonIteratorInit(arr);

        assert_string_equal("first", JsonPrimitiveGetAsString(JsonIteratorNextValue(&it)));
        assert_string_equal("second", JsonPrimitiveGetAsString(JsonIteratorNextValue(&it)));
        assert_false(JsonIteratorNextValue(&it));
    }

    JsonElementDestroy(arr);
}

static void test_parse_object_simple(void **state)
{
    const char *data = OBJECT_SIMPLE;
    JsonElement *obj = JsonParse(&data);

    assert_string_equal(JsonObjectGetAsString(obj, "second"), "two");
    assert_string_equal(JsonObjectGetAsString(obj, "first"), "one");
    assert_int_equal(JsonObjectGetAsString(obj, "third"), NULL);

    JsonElementDestroy(obj);
}

static void test_parse_array_simple(void **state)
{
    const char *data = ARRAY_SIMPLE;
    JsonElement *arr = JsonParse(&data);

    assert_string_equal(JsonArrayGetAsString(arr, 1), "two");
    assert_string_equal(JsonArrayGetAsString(arr, 0), "one");

    JsonElementDestroy(arr);
}

static void test_parse_object_compound(void **state)
{
    const char *data = OBJECT_COMPOUND;
    JsonElement *obj = JsonParse(&data);

    assert_string_equal(JsonObjectGetAsString(obj, "first"), "one");

    JsonElement *second = JsonObjectGetAsObject(obj, "second");

    assert_string_equal(JsonObjectGetAsString(second, "third"), "three");

    JsonElement *fourth = JsonObjectGetAsObject(obj, "fourth");

    assert_string_equal(JsonObjectGetAsString(fourth, "fifth"), "five");

    JsonElementDestroy(obj);
}

static void test_parse_object_diverse(void **state)
{
    {
        const char *data = "{ \"a\": 1, \"b\": \"snookie\", \"c\": 1.0, \"d\": {}, \"e\": [], \"f\": true, \"g\": false, \"h\": null }";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "{\"a\":1,\"b\":\"snookie\",\"c\":1.0,\"d\":{},\"e\":[],\"f\":true,\"g\":false,\"h\":null}";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }
}

static void test_parse_array_object(void **state)
{
    const char *data = ARRAY_OBJECT;
    JsonElement *arr = JsonParse(&data);

    JsonElement *first = JsonArrayGetAsObject(arr, 0);

    assert_string_equal(JsonObjectGetAsString(first, "first"), "one");

    JsonElementDestroy(arr);
}

static void test_parse_empty(void **state)
{
    const char *data = "";
    JsonElement *json = JsonParse(&data);

    assert_false(json);
}

static void test_parse_good_numbers(void **state)
{
    {
        const char *data = "[0.1]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "[0.1234567890123456789]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "[0.1234e10]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "[0.1234e+10]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "[0.1234e-10]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "[1203e10]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "[1203e+10]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "[123e-10]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "[0e-10]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "[0.0e-10]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "[-0.0e-10]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }
}

static void test_parse_bad_numbers(void **state)
{
    {
        const char *data = "[01]";
        assert_false(JsonParse(&data));
    }

    {
        const char *data = "[01.1]";
        assert_false(JsonParse(&data));
    }

    {
        const char *data = "[1.]";
        assert_false(JsonParse(&data));
    }

    {
        const char *data = "[e10]";
        assert_false(JsonParse(&data));
    }

    {
        const char *data = "[-e10]";
        assert_false(JsonParse(&data));
    }

    {
        const char *data = "[+2]";
        assert_false(JsonParse(&data));
    }

    {
        const char *data = "[1e]";
        assert_false(JsonParse(&data));
    }

    {
        const char *data = "[e10]";
        assert_false(JsonParse(&data));
    }
}

static void test_parse_trim(void **state)
{
    const char *data = "           []    ";
    JsonElement *json = JsonParse(&data);

    assert_true(json);

    JsonElementDestroy(json);
}

static void test_parse_array_extra_closing(void **state)
{
    const char *data = "  []]";
    JsonElement *json = JsonParse(&data);

    assert_true(json);

    JsonElementDestroy(json);
}

static void test_parse_array_diverse(void **state)
{
    {
        const char *data = "[1, \"snookie\", 1.0, {}, [], true, false, null ]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }

    {
        const char *data = "[1,\"snookie\",1.0,{},[],true,false,null]";
        JsonElement *json = JsonParse(&data);
        assert_true(json);
        JsonElementDestroy(json);
    }
}

static void test_parse_bad_apple2(void **state)
{
    const char *data = "][";
    JsonElement *json = JsonParse(&data);

    assert_false(json);
}

static void test_parse_object_garbage(void **state)
{
    {
        const char *data = "{ \"first\": 1, garbage \"second\": 2 }";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }

    {
        const char *data = "{ \"first\": 1 garbage \"second\": 2 }";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }

    {
        const char *data = "{ \"first\": garbage, \"second\": 2 }";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }

    {
        const char *data = "{ \"first\": garbage \"second\": 2 }";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }
}

static void test_parse_object_nested_garbage(void **state)
{
    {
        const char *data = "{ \"first\": { garbage } }";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }

    {
        const char *data = "{ \"first\": [ garbage ] }";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }
}

static void test_parse_array_garbage(void **state)
{
    {
        const char *data = "[1, garbage]";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }

    {
        const char *data = "[1 garbage]";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }

    {
        const char *data = "[garbage]";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }

    {
        const char *data = "[garbage, 1]";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }
}

static void test_parse_array_nested_garbage(void **state)
{
    {
        const char *data = "[1, [garbage]]";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }

    {
        const char *data = "[1, { garbage }]";
        JsonElement *json = JsonParse(&data);
        assert_false(json);
    }
}

static void test_array_remove_range(void **state)
{
    {
        // remove whole
        JsonElement *arr = JsonArrayCreate(5);

        JsonArrayAppendString(arr, "one");
        JsonArrayAppendString(arr, "two");
        JsonArrayAppendString(arr, "three");
        JsonArrayRemoveRange(arr, 0, 2);

        assert_int_equal(JsonElementLength(arr), 0);

        JsonElementDestroy(arr);
    }

    {
        // remove middle
        JsonElement *arr = JsonArrayCreate(5);

        JsonArrayAppendString(arr, "one");
        JsonArrayAppendString(arr, "two");
        JsonArrayAppendString(arr, "three");
        JsonArrayRemoveRange(arr, 1, 1);

        assert_int_equal(JsonElementLength(arr), 2);
        assert_string_equal(JsonArrayGetAsString(arr, 0), "one");
        assert_string_equal(JsonArrayGetAsString(arr, 1), "three");

        JsonElementDestroy(arr);
    }

    {
        // remove rest
        JsonElement *arr = JsonArrayCreate(5);

        JsonArrayAppendString(arr, "one");
        JsonArrayAppendString(arr, "two");
        JsonArrayAppendString(arr, "three");
        JsonArrayRemoveRange(arr, 1, 2);

        assert_int_equal(JsonElementLength(arr), 1);
        assert_string_equal(JsonArrayGetAsString(arr, 0), "one");

        JsonElementDestroy(arr);
    }

    {
        // remove but last
        JsonElement *arr = JsonArrayCreate(5);

        JsonArrayAppendString(arr, "one");
        JsonArrayAppendString(arr, "two");
        JsonArrayAppendString(arr, "three");
        JsonArrayRemoveRange(arr, 0, 1);

        assert_int_equal(JsonElementLength(arr), 1);
        assert_string_equal(JsonArrayGetAsString(arr, 0), "three");

        JsonElementDestroy(arr);
    }
}

int main()
{
    const UnitTest tests[] =
    {
        unit_test(test_new_delete),
        unit_test(test_show_string),
        unit_test(test_show_object_simple),
        unit_test(test_show_object_escaped),
        unit_test(test_show_object_numeric),
        unit_test(test_show_object_compound),
        unit_test(test_show_object_array),
        unit_test(test_show_array),
        unit_test(test_show_array_boolean),
        unit_test(test_show_array_object),
        unit_test(test_show_array_empty),
        unit_test(test_object_get_string),
        unit_test(test_object_get_array),
        unit_test(test_object_iterator),
        unit_test(test_array_get_string),
        unit_test(test_array_iterator),
        unit_test(test_parse_object_simple),
        unit_test(test_parse_array_simple),
        unit_test(test_parse_object_compound),
        unit_test(test_parse_object_diverse),
        unit_test(test_parse_array_object),
        unit_test(test_parse_empty),
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
        unit_test(test_array_remove_range)
    };

    return run_tests(tests);
}
