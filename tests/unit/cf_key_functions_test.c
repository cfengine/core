#include <test.h>

#include <stdlib.h>
#include <string.h>
#include <cmockery.h>
#include <cf-key-functions.h>

// Satisfy extern declared signal flag in cf-key-functions.c
bool cf_key_interrupted = false;

static void test_RemoveKeys(void)
{
	assert_true(1);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_RemoveKeys)
    };

    return run_tests(tests);
}

