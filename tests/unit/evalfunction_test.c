#include <test.h>

/* Protect against duplicate definition of symbol CF_FNCALL_TYPES since we are
 * including evalfunction.c */
#define CFENGINE_EVALFUNCTION_TEST_C

#include <eval_context.h>
#include <evalfunction.c>

#ifndef __MINGW32__
#include <sys/mman.h>
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

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

static void test_module_protocol_percent_no_delimiter(void)
{
    EvalContext *ctx = EvalContextNew();
    StringSet *tags = StringSetNew();
    long persistence = 0;
    char context[CF_BUFSIZE] = "test";

    /* A well-formed '%name=<json>' line still defines the container, so the
     * added delimiter check does not reject valid input. */
    char *ok = xstrdup("%good={\"k\":1}");
    ModuleProtocol(ctx, "/dev/null", ok, false, context, sizeof(context),
                   tags, &persistence);
    VarRef *good = VarRefParseFromScope("good", context);
    assert_true(EvalContextVariableGet(ctx, good, NULL) != NULL);
    VarRefDestroy(good);
    free(ok);

#ifndef __MINGW32__
    /* A '%' line with no '=' makes "length - strlen(name) - 1 - 1" underflow
     * to SIZE_MAX and steps the source pointer one past the terminating NUL,
     * so BufferAppend() scans off the end of the line buffer. Place the line
     * so its NUL is the last readable byte before a PROT_NONE guard page: the
     * unpatched code walks into the guard page (crashes), the patched code
     * skips the line. */
    const size_t pagesize = (size_t) sysconf(_SC_PAGESIZE);
    char *region = mmap(NULL, pagesize * 2, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert_true(region != MAP_FAILED);
    assert_int_equal(mprotect(region + pagesize, pagesize, PROT_NONE), 0);

    const char *bad = "%bad";
    char *line = region + pagesize - strlen(bad) - 1; /* NUL at page boundary */
    memcpy(line, bad, strlen(bad) + 1);

    ModuleProtocol(ctx, "/dev/null", line, false, context, sizeof(context),
                   tags, &persistence);
    VarRef *ref = VarRefParseFromScope("bad", context);
    assert_true(EvalContextVariableGet(ctx, ref, NULL) == NULL);
    VarRefDestroy(ref);

    assert_int_equal(munmap(region, pagesize * 2), 0);
#endif

    StringSetDestroy(tags);
    EvalContextDestroy(ctx);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] = {
        unit_test(test_hostinnetgroup_found),
        unit_test(test_hostinnetgroup_not_found),
        unit_test(test_basename),
        unit_test(test_module_protocol_percent_no_delimiter),
    };

    return run_tests(tests);
}
