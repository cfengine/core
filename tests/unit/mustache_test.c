#include <test.h>

#include <mustache.h>
#include <files_lib.h>
#include <misc_lib.h>                                          /* xsnprintf */


size_t TestSpecFile(const char *testfile)
{
    char path[PATH_MAX];
    xsnprintf(path, sizeof(path), "%s/mustache_%s.json",
              TESTDATADIR, testfile);

    Writer *w = FileRead(path, SIZE_MAX, NULL);
    if (w == NULL)
    {
        Log(LOG_LEVEL_ERR, "Error reading JSON input file '%s'", path);
        fail();
    }
    JsonElement *spec = NULL;
    const char *data = StringWriterData(w);
    if (JsonParse(&data, &spec) != JSON_PARSE_OK)
    {
        Log(LOG_LEVEL_ERR, "Error parsing JSON input file '%s'", path);
        fail();
    }
    WriterClose(w);

    JsonElement *tests = JsonObjectGetAsArray(spec, "tests");

    size_t num_failures = 0;
    for (size_t i = 0; i < JsonLength(tests); i++)
    {
        JsonElement *test_obj = JsonAt(tests, i);

        fprintf(stdout, "Testing %s:%s ...", testfile, JsonObjectGetAsString(test_obj, "name"));

        Buffer *out = BufferNew();

        const char *templ = JsonObjectGetAsString(test_obj, "template");
        const char *expected = JsonObjectGetAsString(test_obj, "expected");
        const JsonElement *data = JsonObjectGet(test_obj, "data");

        if (!MustacheRender(out, templ, data) || strcmp(expected, BufferData(out)) != 0)
        {
            num_failures++;
            fprintf(stdout, "FAIL \n%s\n != \n%s\n", expected, BufferData(out));
        }
        else
        {
            fprintf(stdout, "OK\n");
        }

        BufferDestroy(out);
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
        fprintf(stdout, "Failures in comments: %llu\n",
                (unsigned long long)comments_fail);
        fprintf(stdout, "Failures in interpolation: %llu\n",
                (unsigned long long)interpolation_fail);
        fprintf(stdout, "Failures in sections: %llu\n",
                (unsigned long long)sections_fail);
        fprintf(stdout, "Failures in delimiters: %llu\n",
                (unsigned long long)delimiters_fail);
        fprintf(stdout, "Failures in inverted: %llu\n",
                (unsigned long long)inverted_fail);
        fprintf(stdout, "Failures in extra: %llu\n",
                (unsigned long long)inverted_fail);
        fprintf(stdout, "TOTAL FAILURES: %llu\n",
                (unsigned long long)num_failures);

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
