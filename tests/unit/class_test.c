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
        assert_true(ref.ns == NULL);
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
        assert_false(ClassTablePut(t, "foo", "127.0.0.1", true, CONTEXT_SCOPE_BUNDLE));
        Class *cls = ClassTableGet(t, "foo", "127_0_0_1");
        assert_string_equal("foo", cls->ns);
        assert_string_equal("127_0_0_1", cls->name);
        assert_true(cls->is_soft);
        ClassTableDestroy(t);
    }
}

static void test_default_ns(void)
{
    {
        ClassTable *t = ClassTableNew();
        assert_false(ClassTablePut(t, NULL, "127.0.0.1", false, CONTEXT_SCOPE_NAMESPACE));
        Class *cls = ClassTableGet(t, NULL, "127_0_0_1");
        assert_true(cls->ns == NULL);
        cls = ClassTableGet(t, "default", "127_0_0_1");
        assert_true(cls->ns == NULL);
        assert_string_equal("127_0_0_1", cls->name);
        assert_false(cls->is_soft);
        ClassTableDestroy(t);
    }

    {
        ClassTable *t = ClassTableNew();
        assert_false(ClassTablePut(t, "default", "127.0.0.1", false, CONTEXT_SCOPE_NAMESPACE));
        Class *cls = ClassTableGet(t, NULL, "127_0_0_1");
        assert_true(cls->ns == NULL);
        cls = ClassTableGet(t, "default", "127_0_0_1");
        assert_true(cls->ns == NULL);
        assert_string_equal("127_0_0_1", cls->name);
        assert_false(cls->is_soft);
        ClassTableDestroy(t);
    }
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_default_ns),
        unit_test(test_ns),
        unit_test(test_class_ref),
    };

    return run_tests(tests);
}
