#include <test.h>

#include <cf-execd-runagent.h>

#include <platform.h>
#include <alloc.h>
#include <string_lib.h>
#include <writer.h>
#include <json.h>
#include <known_dirs.h>

static char TEST_WORKDIR[] = "/tmp/cf_execd_runagent_test.XXXXXX";
static char *STUB_PATH = NULL;

/* Install a stand-in for <bindir>/cf-runagent that prints each of its
 * arguments on a line of its own, so the tests see exactly the argv that
 * HandleRunagentRequest() hands to cf-runagent. */
static void InstallRunagentStub(void)
{
    assert_true(mkdtemp(TEST_WORKDIR) != NULL);
    setenv("CFENGINE_TEST_OVERRIDE_WORKDIR", TEST_WORKDIR, 1);

    const char *bindir = GetBinDir();
    assert_true(StringStartsWith(bindir, TEST_WORKDIR));
    assert_int_equal(mkdir(bindir, 0755), 0);

    xasprintf(&STUB_PATH, "%s/cf-runagent", bindir);
    FILE *stub = fopen(STUB_PATH, "w");
    assert_true(stub != NULL);
    fputs("#!/bin/sh\nprintf '%s\\n' \"$@\"\n", stub);
    assert_int_equal(fclose(stub), 0);
    assert_int_equal(chmod(STUB_PATH, 0755), 0);
}

static void RemoveRunagentStub(void)
{
    unlink(STUB_PATH);
    rmdir(GetBinDir());
    rmdir(TEST_WORKDIR);
    free(STUB_PATH);
}

/* Send 'request' (with a trailing newline, as a socket client does) to
 * HandleRunagentRequest() over a socketpair and return the reply it wrote,
 * NUL-terminated. Caller frees. */
static char *RunRequest(const char *request)
{
    int sv[2];
    assert_int_equal(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    char *line = StringConcatenate(2, request, "\n");
    ssize_t to_write = (ssize_t) strlen(line);
    assert_int_equal(write(sv[0], line, to_write), to_write);
    free(line);
    shutdown(sv[0], SHUT_WR);

    /* The dup is because HandleRunagentRequest() takes ownership of the fd
     * (fdopen() + fclose()). */
    HandleRunagentRequest(dup(sv[1]), "/nonexistent/cf-agent-run");
    close(sv[1]);

    size_t cap = 4096;
    char *reply = xmalloc(cap);
    size_t len = 0;
    ssize_t n;
    while ((n = read(sv[0], reply + len, cap - 1 - len)) > 0)
    {
        len += (size_t) n;
        if (len == cap - 1)
        {
            cap *= 2;
            reply = xrealloc(reply, cap);
        }
    }
    reply[len] = '\0';
    close(sv[0]);
    return reply;
}

/* Assert that the stub was run as "cf-runagent -b -H <request>", with the
 * whole request as the single argument of -H. */
static void AssertRequestPassedAsSingleArgument(const char *request)
{
    char *reply = RunRequest(request);

    char *stub_output = StringConcatenate(3, "-b\n-H\n", request, "\n");
    Writer *encoded = StringWriter();
    JsonEncodeStringWriter(stub_output, encoded);
    char *expected;
    xasprintf(&expected, "\"output\" : \"%s\",", StringWriterData(encoded));

    assert_true(strstr(reply, expected) != NULL);
    assert_true(strstr(reply, "\"exit_code\": 0") != NULL);

    free(expected);
    WriterClose(encoded);
    free(stub_output);
    free(reply);
}

/* A request carrying whitespace or quotes must reach cf-runagent as one
 * argument rather than being split into further options. */
static void test_injection_requests_stay_one_argument(void)
{
    static const char *const injections[] = {
        "host001 --remote-bundles=foo", /* space */
        "host001\t-f /tmp/evil.cf",     /* tab */
        "host001 -D attacker_class",    /* space + option */
        "'host001' -s attacker.pub",    /* single quotes */
        "\"host001\" -f /tmp/evil.cf",  /* double quotes */
        "host`id`",                     /* backticks */
    };

    for (size_t i = 0; i < sizeof(injections) / sizeof(injections[0]); i++)
    {
        AssertRequestPassedAsSingleArgument(injections[i]);
    }
}

static void test_valid_requests_passed_through(void)
{
    static const char *const valids[] = {
        "host001.example.com",
        "192.0.2.10",
        "2001:db8::1",         /* IPv6, contains colons */
        "host001:5308",        /* host:port */
        "a.example,b.example", /* comma-separated host list */
    };

    for (size_t i = 0; i < sizeof(valids) / sizeof(valids[0]); i++)
    {
        AssertRequestPassedAsSingleArgument(valids[i]);
    }
}

/* An empty request would become '-H ""', which makes cf-runagent hail the
 * hosts from policy instead, so it must be refused before anything is run. */
static void test_empty_request_refused(void)
{
    char *reply = RunRequest("");
    assert_true(strstr(reply, "Empty runagent request") != NULL);
    assert_true(strstr(reply, "\"output\"") == NULL);
    free(reply);
}

int main(void)
{
    PRINT_TEST_BANNER();

    InstallRunagentStub();

    const UnitTest tests[] = {
        unit_test(test_injection_requests_stay_one_argument),
        unit_test(test_valid_requests_passed_through),
        unit_test(test_empty_request_refused),
    };

    int ret = run_tests(tests);

    RemoveRunagentStub();
    return ret;
}
