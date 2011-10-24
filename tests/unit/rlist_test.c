#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmockery.h>

#include "cf3.defs.h"
#include "cf3.extern.h"

static void test_prepend_scalar(void **state)
{
struct Rlist *list = NULL;

PrependRScalar(&list, "stuff", CF_SCALAR);
PrependRScalar(&list, "more-stuff", CF_SCALAR);

assert_string_equal(list->item, "more-stuff");

DeleteRlist(list);
}

static void test_length(void **state)
{
struct Rlist *list = NULL;

assert_int_equal(RlistLen(list), 0);

PrependRScalar(&list, "stuff", CF_SCALAR);
assert_int_equal(RlistLen(list), 1);

PrependRScalar(&list, "more-stuff", CF_SCALAR);
assert_int_equal(RlistLen(list), 2);

DeleteRlist(list);
}

static void test_prepend_scalar_idempotent(void **state)
{
struct Rlist *list = NULL;

IdempPrependRScalar(&list, "stuff", CF_SCALAR);
IdempPrependRScalar(&list, "stuff", CF_SCALAR);

assert_string_equal(list->item, "stuff");
assert_int_equal(RlistLen(list), 1);

DeleteRlist(list);
}


static void test_copy(void **state)
{
struct Rlist *list = NULL, *copy = NULL;

PrependRScalar(&list, "stuff", CF_SCALAR);
PrependRScalar(&list, "more-stuff", CF_SCALAR);

copy = CopyRlist(list);

assert_string_equal(list->item, copy->item);
assert_string_equal(list->next->item, copy->next->item);

DeleteRlist(list);
DeleteRlist(copy);
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_prepend_scalar),
   unit_test(test_prepend_scalar_idempotent),
   unit_test(test_length),
   unit_test(test_copy)
   };

return run_tests(tests);
}
