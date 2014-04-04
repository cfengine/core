#include <test.h>

#include <string.h>
#include <stdlib.h>
#include <matching.h>

void test_has_regex_meta_chars(void)
{
    assert_false(HasRegexMetaChars("string"));
    assert_false(HasRegexMetaChars("ala ma kota a kot ma ale"));

    assert_true(HasRegexMetaChars("^string"));
    assert_true(HasRegexMetaChars("^(string)"));
    assert_true(HasRegexMetaChars("string$"));
    assert_true(HasRegexMetaChars("(string)$"));
    assert_true(HasRegexMetaChars("string.*"));
    assert_true(HasRegexMetaChars("string*"));
    assert_true(HasRegexMetaChars("string."));
    assert_true(HasRegexMetaChars("a?"));
    assert_true(HasRegexMetaChars("a*"));
    assert_true(HasRegexMetaChars("a+"));
    assert_true(HasRegexMetaChars("a{3}"));
    assert_true(HasRegexMetaChars("a{3,6}"));
    assert_true(HasRegexMetaChars("(a|b)"));
    assert_true(HasRegexMetaChars("a(bcd)*"));
    assert_true(HasRegexMetaChars("c.+t"));
    assert_true(HasRegexMetaChars("a(bcd)?e"));
    assert_true(HasRegexMetaChars("a(bcd){2,3}e"));
    assert_true(HasRegexMetaChars("(yes)|(no)"));
    assert_true(HasRegexMetaChars("pro(b|n|r|l)ate"));
    assert_true(HasRegexMetaChars("c[aeiou]t"));
    assert_true(HasRegexMetaChars("^[a-zA-Z0-9_]+$"));
    assert_true(HasRegexMetaChars("\\d{5}"));
    assert_true(HasRegexMetaChars("\\d"));
}

int main()
{
    const UnitTest tests[] =
    {
        unit_test(test_has_regex_meta_chars),
    };

    PRINT_TEST_BANNER();
    return run_tests(tests);
}

