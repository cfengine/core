#include <test.h>

#include <class.h>

static void test_class_ref(void)
{
    {
        ClassRef ref = ClassRefParse("class");
        assert_true(ref.ns == NULL);
        assert_string_equal("class", ref.name);
        char *expr = ClassRefToString(ref.ns, ref.name);
        assert_string_equal("class", expr);
        free(expr);
        ClassRefDestroy(ref);
    }

    {
        ClassRef ref = ClassRefParse("default:class");
        assert_string_equal("default", ref.ns);
        assert_string_equal("class", ref.name);
        char *expr = ClassRefToString(ref.ns, ref.name);
        assert_string_equal("class", expr);
        free(expr);
        ClassRefDestroy(ref);
    }

    {
        ClassRef ref = ClassRefParse("ns:class");
        assert_string_equal("ns", ref.ns);
        assert_string_equal("class", ref.name);
        char *expr = ClassRefToString(ref.ns, ref.name);
        assert_string_equal("ns:class", expr);
        free(expr);
        ClassRefDestroy(ref);
    }
}

static void test_ns(void)
{
    {
        ClassTable *t = ClassTableNew();
        assert_false(ClassTablePut(t, "foo", "127.0.0.1", true, CONTEXT_SCOPE_BUNDLE, NULL));
        Class *cls = ClassTableGet(t, "foo", "127_0_0_1");
        assert_string_equal("foo", cls->ns);
        assert_string_equal("127_0_0_1", cls->name);
        assert_true(cls->is_soft);

        cls = ClassTableMatch(t, "foo:127_0_.*");
        assert_true(cls);
        cls = ClassTableMatch(t, "foo:127_1_.*");
        assert_false(cls);
        cls = ClassTableMatch(t, "127_0_.*");
        assert_false(cls);

        ClassTableDestroy(t);
    }
}

static void test_default_ns(void)
{
    {
        ClassTable *t = ClassTableNew();
        assert_false(ClassTablePut(t, NULL, "127.0.0.1", false, CONTEXT_SCOPE_NAMESPACE, NULL));
        Class *cls = ClassTableGet(t, NULL, "127_0_0_1");
        assert_true(cls->ns == NULL);
        cls = ClassTableGet(t, "default", "127_0_0_1");
        assert_true(cls->ns == NULL);
        assert_string_equal("127_0_0_1", cls->name);
        assert_false(cls->is_soft);

        cls = ClassTableMatch(t, "127_0_.*");
        assert_true(cls);
        cls = ClassTableMatch(t, "127_1_.*");
        assert_false(cls);

        ClassTableDestroy(t);
    }

    {
        ClassTable *t = ClassTableNew();
        assert_false(ClassTablePut(t, "default", "127.0.0.1", false, CONTEXT_SCOPE_NAMESPACE, NULL));
        Class *cls = ClassTableGet(t, NULL, "127_0_0_1");
        assert_true(cls->ns == NULL);
        cls = ClassTableGet(t, "default", "127_0_0_1");
        assert_true(cls->ns == NULL);
        assert_string_equal("127_0_0_1", cls->name);
        assert_false(cls->is_soft);
        ClassTableDestroy(t);
    }
}

static void test_put_replace(void)
{
    ClassTable *t = ClassTableNew();
    assert_false(ClassTablePut(t, NULL, "test", false, CONTEXT_SCOPE_NAMESPACE, NULL));
    Class *cls = ClassTableGet(t, NULL, "test");
    assert_true(cls);
    assert_int_equal(CONTEXT_SCOPE_NAMESPACE, cls->scope);

    assert_true(ClassTablePut(t, NULL, "test", true, CONTEXT_SCOPE_BUNDLE, NULL));
    cls = ClassTableGet(t, NULL, "test");
    assert_true(cls);
    assert_int_equal(CONTEXT_SCOPE_BUNDLE, cls->scope);

    ClassTableDestroy(t);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_default_ns),
        unit_test(test_ns),
        unit_test(test_class_ref),
        unit_test(test_put_replace),
    };

    return run_tests(tests);
}
