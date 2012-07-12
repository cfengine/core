#include "test.h"
#include "policy.h"
#include "parser.h"

static Sequence *LoadAndCheck(const char *filename)
{
    char path[1024];
    sprintf(path, "%s/%s", TESTDATADIR, filename);

    Policy *p = PolicyNew();
    ParserParseFile(p, path);

    Sequence *errs = SequenceCreate(10, PolicyErrorDestroy);
    PolicyCheck(p, errs);

    return errs;
}

static void test_bundle_redefinition(void **state)
{
    Sequence *errs = LoadAndCheck("bundle_redefinition.cf");
    assert_int_equal(2, errs->length);

    SequenceDestroy(errs);
}

static void test_bundle_reserved_name(void **state)
{
    Sequence *errs = LoadAndCheck("bundle_reserved_name.cf");
    assert_int_equal(1, errs->length);

    SequenceDestroy(errs);
}

static void test_body_redefinition(void **state)
{
    Sequence *errs = LoadAndCheck("body_redefinition.cf");
    assert_int_equal(2, errs->length);

    SequenceDestroy(errs);
}

static void test_subtype_invalid(void **state)
{
    Sequence *errs = LoadAndCheck("subtype_invalid.cf");
    assert_int_equal(1, errs->length);

    SequenceDestroy(errs);
}

static void test_vars_multiple_types(void **state)
{
    Sequence *errs = LoadAndCheck("vars_multiple_types.cf");
    assert_int_equal(1, errs->length);

    SequenceDestroy(errs);
}

static void test_methods_invalid_arity(void **state)
{
    Sequence *errs = LoadAndCheck("methods_invalid_arity.cf");
    assert_int_equal(1, errs->length);

    SequenceDestroy(errs);
}

int main()
{
    const UnitTest tests[] =
    {
        unit_test(test_bundle_redefinition),
        unit_test(test_bundle_reserved_name),
        unit_test(test_body_redefinition),
        unit_test(test_subtype_invalid),
        unit_test(test_vars_multiple_types),
        unit_test(test_methods_invalid_arity),
    };

    return run_tests(tests);
}
