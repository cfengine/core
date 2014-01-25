#include <test.h>

#include <expand.h>
#include <rlist.h>
#include <scope.h>
#include <eval_context.h>

static void test_map_iterators_from_rval_empty(void)
{
    EvalContext *ctx = EvalContextNew();

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
    EvalContextDestroy(ctx);
}

static void test_map_iterators_from_rval_literal(void)
{
    EvalContext *ctx = EvalContextNew();
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
    EvalContextDestroy(ctx);
}

static void test_map_iterators_from_rval_naked_list_var(void)
{
    EvalContext *ctx = EvalContextNew();
    Policy *p = PolicyNew();
    Bundle *bp = PolicyAppendBundle(p, "default", "scope", "agent", NULL, NULL);

    Rlist *list = NULL;
    RlistAppend(&list, "jersey", RVAL_TYPE_SCALAR);

    VarRef *lval = VarRefParse("scope.jwow");

    EvalContextVariablePut(ctx, lval, list, CF_DATA_TYPE_STRING_LIST, NULL);

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
    }

    EvalContextStackPopFrame(ctx);

    VarRefDestroy(lval);
    PolicyDestroy(p);
    EvalContextDestroy(ctx);
}

static void test_map_iterators_from_rval_naked_list_var_namespace(void)
{
    EvalContext *ctx = EvalContextNew();
    Policy *p = PolicyNew();
    Bundle *bp = PolicyAppendBundle(p, "ns", "scope", "agent", NULL, NULL);

    Rlist *list = NULL;
    RlistAppend(&list, "jersey", RVAL_TYPE_SCALAR);

    VarRef *lval = VarRefParse("ns:scope.jwow");

    EvalContextVariablePut(ctx, lval, list, CF_DATA_TYPE_STRING_LIST, NULL);

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
    }

    EvalContextStackPopFrame(ctx);

    VarRefDestroy(lval);
    PolicyDestroy(p);
    EvalContextDestroy(ctx);
}

static void test_expand_scalar_two_scalars_concat(void)
{
    EvalContext *ctx = EvalContextNew();
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

    char res[CF_EXPANDSIZE] = { 0 };
    ExpandScalar(ctx, "default", "bundle", "a $(one) b $(two)c", res);

    assert_string_equal("a first b secondc", res);

    EvalContextDestroy(ctx);
}

static void test_expand_scalar_two_scalars_nested(void)
{
    EvalContext *ctx = EvalContextNew();
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

    char res[CF_EXPANDSIZE] = { 0 };
    ExpandScalar(ctx, "default", "bundle", "a $($(two))b", res);

    assert_string_equal("a firstb", res);

    EvalContextDestroy(ctx);
}

static void test_expand_scalar_array_concat(void)
{
    EvalContext *ctx = EvalContextNew();
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

    char res[CF_EXPANDSIZE] = { 0 };
    ExpandScalar(ctx, "default", "bundle", "a $(foo[one]) b $(foo[two])c", res);

    assert_string_equal("a first b secondc", res);

    EvalContextDestroy(ctx);
}

static void test_expand_scalar_array_with_scalar_arg(void)
{
    EvalContext *ctx = EvalContextNew();
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

    char res[CF_EXPANDSIZE] = { 0 };
    ExpandScalar(ctx, "default", "bundle", "a$(foo[$(bar)])b", res);

    assert_string_equal("afirstb", res);

    EvalContextDestroy(ctx);
}

static PromiseResult actuator_expand_promise_array_with_scalar_arg(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert_string_equal("first", pp->promiser);
    return PROMISE_RESULT_NOOP;
}

static void test_expand_promise_array_with_scalar_arg(void)
{
    EvalContext *ctx = EvalContextNew();
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
    Promise *promise = PromiseTypeAppendPromise(promise_type, "$(foo[$(bar)])", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, "any");

    EvalContextStackPushBundleFrame(ctx, bundle, NULL, false);
    ExpandPromise(ctx, promise, actuator_expand_promise_array_with_scalar_arg, NULL);
    EvalContextStackPopFrame(ctx);

    PolicyDestroy(policy);
    EvalContextDestroy(ctx);
}


static int actuator_state = 0;

static PromiseResult actuator_expand_promise_slist(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
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

static void test_expand_promise_slist(void)
{
    actuator_state = 0;

    EvalContext *ctx = EvalContextNew();
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
    Promise *promise = PromiseTypeAppendPromise(promise_type, "$(foo)", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, "any");

    EvalContextStackPushBundleFrame(ctx, bundle, NULL, false);
    ExpandPromise(ctx, promise, actuator_expand_promise_slist, NULL);
    EvalContextStackPopFrame(ctx);

    assert_int_equal(2, actuator_state);

    PolicyDestroy(policy);
    EvalContextDestroy(ctx);
}


static PromiseResult actuator_expand_promise_array_with_slist_arg(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
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

static void test_expand_promise_array_with_slist_arg(void)
{
    actuator_state = 0;

    EvalContext *ctx = EvalContextNew();
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
    Promise *promise = PromiseTypeAppendPromise(promise_type, "$(arr[$(keys)])", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, "any");

    EvalContextStackPushBundleFrame(ctx, bundle, NULL, false);
    ExpandPromise(ctx, promise, actuator_expand_promise_array_with_slist_arg, NULL);
    EvalContextStackPopFrame(ctx);

    assert_int_equal(2, actuator_state);

    PolicyDestroy(policy);
    EvalContextDestroy(ctx);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_map_iterators_from_rval_empty),
        unit_test(test_map_iterators_from_rval_literal),
        unit_test(test_map_iterators_from_rval_naked_list_var),
        unit_test(test_map_iterators_from_rval_naked_list_var_namespace),
        unit_test(test_expand_scalar_two_scalars_concat),
        unit_test(test_expand_scalar_two_scalars_nested),
        unit_test(test_expand_scalar_array_concat),
        unit_test(test_expand_scalar_array_with_scalar_arg),
        unit_test(test_expand_promise_array_with_scalar_arg),
        unit_test(test_expand_promise_slist),
        unit_test(test_expand_promise_array_with_slist_arg)
    };

    return run_tests(tests);
}
