#include <test.h>

#include <variable.h>
#include <rlist.h>

static bool PutVar(VariableTable *table, char *var_str)
{
    VarRef *ref = VarRefParse(var_str);
    Rval rval = (Rval) { var_str, RVAL_TYPE_SCALAR };
    bool ret = VariableTablePut(table, ref, &rval, CF_DATA_TYPE_STRING, NULL, NULL);
    VarRefDestroy(ref);
    return ret;
}

static VariableTable *ReferenceTable(void)
{
    VariableTable *t = VariableTableNew();

    assert_false(PutVar(t, "scope1.lval1"));
    assert_false(PutVar(t, "scope1.lval2"));
    assert_false(PutVar(t, "scope2.lval1"));
    {
        VarRef *ref = VarRefParse("scope1.array[one]");
        Rval rval = (Rval) { "scope1.array[one]", RVAL_TYPE_SCALAR };
        assert_false(VariableTablePut(t, ref, &rval, CF_DATA_TYPE_STRING, NULL, NULL));
        VarRefDestroy(ref);
    }
    {
        VarRef *ref = VarRefParse("scope1.array[two]");
        Rval rval = (Rval) { "scope1.array[two]", RVAL_TYPE_SCALAR };
        assert_false(VariableTablePut(t, ref, &rval, CF_DATA_TYPE_STRING, NULL, NULL));
        VarRefDestroy(ref);
    }
    {
        VarRef *ref = VarRefParse("scope1.array[two][three]");
        Rval rval = (Rval) { "scope1.array[two][three]", RVAL_TYPE_SCALAR };
        assert_false(VariableTablePut(t, ref, &rval, CF_DATA_TYPE_STRING, NULL, NULL));
        VarRefDestroy(ref);
    }
    {
        VarRef *ref = VarRefParse("scope1.array[two][four]");
        Rval rval = (Rval) { "scope1.array[two][four]", RVAL_TYPE_SCALAR };
        assert_false(VariableTablePut(t, ref, &rval, CF_DATA_TYPE_STRING, NULL, NULL));
        VarRefDestroy(ref);
    }

    {
        VarRef *ref = VarRefParse("scope3.array[te][st]");
        Rval rval = (Rval) { "scope3.array[te][st]", RVAL_TYPE_SCALAR };
        assert_false(VariableTablePut(t, ref, &rval, CF_DATA_TYPE_STRING, NULL, NULL));
        VarRefDestroy(ref);
    }
    {
        VarRef *ref = VarRefParse("scope3.array[t][e][s][t]");
        Rval rval = (Rval) { "scope3.array[t][e][s][t]", RVAL_TYPE_SCALAR };
        assert_false(VariableTablePut(t, ref, &rval, CF_DATA_TYPE_STRING, NULL, NULL));
        VarRefDestroy(ref);
    }

    assert_false(PutVar(t, "ns1:scope1.lval1"));
    assert_false(PutVar(t, "ns1:scope1.lval2"));
    assert_false(PutVar(t, "ns1:scope2.lval1"));

    return t;
}

static void TestGet(VariableTable *t, const char *ref_str)
{
    VarRef *ref = VarRefParse(ref_str);
    Variable *v = VariableTableGet(t, ref);
    assert_true(v != NULL);
    assert_int_equal(0, VarRefCompare(ref, v->ref));
    assert_string_equal(ref_str, RvalScalarValue(v->rval));
    VarRefDestroy(ref);
}

static void test_get_in_default_namespace(void)
{
    VariableTable *t = ReferenceTable();

    TestGet(t, "scope1.lval1");
    TestGet(t, "scope1.lval2");
    TestGet(t, "scope2.lval1");

    VariableTableDestroy(t);
}

// Redmine 6674
static void test_multi_index_array_conflation(void)
{
    VariableTable *t = ReferenceTable();

    TestGet(t, "scope3.array[te][st]");
    TestGet(t, "scope3.array[t][e][s][t]");

    VariableTableDestroy(t);
}

static void test_get_different_namespaces(void)
{
    VariableTable *t = ReferenceTable();

    TestGet(t, "scope1.lval1");
    TestGet(t, "ns1:scope1.lval1");

    VariableTableDestroy(t);
}

static void test_get_indices(void)
{
    VariableTable *t = ReferenceTable();

    TestGet(t, "scope1.array[one]");
    TestGet(t, "scope1.array[two]");

    VariableTableDestroy(t);
}

static void test_replace(void)
{
    VariableTable *t = ReferenceTable();

    VarRef *ref = VarRefParse("scope1.lval1");
    TestGet(t, "scope1.lval1");

    Rval rval = (Rval) { "foo", RVAL_TYPE_SCALAR };
    assert_true(VariableTablePut(t, ref, &rval, CF_DATA_TYPE_STRING, NULL, NULL));

    Variable *v = VariableTableGet(t, ref);
    assert_true(v != NULL);
    assert_string_equal("foo", RvalScalarValue(v->rval));

    VarRefDestroy(ref);

    VariableTableDestroy(t);
}

static void test_remove(void)
{
    VariableTable *t = ReferenceTable();

    {
        VarRef *ref = VarRefParse("scope1.array[one]");
        assert_true(VariableTableRemove(t, ref));
        assert_true(VariableTableGet(t, ref) == NULL);
        assert_false(VariableTableRemove(t, ref));

        assert_int_equal(11, VariableTableCount(t, NULL, NULL, NULL));

        VarRefDestroy(ref);
    }

    {
        VarRef *ref = VarRefParse("ns1:scope1.lval1");
        assert_true(VariableTableRemove(t, ref));
        assert_true(VariableTableGet(t, ref) == NULL);
        assert_false(VariableTableRemove(t, ref));

        assert_int_equal(10, VariableTableCount(t, NULL, NULL, NULL));

        VarRefDestroy(ref);
    }

    VariableTableDestroy(t);
}

static void test_clear(void)
{
    {
        VariableTable *t = ReferenceTable();
        assert_false(VariableTableClear(t, "xxx", NULL, NULL));
        assert_false(VariableTableClear(t, NULL, "xxx", NULL));
        assert_false(VariableTableClear(t, NULL, NULL, "xxx"));
        assert_int_equal(12, VariableTableCount(t, NULL, NULL, NULL));
        VariableTableDestroy(t);
    }

    {
        VariableTable *t = ReferenceTable();
        assert_true(VariableTableClear(t, NULL, NULL, NULL));
        assert_int_equal(0, VariableTableCount(t, NULL, NULL, NULL));
        VariableTableDestroy(t);
    }

    {
        VariableTable *t = ReferenceTable();
        assert_true(VariableTableClear(t, "default", NULL, NULL));
        assert_int_equal(3, VariableTableCount(t, NULL, NULL, NULL));
        VariableTableDestroy(t);
    }

    {
        VariableTable *t = ReferenceTable();
        assert_true(VariableTableClear(t, "default", "scope1", NULL));
        assert_int_equal(6, VariableTableCount(t, NULL, NULL, NULL));
        VariableTableDestroy(t);
    }

    {
        VariableTable *t = ReferenceTable();
        assert_true(VariableTableClear(t, "default", NULL, "array"));
        assert_int_equal(6, VariableTableCount(t, NULL, NULL, NULL));
        VariableTableDestroy(t);
    }

    {
        VariableTable *t = ReferenceTable();
        assert_true(VariableTableClear(t, "ns1", NULL, NULL));
        assert_int_equal(9, VariableTableCount(t, NULL, NULL, NULL));
        VariableTableDestroy(t);
    }

    {
        VariableTable *t = ReferenceTable();
        assert_true(VariableTableClear(t, "ns1", "scope2", NULL));
        assert_int_equal(11, VariableTableCount(t, NULL, NULL, NULL));
        VariableTableDestroy(t);
    }

    {
        VariableTable *t = ReferenceTable();
        assert_true(VariableTableClear(t, "default", "scope1", "lval1"));
        assert_int_equal(11, VariableTableCount(t, NULL, NULL, NULL));
        VariableTableDestroy(t);
    }

    {
        VariableTable *t = ReferenceTable();
        assert_true(VariableTableClear(t, "default", "scope1", "lval1"));
        assert_int_equal(11, VariableTableCount(t, NULL, NULL, NULL));
        VariableTableDestroy(t);
    }
}

static void test_counting(void)
{
    VariableTable *t = ReferenceTable();

    assert_int_equal(12, VariableTableCount(t, NULL, NULL, NULL));
    assert_int_equal(9, VariableTableCount(t, "default", NULL, NULL));
    assert_int_equal(8, VariableTableCount(t, NULL, "scope1", NULL));
    assert_int_equal(6, VariableTableCount(t, "default", "scope1", NULL));
    assert_int_equal(4, VariableTableCount(t, NULL, NULL, "lval1"));
    assert_int_equal(3, VariableTableCount(t, "ns1", NULL, NULL));
    assert_int_equal(2, VariableTableCount(t, "ns1", "scope1", NULL));
    assert_int_equal(6, VariableTableCount(t, NULL, NULL, "array"));
    assert_int_equal(1, VariableTableCount(t, "default", "scope1", "lval1"));

    VariableTableDestroy(t);
}

static void test_iterate_indices(void)
{
    VariableTable *t = ReferenceTable();

    {
        VarRef *ref = VarRefParse("default:scope1.array");
        VariableTableIterator *iter = VariableTableIteratorNewFromVarRef(t, ref);

        unsigned short number_of_entries = 0;
        while (VariableTableIteratorNext(iter))
        {
            number_of_entries++;
        }
        assert_int_equal(4, number_of_entries);

        VariableTableIteratorDestroy(iter);
        VarRefDestroy(ref);
    }

    {
        VarRef *ref = VarRefParse("default:scope1.array[two]");
        VariableTableIterator *iter = VariableTableIteratorNewFromVarRef(t, ref);

        unsigned short number_of_entries = 0;
        while (VariableTableIteratorNext(iter))
        {
            number_of_entries++;
        }

        assert_int_equal(3, number_of_entries);

        VariableTableIteratorDestroy(iter);
        VarRefDestroy(ref);
    }

    VariableTableDestroy(t);
}

// Below test relies on the ordering items in RB tree which is strongly
// related to the hash function used.
/* No more relevant, RBTree has been replaced with Map. */
#if 0
static void test_iterate_indices_ordering_related(void)
{
    VariableTable *t = ReferenceTable();

    {
        VarRef *ref = VarRefParse("default:scope1.array");
        VariableTableIterator *iter = VariableTableIteratorNewFromVarRef(t, ref);

        Variable *v = VariableTableIteratorNext(iter);
        assert_true(v != NULL);
        assert_int_equal(1, v->ref->num_indices);
        assert_string_equal("two", v->ref->indices[0]);

        v = VariableTableIteratorNext(iter);
        assert_true(v != NULL);
        assert_int_equal(2, v->ref->num_indices);
        assert_string_equal("two", v->ref->indices[0]);
        assert_string_equal("three", v->ref->indices[1]);

        v = VariableTableIteratorNext(iter);
        assert_true(v != NULL);
        assert_int_equal(2, v->ref->num_indices);
        assert_string_equal("two", v->ref->indices[0]);
        assert_string_equal("four", v->ref->indices[1]);

        v = VariableTableIteratorNext(iter);
        assert_true(v != NULL);
        assert_int_equal(1, v->ref->num_indices);
        assert_string_equal("one", v->ref->indices[0]);

        assert_false(VariableTableIteratorNext(iter) != NULL);

        VariableTableIteratorDestroy(iter);
        VarRefDestroy(ref);
    }

    {
        VarRef *ref = VarRefParse("default:scope1.array[two]");
        VariableTableIterator *iter = VariableTableIteratorNewFromVarRef(t, ref);

        Variable *v = VariableTableIteratorNext(iter);
        assert_true(v != NULL);
        assert_int_equal(1, v->ref->num_indices);
        assert_string_equal("two", v->ref->indices[0]);

        v = VariableTableIteratorNext(iter);
        assert_true(v != NULL);
        assert_int_equal(2, v->ref->num_indices);
        assert_string_equal("two", v->ref->indices[0]);
        assert_string_equal("three", v->ref->indices[1]);

        v = VariableTableIteratorNext(iter);
        assert_true(v != NULL);
        assert_int_equal(2, v->ref->num_indices);
        assert_string_equal("two", v->ref->indices[0]);
        assert_string_equal("four", v->ref->indices[1]);

        assert_false(VariableTableIteratorNext(iter) != NULL);

        VariableTableIteratorDestroy(iter);
        VarRefDestroy(ref);
    }

    VariableTableDestroy(t);
}
#endif

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_get_in_default_namespace),
        unit_test(test_get_different_namespaces),
        unit_test(test_get_indices),
//        unit_test(test_iterate_indices_ordering_related),
        unit_test(test_multi_index_array_conflation),
        unit_test(test_replace),
        unit_test(test_remove),
        unit_test(test_clear),
        unit_test(test_counting),
        unit_test(test_iterate_indices),
    };

    return run_tests(tests);
}
