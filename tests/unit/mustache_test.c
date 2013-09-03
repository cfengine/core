#include <test.h>
#include <mustache.h>
#include <files_lib.h>

size_t TestSpecFile(const char *testfile)
{
    char path[1024];
    sprintf(path, "%s/mustache_%s.json", TESTDATADIR, testfile);

    char *contents = NULL;
    if (FileReadMax(&contents, path, SIZE_MAX) == -1)
    {
        Log(LOG_LEVEL_ERR, "Error reading JSON input file '%s'", path);
        fail();
    }
    JsonElement *spec = NULL;
    const char *data = contents;
    if (JsonParse(&data, &spec) != JSON_PARSE_OK)
    {
        Log(LOG_LEVEL_ERR, "Error parsing JSON input file '%s'", path);
        fail();
    }

    free(contents);

    JsonElement *tests = JsonObjectGetAsArray(spec, "tests");

    size_t num_failures = 0;
    for (size_t i = 0; i < JsonLength(tests); i++)
    {
        JsonElement *test_obj = JsonAt(tests, i);

        fprintf(stdout, "Testing %s:%s ...", testfile, JsonObjectGetAsString(test_obj, "name"));

        Writer *out = StringWriter();

        const char *templ = JsonObjectGetAsString(test_obj, "template");
        const char *expected = JsonObjectGetAsString(test_obj, "expected");
        const JsonElement *data = JsonObjectGetAsObject(test_obj, "data");

        if (!MustacheRender(out, templ, data) || strcmp(expected, StringWriterData(out)) != 0)
        {
            num_failures++;
            fprintf(stdout, "FAIL \n%s\n != \n%s\n", expected, StringWriterData(out));
        }
        else
        {
            fprintf(stdout, "OK\n");
        }

        WriterClose(out);
    }

    JsonDestroy(spec);

    return num_failures;
}

static void test_spec(void)
{
    size_t num_failures = 0;

    size_t comments_fail = TestSpecFile("comments");
    size_t interpolation_fail = TestSpecFile("interpolation");
    size_t sections_fail = TestSpecFile("sections");
    size_t delimiters_fail = TestSpecFile("delimiters");
    size_t inverted_fail = TestSpecFile("inverted");
    size_t extra_fail = TestSpecFile("extra");

    num_failures = comments_fail + interpolation_fail + sections_fail + delimiters_fail + inverted_fail + extra_fail;
    if (num_failures > 0)
    {
        fprintf(stdout, "Failures in comments: %d\n", comments_fail);
        fprintf(stdout, "Failures in interpolation: %d\n", interpolation_fail);
        fprintf(stdout, "Failures in sections: %d\n", sections_fail);
        fprintf(stdout, "Failures in delimiters: %d\n", delimiters_fail);
        fprintf(stdout, "Failures in inverted: %d\n", inverted_fail);
        fprintf(stdout, "Failures in extra: %d\n", inverted_fail);
        fprintf(stdout, "TOTAL FAILURES: %d\n", num_failures);

        fail();
    }
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_spec),
    };

    return run_tests(tests);
}
