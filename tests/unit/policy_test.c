#include "test.h"

#include "policy.h"
#include "parser.h"
#include "rlist.h"
#include "fncall.h"

static Policy *LoadPolicy(const char *filename)
{
    char path[1024];
    sprintf(path, "%s/%s", TESTDATADIR, filename);

    return ParserParseFile(path);
}

static Seq *LoadAndCheck(const char *filename)
{
    Policy *p = LoadPolicy(filename);

    Seq *errs = SeqNew(10, PolicyErrorDestroy);
    PolicyCheckPartial(p, errs);

    return errs;
}

static void test_bundle_redefinition(void **state)
{
    Seq *errs = LoadAndCheck("bundle_redefinition.cf");
    assert_int_equal(2, errs->length);

    SeqDestroy(errs);
}

static void test_bundle_reserved_name(void **state)
{
    Seq *errs = LoadAndCheck("bundle_reserved_name.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

static void test_body_redefinition(void **state)
{
    Seq *errs = LoadAndCheck("body_redefinition.cf");
    assert_int_equal(2, errs->length);

    SeqDestroy(errs);
}

static void test_subtype_invalid(void **state)
{
    Seq *errs = LoadAndCheck("subtype_invalid.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

static void test_vars_multiple_types(void **state)
{
    Seq *errs = LoadAndCheck("vars_multiple_types.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

static void test_methods_invalid_arity(void **state)
{
    Seq *errs = LoadAndCheck("methods_invalid_arity.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

static void test_promise_duplicate_handle(void **state)
{
    Seq *errs = LoadAndCheck("promise_duplicate_handle.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

static void test_policy_json_to_from(void **state)
{
    Policy *policy = NULL;
    {
        Policy *original = LoadPolicy("benchmark.cf");
        JsonElement *json = PolicyToJson(original);
        PolicyDestroy(original);
        policy = PolicyFromJson(json);
        JsonElementDestroy(json);
    }
    assert_true(policy);

    assert_int_equal(1, SeqLength(policy->bundles));
    assert_int_equal(2, SeqLength(policy->bodies));

    {
        Bundle *main_bundle = PolicyGetBundle(policy, NULL, "agent", "main");
        assert_true(main_bundle);
        {
            {
                SubType *files = BundleGetSubType(main_bundle, "files");
                assert_true(files);
                assert_int_equal(1, SeqLength(files->promises));

                for (size_t i = 0; i < SeqLength(files->promises); i++)
                {
                    Promise *promise = SeqAt(files->promises, i);

                    if (strcmp("/tmp/stuff", promise->promiser) == 0)
                    {
                        assert_string_equal("any", promise->classes);

                        assert_int_equal(2, SeqLength(promise->conlist));

                        {
                            Constraint *create = PromiseGetConstraint(promise, "create");
                            assert_true(create);
                            assert_string_equal("create", create->lval);
                            assert_string_equal("true", RvalScalarValue(create->rval));
                        }

                        {
                            Constraint *create = PromiseGetConstraint(promise, "perms");
                            assert_true(create);
                            assert_string_equal("perms", create->lval);
                            assert_string_equal("myperms", RvalScalarValue(create->rval));
                        }
                    }
                    else
                    {
                        fprintf(stderr, "Found unknown promise");
                        fail();
                    }
                }
            }

            {
                SubType *reports = BundleGetSubType(main_bundle, "reports");
                assert_true(reports);
                assert_int_equal(1, SeqLength(reports->promises));

                for (size_t i = 0; i < SeqLength(reports->promises); i++)
                {
                    Promise *promise = SeqAt(reports->promises, i);

                    if (strcmp("Hello, world", promise->promiser) == 0)
                    {
                        assert_string_equal("cfengine", promise->classes);

                        assert_int_equal(1, SeqLength(promise->conlist));

                        {
                            Constraint *friend_pattern = SeqAt(promise->conlist, 0);
                            assert_true(friend_pattern);
                            assert_string_equal("friend_pattern", friend_pattern->lval);
                            assert_int_equal(RVAL_TYPE_FNCALL, friend_pattern->rval.type);
                            FnCall *fn = RvalFnCallValue(friend_pattern->rval);
                            assert_string_equal("hash", fn->name);
                            assert_int_equal(2, RlistLen(fn->args));
                        }
                    }
                    else
                    {
                        fprintf(stderr, "Found unknown promise");
                        fail();
                    }
                }
            }
        }
    }

    {
        Body *myperms = PolicyGetBody(policy, NULL, "perms", "myperms");
        assert_true(myperms);

        {
            Seq *mode_cps = BodyGetConstraint(myperms, "mode");
            assert_int_equal(1, SeqLength(mode_cps));

            Constraint *mode = SeqAt(mode_cps, 0);
            assert_string_equal("mode", mode->lval);
            assert_string_equal("555", RvalScalarValue(mode->rval));
        }
    }

    PolicyDestroy(policy);
}

static void test_util_bundle_qualified_name(void **state)
{
    Bundle *b = xcalloc(1, sizeof(struct Bundle_));
    assert_false(BundleQualifiedName(b));

    b->name = "bar";

    char *fqname = BundleQualifiedName(b);
    assert_string_equal("default:bar", fqname);
    free(fqname);

    b->ns = "foo";
    fqname = BundleQualifiedName(b);
    assert_string_equal("foo:bar", fqname);
    free(fqname);

    free(b);
}

int main()
{
    const UnitTest tests[] =
    {
        unit_test(test_bundle_redefinition),
        unit_test(test_bundle_reserved_name),
        unit_test(test_body_redefinition),
        unit_test(test_subtype_invalid),
        unit_test(test_vars_multiple_types),
        unit_test(test_methods_invalid_arity),
        unit_test(test_promise_duplicate_handle),

        unit_test(test_policy_json_to_from),

        unit_test(test_util_bundle_qualified_name)
    };

    return run_tests(tests);
}
