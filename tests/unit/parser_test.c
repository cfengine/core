#include "test.h"

#include "policy.h"
#include "parser.h"

static Policy *LoadPolicy(const char *filename)
{
    char path[1024];
    sprintf(path, "%s/%s", TESTDATADIR, filename);

    return ParserParseFile(path);
}

void test_benchmark(void **state)
{
    Policy *p = LoadPolicy("benchmark.cf");
    assert_true(p);
    PolicyDestroy(p);
}

void test_bundle_invalid_type(void **state)
{
    assert_false(LoadPolicy("bundle_invalid_type.cf"));
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_benchmark),
        unit_test(test_bundle_invalid_type),
    };

    return run_tests(tests);
}
