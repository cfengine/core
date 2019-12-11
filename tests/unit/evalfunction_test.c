#include <test.h>

#include <eval_context.h>
#include <evalfunction.c>

static bool netgroup_more = false;

#if SETNETGRENT_RETURNS_INT
int
#else
void
#endif
setnetgrent(const char *netgroup)
{
    if (strcmp(netgroup, "valid_netgroup") == 0)
    {
        netgroup_more = true;
#if SETNETGRENT_RETURNS_INT
        return 1;
#else
        return;
#endif
    }
    netgroup_more = false;

#if SETNETGRENT_RETURNS_INT
    return 0;
#endif
}

int getnetgrent(char **hostp, char **userp, char **domainp)
{
    if (netgroup_more)
    {
        *hostp = NULL;
        *userp = "user";
        *domainp = NULL;

        netgroup_more = false;
        return 1;
    }
    else
    {
        return 0;
    }
}

static void test_hostinnetgroup_found(void)
{
    EvalContext *ctx = EvalContextNew();

    FnCallResult res;
    Rlist *args = NULL;

    RlistAppendScalar(&args, "valid_netgroup");

    res = FnCallHostInNetgroup(ctx, NULL, NULL, args);
    assert_string_equal("any", (char *) res.rval.item);

    RvalDestroy(res.rval);
    RlistDestroy(args);
    EvalContextDestroy(ctx);
}

static void test_hostinnetgroup_not_found(void)
{
    EvalContext *ctx = EvalContextNew();

    FnCallResult res;
    Rlist *args = NULL;

    RlistAppendScalar(&args, "invalid_netgroup");

    res = FnCallHostInNetgroup(ctx, NULL, NULL, args);
    assert_string_equal("!any", (char *) res.rval.item);

    RvalDestroy(res.rval);
    RlistDestroy(args);
    EvalContextDestroy(ctx);
}

#define basename_single_testcase(input, suffix, expected)      \
    {                                                          \
        FnCallResult res;                                      \
        Rlist *args = NULL;                                    \
                                                               \
        RlistAppendScalar(&args, input);                       \
        if (suffix != NULL)                                    \
        {                                                      \
            RlistAppendScalar(&args, suffix);                  \
        }                                                      \
                                                               \
        FnCall *call = FnCallNew("basename", args);            \
                                                               \
        res = FnCallBasename(NULL, NULL, call, args);          \
        assert_string_equal(expected, (char *) res.rval.item); \
                                                               \
        RvalDestroy(res.rval);                                 \
        FnCallDestroy(call);                                   \
    }

static void test_basename(void)
{
    basename_single_testcase("/", NULL, "/");
    basename_single_testcase("//", NULL, "/");
    basename_single_testcase("///", NULL, "/");
    basename_single_testcase("///////", NULL, "/");
    basename_single_testcase("./", NULL, ".");
    basename_single_testcase(".", NULL, ".");
    basename_single_testcase("", NULL, "");

    basename_single_testcase("/foo/bar", NULL, "bar");
    basename_single_testcase("/foo/bar/", NULL, "bar");
    basename_single_testcase("//a//b///c////", NULL, "c");

    basename_single_testcase("", "", "");
    basename_single_testcase("/", "", "/");
    basename_single_testcase("/foo/bar.txt", ".txt", "bar");
    basename_single_testcase("/foo/bar.txt/", ".txt", "bar");
    basename_single_testcase("//a//b///c////", "", "c");
    basename_single_testcase("//a//b///c////", "blah", "c");
    basename_single_testcase("//a//b///c.csv////", ".csv", "c");
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] = {
        unit_test(test_hostinnetgroup_found),
        unit_test(test_hostinnetgroup_not_found),
        unit_test(test_basename),
    };

    return run_tests(tests);
}
