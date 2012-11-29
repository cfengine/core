#include "test.h"

#include "assoc.h"

static void test_create_destroy(void **state)
{
    CfAssoc *ap = NewAssoc("hello", (Rval) { "world", CF_SCALAR }, cf_str);
    DeleteAssoc(ap);
}

static void test_copy(void **state)
{
    CfAssoc *ap = NewAssoc("hello", (Rval) { "world", CF_SCALAR }, cf_str);
    CfAssoc *ap2 = CopyAssoc(ap);

    assert_string_equal(ap->lval, ap2->lval);
    assert_string_equal(ap->rval.item, ap2->rval.item);
    assert_int_equal(ap->rval.rtype, ap2->rval.rtype);
    assert_int_equal(ap->dtype, ap2->dtype);

    DeleteAssoc(ap);
    DeleteAssoc(ap2);
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
