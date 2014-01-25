#include <test.h>

#include <assoc.h>

static void test_create_destroy(void)
{
    CfAssoc *ap = NewAssoc("hello", (Rval) { "world", RVAL_TYPE_SCALAR }, CF_DATA_TYPE_STRING);
    DeleteAssoc(ap);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_create_destroy),
    };

    return run_tests(tests);
}
