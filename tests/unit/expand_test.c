#include "test.h"

#include "expand.h"
#include "rlist.h"
#include "scope.h"

void test_map_iterators_from_rval_empty(void **state)
{
    Rlist *lists = NULL;
    MapIteratorsFromRval("none", &lists, (Rval) { "", RVAL_TYPE_SCALAR });

    assert_int_equal(0, RlistLen(lists));
}

void test_map_iterators_from_rval_literal(void **state)
{
    Rlist *lists = NULL;
    MapIteratorsFromRval("none", &lists, (Rval) { "snookie", RVAL_TYPE_SCALAR });

    assert_int_equal(0, RlistLen(lists));
}

void test_map_iterators_from_rval_naked_list_var(void **state)
{
    ScopeDeleteAll();
    ScopeSetCurrent("scope");

    Rlist *list = NULL;
    RlistAppend(&list, "jersey", RVAL_TYPE_SCALAR);

    VarRef lval = VarRefParse("scope.jwow");

    ScopeAddVariableHash(lval, (Rval) { list, RVAL_TYPE_LIST }, DATA_TYPE_STRING_LIST, NULL, 0);

    Rlist *lists = NULL;
    MapIteratorsFromRval("scope", &lists, (Rval) { "${jwow}", RVAL_TYPE_SCALAR });

    assert_int_equal(1, RlistLen(lists));
    assert_string_equal("jwow", lists->item);

    VarRefDestroy(lval);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_map_iterators_from_rval_empty),
        unit_test(test_map_iterators_from_rval_literal),
        unit_test(test_map_iterators_from_rval_naked_list_var),
    };

    return run_tests(tests);
}
