#include <test.h>

#include <alloc.h>
#include <stdio.h>
#include <stdlib.h> /* for getenv switch to hook calloc or not: CALLOC_HOOK_ACTIVE */

extern void *__libc_calloc(size_t size);
int calloc_hook_active = 0;

void *calloc(size_t nmemb __attribute__((unused)), size_t size)
{
    if (calloc_hook_active)
    {
        return NULL;
    }
    else
    {
        return __libc_calloc(size);
    }
}

void test_xcalloc_fail(void)
{
    calloc_hook_active = (getenv("CALLOC_HOOK_ACTIVE") != NULL ? 1 : 0);
    void *foo = xcalloc(1, 4);
    assert_true(foo != NULL);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] = {
        unit_test(test_xcalloc_fail),
    };

    return run_tests(tests);
}
