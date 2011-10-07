#include "cf3.defs.h"
#include "cf3.extern.h"

#include <setjmp.h>
#include <cmockery.h>

static void test_create_destroy(void **state)
{
struct CfAssoc *ap = NewAssoc("hello", "world", CF_SCALAR, cf_str);
DeleteAssoc(ap);
}

static void test_copy(void **state)
{
struct CfAssoc *ap = NewAssoc("hello", "world", CF_SCALAR, cf_str);
struct CfAssoc *ap2 = CopyAssoc(ap);

assert_string_equal(ap->lval, ap2->lval);
assert_string_equal(ap->rval, ap2->rval);
assert_int_equal(ap->rtype, ap2->rtype);
assert_int_equal(ap->dtype, ap2->dtype);

DeleteAssoc(ap);
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_create_destroy),
   unit_test(test_copy)
   };

return run_tests(tests);
}

