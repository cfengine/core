#include <cf3.defs.h>
#include <string_lib.h>
#include <regex.h>
#include <encode.h>

#include <conversion.h>

#include <test.h>

static const char *lo_alphabet = "abcdefghijklmnopqrstuvwxyz";
static const char *hi_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static void test_get_token(void)
{
    {
        const char *str = "  abc def ,efg ";
        size_t len = strlen(str);

        assert_int_equal(3, StringCountTokens(str, len, ", "));

        {
            StringRef ref = StringGetToken(str, len, 0, ", ");
            assert_int_equal(3, ref.len);
            assert_memory_equal("abc", ref.data, 3);
        }

        {
            StringRef ref = StringGetToken(str, len, 1, ", ");
            assert_int_equal(3, ref.len);
            assert_memory_equal("def", ref.data, 3);
        }

        {
            StringRef ref = StringGetToken(str, len, 2, ", ");
            assert_int_equal(3, ref.len);
            assert_memory_equal("efg", ref.data, 3);
        }
    }

    {
        const char *str = "abc";
        size_t len = strlen(str);

        assert_int_equal(1, StringCountTokens(str, len, ", "));

        {
            StringRef ref = StringGetToken(str, len, 0, ", ");
            assert_int_equal(3, ref.len);
            assert_memory_equal("abc", ref.data, 3);
        }
    }

    {
        const char *str = "abc ";
        size_t len = strlen(str);

        assert_int_equal(1, StringCountTokens(str, len, ", "));

        {
            StringRef ref = StringGetToken(str, len, 0, ", ");
            assert_int_equal(3, ref.len);
            assert_memory_equal("abc", ref.data, 3);
        }
    }
}

static void test_mix_case_tolower(void)
{
    char str[] = "aBcD";
    ToLowerStrInplace(str);

    assert_string_equal(str, "abcd");
}

static void test_empty_tolower(void)
{
    char str[] = "";
    ToLowerStrInplace(str);

    assert_string_equal(str, "");
}

static void test_weird_chars_tolower(void)
{
    static const char *weirdstuff = "1345\0xff%$#@!";

    char weirdstuff_copy_lowercased[CF_MAXVARSIZE];
    strlcpy(weirdstuff_copy_lowercased, weirdstuff, CF_MAXVARSIZE);
    ToLowerStrInplace(weirdstuff_copy_lowercased);

    assert_string_equal(weirdstuff_copy_lowercased, weirdstuff);
}

static void test_alphabet_tolower(void)
{
    char lo_alphabet_lowercased[CF_MAXVARSIZE];
    strlcpy(lo_alphabet_lowercased, lo_alphabet, CF_MAXVARSIZE);
    ToLowerStrInplace(lo_alphabet_lowercased);

    assert_string_equal(lo_alphabet_lowercased, lo_alphabet);
}

static void test_hi_alphabet_tolower(void)
{
    char hi_alphabet_lowercased[CF_MAXVARSIZE];
    strlcpy(hi_alphabet_lowercased, hi_alphabet, CF_MAXVARSIZE);
    ToLowerStrInplace(hi_alphabet_lowercased);

    assert_string_equal(hi_alphabet_lowercased, lo_alphabet);
}

static void test_inplace_tolower(void)
{
    char abc[] = "abc";
    char def[] = "def";

    ToLowerStrInplace(abc);
    ToLowerStrInplace(def);

    assert_string_equal(abc, "abc");
    assert_string_equal(def, "def");
}

static void test_mix_case_toupper(void)
{
    char str[] = "aBcD";
    ToUpperStrInplace(str);
    assert_string_equal(str, "ABCD");
}

static void test_empty_toupper(void)
{
    char str[] = "";
    ToUpperStrInplace(str);
    assert_string_equal(str, "");
}

static void test_weird_chars_toupper(void)
{
    static const char *weirdstuff = "1345\0xff%$#@!";

    char weirdstuff_copy_uppercased[CF_MAXVARSIZE];
    strlcpy(weirdstuff_copy_uppercased, weirdstuff, CF_MAXVARSIZE);
    ToUpperStrInplace(weirdstuff_copy_uppercased);

    assert_string_equal(weirdstuff_copy_uppercased, weirdstuff);
}

static void test_alphabet_toupper(void)
{
    char lo_alphabet_uppercased[CF_MAXVARSIZE];
    strlcpy(lo_alphabet_uppercased, lo_alphabet, CF_MAXVARSIZE);
    ToUpperStrInplace(lo_alphabet_uppercased);

    assert_string_equal(lo_alphabet_uppercased, hi_alphabet);
}

static void test_hi_alphabet_toupper(void)
{
    char hi_alphabet_uppercased[CF_MAXVARSIZE];
    strlcpy(hi_alphabet_uppercased, hi_alphabet, CF_MAXVARSIZE);
    ToUpperStrInplace(hi_alphabet_uppercased);

    assert_string_equal(hi_alphabet_uppercased, hi_alphabet);
}

static void test_inplace_toupper(void)
{
    char abc[] = "abc";
    char def[] = "def";

    ToUpperStrInplace(abc);
    ToUpperStrInplace(def);

    assert_string_equal(abc, "ABC");
    assert_string_equal(def, "DEF");
}

static void test_long_search(void)
{
    char *ns = SearchAndReplace("abc", "abcabc", "test");

    assert_string_equal(ns, "abc");
    free(ns);
}

static void test_replace_empty_pattern(void)
{
    char *ns = SearchAndReplace("foobarbaz", "", "abc");

    assert_string_equal(ns, "foobarbaz");
    free(ns);
}

static void test_replace_empty_replacement(void)
{
    char *ns = SearchAndReplace("foobarbaz", "a", "");

    assert_string_equal(ns, "foobrbz");
    free(ns);
}

static void test_replace_eq_size(void)
{
    char *new_string = SearchAndReplace("sasza szedl sucha szosa", "sz", "xx");

    assert_string_equal(new_string, "saxxa xxedl sucha xxosa");
    free(new_string);
}

static void test_replace_more_size(void)
{
    char *new_string = SearchAndReplace("sasza szedl sucha szosa", "sz", "xxx");

    assert_string_equal(new_string, "saxxxa xxxedl sucha xxxosa");
    free(new_string);
}

static void test_replace_less_size(void)
{
    char *new_string = SearchAndReplace("sasza szedl sucha szosa", "sz", "x");

    assert_string_equal(new_string, "saxa xedl sucha xosa");
    free(new_string);
}

static void test_no_replace(void)
{
    char *new_string = SearchAndReplace("sasza szedl sucha szosa",
                                        "no_such_pattern", "x");

    assert_string_equal(new_string, "sasza szedl sucha szosa");
    free(new_string);
}

static void test_concatenate(void)
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

static void test_substring_overshoot(void)
{
    char *new_string = StringSubstring("abcdef", 6, 0, 10);

    assert_string_equal(new_string, "abcdef");
    free(new_string);
}

static void test_substring_positive(void)
{
    char *new_string = StringSubstring("abcdef", 6, 2, 3);

    assert_string_equal(new_string, "cde");
    free(new_string);
}

static void test_substring_negative_length(void)
{
    char *new_string = StringSubstring("abcdef", 6, 2, -1);

    assert_string_equal(new_string, "cde");
    free(new_string);
}

static void test_substring_negative(void)
{
    char *new_string = StringSubstring("abcdef", 6, -3, -1);

    assert_string_equal(new_string, "de");
    free(new_string);
}

static void test_substring_evil(void)
{
    char *new_string = StringSubstring("abcdef", 6, 4, -4);

    assert_int_equal(new_string, NULL);
}

static void test_string_to_long(void)
{
    // Basic usage:
    assert_int_equal(StringToLongExitOnError("0"),    0);
    assert_int_equal(StringToLongExitOnError("-0"),   0);
    assert_int_equal(StringToLongExitOnError("+0"),   0);
    assert_int_equal(StringToLongExitOnError("123"),  123);
    assert_int_equal(StringToLongExitOnError("+123"), 123);

    // WARNING: Some platforms have 32-bit long, 2,147,483,647 is LONG_MAX
    assert_int_equal(StringToLongExitOnError("2147483647"), 2147483647);
    assert_int_equal(StringToLongExitOnError("1987654320"), 1987654320);

    // Negative numbers:
    assert_int_equal((int)StringToLongExitOnError("-1"),       -1);
    assert_int_equal(     StringToLongExitOnError("-1"), (long)-1);
    assert_int_equal((int)StringToLongExitOnError("-1"),       -1);
    assert_int_equal(     StringToLongExitOnError("-1"), (long)-1);

    // Leading spaces:
    assert_int_equal(StringToLongExitOnError(" 0") ,   0);
    assert_int_equal(StringToLongExitOnError(" 123"),  123);
    assert_int_equal(StringToLongExitOnError(" -123"), (long)-123);
    assert_int_equal(StringToLongExitOnError("             0"),   0);
    assert_int_equal(StringToLongExitOnError("             123"), 123);
    assert_int_equal(StringToLongExitOnError("            -123"), (long)-123);

    // Trailing spaces:
    assert_int_equal(StringToLongExitOnError("0 "),    0);
    assert_int_equal(StringToLongExitOnError("789 "),  789);
    assert_int_equal(StringToLongExitOnError("-789 "), (long)-789);
    assert_int_equal(StringToLongExitOnError("0               "), 0);
    assert_int_equal(StringToLongExitOnError("789             "), 789);
    assert_int_equal(StringToLongExitOnError("-789            "), (long)-789);

    // More spaces:
    assert_int_equal(StringToLongExitOnError("   0    "), 0);
    assert_int_equal(StringToLongExitOnError("   -0   "), 0);
    assert_int_equal(StringToLongExitOnError("   456  "), 456);

    // Space separated numbers:
    assert_int_equal(StringToLongExitOnError("   456  9  "), 456);
    assert_int_equal(StringToLongExitOnError("1 0"),         1);
}

static void test_string_to_long_default(void)
{
    assert_int_equal(StringToLongDefaultOnError("0",10), 0);
    assert_int_equal(StringToLongDefaultOnError(" ",10), 10);
    assert_int_equal(StringToLongDefaultOnError("error",123), 123);
    assert_int_equal(StringToLongDefaultOnError("-error",-123), (long)-123);
}

static void test_string_to_long_errors(void)
{
    // A succesful call to StringToLong should return 0:
    long target = 0;
    assert(StringToLong("1234",&target) == 0);
    assert(target == 1234);

    // Test that invalid inputs give error return code:
    assert(StringToLong("",       &target) != 0);
    assert(StringToLong(" ",      &target) != 0);
    assert(StringToLong("error",  &target) != 0);
    assert(StringToLong("-error", &target) != 0);
    assert(StringToLong("ffff",   &target) != 0);
    assert(StringToLong("1d",     &target) != 0);
    assert(StringToLong("56789d", &target) != 0);
    assert(StringToLong("9999999999999999999999999999999",&target) == ERANGE);
    assert(StringToLong(" 999999999999999999999999999999",&target) == ERANGE);
    assert(StringToLong("-999999999999999999999999999999",&target) == ERANGE);

    // Test that error logging function can be called:
    LogStringToLongError("-999999999999999999999999999999", "string_lib_test",
                         ERANGE);

    // Check that target is unmodified after errors:
    assert(target == 1234);
}

static void test_string_to_long_unsafe(void)
{
    assert_int_equal(StringToLongUnsafe("0"),  0);
    assert_int_equal(StringToLongUnsafe("1"),  1);
    assert_int_equal(StringToLongUnsafe(" 0"), 0);
    assert_int_equal(StringToLongUnsafe(" 1"), 1);
    assert_int_equal(StringToLongUnsafe("-1"),    (long)-1);
    assert_int_equal(StringToLongUnsafe(" -1"),   (long)-1);
    assert_int_equal(StringToLongUnsafe(" -987"), (long)-987);
    assert_int_equal(StringToLongUnsafe("1987654320"),   1987654320);
    assert_int_equal(StringToLongUnsafe(" 1987654320"),  1987654320);
    assert_int_equal(StringToLongUnsafe(" -1987654320"), (long)-1987654320);

    // Weird edge case:
    assert_int_equal(StringToLongUnsafe(""), 0);
}

// StringToLongExitOnError should replace StringToLongUnsafe:
#define assert_string_to_long_unsafe(x)\
{\
    assert_int_equal(StringToLongExitOnError(x), StringToLongUnsafe(x));\
}

static void test_string_to_long_compatibility(void)
{
    // All these inputs should give same result for new and old function:
    assert_string_to_long_unsafe("0");
    assert_string_to_long_unsafe("-1");
    assert_string_to_long_unsafe("-0");
    assert_string_to_long_unsafe("  -0");
    assert_string_to_long_unsafe("1");
    assert_string_to_long_unsafe("123");
    assert_string_to_long_unsafe("1987654320");
    assert_string_to_long_unsafe("          1987654320");

    // Old function (StringToLongUnsafe) does not allow trailing whitespace
}

static void test_string_from_long(void)
{
    char *result = StringFromLong(123456789);
    assert_string_equal("123456789", result);
    free(result);
    result = StringFromLong(-123456789);
    assert_string_equal("-123456789", result);
    free(result);
}

static void test_string_to_double(void)
{
    assert_true(1234.1234 == StringToDouble("1234.1234"));
}

static void test_string_from_double(void)
{
    char *result = StringFromDouble(1234.1234);
    assert_string_equal("1234.12", result);
    free(result);
}

static void test_safe_compare(void)
{
    // Strings which are equal:
    assert_true(StringSafeCompare(NULL, NULL) == 0);
    assert_true(StringSafeCompare("", "") == 0);
    assert_true(StringSafeCompare("a", "a") == 0);
    assert_true(StringSafeCompare("abc", "abc") == 0);
    assert_true(StringSafeCompare("Hello, world!", "Hello, world!") == 0);

    // Strings which are not equal:
    assert_true(StringSafeCompare("abc", "abC") != 0);
    assert_true(StringSafeCompare("a", "b") != 0);

    // Test ordering of strings (correct sign):
    assert_true(StringSafeCompare(NULL, "a") <= -1);
    assert_true(StringSafeCompare("", "a") <= -1);
    assert_true(StringSafeCompare("a", NULL) >= 1);
    assert_true(StringSafeCompare("a", "") >= 1);
    assert_true(StringSafeCompare("albatross", "bear") <= -1);
    assert_true(StringSafeCompare("lynx", "chicken") >= 1);
}

static void test_safe_equal(void)
{
    assert_true(StringSafeEqual(NULL, NULL));
    assert_true(StringSafeEqual("a", "a"));
    assert_true(StringSafeEqual("abcdefghijklmnopqrstuvwxyz", "abcdefghijklmnopqrstuvwxyz"));
    assert_true(StringSafeEqual("0123456789", "0123456789"));
    assert_true(StringSafeEqual("CamelCase", "CamelCase"));
    assert_true(StringSafeEqual("(){}[]<>", "(){}[]<>"));
    assert_true(StringSafeEqual("+-*/%%^", "+-*/%%^"));

    assert_false(StringSafeEqual("", NULL));
    assert_false(StringSafeEqual(NULL, ""));
    assert_false(StringSafeEqual("a", "b"));
    assert_false(StringSafeEqual("a", "A"));
    assert_false(StringSafeEqual("abc def", "abc deF"));
}

static void test_safe_compare_ignore_case(void)
{
    // Strings which are equal:
    assert_true(StringSafeCompare_IgnoreCase(NULL, NULL) == 0);
    assert_true(StringSafeCompare_IgnoreCase("", "") == 0);
    assert_true(StringSafeCompare_IgnoreCase("a", "a") == 0);
    assert_true(StringSafeCompare_IgnoreCase("abc", "abc") == 0);
    assert_true(StringSafeCompare_IgnoreCase("Hello, world!", "Hello, world!") == 0);

    // Strings with only case differences:
    assert_true(StringSafeCompare_IgnoreCase("abc", "abC") == 0);
    assert_true(StringSafeCompare_IgnoreCase("a", "A") == 0);
    assert_true(StringSafeCompare_IgnoreCase("HELLO, WORLD!", "Hello, world!") == 0);
    assert_true(StringSafeCompare_IgnoreCase("HELLO, WORLD!", "hello, world!") == 0);

    // Test ordering of strings (correct sign):
    assert_true(StringSafeCompare_IgnoreCase(NULL, "a") <= -1);
    assert_true(StringSafeCompare_IgnoreCase("", "a") <= -1);
    assert_true(StringSafeCompare_IgnoreCase("a", NULL) >= 1);
    assert_true(StringSafeCompare_IgnoreCase("a", "") >= 1);
    assert_true(StringSafeCompare_IgnoreCase("albatross", "bear") <= -1);
    assert_true(StringSafeCompare_IgnoreCase("lynx", "chicken") >= 1);

    // Cases where StringSafeCompare and StringSafeCompare_IgnoreCase should be the same:
    assert_int_equal(StringSafeCompare("a", "b"), StringSafeCompare_IgnoreCase("a", "b"));
    assert_int_equal(StringSafeCompare("a", "b"), StringSafeCompare_IgnoreCase("A", "B"));
    assert_int_equal(StringSafeCompare("A", "B"), StringSafeCompare_IgnoreCase("a", "b"));
    assert_int_equal(StringSafeCompare("bbc", "bbd"), StringSafeCompare_IgnoreCase("BBC", "bbd"));
    assert_int_equal(StringSafeCompare("bbc", "bbd"), StringSafeCompare_IgnoreCase("BBC", "BBd"));
}

static void test_safe_equal_ignore_case(void)
{
    assert_true(StringSafeEqual_IgnoreCase(NULL, NULL));
    assert_true(StringSafeEqual_IgnoreCase("a", "a"));
    assert_true(StringSafeEqual_IgnoreCase("a", "A"));
    assert_true(StringSafeEqual_IgnoreCase(hi_alphabet, lo_alphabet));
    assert_true(StringSafeEqual_IgnoreCase("0123456789", "0123456789"));
    assert_true(StringSafeEqual_IgnoreCase("CamelCase", "camelcase"));
    assert_true(StringSafeEqual_IgnoreCase("(){}[]<>", "(){}[]<>"));
    assert_true(StringSafeEqual_IgnoreCase("+-*/%%^", "+-*/%%^"));
    assert_true(StringSafeEqual_IgnoreCase("abc def", "abc deF"));

    assert_false(StringSafeEqual_IgnoreCase("", NULL));
    assert_false(StringSafeEqual_IgnoreCase(NULL, ""));
    assert_false(StringSafeEqual_IgnoreCase("a", "b"));
}

static void test_safe_equal_n(void)
{
    assert_true(StringSafeEqualN("abcd", "abcX", 3));
    assert_true(StringSafeEqualN_IgnoreCase("abcd", "ABCX", 3));

    assert_false(StringSafeEqualN("abcd", "abXX", 3));
    assert_false(StringSafeEqualN_IgnoreCase("abcd", "ABXX", 3));

    assert_true(StringSafeEqualN("123abc", "123abc", 1000));
    assert_true(StringSafeEqualN_IgnoreCase("123abc", "123ABC", 1000));
}

static void test_match(void)
{
    assert_true(StringMatch("^a.*$", "abc", NULL, NULL));
    assert_true(StringMatch("a", "a", NULL, NULL));
    assert_true(StringMatch("a", "ab", NULL, NULL));
    assert_false(StringMatch("^a.*$", "bac", NULL, NULL));
}


static void test_match_full(void)
{
    assert_true(StringMatchFull("^a.*$", "abc"));
    assert_true(StringMatchFull("a", "a"));
    assert_false(StringMatchFull("a", "ab"));
    assert_false(StringMatchFull("^a.*$", "bac"));
}

static void test_encode_base64(void)
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


static void test_escape_char_copy(void)
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

static void test_chop_no_spaces(void)
{
    char s[] = "abc";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("abc", s);
}

static void test_chop_single_space(void)
{
    char s[] = "abc ";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("abc", s);
}

static void test_chop_two_spaces(void)
{
    char s[] = "abc  ";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("abc", s);
}

static void test_chop_empty(void)
{
    char s[] = "";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("", s);
}

static void test_chop_empty_single_space(void)
{
    char s[] = " ";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("", s);
}

static void test_chop_empty_two_spaces(void)
{
    char s[] = "  ";
    Chop(s, CF_EXPANDSIZE);
    assert_string_equal("", s);
}

static void test_ends_with(void)
{
    assert_true(StringEndsWith("file.json", ".json"));
    assert_true(StringEndsWith("file.json", "file.json"));
    assert_false(StringEndsWith(".json", "file"));
    assert_false(StringEndsWith("a", "aa"));
}

char *test_stringvformat_sup(const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    char *fmted = StringVFormat(s, ap);
    va_end(ap);
    return fmted;
}

static void test_stringvformat(void)
{
    char *s = test_stringvformat_sup("%s%d", "abc", 42);
    assert_string_equal(s, "abc42");
    free(s);
}

static void test_stringformat(void)
{
    char *s = StringFormat("%d%s%d", 1, "a", 2);
    assert_string_equal(s, "1a2");
    free(s);
}

static void test_stringscanfcapped(void)
{
    char buf[20];
    char sp[30];

    strcpy(sp,"");
    StringNotMatchingSetCapped(sp,20,"\n",buf);
    assert_string_equal(buf, "");

    strcpy(sp,"\n");
    StringNotMatchingSetCapped(sp,20,"\n",buf);
    assert_string_equal(buf, "");

    strcpy(sp,"\n2345678901234567890abcdefghi");
    StringNotMatchingSetCapped(sp,20,"\n",buf);
    assert_string_equal(buf, "");

    strcpy(sp,"12345678901234567890abcdefghi");
    StringNotMatchingSetCapped(sp,20,"\n",buf);
    assert_string_equal(buf, "1234567890123456789");

    strcpy(sp,"12345678901234567890abcde\nghi");
    StringNotMatchingSetCapped(sp,20,"\n",buf);
    assert_string_equal(buf, "1234567890123456789");

    strcpy(sp,"123456789012345\n7890abcdefghi");
    StringNotMatchingSetCapped(sp,20,"\n",buf);
    assert_string_equal(buf, "123456789012345");
}

static void test_PathAppend(void)
{
    char dst[10];
    bool ret;

    {                                                           /* fits */
        dst[0] = '\0';
        ret = PathAppend(dst, sizeof(dst), "blah", '/');
        assert_string_equal(dst, "/blah");
        assert_true(ret);
    }
    {                             /* SAME, but string already has separator */
        strcpy(dst, "/");
        ret = PathAppend(dst, sizeof(dst), "blah", '/');
        assert_string_equal(dst, "/blah");
        assert_true(ret);
    }
    {                                                 /* trailing separator */
        dst[0] = '\0';
        ret = PathAppend(dst, sizeof(dst), "blah/", '/');
        assert_string_equal(dst, "/blah/");
        assert_true(ret);
    }
    {                       /* SAME, but string already has separator ahead */
        strcpy(dst, "/");
        ret = PathAppend(dst, sizeof(dst), "blah/", '/');
        assert_string_equal(dst, "/blah/");
        assert_true(ret);
    }
    {                                                        /* barely fits */
        dst[0] = '\0';
        ret = PathAppend(dst, 6, "blah", '/');
        assert_string_equal(dst, "/blah");
        assert_true(ret);
    }
    {                             /* SAME, but string already has separator */
        strcpy(dst, "/");
        ret = PathAppend(dst, 6, "blah", '/');
        assert_string_equal(dst, "/blah");
        assert_true(ret);
    }
    {                           /* barely not fits (off by one), do nothing */
        dst[0] = '\0';
        ret = PathAppend(dst, 5, "blah", '/');
        assert_string_equal(dst, "");
        assert_false(ret);
    }
    {                             /* SAME, but string already has separator */
        strcpy(dst, "/");
        ret = PathAppend(dst, 5, "blah", '/');
        assert_string_equal(dst, "/");
        assert_false(ret);
    }
    {                                               /* overflow, do nothing */
        dst[0] = '\0';
        ret = PathAppend(dst, 2, "blah", '/');
        assert_string_equal(dst, "");
        assert_false(ret);
    }
    {                             /* SAME, but string already has separator */
        strcpy(dst, "/");
        ret = PathAppend(dst, 2, "blah", '/');
        assert_string_equal(dst, "/");
        assert_false(ret);
    }
}

static void test_StrCat(void)
{
    char dst[10];
    size_t dst_len;

    {
        dst[0] = '\0';
        dst_len = 0;
        StrCat(dst, sizeof(dst), &dst_len, "blah", 0);
        assert_string_equal(dst, "blah");
        assert_int_equal(dst_len, 4);
        StrCat(dst, sizeof(dst), &dst_len, "", 0);
        assert_string_equal(dst, "blah");
        assert_int_equal(dst_len, 4);
        StrCat(dst, sizeof(dst), &dst_len, " ", 0);
        assert_string_equal(dst, "blah ");
        assert_int_equal(dst_len, 5);
        StrCat(dst, sizeof(dst), &dst_len, "blue", 0);
        assert_string_equal(dst, "blah blue");
        assert_int_equal(dst_len, 9);
        /* Append one OVERFLOWing character. */
        StrCat(dst, sizeof(dst), &dst_len, "1", 0);
        /* It should protect against overflow. */
        assert_string_equal(dst, "blah blue");
        /* But the length indicates the needed length. */
        assert_int_equal(dst_len, 10);
    }
    {                        /* The string to append is not '\0'-terminated */
        const char *src = "blah blue";
        dst[0] = '\0';
        dst_len = 0;
        StrCat(dst, sizeof(dst), &dst_len, src, 4);
        assert_string_equal(dst, "blah");
        assert_int_equal(dst_len, 4);
        StrCat(dst, sizeof(dst), &dst_len, src, 4);
        assert_string_equal(dst, "blahblah");
        assert_int_equal(dst_len, 8);
        StrCat(dst, sizeof(dst), &dst_len, src, 2);
        assert_string_equal(dst, "blahblahb");                  /* overflow */
        assert_int_equal(dst_len, 10);
    }
    {
        dst[0] = '\0';
        dst_len = 0;
        StrCat(dst, 4, &dst_len, "blah", 0);
        assert_string_equal(dst, "bla");
        /* Overflow so dst_len indicates the needed length. */
        assert_int_equal(dst_len, 4);
        StrCat(dst, 4, &dst_len, "", 0);
        assert_string_equal(dst, "bla");
        assert_int_equal(dst_len, 4);
        StrCat(dst, 4, &dst_len, "blue", 0);
        assert_string_equal(dst, "bla");
        assert_int_equal(dst_len, 8);
    }
    {                                      /* SAME but pass NULL as dst_len */
        dst[0] = '\0';
        StrCat(dst, 4, NULL, "blah", 0);
        assert_string_equal(dst, "bla");
        StrCat(dst, 4, NULL, "", 0);
        assert_string_equal(dst, "bla");
        StrCat(dst, 4, NULL, "blue", 0);
        assert_string_equal(dst, "bla");
    }

    {                           /* Do not reset dst but reset only dst_len. */
        dst_len = 0;
        StrCat(dst, sizeof(dst), &dst_len, "1", 0);
        assert_string_equal(dst, "1");
        assert_int_equal(dst_len, 1);
    }
}

static void test_StrCatDelim(void)
{
    char dst[10];
    size_t dst_len;

    {                    /* Simple appends, we don't care about truncation. */
        dst[0] = '\0';
        StrCatDelim(dst, sizeof(dst), NULL, "blah", ',');
        StrCatDelim(dst, sizeof(dst), NULL, "blah", ',');
        assert_string_equal(dst, "blah,blah");
        StrCatDelim(dst, sizeof(dst), NULL, "blah", ',');
        assert_string_equal(dst, "blah,blah");
        StrCatDelim(dst, sizeof(dst), NULL, "1", ',');
        assert_string_equal(dst, "blah,blah");
    }
    {                                        /* SAME, but check truncation. */
        dst[0] = '\0';
        dst_len = 0;
        StrCatDelim(dst, sizeof(dst), &dst_len, "blah", ',');
        assert_int_equal(dst_len, 4);
        StrCatDelim(dst, sizeof(dst), &dst_len, "blah", ',');
        assert_string_equal(dst, "blah,blah");
        assert_int_equal(dst_len, 9);
        StrCatDelim(dst, sizeof(dst), &dst_len, "blah", ',');
        assert_string_equal(dst, "blah,blah");
        assert_int_equal(dst_len, 14);                     /* needed length */
        StrCatDelim(dst, sizeof(dst), &dst_len, "1", ',');
        assert_string_equal(dst, "blah,blah");
        assert_int_equal(dst_len, 16);
    }
    {                                               /* Only the comma fits. */
        strcpy(dst, "12345678");
        StrCatDelim(dst, sizeof(dst), NULL, "1", ',');
        assert_string_equal(dst, "12345678");
    }
    {                                        /* SAME, but check truncation. */
        strcpy(dst, "12345678");
        dst_len = 8;
        StrCatDelim(dst, sizeof(dst), &dst_len, "1", ',');
        assert_string_equal(dst, "12345678");
        assert_int_equal(dst_len, 10);           /* 10 is the needed length */
    }
}


int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_get_token),

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
        unit_test(test_string_to_long_default),
        unit_test(test_string_to_long_errors),
        unit_test(test_string_to_long_unsafe),
        unit_test(test_string_to_long_compatibility),
        unit_test(test_string_from_long),
        unit_test(test_string_to_double),
        unit_test(test_string_from_double),

        unit_test(test_safe_compare),
        unit_test(test_safe_equal),
        unit_test(test_safe_compare_ignore_case),
        unit_test(test_safe_equal_ignore_case),
        unit_test(test_safe_equal_n),

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

        unit_test(test_ends_with),

        unit_test(test_stringformat),
        unit_test(test_stringvformat),

        unit_test(test_stringscanfcapped),

        unit_test(test_PathAppend),
        unit_test(test_StrCat),
        unit_test(test_StrCatDelim),
    };

    return run_tests(tests);
}
