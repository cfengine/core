#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmockery.h>

#include "cf3.defs.h"
#include "cf3.extern.h"

/*
 * FIXME: dependencies we want to cut later
 */
int FuzzySetMatch(char *s1, char *s2)
{
return 0;
}

void DeleteScope(char *name)
{
}

void NewScope(char *name)
{
}

void ForceScalar(char *lval, char *rval)
{
}

bool IsExcluded(const char *exception)
{
return false;
}

void PromiseRef(enum cfreport level, struct Promise *pp)
{
}

enum insert_match String2InsertMatch(char *s)
{
return 0;
}

/*
 * End of deps
 */

static void test_create_destroy(void **state)
{
struct AlphaList l;
InitAlphaList(&l);
DeleteAlphaList(&l);
}

static void test_prepend(void **state)
{
struct AlphaList l;
InitAlphaList(&l);
assert_int_equal(InAlphaList(l, "mystring"), 0);
PrependAlphaList(&l, "mystring");
assert_int_equal(InAlphaList(l, "mystring"), 1);
DeleteAlphaList(&l);
}

static void test_prepend_empty(void **state)
{
struct AlphaList l;
InitAlphaList(&l);
assert_int_equal(InAlphaList(l, "hello"), 0);
PrependAlphaList(&l, "hello");
assert_int_equal(InAlphaList(l, "hello"), 1);
DeleteAlphaList(&l);
}

static void test_shallow_copy(void **state)
{
struct AlphaList a, b;
InitAlphaList(&a);
InitAlphaList(&b);

PrependAlphaList(&a, "hello");

assert_int_equal(InAlphaList(b, "hello"), 0);
CopyAlphaListPointers(&b, &a);
assert_int_equal(InAlphaList(b, "hello"), 1);

DeleteAlphaList(&a);
DeleteAlphaList(&b);
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_create_destroy),
   unit_test(test_prepend),
   unit_test(test_prepend_empty),
   /* unit_test(test_shallow_copy), */
   /* unit_test(test_idemp_prepend), */
   /* unit_test(test_in), */
   /* unit_test(test_match), */
   };

return run_tests(tests);
}

