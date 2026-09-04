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

static void test_isnakedvar()
{
    assert_true(IsNakedVar("$(whatever)", '$'));
    assert_true(IsNakedVar("${whatever}", '$'));
    assert_true(IsNakedVar("$(blah$(blue))", '$'));

    assert_false(IsNakedVar("$(blah)blue", '$'));
    assert_false(IsNakedVar("blah$(blue)", '$'));
    assert_false(IsNakedVar("$(blah)$(blue)", '$'));
    assert_false(IsNakedVar("$(blah}", '$'));
}


#if 0
static void test_map_iterators_from_rval_empty(void **state)
{
    EvalContext *ctx = *state;

    Policy *p = PolicyNew();
    Bundle *bp = PolicyAppendBundle(p, "default", "none", "agent", NULL, NULL, EVAL_ORDER_UNDEFINED);

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
    Bundle *bp = PolicyAppendBundle(p, "default", "none", "agent", NULL, NULL, EVAL_ORDER_UNDEFINED);

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
    Bundle *bp = PolicyAppendBundle(p, "default", "scope", "agent", NULL, NULL, EVAL_ORDER_UNDEFINED);

    {
        Rlist *list = NULL;
        RlistAppend(&list, "jersey", RVAL_TYPE_SCALAR);
        VarRef *lval = VarRefParse("scope.jwow");

        EvalContextVariablePut(ctx, lval, list, CF_DATA_TYPE_STRING_LIST, NULL);

        VarRefDestroy(lval);
        RlistDestroy(list);
    }

    EvalContextStackPushBundleFrame(ctx, bp, NULL, false, NULL);

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
    Bundle *bp = PolicyAppendBundle(p, "ns", "scope", "agent", NULL, NULL, EVAL_ORDER_UNDEFINED);

    {
        Rlist *list = NULL;
        RlistAppend(&list, "jersey", RVAL_TYPE_SCALAR);
        VarRef *lval = VarRefParse("ns:scope.jwow");

        EvalContextVariablePut(ctx, lval, list, CF_DATA_TYPE_STRING_LIST, NULL);

        VarRefDestroy(lval);
        RlistDestroy(list);
    }

    EvalContextStackPushBundleFrame(ctx, bp, NULL, false, NULL);

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
    Bundle *bundle = PolicyAppendBundle(policy, NamespaceDefault(), "bundle", "agent", NULL, NULL, EVAL_ORDER_UNDEFINED);
    BundleSection *section = BundleAppendSection(bundle, "dummy");
    Promise *promise = BundleSectionAppendPromise(section, "$(foo[$(bar)])", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, "any", NULL);

    EvalContextStackPushBundleFrame(ctx, bundle, NULL, false, NULL);
    EvalContextStackPushBundleSectionFrame(ctx, section);
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
    Bundle *bundle = PolicyAppendBundle(policy, NamespaceDefault(), "bundle", "agent", NULL, NULL, EVAL_ORDER_UNDEFINED);
    BundleSection *section = BundleAppendSection(bundle, "dummy");
    Promise *promise = BundleSectionAppendPromise(section, "$(foo)", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, "any", NULL);

    EvalContextStackPushBundleFrame(ctx, bundle, NULL, false, NULL);
    EvalContextStackPushBundleSectionFrame(ctx, section);
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
    Bundle *bundle = PolicyAppendBundle(policy, NamespaceDefault(), "bundle", "agent", NULL, NULL, EVAL_ORDER_UNDEFINED);
    BundleSection *section = BundleAppendSection(bundle, "dummy");
    Promise *promise = BundleSectionAppendPromise(section, "$(arr[$(keys)])", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, "any", NULL);

    EvalContextStackPushBundleFrame(ctx, bundle, NULL, false, NULL);
    EvalContextStackPushBundleSectionFrame(ctx, section);
    ExpandPromise(ctx, promise, actuator_expand_promise_array_with_slist_arg, NULL);
    EvalContextStackPopFrame(ctx);
    EvalContextStackPopFrame(ctx);

    assert_int_equal(2, actuator_state);

    PolicyDestroy(policy);
}

/* Puts one secret-tagged and one ordinary variable in the same bundle, and
 * asserts the tag really landed -- without that control every expectation
 * below would also hold for a build where nothing is ever tagged secret. */
static void PutSecretAndPlain(EvalContext *ctx)
{
    VarRef *secret = VarRefParse("default:bundle.password");
    assert_true(EvalContextVariablePut(ctx, secret, "hunter2",
                                       CF_DATA_TYPE_STRING, "secret"));
    assert_true(EvalContextVariableIsTaggedSecret(ctx, secret));
    VarRefDestroy(secret);

    VarRef *plain = VarRefParse("default:bundle.user");
    assert_true(EvalContextVariablePut(ctx, plain, "alice",
                                       CF_DATA_TYPE_STRING, NULL));
    assert_false(EvalContextVariableIsTaggedSecret(ctx, plain));
    VarRefDestroy(plain);
}

/* ExpandScalar() itself is unchanged: a secret expands like anything else. */
static void test_expand_scalar_secret_inlined_by_default(void **state)
{
    EvalContext *ctx = *state;
    PutSecretAndPlain(ctx);

    Buffer *res = BufferNew();
    ExpandScalar(ctx, "default", "bundle", "$(user):$(password)", res);

    assert_string_equal("alice:hunter2", BufferData(res));
    BufferDestroy(res);
}

static void test_expand_scalar_keep_secrets(void **state)
{
    EvalContext *ctx = *state;
    PutSecretAndPlain(ctx);

    Buffer *res = BufferNew();
    ExpandScalarKeepSecrets(ctx, "default", "bundle", "$(user):$(password)", res);

    /* Only the secret is held back; the ordinary variable still expands. */
    assert_string_equal("alice:$(password)", BufferData(res));
    BufferDestroy(res);
}

static void test_expand_scalar_secrets_only(void **state)
{
    EvalContext *ctx = *state;
    PutSecretAndPlain(ctx);

    Buffer *res = BufferNew();
    ExpandScalarSecretsOnly(ctx, "default", "bundle", "$(user):$(password)", res);

    /* The mirror image: a resolvable non-secret reference is left alone. */
    assert_string_equal("$(user):hunter2", BufferData(res));
    BufferDestroy(res);
}

/* Deferring and then resolving has to end up where expanding in one go would,
 * or a commands promise would run something other than what it promised. */
static void test_expand_scalar_secret_round_trip(void **state)
{
    EvalContext *ctx = *state;
    PutSecretAndPlain(ctx);

    const char *const cases[] = {
        "$(user):$(password)",
        "${user}:${password}",          /* the brace form is preserved too */
        "$(password)$(password)",
        "no references at all",
        "a$(undefined)b",               /* unresolvable, untouched by both */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        char *const direct = ExpandScalar(ctx, "default", "bundle", cases[i], NULL);
        char *const deferred = ExpandScalarKeepSecrets(ctx, "default", "bundle",
                                                       cases[i], NULL);
        char *const resolved = ExpandScalarSecretsOnly(ctx, "default", "bundle",
                                                       deferred, NULL);
        assert_string_equal(direct, resolved);
        free(direct);
        free(deferred);
        free(resolved);
    }
}

/* A secret used as part of a variable *name* cannot be resolved late: the
 * outer name is not known until the inner one is substituted. Both modes leave
 * the whole reference alone rather than leaking the inner value. */
static void test_expand_scalar_secret_nested_reference(void **state)
{
    EvalContext *ctx = *state;
    PutSecretAndPlain(ctx);
    {
        VarRef *lval = VarRefParse("default:bundle.foo[hunter2]");
        EvalContextVariablePut(ctx, lval, "bar", CF_DATA_TYPE_STRING, NULL);
        VarRefDestroy(lval);
    }

    /* Control: expanded in one go, this resolves all the way through. */
    char *const direct = ExpandScalar(ctx, "default", "bundle",
                                      "a$(foo[$(password)])b", NULL);
    assert_string_equal("abarb", direct);
    free(direct);

    char *const deferred = ExpandScalarKeepSecrets(ctx, "default", "bundle",
                                                   "a$(foo[$(password)])b", NULL);
    assert_string_equal("a$(foo[$(password)])b", deferred);

    char *const resolved = ExpandScalarSecretsOnly(ctx, "default", "bundle",
                                                   deferred, NULL);
    assert_string_equal("a$(foo[$(password)])b", resolved);
    free(deferred);
    free(resolved);
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
        unit_test(test_isnakedvar),
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
        unit_test_setup_teardown(test_expand_scalar_secret_inlined_by_default, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_scalar_keep_secrets, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_scalar_secrets_only, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_scalar_secret_round_trip, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_scalar_secret_nested_reference, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_scalar_nested_inner_undefined, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_list_nested, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_promise_array_with_scalar_arg, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_promise_slist, test_setup, test_teardown),
        unit_test_setup_teardown(test_expand_promise_array_with_slist_arg, test_setup, test_teardown)
    };

    return run_tests(tests);
}
