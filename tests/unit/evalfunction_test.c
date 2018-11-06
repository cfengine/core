#include <test.h>

#include <eval_context.h>
#include <evalfunction.h>

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

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_hostinnetgroup_found),
        unit_test(test_hostinnetgroup_not_found),
    };

    return run_tests(tests);
}
