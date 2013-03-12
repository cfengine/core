#include "test.h"

#include "expand.h"
#include "rlist.h"

void test_map_iterators_from_rval_empty(void **state)
{
    Rlist *scalars = NULL, *lists = NULL;
    MapIteratorsFromRval("none", &scalars, &lists, (Rval) { "", RVAL_TYPE_SCALAR });

    assert_int_equal(0, RlistLen(scalars));
    assert_int_equal(0, RlistLen(lists));
}

void test_map_iterators_from_rval_literal(void **state)
{
    Rlist *scalars = NULL, *lists = NULL;
    MapIteratorsFromRval("none", &scalars, &lists, (Rval) { "snookie", RVAL_TYPE_SCALAR });

    assert_int_equal(0, RlistLen(scalars));
    assert_int_equal(0, RlistLen(lists));
}

void test_map_iterators_from_rval_naked_var_nonresolvable(void **state)
{
    Rlist *scalars = NULL, *lists = NULL;
    MapIteratorsFromRval("scope", &scalars, &lists, (Rval) { "${snookie}", RVAL_TYPE_SCALAR });

    assert_int_equal(0, RlistLen(scalars));
    assert_int_equal(0, RlistLen(lists));
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_map_iterators_from_rval_empty),
        unit_test(test_map_iterators_from_rval_literal),
        unit_test(test_map_iterators_from_rval_naked_var_nonresolvable)
    };

    return run_tests(tests);
}
