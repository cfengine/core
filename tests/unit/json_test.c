#include "test.h"
#include "json.h"

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
    char *output = StringWriterClose(writer);

    assert_string_equal("\"snookie\"", output);

    JsonElementDestroy(str);
    free(output);
}

static void test_show_object_simple(void **state)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendString(json, "first", "one");
    JsonObjectAppendString(json, "second", "two");

    Writer *writer = StringWriter();

    JsonElementPrint(writer, json, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_SIMPLE, output);

    JsonElementDestroy(json);
    free(output);
}

static void test_show_object_escaped(void **state)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendString(json, "escaped", "quote\"stuff \t \n\n");

    Writer *writer = StringWriter();

    JsonElementPrint(writer, json, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_ESCAPED, output);

    JsonElementDestroy(json);
    free(output);
}

static void test_show_object_numeric(void **state)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendReal(json, "real", 1234.5678);
    JsonObjectAppendInteger(json, "int", -1234567890);

    Writer *writer = StringWriter();

    JsonElementPrint(writer, json, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_NUMERIC, output);

    JsonElementDestroy(json);
    free(output);
}

static void test_show_object_boolean(void **state)
{
    JsonElement *json = JsonObjectCreate(10);

    JsonObjectAppendBool(json, "bool_value", true);

    Writer *writer = StringWriter();

    JsonElementPrint(writer, json, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_BOOLEAN, output);

    JsonElementDestroy(json);
    free(output);
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
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_COMPOUND, output);

    JsonElementDestroy(json);
    free(output);
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
    char *output = StringWriterClose(writer);

    assert_string_equal(OBJECT_ARRAY, output);

    JsonElementDestroy(json);
    free(output);
}

static void test_show_array(void **state)
{
    JsonElement *array = JsonArrayCreate(10);

    JsonArrayAppendString(array, "one");
    JsonArrayAppendString(array, "two");

    Writer *writer = StringWriter();

    JsonElementPrint(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(ARRAY_SIMPLE, output);

    JsonElementDestroy(array);
    free(output);
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

static void test_show_array_numeric(void **state)
{
    JsonElement *array = JsonArrayCreate(10);

    JsonArrayAppendInteger(array, 123);
    JsonArrayAppendReal(array, 123.1234);

    Writer *writer = StringWriter();

    JsonElementPrint(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal(ARRAY_NUMERIC, output);

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
    char *output = StringWriterClose(writer);

    assert_string_equal(ARRAY_OBJECT, output);

    JsonElementDestroy(array);
    free(output);
}

static void test_show_array_empty(void **state)
{
    JsonElement *array = JsonArrayCreate(10);

    Writer *writer = StringWriter();

    JsonElementPrint(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal("[]", output);

    JsonElementDestroy(array);
    free(output);
}

static void test_show_array_nan(void **state)
{
    JsonElement *array = JsonArrayCreate(10);
    JsonArrayAppendReal(array, sqrt(-1));

    Writer *writer = StringWriter();

    JsonElementPrint(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal("[\n  0.0000\n]", output);

    JsonElementDestroy(array);
    free(output);
}

static void test_show_array_infinity(void **state)
{
    JsonElement *array = JsonArrayCreate(10);
    JsonArrayAppendReal(array, INFINITY);

    Writer *writer = StringWriter();

    JsonElementPrint(writer, array, 0);
    char *output = StringWriterClose(writer);

    assert_string_equal("[\n  0.0000\n]", output);

    JsonElementDestroy(array);
    free(output);
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

    JsonElementDestroy(obj);
}

static void test_object_iterator(void **state)
{
    JsonElement *obj = JsonObjectCreate(10);

    JsonObjectAppendString(obj, "first", "one");
    JsonObjectAppendString(obj, "second", "two");
    JsonObjectAppendInteger(obj, "third", 3);
    JsonObjectAppendBool(obj, "fourth", true);
    JsonObjectAppendBool(obj, "fifth", false);


    {
        JsonIterator it = JsonIteratorInit(obj);

        assert_string_equal("first", JsonIteratorNextKey(&it));
        assert_string_equal("second", JsonIteratorNextKey(&it));
        assert_string_equal("third", JsonIteratorNextKey(&it));
        assert_string_equal("fourth", JsonIteratorNextKey(&it));
        assert_string_equal("fifth", JsonIteratorNextKey(&it));
        assert_false(JsonIteratorNextKey(&it));
    }

    {
        JsonIterator it = JsonIteratorInit(obj);

        assert_string_equal("one", JsonPrimitiveGetAsString(JsonIteratorNextValue(&it)));
        assert_string_equal("two", JsonPrimitiveGetAsString(JsonIteratorNextValue(&it)));
        assert_int_equal(3, JsonPrimitiveGetAsInteger(JsonIteratorNextValue(&it)));
        assert_true(JsonPrimitiveGetAsBool(JsonIteratorNextValue(&it)));
        assert_false(JsonPrimitiveGetAsBool(JsonIteratorNextValue(&it)));
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

static void test_parse_object_escaped(void **state)
{
    const char *escaped_string = "\\\"/var/cfenigne/bin/cf-know\\\" ";
    const char *key = "json_element_key";

    Writer *writer = StringWriter();
    WriterWriteF(writer, "{ \"%s\" : \"%s\" }", key, escaped_string);

    const char *json_string = StringWriterData(writer);

    JsonElement *obj = JsonParse(&json_string);

    assert_int_not_equal(obj, NULL);
    assert_string_equal(JsonObjectGetAsString(obj, key), escaped_string);

    WriterClose(writer);
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

static void test_iterator_current(void **state)
{
    const char *data = ARRAY_SIMPLE;
    JsonElement *arr = JsonParse(&data);

    JsonElement *json = JsonObjectCreate(1);
    JsonObjectAppendArray(json, "array", arr);

    JsonIterator it = JsonIteratorInit(json);
    while (JsonIteratorNextValue(&it) != NULL)
    {
        assert_int_equal((int)JsonIteratorCurrentElementType(&it),
                         (int)JSON_ELEMENT_TYPE_CONTAINER);
        assert_int_equal((int)JsonIteratorCurrentContrainerType(&it),
                         (int)JSON_CONTAINER_TYPE_ARRAY);
        assert_string_equal(JsonIteratorCurrentKey(&it), "array");
    }

    JsonElementDestroy(json);
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

static void test_remove_key_from_object(void **state)
{
    JsonElement *object = JsonObjectCreate(3);

    JsonObjectAppendInteger(object, "one", 1);
    JsonObjectAppendInteger(object, "two", 2);
    JsonObjectAppendInteger(object, "three", 3);

    JsonObjectRemoveKey(object, "two");

    assert_int_equal(2, JsonElementLength(object));

    JsonElementDestroy(object);
}

static void test_detach_key_from_object(void **state)
{
    JsonElement *object = JsonObjectCreate(3);

    JsonObjectAppendInteger(object, "one", 1);
    JsonObjectAppendInteger(object, "two", 2);
    JsonObjectAppendInteger(object, "three", 3);

    JsonElement *detached = JsonObjectDetachKey(object, "two");

    assert_int_equal(2, JsonElementLength(object));
    JsonElementDestroy(object);

    assert_int_equal(1, JsonElementLength(detached));
    JsonElementDestroy(detached);
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
        unit_test(test_show_object_boolean),
        unit_test(test_show_object_compound),
        unit_test(test_show_object_array),
        unit_test(test_show_array),
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
        unit_test(test_array_remove_range),
        unit_test(test_remove_key_from_object),
        unit_test(test_detach_key_from_object),
        unit_test(test_parse_object_escaped)
    };

    return run_tests(tests);
}
