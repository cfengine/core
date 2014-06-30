#include <test.h>

#include <eval_context.h>
#include <misc_lib.h>                                          /* xsnprintf */


void tests_setup(void)
{
    static char env[] = /* Needs to be static for putenv() */
        "CFENGINE_TEST_OVERRIDE_WORKDIR=/tmp/CFENGINE_eval_context_test.XXXXXX";
    char *workdir = strchr(env, '=');
    assert(workdir && workdir[1] == '/');
    workdir++;

    mkdtemp(workdir);
    strlcpy(CFWORKDIR, workdir, CF_BUFSIZE);
    putenv(env);

    char state_dir[PATH_MAX];
    xsnprintf(state_dir, PATH_MAX, "%s/state", workdir);
    mkdir(state_dir, 0766);
}

void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

static void test_class_persistence(void)
{
    EvalContext *ctx = EvalContextNew();

    // simulate old version
    {
        CF_DB *dbp;
        PersistentClassInfo i;
        assert_true(OpenDB(&dbp, dbid_state));

        i.expires = UINT_MAX;
        i.policy = CONTEXT_STATE_POLICY_RESET;

        WriteDB(dbp, "old", &i, sizeof(PersistentClassInfo));

        CloseDB(dbp);
    }

    // e.g. by monitoring
    EvalContextHeapPersistentSave(ctx, "class1", 3, CONTEXT_STATE_POLICY_PRESERVE, "a,b");

    // e.g. by a class promise in a bundle with a namespace
    {
        Policy *p = PolicyNew();
        Bundle *bp = PolicyAppendBundle(p, "ns1", "bundle1", "agent", NULL, NULL);

        EvalContextStackPushBundleFrame(ctx, bp, NULL, false);
        EvalContextHeapPersistentSave(ctx, "class2", 5, CONTEXT_STATE_POLICY_PRESERVE, "x");
        EvalContextStackPopFrame(ctx);

        PolicyDestroy(p);
    }

    EvalContextHeapPersistentLoadAll(ctx);

    {
        const Class *cls = EvalContextClassGet(ctx, "default", "old");
        assert_true(cls != NULL);

        assert_string_equal("old", cls->name);
        assert_true(cls->tags != NULL);
        assert_int_equal(1, StringSetSize(cls->tags));
        assert_true(StringSetContains(cls->tags, "source=persistent"));
    }

    {
        const Class *cls = EvalContextClassGet(ctx, "default", "class1");
        assert_true(cls != NULL);

        assert_string_equal("class1", cls->name);
        assert_true(cls->tags != NULL);
        assert_int_equal(3, StringSetSize(cls->tags));
        assert_true(StringSetContains(cls->tags, "source=persistent"));
        assert_true(StringSetContains(cls->tags, "a"));
        assert_true(StringSetContains(cls->tags, "b"));
    }

    {
        const Class *cls = EvalContextClassGet(ctx, "ns1", "class2");
        assert_true(cls != NULL);

        assert_string_equal("ns1", cls->ns);
        assert_string_equal("class2", cls->name);
        assert_true(cls->tags != NULL);
        assert_int_equal(2, StringSetSize(cls->tags));
        assert_true(StringSetContains(cls->tags, "source=persistent"));
        assert_true(StringSetContains(cls->tags, "x"));
    }

    EvalContextDestroy(ctx);
}

int main()
{
    PRINT_TEST_BANNER();
    tests_setup();

    const UnitTest tests[] =
    {
        unit_test(test_class_persistence),
    };

    int ret = run_tests(tests);

    tests_teardown();

    return ret;
}

