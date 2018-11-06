#include <test.h>

#include <policy.h>
#include <parser.h>
#include <rlist.h>
#include <fncall.h>
#include <eval_context.h>
#include <item_lib.h>
#include <bootstrap.h>
#include <misc_lib.h>                                          /* xsnprintf */

static Policy *TestParsePolicy(const char *filename)
{
    char path[PATH_MAX];
    xsnprintf(path, sizeof(path), "%s/%s", TESTDATADIR, filename);

    return ParserParseFile(AGENT_TYPE_COMMON, path, PARSER_WARNING_ALL, PARSER_WARNING_ALL);
}

static void DumpErrors(Seq *errs)
{
    if (SeqLength(errs) > 0)
    {
        Writer *writer = FileWriter(stdout);
        for (size_t i = 0; i < errs->length; i++)
        {
            PolicyErrorWrite(writer, errs->data[i]);
        }
        FileWriterDetach(writer);
    }
}

static Seq *LoadAndCheck(const char *filename)
{
    Policy *p = TestParsePolicy(filename);

    Seq *errs = SeqNew(10, PolicyErrorDestroy);
    PolicyCheckPartial(p, errs);

    DumpErrors(errs);
    PolicyDestroy(p);

    return errs;
}

static Seq *LoadAndCheckString(const char *policy_code)
{
    char tmp[] = TESTDATADIR "/cfengine_test.XXXXXX";
    mkstemp(tmp);

    {
        FILE *out = fopen(tmp, "w");
        Writer *w = FileWriter(out);
        WriterWrite(w, policy_code);

        WriterClose(w);
    }

    Policy *p = ParserParseFile(AGENT_TYPE_COMMON, tmp, PARSER_WARNING_ALL, PARSER_WARNING_ALL);
    assert_true(p);

    Seq *errs = SeqNew(10, PolicyErrorDestroy);
    PolicyCheckPartial(p, errs);

    PolicyDestroy(p);
    unlink(tmp);
    return errs;
}

static void test_failsafe(void)
{
    char tmp[] = TESTDATADIR "/cfengine_test.XXXXXX";
    mkstemp(tmp);

    WriteBuiltinFailsafePolicyToPath(tmp);

    Policy *failsafe = ParserParseFile(AGENT_TYPE_COMMON, tmp, PARSER_WARNING_ALL, PARSER_WARNING_ALL);

    unlink(tmp);

    assert_true(failsafe);

    Seq *errs = SeqNew(10, PolicyErrorDestroy);
    PolicyCheckPartial(failsafe, errs);

    DumpErrors(errs);
    assert_int_equal(0, SeqLength(errs));

    {
        EvalContext *ctx = EvalContextNew();

        PolicyCheckRunnable(ctx, failsafe, errs, false);

        DumpErrors(errs);
        assert_int_equal(0, SeqLength(errs));

        EvalContextDestroy(ctx);
    }

    assert_int_equal(0, (SeqLength(errs)));

    SeqDestroy(errs);
    PolicyDestroy(failsafe);
}


static void test_bundle_redefinition(void)
{
    Seq *errs = LoadAndCheck("bundle_redefinition.cf");
    assert_int_equal(2, errs->length);

    SeqDestroy(errs);
}

static void test_bundle_reserved_name(void)
{
    Seq *errs = LoadAndCheck("bundle_reserved_name.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

static void test_body_redefinition(void)
{
    Seq *errs = LoadAndCheck("body_redefinition.cf");
    assert_int_equal(2, errs->length);

    SeqDestroy(errs);
}

static void test_body_control_no_arguments(void)
{
    Seq *errs = LoadAndCheck("body_control_no_arguments.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

static void test_vars_multiple_types(void)
{
    Seq *errs = LoadAndCheck("vars_multiple_types.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

static void test_methods_invalid_arity(void)
{
    Seq *errs = LoadAndCheck("methods_invalid_arity.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

static void test_promise_duplicate_handle(void)
{
    Seq *errs = LoadAndCheck("promise_duplicate_handle.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

static void test_policy_json_to_from(void)
{
    EvalContext *ctx = EvalContextNew();
    Policy *policy = NULL;
    {
        Policy *original = TestParsePolicy("benchmark.cf");
        JsonElement *json = PolicyToJson(original);
        PolicyDestroy(original);
        policy = PolicyFromJson(json);
        JsonDestroy(json);
    }
    assert_true(policy);

    assert_int_equal(1, SeqLength(policy->bundles));
    assert_int_equal(2, SeqLength(policy->bodies));

    {
        Bundle *main_bundle = PolicyGetBundle(policy, NULL, "agent", "main");
        assert_true(main_bundle);
        {
            {
                const PromiseType *files = BundleGetPromiseType(main_bundle, "files");
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
                const char* reportOutput[2] = { "Hello, CFEngine", "Hello, world" };
                const char* reportClass[2] = { "cfengine", "any" };
                const PromiseType *reports = BundleGetPromiseType(main_bundle, "reports");
                assert_true(reports);
                assert_int_equal(2, SeqLength(reports->promises));

                for (size_t i = 0; i < SeqLength(reports->promises); i++)
                {
                    Promise *promise = SeqAt(reports->promises, i);

                    if (strcmp(reportOutput[i], promise->promiser) == 0)
                    {
                        assert_string_equal(reportClass[i], promise->classes);

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
            SeqDestroy(mode_cps);
        }
    }

    PolicyDestroy(policy);
    EvalContextDestroy(ctx);
}

static void test_policy_json_offsets(void)
{
    JsonElement *json = NULL;
    {
        Policy *original = TestParsePolicy("benchmark.cf");
        json = PolicyToJson(original);
        PolicyDestroy(original);
    }
    assert_true(json);

    JsonElement *json_bundles = JsonObjectGetAsArray(json, "bundles");
    {
        JsonElement *main_bundle = JsonArrayGetAsObject(json_bundles, 0);
        int line = JsonPrimitiveGetAsInteger(JsonObjectGet(main_bundle, "line"));
        assert_int_equal(9, line);

        JsonElement *json_promise_types = JsonObjectGetAsArray(main_bundle, "promiseTypes");
        {
            JsonElement *json_reports_type = JsonArrayGetAsObject(json_promise_types, 0);
            line = JsonPrimitiveGetAsInteger(JsonObjectGet(json_reports_type, "line"));
            assert_int_equal(11, line);

            JsonElement *json_contexts = JsonObjectGetAsArray(json_reports_type, "contexts");
            JsonElement *cf_context = JsonArrayGetAsObject(json_contexts, 0);
            JsonElement *cf_context_promises = JsonObjectGetAsArray(cf_context, "promises");
            JsonElement *hello_cf_promise = JsonArrayGetAsObject(cf_context_promises, 0);

            line = JsonPrimitiveGetAsInteger(JsonObjectGet(hello_cf_promise, "line"));
            assert_int_equal(13, line);
            JsonElement *hello_cf_attribs = JsonObjectGetAsArray(hello_cf_promise, "attributes");
            {
                JsonElement *friend_pattern_attrib = JsonArrayGetAsObject(hello_cf_attribs, 0);

                line = JsonPrimitiveGetAsInteger(JsonObjectGet(friend_pattern_attrib, "line"));
                assert_int_equal(14, line);
            }
        }
    }

    JsonElement *json_bodies = JsonObjectGetAsArray(json, "bodies");
    {
        JsonElement *control_body = JsonArrayGetAsObject(json_bodies, 0);
        int line = JsonPrimitiveGetAsInteger(JsonObjectGet(control_body, "line"));
        assert_int_equal(4, line);

        JsonElement *myperms_body = JsonArrayGetAsObject(json_bodies, 1);
        line = JsonPrimitiveGetAsInteger(JsonObjectGet(myperms_body, "line"));
        assert_int_equal(28, line);

        JsonElement *myperms_contexts = JsonObjectGetAsArray(myperms_body, "contexts");
        JsonElement *any_context = JsonArrayGetAsObject(myperms_contexts, 0);
        JsonElement *any_attribs = JsonObjectGetAsArray(any_context, "attributes");
        {
            JsonElement *mode_attrib = JsonArrayGetAsObject(any_attribs, 0);
            line = JsonPrimitiveGetAsInteger(JsonObjectGet(mode_attrib, "line"));
            assert_int_equal(30, line);
        }
    }

    JsonDestroy(json);
}


static void test_util_bundle_qualified_name(void)
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

static void test_util_qualified_name_components(void)
{
    {
        char *ns = QualifiedNameNamespaceComponent(":");
        assert_string_equal("", ns);
        free(ns);

        char *sym = QualifiedNameScopeComponent(":");
        assert_string_equal("", sym);
        free(sym);
    }

    {
        char *ns = QualifiedNameNamespaceComponent("");
        assert_false(ns);
        free(ns);

        char *sym = QualifiedNameScopeComponent("");
        assert_string_equal("", sym);
        free(sym);
    }

    {
        char *ns = QualifiedNameNamespaceComponent("foo");
        assert_false(ns);
        free(ns);

        char *sym = QualifiedNameScopeComponent("foo");
        assert_string_equal("foo", sym);
        free(sym);
    }

    {
        char *ns = QualifiedNameNamespaceComponent(":foo");
        assert_string_equal("", ns);
        free(ns);

        char *sym = QualifiedNameScopeComponent(":foo");
        assert_string_equal("foo", sym);
        free(sym);
    }

    {
        char *ns = QualifiedNameNamespaceComponent("foo:");
        assert_string_equal("foo", ns);
        free(ns);

        char *sym = QualifiedNameScopeComponent("foo:");
        assert_string_equal("", sym);
        free(sym);
    }

    {
        char *ns = QualifiedNameNamespaceComponent("foo:bar");
        assert_string_equal("foo", ns);
        free(ns);

        char *sym = QualifiedNameScopeComponent("foo:bar");
        assert_string_equal("bar", sym);
        free(sym);
    }
}

static void test_promiser_empty_varref(void)
{
    Seq *errs = LoadAndCheck("promiser_empty_varref.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

static void test_constraint_comment_nonscalar(void)
{
    Seq *errs = LoadAndCheck("constraint_comment_nonscalar.cf");
    assert_int_equal(1, errs->length);

    SeqDestroy(errs);
}

// TODO: consider moving this into a mod_common_test
static void test_body_action_with_log_repaired_needs_log_string(void)
{
    {
        Seq *errs = LoadAndCheckString("body action foo {"
                                       "  log_repaired => '/tmp/abc';"
                                       "}");
        assert_int_equal(1, errs->length);
        SeqDestroy(errs);
    }

    {
        Seq *errs = LoadAndCheckString("body action foo {"
                                       "  log_repaired => '/tmp/abc';"
                                       "  log_string => 'stuff';"
                                       "}");
        assert_int_equal(0, errs->length);
        SeqDestroy(errs);
    }
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_failsafe),

        unit_test(test_bundle_redefinition),
        unit_test(test_bundle_reserved_name),
        unit_test(test_body_redefinition),
        unit_test(test_body_control_no_arguments),
        unit_test(test_vars_multiple_types),
        unit_test(test_methods_invalid_arity),
        unit_test(test_promise_duplicate_handle),

        unit_test(test_policy_json_to_from),
        unit_test(test_policy_json_offsets),

        unit_test(test_util_bundle_qualified_name),
        unit_test(test_util_qualified_name_components),

        unit_test(test_constraint_comment_nonscalar),

        unit_test(test_promiser_empty_varref),

        unit_test(test_body_action_with_log_repaired_needs_log_string),
    };

    return run_tests(tests);
}

// STUBS
