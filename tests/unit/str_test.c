#include "cf3.defs.h"
#include "cf3.extern.h"

#include <setjmp.h>
#include <cmockery.h>

static void test_mix_case(void **state)
{
assert_string_equal(ToLowerStr("aBcD"), "abcd");
}

static void test_empty(void **state)
{
assert_string_equal(ToLowerStr(""), "");
}

static void test_weird_chars(void **state)
{
static const char *weirdstuff = "1345\0xff%$#@!";
assert_string_equal(ToLowerStr(weirdstuff), weirdstuff);
}

static const char *alphabet = "abcdefghijklmnopqrstuvwxyz";

static void test_alphabet(void **state)
{
assert_string_equal(ToLowerStr(alphabet), alphabet);
}

static void test_up_alphabet(void **state)
{
assert_string_equal(ToLowerStr("ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
                    alphabet);
}

/* Demonstrates misfeature of original design */
static void test_aliasing(void **state)
{
char *abc = ToLowerStr("abc");
char *def = ToLowerStr("def");

assert_string_equal(abc, "def");
assert_string_equal(def, "def");
}

static void test_inplace(void **state)
{
char abc[] = "abc";
char def[] = "def";

ToLowerStrInplace(abc);
ToLowerStrInplace(def);

assert_string_equal(abc, "abc");
assert_string_equal(def, "def");
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_mix_case),
   unit_test(test_empty),
   unit_test(test_weird_chars),
   unit_test(test_alphabet),
   unit_test(test_up_alphabet),
   unit_test(test_aliasing),
   unit_test(test_inplace),
   };

return run_tests(tests);
}


/* Stub out functions we do not use in test */

void FatalError(char *s, ...)
{
fail();
exit(42);
}
