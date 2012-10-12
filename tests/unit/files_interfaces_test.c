#include "test.h"
#include "files_interfaces.h"

static void test_cfreadline(void **state)
{

}


int main()
{
    const UnitTest tests[] =
    {
        unit_test(test_cfreadline),
    };

    PRINT_TEST_BANNER();
    return run_tests(tests);
}

