#include <test.h>

#include <var_expressions.h>

static void test_plain_variable_with_no_stuff_in_it(void)
{
    VarRef *ref = VarRefParse("foo");
    assert_false(ref->ns);
    assert_false(ref->scope);
    assert_string_equal("foo", ref->lval);
    assert_int_equal(0, ref->num_indices);
    assert_false(ref->indices);
    VarRefDestroy(ref);
}

static void test_scoped(void)
{
    VarRef *ref = VarRefParse("scope.lval");
    assert_false(ref->ns);
    assert_string_equal("scope", ref->scope);
    assert_string_equal("lval", ref->lval);
    assert_int_equal(0, ref->num_indices);
    assert_false(ref->indices);
    VarRefDestroy(ref);
}

static void test_full(void)
{
    VarRef *ref = VarRefParse("ns:scope.lval");
    assert_string_equal("ns", ref->ns);
    assert_string_equal("scope", ref->scope);
    assert_string_equal("lval", ref->lval);
    assert_int_equal(0, ref->num_indices);
    assert_false(ref->indices);
    VarRefDestroy(ref);
}

static void test_dotted_array(void)
{
    VarRef *ref = VarRefParse("ns:scope.lval[la.la]");
    assert_string_equal("ns", ref->ns);
    assert_string_equal("scope", ref->scope);
    assert_string_equal("lval", ref->lval);
    assert_int_equal(1, ref->num_indices);
    assert_string_equal("la.la", ref->indices[0]);
    VarRefDestroy(ref);
}

static void test_levels(void)
{
    VarRef *ref = VarRefParse("ns:scope.lval[x][y][z]");
    assert_string_equal("ns", ref->ns);
    assert_string_equal("scope", ref->scope);
    assert_string_equal("lval", ref->lval);
    assert_int_equal(3, ref->num_indices);
    assert_string_equal("x", ref->indices[0]);
    assert_string_equal("y", ref->indices[1]);
    assert_string_equal("z", ref->indices[2]);
    VarRefDestroy(ref);
}

static void test_unqualified_array(void)
{
    VarRef *ref = VarRefParse("lval[x]");
    assert_false(ref->ns);
    assert_false(ref->scope);
    assert_string_equal("lval", ref->lval);
    assert_int_equal(1, ref->num_indices);
    assert_string_equal("x", ref->indices[0]);
    VarRefDestroy(ref);
}

static void test_qualified_array(void)
{
    VarRef *ref = VarRefParse("scope.lval[x]");
    assert_false(ref->ns);
    assert_string_equal("scope", ref->scope);
    assert_string_equal("lval", ref->lval);
    assert_int_equal(1, ref->num_indices);
    assert_string_equal("x", ref->indices[0]);
    VarRefDestroy(ref);
}

static void test_nested_array(void)
{
    VarRef *ref = VarRefParse("scope.lval[$(other[x])]");
    assert_false(ref->ns);
    assert_string_equal("scope", ref->scope);
    assert_string_equal("lval", ref->lval);
    assert_int_equal(1, ref->num_indices);
    assert_string_equal("$(other[x])", ref->indices[0]);
    VarRefDestroy(ref);
}

static void test_array_with_dot_colon_in_index(void)
{
    VarRef *ref = VarRefParse("lval[x-x.x:x]");
    assert_false(ref->ns);
    assert_false(ref->scope);
    assert_string_equal("lval", ref->lval);
    assert_int_equal(1, ref->num_indices);
    assert_string_equal("x-x.x:x", ref->indices[0]);
    VarRefDestroy(ref);
}

static void test_special_scope(void)
{
    Policy *p = PolicyNew();
    Bundle *bp = PolicyAppendBundle(p, "ns", "b", "agent", NULL, NULL);

    {
        VarRef *ref = VarRefParseFromBundle("c.lval", bp);
        assert_string_equal("ns", ref->ns);
        assert_string_equal("c", ref->scope);
        assert_string_equal("lval", ref->lval);
        VarRefDestroy(ref);
    }

    {
        VarRef *ref = VarRefParseFromBundle("sys.lval", bp);
        assert_false(ref->ns);
        assert_string_equal("sys", ref->scope);
        assert_string_equal("lval", ref->lval);
        VarRefDestroy(ref);
    }
    PolicyDestroy(p);
}

static void CheckToStringQualified(const char *str, const char *expect)
{
    VarRef *ref = VarRefParse(str);
    char *out = VarRefToString(ref, true);
    assert_string_equal(expect, out);
    free(out);
    VarRefDestroy(ref);
}

static void test_to_string_qualified(void)
{
    CheckToStringQualified("ns:scope.lval[x][y]", "ns:scope.lval[x][y]");
    CheckToStringQualified("ns:scope.lval[x]", "ns:scope.lval[x]");
    CheckToStringQualified("ns:scope.lval", "ns:scope.lval");
    CheckToStringQualified("scope.lval", "default:scope.lval");
    CheckToStringQualified("lval", "lval");
}

static void test_to_string_unqualified(void)
{
    {
        VarRef *ref = VarRefParse("ns:scope.lval[x][y]");
        char *out = VarRefToString(ref, false);
        assert_string_equal("lval[x][y]", out);
        free(out);
        VarRefDestroy(ref);
    }

    {
        VarRef *ref = VarRefParse("ns:scope.lval[x]");
        char *out = VarRefToString(ref, false);
        assert_string_equal("lval[x]", out);
        free(out);
        VarRefDestroy(ref);
    }

    {
        VarRef *ref = VarRefParse("scope.lval");
        char *out = VarRefToString(ref, false);
        assert_string_equal("lval", out);
        free(out);
        VarRefDestroy(ref);
    }

    {
        VarRef *ref = VarRefParse("lval");
        char *out = VarRefToString(ref, false);
        assert_string_equal("lval", out);
        free(out);
        VarRefDestroy(ref);
    }
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_plain_variable_with_no_stuff_in_it),
        unit_test(test_scoped),
        unit_test(test_full),
        unit_test(test_dotted_array),
        unit_test(test_levels),
        unit_test(test_unqualified_array),
        unit_test(test_qualified_array),
        unit_test(test_nested_array),
        unit_test(test_array_with_dot_colon_in_index),
        unit_test(test_special_scope),
        unit_test(test_to_string_qualified),
        unit_test(test_to_string_unqualified),
    };

    return run_tests(tests);
}
