#include "test.h"

#include "expand.h"
#include "rlist.h"
#include "scope.h"

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

void test_map_iterators_from_rval_naked_scalar_var_nonresolvable(void **state)
{
    Rlist *scalars = NULL, *lists = NULL;
    MapIteratorsFromRval("scope", &scalars, &lists, (Rval) { "${snookie}", RVAL_TYPE_SCALAR });

    assert_int_equal(0, RlistLen(scalars));
    assert_int_equal(0, RlistLen(lists));
}

void test_map_iterators_from_rval_naked_scalar_var(void **state)
{
    ScopeDeleteAll();
    ScopeNew("scope");
    ScopeSetCurrent("scope");
    ScopeAddVariableHash("scope", "snookie", (Rval) { "jersey", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING, NULL, 0);

    Rlist *scalars = NULL, *lists = NULL;
    MapIteratorsFromRval("scope", &scalars, &lists, (Rval) { "${snookie}", RVAL_TYPE_SCALAR });

    assert_int_equal(1, RlistLen(scalars));
    assert_string_equal("snookie", scalars->item);
    assert_int_equal(0, RlistLen(lists));
}

void test_map_iterators_from_rval_scalar_var(void **state)
{
    ScopeDeleteAll();
    ScopeNew("scope");
    ScopeSetCurrent("scope");
    ScopeAddVariableHash("scope", "snookie", (Rval) { "jersey", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING, NULL, 0);

    Rlist *scalars = NULL, *lists = NULL;
    MapIteratorsFromRval("scope", &scalars, &lists, (Rval) { "abc${snookie} def", RVAL_TYPE_SCALAR });

    assert_int_equal(1, RlistLen(scalars));
    assert_string_equal("snookie", scalars->item);
    assert_int_equal(0, RlistLen(lists));
}

void test_map_iterators_from_rval_naked_list_var(void **state)
{
    ScopeDeleteAll();
    ScopeNew("scope");
    ScopeSetCurrent("scope");

    Rlist *list = NULL;
    RlistAppend(&list, "jersey", RVAL_TYPE_SCALAR);

    ScopeAddVariableHash("scope", "jwow", (Rval) { list, RVAL_TYPE_LIST }, DATA_TYPE_STRING_LIST, NULL, 0);

    Rlist *scalars = NULL, *lists = NULL;
    MapIteratorsFromRval("scope", &scalars, &lists, (Rval) { "${jwow}", RVAL_TYPE_SCALAR });

    assert_int_equal(0, RlistLen(scalars));
    assert_int_equal(1, RlistLen(lists));
    assert_string_equal("jwow", lists->item);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_map_iterators_from_rval_empty),
        unit_test(test_map_iterators_from_rval_literal),
        unit_test(test_map_iterators_from_rval_naked_scalar_var_nonresolvable),
        unit_test(test_map_iterators_from_rval_naked_scalar_var),
        unit_test(test_map_iterators_from_rval_scalar_var),
        unit_test(test_map_iterators_from_rval_naked_list_var),
    };

    return run_tests(tests);
}
