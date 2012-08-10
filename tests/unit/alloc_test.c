#include "cf3.defs.h"

#include <setjmp.h>
#include <cmockery.h>
#include <stdarg.h>

void test_xasprintf(void **p)
{
    char *s;
    int res = xasprintf(&s, "Foo%d%s", 123, "17");

    assert_int_equal(res, 8);
    assert_string_equal(s, "Foo12317");
    free(s);
}

void test_xvasprintf_sub(const char *fmt, ...)
{
    char *s;
    va_list ap;

    va_start(ap, fmt);
    int res = xvasprintf(&s, fmt, ap);

    va_end(ap);
    assert_int_equal(res, 8);
    assert_string_equal(s, "Foo12317");
    free(s);
}

void test_xvasprintf(void **p)
{
    test_xvasprintf_sub("Foo%d%s", 123, "17");
}

int main()
{
    const UnitTest tests[] =
{
        unit_test(test_xasprintf),
        unit_test(test_xvasprintf),
    };

    return run_tests(tests);
}
