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
