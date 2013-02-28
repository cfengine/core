#include "cf3.defs.h"
#include "string_lib.h"

#include "conversion.h"

#include <setjmp.h>
#include <cmockery.h>

static const char *lo_alphabet = "abcdefghijklmnopqrstuvwxyz";
static const char *hi_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static void test_mix_case_tolower(void **state)
{
    char str[] = "aBcD";
    ToLowerStrInplace(str);

    assert_string_equal(str, "abcd");
}

static void test_empty_tolower(void **state)
{
    char str[] = "";
    ToLowerStrInplace(str);

    assert_string_equal(str, "");
}

static void test_weird_chars_tolower(void **state)
{
    static const char *weirdstuff = "1345\0xff%$#@!";

    char weirdstuff_copy_lowercased[CF_MAXVARSIZE];
    strncpy(weirdstuff_copy_lowercased, weirdstuff, CF_MAXVARSIZE);
    ToLowerStrInplace(weirdstuff_copy_lowercased);

    assert_string_equal(weirdstuff_copy_lowercased, weirdstuff);
}

static void test_alphabet_tolower(void **state)
{
    char lo_alphabet_lowercased[CF_MAXVARSIZE];
    strncpy(lo_alphabet_lowercased, lo_alphabet, CF_MAXVARSIZE);
    ToLowerStrInplace(lo_alphabet_lowercased);

    assert_string_equal(lo_alphabet_lowercased, lo_alphabet);
}

static void test_hi_alphabet_tolower(void **state)
{
    char hi_alphabet_lowercased[CF_MAXVARSIZE];
    strncpy(hi_alphabet_lowercased, hi_alphabet, CF_MAXVARSIZE);
    ToLowerStrInplace(hi_alphabet_lowercased);

    assert_string_equal(hi_alphabet_lowercased, lo_alphabet);
}

static void test_inplace_tolower(void **state)
{
    char abc[] = "abc";
    char def[] = "def";

    ToLowerStrInplace(abc);
    ToLowerStrInplace(def);

    assert_string_equal(abc, "abc");
    assert_string_equal(def, "def");
}

static void test_mix_case_toupper(void **state)
{
    char str[] = "aBcD";
    ToUpperStrInplace(str);
    assert_string_equal(str, "ABCD");
}

static void test_empty_toupper(void **state)
{
    char str[] = "";
    ToUpperStrInplace(str);
    assert_string_equal(str, "");
}

static void test_weird_chars_toupper(void **state)
{
    static const char *weirdstuff = "1345\0xff%$#@!";

    char weirdstuff_copy_uppercased[CF_MAXVARSIZE];
    strncpy(weirdstuff_copy_uppercased, weirdstuff, CF_MAXVARSIZE);
    ToUpperStrInplace(weirdstuff_copy_uppercased);

    assert_string_equal(weirdstuff_copy_uppercased, weirdstuff);
}

static void test_alphabet_toupper(void **state)
{
    char lo_alphabet_uppercased[CF_MAXVARSIZE];
    strncpy(lo_alphabet_uppercased, lo_alphabet, CF_MAXVARSIZE);
    ToUpperStrInplace(lo_alphabet_uppercased);

    assert_string_equal(lo_alphabet_uppercased, hi_alphabet);
}

static void test_hi_alphabet_toupper(void **state)
{
    char hi_alphabet_uppercased[CF_MAXVARSIZE];
    strncpy(hi_alphabet_uppercased, hi_alphabet, CF_MAXVARSIZE);
    ToUpperStrInplace(hi_alphabet_uppercased);

    assert_string_equal(hi_alphabet_uppercased, hi_alphabet);
}

static void test_inplace_toupper(void **state)
{
    char abc[] = "abc";
    char def[] = "def";

    ToUpperStrInplace(abc);
    ToUpperStrInplace(def);

    assert_string_equal(abc, "ABC");
    assert_string_equal(def, "DEF");
}

static void test_long_search(void **state)
{
    char *ns = SearchAndReplace("abc", "abcabc", "test");

    assert_string_equal(ns, "abc");
    free(ns);
}

static void test_replace_empty_pattern(void **state)
{
    char *ns = SearchAndReplace("foobarbaz", "", "abc");

    assert_string_equal(ns, "foobarbaz");
    free(ns);
}

static void test_replace_empty_replacement(void **state)
{
    char *ns = SearchAndReplace("foobarbaz", "a", "");

    assert_string_equal(ns, "foobrbz");
    free(ns);
}

static void test_replace_eq_size(void **state)
{
    char *new_string = SearchAndReplace("sasza szedl sucha szosa", "sz", "xx");

    assert_string_equal(new_string, "saxxa xxedl sucha xxosa");
    free(new_string);
}

static void test_replace_more_size(void **state)
{
    char *new_string = SearchAndReplace("sasza szedl sucha szosa", "sz", "xxx");

    assert_string_equal(new_string, "saxxxa xxxedl sucha xxxosa");
    free(new_string);
}

static void test_replace_less_size(void **state)
{
    char *new_string = SearchAndReplace("sasza szedl sucha szosa", "sz", "x");

    assert_string_equal(new_string, "saxa xedl sucha xosa");
    free(new_string);
}

static void test_no_replace(void **state)
{
    char *new_string = SearchAndReplace("sasza szedl sucha szosa",
                                        "no_such_pattern", "x");

    assert_string_equal(new_string, "sasza szedl sucha szosa");
    free(new_string);
}

static void test_concatenate(void **state)
{
    char *new_string = StringConcatenate(2, "snookie", "sitch");
    assert_string_equal(new_string, "snookiesitch");
    free(new_string);

    new_string = StringConcatenate(4, "a", NULL, "c", "d");
    assert_string_equal(new_string, "acd");
    free(new_string);

    new_string = StringConcatenate(3, "a", "b", "c", "d");
    assert_string_equal(new_string, "abc");
    free(new_string);

    new_string = StringConcatenate(1, "stuff");
    assert_string_equal(new_string, "stuff");
    free(new_string);

    new_string = StringConcatenate(0, NULL);
    assert_false(new_string);
}

static void test_substring_overshoot(void **state)
{
    char *new_string = StringSubstring("abcdef", 6, 0, 10);

    assert_string_equal(new_string, "abcdef");
    free(new_string);
}

static void test_substring_positive(void **state)
{
    char *new_string = StringSubstring("abcdef", 6, 2, 3);

    assert_string_equal(new_string, "cde");
    free(new_string);
}

static void test_substring_negative_length(void **state)
{
    char *new_string = StringSubstring("abcdef", 6, 2, -1);

    assert_string_equal(new_string, "cde");
    free(new_string);
}

static void test_substring_negative(void **state)
{
    char *new_string = StringSubstring("abcdef", 6, -3, -1);

    assert_string_equal(new_string, "de");
    free(new_string);
}

static void test_substring_evil(void **state)
{
    char *new_string = StringSubstring("abcdef", 6, 4, -4);

    assert_int_equal(new_string, NULL);
}

static void test_string_to_long(void **state)
{
    assert_int_equal(1234567, StringToLong("1234567"));
}

static void test_string_from_long(void **state)
{
    assert_string_equal("123456789", StringFromLong(123456789));
    assert_string_equal("-123456789", StringFromLong(-123456789));
}

static void test_string_to_double(void **state)
{
    assert_true(1234.1234 == StringToDouble("1234.1234"));
}

static void test_safe_compare(void **state)
{
    assert_true(StringSafeCompare(NULL, NULL) == 0);
    assert_true(StringSafeCompare(NULL, "a") != 0);
    assert_true(StringSafeCompare("a", NULL) != 0);
    assert_true(StringSafeCompare("a", "a") == 0);
    assert_true(StringSafeCompare("a", "b") != 0);
}

static void test_safe_equal(void **state)
{
    assert_true(StringSafeEqual(NULL, NULL));
    assert_false(StringSafeEqual("a", NULL));
    assert_false(StringSafeEqual(NULL, "a"));
    assert_false(StringSafeEqual("a", "b"));
    assert_true(StringSafeEqual("a", "a"));
}

static void test_match(void **state)
{
    assert_true(StringMatch("^a.*$", "abc"));
    assert_true(StringMatch("a", "a"));
    assert_true(StringMatch("a", "ab"));
    assert_false(StringMatch("^a.*$", "bac"));
}


static void test_match_full(void **state)
{
    assert_true(StringMatchFull("^a.*$", "abc"));
    assert_true(StringMatchFull("a", "a"));
    assert_false(StringMatchFull("a", "ab"));
    assert_false(StringMatchFull("^a.*$", "bac"));
}

static void test_encode_base64(void **state)
{
    {
        char *res = StringEncodeBase64("", 0);
        assert_string_equal("", res);
        free(res);
    }

    {
        char *res = StringEncodeBase64("a", 1);
        assert_string_equal("YQ==", res);
        free(res);
    }

    {
        char *res = StringEncodeBase64("aa", 2);
        assert_string_equal("YWE=", res);
        free(res);
    }

    {
        char *res = StringEncodeBase64("aaa", 3);
        assert_string_equal("YWFh", res);
        free(res);
    }

    {
        char *res =  StringEncodeBase64("aaaa", 4);
        assert_string_equal("YWFhYQ==", res);
        free(res);
    }

    {
        char *res = StringEncodeBase64("snookie", 7);
        assert_string_equal("c25vb2tpZQ==", res);
        free(res);
    }

    {
        char *res = StringEncodeBase64("test", 4);
        assert_string_equal("dGVzdA==", res);
        free(res);
    }

    // valgrind leaks should be due to crypto one-time allocations
}


static void test_escape_char_copy(void **state)
{
    char *in1 = "my test with no escape";
    char *out1 = EscapeCharCopy(in1, '7', '\\');
    assert_string_equal(out1, in1);
    free(out1);

    char *in2 = "my test with 'some' escape";
    char *out2 = EscapeCharCopy(in2, '\'', '\\');
    assert_string_equal(out2, "my test with \\'some\\' escape");
    free(out2);

    char *in3 = "my test with 7some7";
    char *out3 = EscapeCharCopy(in3, '7', '\\');
    assert_string_equal(out3, "my test with \\7some\\7");
    free(out3);

    char *in4 = "\"my\" test with 7some7";
    char *out4 = EscapeCharCopy(in4, '\"', '\\');
    assert_string_equal(out4, "\\\"my\\\" test with 7some7");
    free(out4);

    char *in5 = "\"my test with 7some7\"";
    char *out5 = EscapeCharCopy(in5, '\"', '\\');
    assert_string_equal(out5, "\\\"my test with 7some7\\\"");
    free(out5);
}

static void test_chop_no_spaces(void **state)
{
    char s[] = "abc";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("abc", s);
}

static void test_chop_single_space(void **state)
{
    char s[] = "abc ";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("abc", s);
}

static void test_chop_two_spaces(void **state)
{
    char s[] = "abc  ";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("abc", s);
}

static void test_chop_empty(void **state)
{
    char s[] = "";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("", s);
}

static void test_chop_empty_single_space(void **state)
{
    char s[] = " ";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("", s);
}

static void test_chop_empty_two_spaces(void **state)
{
    char s[] = "  ";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("", s);
}

static void test_ends_with(void **state)
{
    assert_true(StringEndsWith("file.json", ".json"));
    assert_true(StringEndsWith("file.json", "file.json"));
    assert_false(StringEndsWith(".json", "file"));
    assert_false(StringEndsWith("a", "aa"));
}

int main()
{
    const UnitTest tests[] =
{
        unit_test(test_mix_case_tolower),
        unit_test(test_empty_tolower),
        unit_test(test_weird_chars_tolower),
        unit_test(test_alphabet_tolower),
        unit_test(test_hi_alphabet_tolower),
        unit_test(test_inplace_tolower),

        unit_test(test_mix_case_toupper),
        unit_test(test_empty_toupper),
        unit_test(test_weird_chars_toupper),
        unit_test(test_alphabet_toupper),
        unit_test(test_hi_alphabet_toupper),
        unit_test(test_inplace_toupper),

        unit_test(test_replace_empty_pattern),
        unit_test(test_replace_empty_replacement),
        unit_test(test_long_search),
        unit_test(test_replace_eq_size),
        unit_test(test_replace_more_size),
        unit_test(test_replace_less_size),
        unit_test(test_no_replace),

        unit_test(test_concatenate),

        unit_test(test_substring_overshoot),
        unit_test(test_substring_positive),
        unit_test(test_substring_negative_length),
        unit_test(test_substring_negative),
        unit_test(test_substring_evil),

        unit_test(test_string_to_long),
        unit_test(test_string_from_long),
        unit_test(test_string_to_double),

        unit_test(test_safe_compare),
        unit_test(test_safe_equal),

        unit_test(test_match),
        unit_test(test_match_full),

        unit_test(test_encode_base64),

        unit_test(test_escape_char_copy),

        unit_test(test_chop_no_spaces),
        unit_test(test_chop_single_space),
        unit_test(test_chop_two_spaces),
        unit_test(test_chop_empty),
        unit_test(test_chop_empty_single_space),
        unit_test(test_chop_empty_two_spaces),

        unit_test(test_ends_with)
    };

    return run_tests(tests);
}

/* LCOV_EXCL_START */

/* Stub out functions we do not use in test */

void __ProgrammingError(const char *file, int lineno, const char *format, ...)
{
    fail();
    exit(42);
}

void FatalError(char *s, ...)
{
    fail();
    exit(42);
}

void CfOut(OutputLevel level, const char *errstr, const char *fmt, ...)
{
    fail();
    exit(42);
}

/* LCOV_EXCL_STOP */
