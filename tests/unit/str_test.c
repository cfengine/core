#include "cf3.defs.h"
#include "cf3.extern.h"

#include <setjmp.h>
#include <cmockery.h>

static const char *lo_alphabet = "abcdefghijklmnopqrstuvwxyz";
static const char *hi_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static void test_mix_case_tolower(void **state)
{
assert_string_equal(ToLowerStr("aBcD"), "abcd");
}

static void test_empty_tolower(void **state)
{
assert_string_equal(ToLowerStr(""), "");
}

static void test_weird_chars_tolower(void **state)
{
static const char *weirdstuff = "1345\0xff%$#@!";
assert_string_equal(ToLowerStr(weirdstuff), weirdstuff);
}


static void test_alphabet_tolower(void **state)
{
assert_string_equal(ToLowerStr(lo_alphabet), lo_alphabet);
}

static void test_hi_alphabet_tolower(void **state)
{
assert_string_equal(ToLowerStr(hi_alphabet),
                    lo_alphabet);
}

/* Demonstrates misfeature of original design */
static void test_aliasing_tolower(void **state)
{
char *abc = ToLowerStr("abc");
char *def = ToLowerStr("def");

assert_string_equal(abc, "def");
assert_string_equal(def, "def");
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
assert_string_equal(ToUpperStr("aBcD"), "ABCD");
}

static void test_empty_toupper(void **state)
{
assert_string_equal(ToUpperStr(""), "");
}

static void test_weird_chars_toupper(void **state)
{
static const char *weirdstuff = "1345\0xff%$#@!";
assert_string_equal(ToUpperStr(weirdstuff), weirdstuff);
}


static void test_alphabet_toupper(void **state)
{
assert_string_equal(ToUpperStr(lo_alphabet), hi_alphabet);
}

static void test_hi_alphabet_toupper(void **state)
{
assert_string_equal(ToUpperStr(hi_alphabet), hi_alphabet);
}

/* Demonstrates misfeature of original design */
static void test_aliasing_toupper(void **state)
{
char *abc = ToUpperStr("abc");
char *def = ToUpperStr("def");

assert_string_equal(abc, "DEF");
assert_string_equal(def, "DEF");
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

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_mix_case_tolower),
   unit_test(test_empty_tolower),
   unit_test(test_weird_chars_tolower),
   unit_test(test_alphabet_tolower),
   unit_test(test_hi_alphabet_tolower),
   unit_test(test_aliasing_tolower),
   unit_test(test_inplace_tolower),

   unit_test(test_mix_case_toupper),
   unit_test(test_empty_toupper),
   unit_test(test_weird_chars_toupper),
   unit_test(test_alphabet_toupper),
   unit_test(test_hi_alphabet_toupper),
   unit_test(test_aliasing_toupper),
   unit_test(test_inplace_toupper),

   unit_test(test_replace_empty_pattern),
   unit_test(test_replace_empty_replacement),
   unit_test(test_long_search),
   unit_test(test_replace_eq_size),
   unit_test(test_replace_more_size),
   unit_test(test_replace_less_size),
   unit_test(test_no_replace),
   };

return run_tests(tests);
}

/* LCOV_EXCL_START */

/* Stub out functions we do not use in test */

void FatalError(char *s, ...)
{
fail();
exit(42);
}

/* LCOV_EXCL_STOP */
