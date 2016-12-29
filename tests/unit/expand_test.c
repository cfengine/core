#include <test.h>

#include <expand.h>
#include <rlist.h>
#include <scope.h>
#include <eval_context.h>
#include <vars.h>

static void test_extract_scalar_prefix()
{
    Buffer *b = BufferNew();
    assert_int_equal(sizeof("hello ") - 1, ExtractScalarPrefix(b, "hello $(world) xy", sizeof("hello $(world) xy") -1));
    assert_string_equal("hello ", BufferData(b));

    BufferClear(b);
    assert_int_equal(sizeof("hello (world) xy") -1, ExtractScalarPrefix(b, "hello (world) xy", sizeof("hello (world) xy") -1));
    assert_string_equal("hello (world) xy", BufferData(b));

    BufferClear(b);
    assert_int_equal(sizeof("hello$)") -1, ExtractScalarPrefix(b, "hello$)$(world)xy", sizeof("hello$)$(world)xy") -1));
    assert_string_equal("hello$)", BufferData(b));

    BufferClear(b);
    assert_int_equal(0, ExtractScalarPrefix(b, "", 0));
    assert_string_equal("", BufferData(b));

    BufferDestroy(b);
}

static void test_extract_reference_(const char *scalar, bool expect_success, const char *outer, const char *inner)
{
    Buffer *b = BufferNew();
    size_t len = strlen(scalar);

    bool success = ExtractScalarReference(b, scalar, len, false);
    assert_true(success == expect_success);
    assert_string_equal(outer, BufferData(b));

    BufferClear(b);
    success = ExtractScalarReference(b, scalar, len, true);
    assert_true(success == expect_success);
    assert_string_equal(inner, BufferData(b));

    BufferDestroy(b);
}

static void test_extract_reference(void)
{
    test_extract_reference_("${stuff}", true, "${stuff}", "stuff");
    test_extract_reference_("$(stuff)", true, "$(stuff)", "stuff");
    test_extract_reference_("abc $def ${x} y", true, "${x}", "x");
    test_extract_reference_("${stuff)", false, "", "");
    test_extract_reference_("abc $def", false, "", "");
    test_extract_reference_("stuff", false, "", "");
    test_extract_reference_("", false, "", "");
    test_extract_reference_("abc $xa ", false, "", "");
    test_extract_reference_("${}", false, "", "");
    test_extract_reference_("x$()a", false, "", "");

    test_extract_reference_("$($(x))", true, "$($(x))", "$(x)");
    test_extract_reference_("$(x${$(y)})", true, "$(x${$(y)})", "x${$(y)}");
    test_extract_reference_("$(x${$(y)}) $(y) ${x${z}}", true, "$(x${$(y)})", "x${$(y)}");
}

#if 0
static void test_map_iterators_from_rval_empty(void **state)
{
    EvalContext *ctx = *state;

    Policy *p = PolicyNew();
    Bundle *bp = PolicyAppendBundle(p, "default", "none", "agent", NULL, NULL);

    Rlist *lists = NULL;
    Rlist *scalars = NULL;
    Rlist *containers =  NULL;
    MapIteratorsFromRval(ctx, bp, (Rval) { "", RVAL_TYPE_SCALAR }, &scalars, &lists, &containers);

    assert_int_equal(0, RlistLen(lists));
    assert_int_equal(0, RlistLen(scalars));
    assert_int_equal(0, RlistLen(containers));

    PolicyDestroy(p);
}

static void test_map_iterators_from_rval_literal(void **state)
{
    EvalContext *ctx = *state;
    Policy *p = PolicyNew();
    Bundle *bp = PolicyAppendBundle(p, "default", "none", "agent", NULL, NULL);

    Rlist *lists = NULL;
    Rlist *scalars = NULL;
    Rlist *containers = NULL;
    MapIteratorsFromRval(ctx, bp, (Rval) { "snookie", RVAL_TYPE_SCALAR }, &scalars, &lists, &containers);

    assert_int_equal(0, RlistLen(lists));
    assert_int_equal(0, RlistLen(scalars));
    assert_int_equal(0, RlistLen(containers));

    PolicyDestroy(p);
}

static void test_map_iterators_from_rval_naked_list_var(void **state)
{
    EvalContext *ctx = *state;
    Policy *p = PolicyNew();
    Bundle *bp = PolicyAppendBundle(p, "default", "scope", "agent", NULL, NULL);

    {
        Rlist *list = NULL;
        RlistAppend(&list, "jersey", RVAL_TYPE_SCALAR);
        VarRef *lval = VarRefParse("scope.jwow");

        EvalContextVariablePut(ctx, lval, list, CF_DATA_TYPE_STRING_LIST, NULL);

        VarRefDestroy(lval);
        RlistDestroy(list);
    }

    EvalContextStackPushBundleFrame(ctx, bp, NULL, false);

    {
        Rlist *lists = NULL;
        Rlist *scalars = NULL;
        Rlist *containers = NULL;
        MapIteratorsFromRval(ctx, bp, (Rval) { "${jwow}", RVAL_TYPE_SCALAR }, &scalars, &lists, &containers);

        assert_int_equal(1, RlistLen(lists));
        assert_string_equal("jwow", RlistScalarValue(lists));
        assert_int_equal(0, RlistLen(scalars));
        assert_int_equal(0, RlistLen(containers));

        RlistDestroy(lists);
    }

    {
        Rlist *lists = NULL;
        Rlist *scalars = NULL;
        Rlist *containers = NULL;
        char *str = xstrdup("${scope.jwow}");
        MapIteratorsFromRval(ctx, bp, (Rval) { str, RVAL_TYPE_SCALAR }, &scalars, &lists, &containers);

        assert_string_equal("${scope#jwow}", str);
        free(str);

        assert_int_equal(1, RlistLen(lists));
        assert_string_equal("scope#jwow", RlistScalarValue(lists));
        assert_int_equal(0, RlistLen(scalars));
        assert_int_equal(0, RlistLen(containers));

        RlistDestroy(lists);
    }

    {
        Rlist *lists = NULL;
        Rlist *scalars = NULL;
        Rlist *containers = NULL;
        char *str = xstrdup("${default:scope.jwow}");
        MapIteratorsFromRval(ctx, bp, (Rval) { str, RVAL_TYPE_SCALAR }, &scalars, &lists, &containers);

        assert_string_equal("${default*scope#jwow}", str);
        free(str);

        assert_int_equal(1, RlistLen(lists));
        assert_string_equal("default*scope#jwow", RlistScalarValue(lists));
        assert_int_equal(0, RlistLen(scalars));
        assert_int_equal(0, RlistLen(containers));

        RlistDestroy(lists);
    }

    EvalContextStackPopFrame(ctx);
    PolicyDestroy(p);
}

static void test_map_iterators_from_rval_naked_list_var_namespace(void **state)
{
    EvalContext *ctx = *state;
    Policy *p = PolicyNew();
    Bundle *bp = PolicyAppendBundle(p, "ns", "scope", "agent", NULL, NULL);

    {
        Rlist *list = NULL;
        RlistAppend(&list, "jersey", RVAL_TYPE_SCALAR);
        VarRef *lval = VarRefParse("ns:scope.jwow");

        EvalContextVariablePut(ctx, lval, list, CF_DATA_TYPE_STRING_LIST, NULL);

        VarRefDestroy(lval);
        RlistDestroy(list);
    }

    EvalContextStackPushBundleFrame(ctx, bp, NULL, false);

    {
        Rlist *lists = NULL;
        Rlist *scalars = NULL;
        Rlist *containers = NULL;
        MapIteratorsFromRval(ctx, bp, (Rval) { "${jwow}", RVAL_TYPE_SCALAR }, &scalars, &lists, &containers);

        assert_int_equal(1, RlistLen(lists));
        assert_string_equal("jwow", RlistScalarValue(lists));
        assert_int_equal(0, RlistLen(scalars));
        assert_int_equal(0, RlistLen(containers));

        RlistDestroy(lists);
    }

    {
        Rlist *lists = NULL;
        Rlist *scalars = NULL;
        Rlist *containers = NULL;
        char *str = xstrdup("${scope.jwow}");
        MapIteratorsFromRval(ctx, bp, (Rval) { str, RVAL_TYPE_SCALAR }, &scalars, &lists, &containers);

        assert_string_equal("${scope#jwow}", str);
        free(str);

        assert_int_equal(1, RlistLen(lists));
        assert_string_equal("scope#jwow", RlistScalarValue(lists));
        assert_int_equal(0, RlistLen(scalars));
        assert_int_equal(0, RlistLen(containers));

        RlistDestroy(lists);
    }

    {
        Rlist *lists = NULL;
        Rlist *scalars = NULL;
        Rlist *containers = NULL;
        char *str = xstrdup("${ns:scope.jwow}");
        MapIteratorsFromRval(ctx, bp, (Rval) { str, RVAL_TYPE_SCALAR }, &scalars, &lists, &containers);

        assert_string_equal("${ns*scope#jwow}", str);
        free(str);

        assert_int_equal(1, RlistLen(lists));
        assert_string_equal("ns*scope#jwow", RlistScalarValue(lists));
        assert_int_equal(0, RlistLen(scalars));
        assert_int_equal(0, RlistLen(containers));

        RlistDestroy(lists);
    }

    EvalContextStackPopFrame(ctx);
    PolicyDestroy(p);
}
#endif
static void test_expand_scalar_two_scalars_concat(void **state)
{
    EvalContext *ctx = *state;
    {
        VarRef *lval = VarRefParse("default:bundle.one");
        EvalContextVariablePut(ctx, lval, "first", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }
    {
        VarRef *lval = VarRefParse("default:bundle.two");
        EvalContextVariablePut(ctx, lval, "second", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }

    Buffer *res = BufferNew();
    ExpandScalar(ctx, "default", "bundle", "a $(one) b $(two)c", res);

    assert_string_equal("a first b secondc", BufferData(res));
    BufferDestroy(res);
}

static void test_expand_scalar_two_scalars_nested(void **state)
{
    EvalContext *ctx = *state;
    {
        VarRef *lval = VarRefParse("default:bundle.one");
        EvalContextVariablePut(ctx, lval, "first", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }
    {
        VarRef *lval = VarRefParse("default:bundle.two");
        EvalContextVariablePut(ctx, lval, "one", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }

    Buffer *res = BufferNew();
    ExpandScalar(ctx, "default", "bundle", "a $($(two))b", res);

    assert_string_equal("a firstb", BufferData(res));
    BufferDestroy(res);
}

static void test_expand_scalar_array_concat(void **state)
{
    EvalContext *ctx = *state;
    {
        VarRef *lval = VarRefParse("default:bundle.foo[one]");
        EvalContextVariablePut(ctx, lval, "first", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }
    {
        VarRef *lval = VarRefParse("default:bundle.foo[two]");
        EvalContextVariablePut(ctx, lval, "second", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }

    Buffer *res = BufferNew();
    ExpandScalar(ctx, "default", "bundle", "a $(foo[one]) b $(foo[two])c", res);

    assert_string_equal("a first b secondc", BufferData(res));
    BufferDestroy(res);
}

static void test_expand_scalar_array_with_scalar_arg(void **state)
{
    EvalContext *ctx = *state;
    {
        VarRef *lval = VarRefParse("default:bundle.foo[one]");
        EvalContextVariablePut(ctx, lval, "first", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }
    {
        VarRef *lval = VarRefParse("default:bundle.bar");
        EvalContextVariablePut(ctx, lval, "one", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }

    Buffer *res = BufferNew();
    ExpandScalar(ctx, "default", "bundle", "a$(foo[$(bar)])b", res);

    assert_string_equal("afirstb", BufferData(res));
    BufferDestroy(res);
}

static void test_expand_scalar_undefined(void **state)
{
    EvalContext *ctx = *state;

    Buffer *res = BufferNew();
    ExpandScalar(ctx, "default", "bundle", "a$(undefined)b", res);

    assert_string_equal("a$(undefined)b", BufferData(res));
    BufferDestroy(res);
}

static void test_expand_scalar_nested_inner_undefined(void **state)
{
    EvalContext *ctx = *state;
    {
        VarRef *lval = VarRefParse("default:bundle.foo[one]");
        EvalContextVariablePut(ctx, lval, "first", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }

    Buffer *res = BufferNew();
    ExpandScalar(ctx, "default", "bundle", "a$(foo[$(undefined)])b", res);

    assert_string_equal("a$(foo[$(undefined)])b", BufferData(res));
    BufferDestroy(res);
}

static void test_expand_list_nested(void **state)
{
    EvalContext *ctx = *state;
    {
        VarRef *lval = VarRefParse("default:bundle.i");
        EvalContextVariablePut(ctx, lval, "one", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }
    {
        VarRef *lval = VarRefParse("default:bundle.inner[one]");
        Rlist *list = NULL;
        RlistAppendScalar(&list, "foo");
        EvalContextVariablePut(ctx, lval, list, CF_DATA_TYPE_STRING_LIST, NULL);
        RlistDestroy(list);
        VarRefDestroy(lval);
    }

    Rlist *outer = NULL;
    RlistAppendScalar(&outer, "@{inner[$(i)]}");

    Rlist *expanded = ExpandList(ctx, "default", "bundle", outer, true);

    assert_int_equal(1, RlistLen(expanded));
    assert_string_equal("foo", RlistScalarValue(expanded));

    RlistDestroy(outer);
    RlistDestroy(expanded);
}

static PromiseResult actuator_expand_promise_array_with_scalar_arg(
    ARG_UNUSED EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert_string_equal("first", pp->promiser);
    return PROMISE_RESULT_NOOP;
}

static void test_expand_promise_array_with_scalar_arg(void **state)
{
    EvalContext *ctx = *state;
    {
        VarRef *lval = VarRefParse("default:bundle.foo[one]");
        EvalContextVariablePut(ctx, lval, "first", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }
    {
        VarRef *lval = VarRefParse("default:bundle.bar");
        EvalContextVariablePut(ctx, lval, "one", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }

    Policy *policy = PolicyNew();
    Bundle *bundle = PolicyAppendBundle(policy, NamespaceDefault(), "bundle", "agent", NULL, NULL);
    PromiseType *promise_type = BundleAppendPromiseType(bundle, "dummy");
    Promise *promise = PromiseTypeAppendPromise(promise_type, "$(foo[$(bar)])", RvalNULL(), "any", NULL);

    EvalContextStackPushBundleFrame(ctx, bundle, NULL, false);
    EvalContextStackPushPromiseTypeFrame(ctx, promise_type);
    ExpandPromise(ctx, promise, actuator_expand_promise_array_with_scalar_arg, NULL);
    EvalContextStackPopFrame(ctx);
    EvalContextStackPopFrame(ctx);

    PolicyDestroy(policy);
}


static int actuator_state = 0;

static PromiseResult actuator_expand_promise_slist(
    ARG_UNUSED EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    if (strcmp("a", pp->promiser) == 0)
    {
        assert_int_equal(0, actuator_state);
        actuator_state++;
    }
    else if (strcmp("b", pp->promiser) == 0)
    {
        assert_int_equal(1, actuator_state);
        actuator_state++;
    }
    else
    {
        fail();
    }
    return PROMISE_RESULT_NOOP;
}

static void test_expand_promise_slist(void **state)
{
    actuator_state = 0;

    EvalContext *ctx = *state;
    {
        VarRef *lval = VarRefParse("default:bundle.foo");
        Rlist *list = NULL;
        RlistAppendScalar(&list, "a");
        RlistAppendScalar(&list, "b");

        EvalContextVariablePut(ctx, lval, list, CF_DATA_TYPE_STRING_LIST, NULL);

        RlistDestroy(list);
        VarRefDestroy(lval);
    }


    Policy *policy = PolicyNew();
    Bundle *bundle = PolicyAppendBundle(policy, NamespaceDefault(), "bundle", "agent", NULL, NULL);
    PromiseType *promise_type = BundleAppendPromiseType(bundle, "dummy");
    Promise *promise = PromiseTypeAppendPromise(promise_type, "$(foo)", RvalNULL(), "any", NULL);

    EvalContextStackPushBundleFrame(ctx, bundle, NULL, false);
    EvalContextStackPushPromiseTypeFrame(ctx, promise_type);
    ExpandPromise(ctx, promise, actuator_expand_promise_slist, NULL);
    EvalContextStackPopFrame(ctx);
    EvalContextStackPopFrame(ctx);

    assert_int_equal(2, actuator_state);

    PolicyDestroy(policy);
}


static PromiseResult actuator_expand_promise_array_with_slist_arg(
    ARG_UNUSED EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    if (strcmp("first", pp->promiser) == 0)
    {
        assert_int_equal(0, actuator_state);
        actuator_state++;
    }
    else if (strcmp("second", pp->promiser) == 0)
    {
        assert_int_equal(1, actuator_state);
        actuator_state++;
    }
    else
    {
        fprintf(stderr, "Got promiser: '%s'\n", pp->promiser);
        fail();
    }
    return PROMISE_RESULT_NOOP;
}

static void test_expand_promise_array_with_slist_arg(void **state)
{
    actuator_state = 0;

    EvalContext *ctx = *state;
    {
        VarRef *lval = VarRefParse("default:bundle.keys");
        Rlist *list = NULL;
        RlistAppendScalar(&list, "one");
        RlistAppendScalar(&list, "two");

        EvalContextVariablePut(ctx, lval, list, CF_DATA_TYPE_STRING_LIST, NULL);

        RlistDestroy(list);
        VarRefDestroy(lval);
    }

    {
        VarRef *lval = VarRefParse("default:bundle.arr[one]");
        EvalContextVariablePut(ctx, lval, "first", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }

    {
        VarRef *lval = VarRefParse("default:bundle.arr[two]");
        EvalContextVariablePut(ctx, lval, "second", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }


    Policy *policy = PolicyNew();
    Bundle *bundle = PolicyAppendBundle(policy, NamespaceDefault(), "bundle", "agent", NULL, NULL);
    PromiseType *promise_type = BundleAppendPromiseType(bundle, "dummy");
    Promise *promise = PromiseTypeAppendPromise(promise_type, "$(arr[$(keys)])", RvalNULL(), "any", NULL);

    EvalContextStackPushBundleFrame(ctx, bundle, NULL, false);
    EvalContextStackPushPromiseTypeFrame(ctx, promise_type);
    ExpandPromise(ctx, promise, actuator_expand_promise_array_with_slist_arg, NULL);
    EvalContextStackPopFrame(ctx);
    EvalContextStackPopFrame(ctx);

    assert_int_equal(2, actuator_state);

    PolicyDestroy(policy);
}

static void test_setup(void **state)
{
    *state = EvalContextNew();
}

static void test_teardown(void **state)
{
    EvalContext *ctx = *state;
    EvalContextDestroy(ctx);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_extract_scalar_prefix),
        unit_test(test_extract_reference),
#if 0
        unit_test_setup_teardown(test_map_iterators_from_rval_empty, test_setup, test_teardown),
        unit_test_setup_teardown(test_map_iterators_from_rval_literal, test_setup, test_teardown),
        unit_test_setup_teardown(test_map_iterators_from_rval_naked_list_var, test_setup, test_teardown),
        unit_test_setup_teardown(test_map_iterators_from_rval_naked_list_var_namespace, test_setup, test_teardown),
#endif
        unit_test_setup_teardown(test_expand_scalar_two_scalars_concat, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_scalar_two_scalars_nested, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_scalar_array_concat, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_scalar_array_with_scalar_arg, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_scalar_undefined, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_scalar_nested_inner_undefined, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_list_nested, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_promise_array_with_scalar_arg, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_promise_slist, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_promise_array_with_slist_arg, test_setup, test_teardown)
    };

    return run_tests(tests);
}
