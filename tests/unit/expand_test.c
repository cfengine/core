#include "test.h"

#include "expand.h"
#include "rlist.h"
#include "scope.h"
#include "env_context.h"

static void test_map_iterators_from_rval_empty(void **state)
{
    EvalContext *ctx = EvalContextNew();

    Rlist *lists = NULL;
    MapIteratorsFromRval(ctx, "none", &lists, (Rval) { "", RVAL_TYPE_SCALAR });

    assert_int_equal(0, RlistLen(lists));

    EvalContextDestroy(ctx);
}

static void test_map_iterators_from_rval_literal(void **state)
{
    EvalContext *ctx = EvalContextNew();

    Rlist *lists = NULL;
    MapIteratorsFromRval(ctx, "none", &lists, (Rval) { "snookie", RVAL_TYPE_SCALAR });

    assert_int_equal(0, RlistLen(lists));

    EvalContextDestroy(ctx);
}

static void test_map_iterators_from_rval_naked_list_var(void **state)
{
    EvalContext *ctx = EvalContextNew();
    ScopeDeleteAll();
    ScopeSetCurrent("scope");

    Rlist *list = NULL;
    RlistAppend(&list, "jersey", RVAL_TYPE_SCALAR);

    VarRef lval = VarRefParse("scope.jwow");

    EvalContextVariablePut(ctx, lval, (Rval) { list, RVAL_TYPE_LIST }, DATA_TYPE_STRING_LIST);

    Rlist *lists = NULL;
    MapIteratorsFromRval(ctx, "scope", &lists, (Rval) { "${jwow}", RVAL_TYPE_SCALAR });

    assert_int_equal(1, RlistLen(lists));
    assert_string_equal("jwow", lists->item);

    VarRefDestroy(lval);
    EvalContextDestroy(ctx);
}

static void test_expand_scalar_two_scalars_concat(void **state)
{
    EvalContext *ctx = EvalContextNew();
    {
        VarRef lval = VarRefParse("default:bundle.one");
        EvalContextVariablePut(ctx, lval, (Rval) { "first", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }
    {
        VarRef lval = VarRefParse("default:bundle.two");
        EvalContextVariablePut(ctx, lval, (Rval) { "second", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }

    char res[CF_EXPANDSIZE] = { 0 };
    ExpandScalar(ctx, "bundle", "a $(one) b $(two)c", res);

    assert_string_equal("a first b secondc", res);

    EvalContextDestroy(ctx);
}

static void test_expand_scalar_two_scalars_nested(void **state)
{
    EvalContext *ctx = EvalContextNew();
    {
        VarRef lval = VarRefParse("default:bundle.one");
        EvalContextVariablePut(ctx, lval, (Rval) { "first", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }
    {
        VarRef lval = VarRefParse("default:bundle.two");
        EvalContextVariablePut(ctx, lval, (Rval) { "one", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }

    char res[CF_EXPANDSIZE] = { 0 };
    ExpandScalar(ctx, "bundle", "a $($(two))b", res);

    assert_string_equal("a firstb", res);

    EvalContextDestroy(ctx);
}

static void test_expand_scalar_array_concat(void **state)
{
    EvalContext *ctx = EvalContextNew();
    {
        VarRef lval = VarRefParse("default:bundle.foo[one]");
        EvalContextVariablePut(ctx, lval, (Rval) { "first", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }
    {
        VarRef lval = VarRefParse("default:bundle.foo[two]");
        EvalContextVariablePut(ctx, lval, (Rval) { "second", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }

    char res[CF_EXPANDSIZE] = { 0 };
    ExpandScalar(ctx, "bundle", "a $(foo[one]) b $(foo[two])c", res);

    assert_string_equal("a first b secondc", res);

    EvalContextDestroy(ctx);
}

static void test_expand_scalar_array_with_scalar_arg(void **state)
{
    EvalContext *ctx = EvalContextNew();
    {
        VarRef lval = VarRefParse("default:bundle.foo[one]");
        EvalContextVariablePut(ctx, lval, (Rval) { "first", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }
    {
        VarRef lval = VarRefParse("default:bundle.bar");
        EvalContextVariablePut(ctx, lval, (Rval) { "one", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }

    char res[CF_EXPANDSIZE] = { 0 };
    ExpandScalar(ctx, "bundle", "a$(foo[$(bar)])b", res);

    assert_string_equal("afirstb", res);

    EvalContextDestroy(ctx);
}

static void actuator_expand_promise_array_with_scalar_arg(EvalContext *ctx, Promise *pp)
{
    assert_string_equal("first", pp->promiser);
}

static void test_expand_promise_array_with_scalar_arg(void **state)
{
    EvalContext *ctx = EvalContextNew();
    {
        VarRef lval = VarRefParse("default:bundle.foo[one]");
        EvalContextVariablePut(ctx, lval, (Rval) { "first", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }
    {
        VarRef lval = VarRefParse("default:bundle.bar");
        EvalContextVariablePut(ctx, lval, (Rval) { "one", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }

    Policy *policy = PolicyNew();
    Bundle *bundle = PolicyAppendBundle(policy, NamespaceDefault(), "bundle", "agent", NULL, NULL);
    PromiseType *promise_type = BundleAppendPromiseType(bundle, "dummy");
    Promise *promise = PromiseTypeAppendPromise(promise_type, "$(foo[$(bar)])", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, "any");

    EvalContextStackPushBundleFrame(ctx, bundle, false);
    ExpandPromise(ctx, promise, actuator_expand_promise_array_with_scalar_arg);
    EvalContextStackPopFrame(ctx);

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
        unit_test(test_expand_scalar_two_scalars_concat),
        unit_test(test_expand_scalar_two_scalars_nested),
        unit_test(test_expand_scalar_array_concat),
        unit_test(test_expand_scalar_array_with_scalar_arg),
        unit_test(test_expand_promise_array_with_scalar_arg)
    };

    return run_tests(tests);
}
