#include "test.h"

#include "var_expressions.h"

static void test_plain_variable_with_no_stuff_in_it(void **state)
{
    VarRef ref = VarRefParse("foo");
    assert_false(ref.ns);
    assert_false(ref.scope);
    assert_string_equal("foo", ref.lval);
    assert_int_equal(0, ref.num_indices);
    assert_false(ref.indices);
    VarRefDestroy(ref);
}

static void test_namespaced(void **state)
{
    VarRef ref = VarRefParse("ns:lval");
    assert_string_equal("ns", ref.ns);
    assert_false(ref.scope);
    assert_string_equal("lval", ref.lval);
    assert_int_equal(0, ref.num_indices);
    assert_false(ref.indices);
    VarRefDestroy(ref);
}

static void test_scoped(void **state)
{
    VarRef ref = VarRefParse("scope.lval");
    assert_false(ref.ns);
    assert_string_equal("scope", ref.scope);
    assert_string_equal("lval", ref.lval);
    assert_int_equal(0, ref.num_indices);
    assert_false(ref.indices);
    VarRefDestroy(ref);
}

static void test_full(void **state)
{
    VarRef ref = VarRefParse("ns:scope.lval");
    assert_string_equal("ns", ref.ns);
    assert_string_equal("scope", ref.scope);
    assert_string_equal("lval", ref.lval);
    assert_int_equal(0, ref.num_indices);
    assert_false(ref.indices);
    VarRefDestroy(ref);
}

static void test_dotted_array(void **state)
{
    VarRef ref = VarRefParse("ns:scope.lval[la.la]");
    assert_string_equal("ns", ref.ns);
    assert_string_equal("scope", ref.scope);
    assert_string_equal("lval", ref.lval);
    assert_int_equal(1, ref.num_indices);
    assert_string_equal("la.la", ref.indices[0]);
    VarRefDestroy(ref);
}

static void test_levels(void **state)
{
    VarRef ref = VarRefParse("ns:scope.lval[x][y][z]");
    assert_string_equal("ns", ref.ns);
    assert_string_equal("scope", ref.scope);
    assert_string_equal("lval", ref.lval);
    assert_int_equal(3, ref.num_indices);
    assert_string_equal("x", ref.indices[0]);
    assert_string_equal("y", ref.indices[1]);
    assert_string_equal("z", ref.indices[2]);
    VarRefDestroy(ref);
}

static void test_unqualified_array(void **state)
{
    VarRef ref = VarRefParse("lval[x]");
    assert_false(ref.ns);
    assert_false(ref.scope);
    assert_string_equal("lval", ref.lval);
    assert_int_equal(1, ref.num_indices);
    assert_string_equal("x", ref.indices[0]);
    VarRefDestroy(ref);
}

static void test_qualified_array(void **state)
{
    VarRef ref = VarRefParse("scope.lval[x]");
    assert_false(ref.ns);
    assert_string_equal("scope", ref.scope);
    assert_string_equal("lval", ref.lval);
    assert_int_equal(1, ref.num_indices);
    assert_string_equal("x", ref.indices[0]);
    VarRefDestroy(ref);
}

static void CheckToString(const char *str)
{
    VarRef ref = VarRefParse(str);
    char *out = VarRefToString(ref);
    assert_string_equal(str, out);
    free(out);
    VarRefDestroy(ref);
}

static void test_to_string(void **state)
{
    CheckToString("ns:scope.lval[x][y]");
    CheckToString("ns:scope.lval[x]");
    CheckToString("ns:scope.lval");
    CheckToString("ns:lval");
    CheckToString("scope.lval");
    CheckToString("lval");
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_plain_variable_with_no_stuff_in_it),
        unit_test(test_namespaced),
        unit_test(test_scoped),
        unit_test(test_full),
        unit_test(test_dotted_array),
        unit_test(test_levels),
        unit_test(test_unqualified_array),
        unit_test(test_qualified_array),
        unit_test(test_to_string)
    };

    return run_tests(tests);
}
