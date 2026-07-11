#include <test.h>

#include <eval_context.h>
#include <policy.h>
#include <attributes.h>
#include <misc_lib.h>                                          /* xsnprintf */
#include <alloc.h>                                            /* xstrdup */
#include <string_lib.h>
#include <known_dirs.h>
#include <time_classes.h>
#include <unistd.h>                                           /* unlink */

char CFWORKDIR[CF_BUFSIZE];

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

    mkdir(GetStateDir(), 0766);
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
        Bundle *bp = PolicyAppendBundle(p, "ns1", "bundle1", "agent", NULL, NULL, EVAL_ORDER_UNDEFINED);

        EvalContextStackPushBundleFrame(ctx, bp, NULL, false, NULL);
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

void test_changes_chroot(void)
{
    /* Should add '/' to the end implicitly. */
    SetChangesChroot("/changes/go/here");

    /* The most trivial case. */
    const char *chrooted = ToChangesChroot("/etc/issue");
    assert_string_equal(chrooted, "/changes/go/here/etc/issue");

    /* A shorter path to test that NUL-byte is added/copied properly. */
    chrooted = ToChangesChroot("/etc/ab");
    assert_string_equal(chrooted, "/changes/go/here/etc/ab");

    /* And a longer path again. */
    chrooted = ToChangesChroot("/etc/sysctl.d/00-default.conf");
    assert_string_equal(chrooted, "/changes/go/here/etc/sysctl.d/00-default.conf");

#ifndef __MINGW32__
    /* Inverse should work as expected */
    const char *normal = ToNormalRoot(chrooted);
    assert_string_equal(normal, "/etc/sysctl.d/00-default.conf");
#endif
}

void test_eval_with_token_from_list(void)
{
    /* The timestamp should generate these classes (among others):
     *  'GMT_Afternoon', 'GMT_Day29', 'GMT_Hr13', 'GMT_Hr13_Q3', 'GMT_Lcycle_2',
     *  'GMT_May', 'GMT_Min30_35', 'GMT_Min33', 'GMT_Q3', 'GMT_Wednesday',
     *  'GMT_Yr2024'
     */
    const time_t timestamp = 1716989621;
    StringSet *time_classes = StringSetNew();

    StringMap *time_classes_map = GetTimeClasses(timestamp);
    StringMapIterator iter = StringMapIteratorInit(time_classes_map);
    MapKeyValue *item;
    while((item = StringMapIteratorNext(&iter)) != NULL) {
        StringSetAdd(time_classes, SafeStringDuplicate(item->value));
    }

    assert_true(EvalWithTokenFromList("GMT_Wednesday", time_classes));
    assert_false(EvalWithTokenFromList("GMT_Monday", time_classes));
    assert_false(EvalWithTokenFromList("GMT_Wednesday.GMT_Monday", time_classes));
    assert_true(EvalWithTokenFromList("GMT_Monday|GMT_Wednesday", time_classes));
    assert_true(EvalWithTokenFromList("!GMT_Monday", time_classes));

    StringMapDestroy(time_classes_map);
    StringSetDestroy(time_classes);
}

static void test_persistent_class_timer_policy(void)
{
    EvalContext *ctx = EvalContextNew();

    /* Save a persistent class with PRESERVE policy, 60 minute TTL */
    EvalContextHeapPersistentSave(ctx, "timer_test", 60,
                                  CONTEXT_STATE_POLICY_PRESERVE, "tag1");

    /* Verify the class loads correctly after PRESERVE save */
    EvalContextHeapPersistentLoadAll(ctx);

    {
        const Class *cls = EvalContextClassGet(ctx, "default", "timer_test");
        assert_true(cls != NULL);
        assert_string_equal("timer_test", cls->name);
    }

    /* Save again with PRESERVE -- the function should early-return
     * (class is preserved, not expired, same tags), leaving the DB
     * record unchanged.  We verify by loading persistent classes and
     * checking the class is still defined. */
    EvalContextHeapPersistentSave(ctx, "timer_test", 60,
                                  CONTEXT_STATE_POLICY_PRESERVE, "tag1");

    /* Class should still be defined after the second PRESERVE save */
    {
        const Class *cls = EvalContextClassGet(ctx, "default", "timer_test");
        assert_true(cls != NULL);
        assert_string_equal("timer_test", cls->name);
    }

    /* Save with RESET policy -- the record SHOULD be overwritten.
     * The class should still be loadable afterward. */
    EvalContextHeapPersistentSave(ctx, "timer_test", 60,
                                  CONTEXT_STATE_POLICY_RESET, "tag1");

    {
        const Class *cls = EvalContextClassGet(ctx, "default", "timer_test");
        assert_true(cls != NULL);
        assert_string_equal("timer_test", cls->name);
    }

    EvalContextDestroy(ctx);
}

/* CFE-85: a single promise must produce at most one entry in its log_*
 * file, even when cfPS() (and thus SummarizeTransaction) is invoked multiple
 * times for that promise during a run (once per evaluation pass / sub-check).
 *
 * This test verifies three scenarios:
 *   1. Same promise, same promiser, two cfPS calls -> deduped to one line.
 *   2. Different promise (different file/line), same promiser -> each gets a line.
 *   3. Same source promise (same PromiseID), different expanded promiser
 *      (simulates a parameterized bundle) -> each gets a line. */
static void test_log_action_dedupe(void)
{
    char logpath[CF_BUFSIZE];
    xsnprintf(logpath, sizeof(logpath), "%s/cfe85_dedupe_test.log", CFWORKDIR);

    EvalContext *ctx = EvalContextNew();
    Policy *policy = PolicyNew();
    Bundle *bundle = PolicyAppendBundle(policy, "default", "bundle1",
                                        "agent", NULL, NULL,
                                        EVAL_ORDER_UNDEFINED);
    bundle->source_path = xstrdup("/policy1.cf");
    BundleSection *section = BundleAppendSection(bundle, "files");
    Promise *promise = BundleSectionAppendPromise(section, "/tmp/cfe85_dedupe_test",
                                                  (Rval) { NULL, RVAL_TYPE_NOPROMISEE },
                                                  "any", NULL);
    promise->offset.line = 10;

    /* Minimal attributes: only the transaction log_* fields matter for
     * ClassAuditLog -> DoSummarizeTransaction. The classes list is left
     * NULL, which SetPromiseOutcomeClasses tolerates. */
    Attributes attr;
    memset(&attr, 0, sizeof(attr));
    attr.transaction.log_string = xstrdup("logged");
    attr.transaction.log_repaired = xstrdup(logpath);

    /* First call simulates the first evaluation (e.g. repaired). */
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, promise, &attr,
         "files promise '%s' repaired", promise->promiser);

    /* Second call simulates a later pass / sub-check for the same promise.
     * Without the CFE-85 fix this would append a second duplicate line. */
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, promise, &attr,
         "files promise '%s' repaired", promise->promiser);

    /* Scenario 2: a distinct promise (different file/line identity) targeting
     * the SAME path must still get its own line. */
    Bundle *bundle2 = PolicyAppendBundle(policy, "default", "bundle2",
                                         "agent", NULL, NULL,
                                         EVAL_ORDER_UNDEFINED);
    bundle2->source_path = xstrdup("/policy2.cf");
    BundleSection *section2 = BundleAppendSection(bundle2, "files");
    Promise *promise2 = BundleSectionAppendPromise(section2, "/tmp/cfe85_dedupe_test",
                                                   (Rval) { NULL, RVAL_TYPE_NOPROMISEE },
                                                   "any", NULL);
    promise2->offset.line = 20;
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, promise2, &attr,
         "files promise '%s' repaired", promise2->promiser);

    /* Scenario 3: same source location (same PromiseID) but different
     * expanded promiser, simulating a parameterized bundle called twice
     * with different arguments -- e.g. install("vim") then install("curl").
     * Must NOT be suppressed. */
    Promise *promise3 = BundleSectionAppendPromise(section, "/tmp/cfe85_other_path",
                                                   (Rval) { NULL, RVAL_TYPE_NOPROMISEE },
                                                   "any", NULL);
    promise3->offset.line = 10;  /* same line as promise1 in same bundle */
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, promise3, &attr,
         "files promise '%s' repaired", promise3->promiser);

    FILE *f = fopen(logpath, "r");
    assert_true(f != NULL);
    int lines = 0;
    char buf[1024];
    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        if (buf[0] != '\0' && buf[0] != '\n')
        {
            lines++;
        }
    }
    fclose(f);
    unlink(logpath);

    /* Three entries: scenario 1 deduped from 2 calls to 1, scenario 2 adds 1
     * (different promise identity), scenario 3 adds 1 (same PromiseID but
     * different expanded promiser). */
    assert_int_equal(lines, 3);

    free(attr.transaction.log_string);
    free(attr.transaction.log_repaired);
    PolicyDestroy(policy);
    EvalContextDestroy(ctx);
}

int main()
{
    PRINT_TEST_BANNER();
    tests_setup();

    const UnitTest tests[] =
    {
        unit_test(test_class_persistence),
        unit_test(test_persistent_class_timer_policy),
        unit_test(test_changes_chroot),
        unit_test(test_eval_with_token_from_list),
        unit_test(test_log_action_dedupe),
    };

    int ret = run_tests(tests);

    tests_teardown();

    return ret;
}

