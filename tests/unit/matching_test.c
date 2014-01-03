#include <test.h>
#include <matching.h>

void test_is_regex(void)
{
    assert_false(IsRegex("string"));
    assert_false(IsRegex("ala ma kota a kot ma ale"));

    // commented cases fail
    assert_true(IsRegex("^string"));
    assert_true(IsRegex("^(string)"));
    assert_true(IsRegex("string$"));
    assert_true(IsRegex("(string)$"));
    assert_true(IsRegex("string.*"));
    assert_true(IsRegex("string*"));
    assert_true(IsRegex("string."));
    assert_true(IsRegex("a?"));
    assert_true(IsRegex("a*"));
    assert_true(IsRegex("a+"));
    assert_true(IsRegex("a{3}"));
    assert_true(IsRegex("a{3,6}"));
    assert_true(IsRegex("(a|b)"));
    assert_true(IsRegex("a(bcd)*"));
    assert_true(IsRegex("c.+t"));
    assert_true(IsRegex("a(bcd)?e"));
    assert_true(IsRegex("a(bcd){2,3}e"));
    assert_true(IsRegex("(yes)|(no)"));
    assert_true(IsRegex("pro(b|n|r|l)ate"));
    assert_true(IsRegex("c[aeiou]t"));
    assert_true(IsRegex("^[a-zA-Z0-9_]+$"));
    assert_true(IsRegex("\\d{5}"));
    assert_true(IsRegex("\\d"));
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_is_regex)
    };

    return run_tests(tests);
}

