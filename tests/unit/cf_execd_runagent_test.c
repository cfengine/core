#include <test.h>

#include <cf-execd-runagent.h>

#include <platform.h>
#include <alloc.h>
#include <string_lib.h>

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

/* A request carrying whitespace or a quote would inject extra cf-runagent
 * arguments; it must be refused before any command is run, so the reply is
 * the malformed-request error and never a command's output. */
static void test_injection_requests_refused(void)
{
    static const char *const injections[] = {
        "host001 --remote-bundles=foo", /* space */
        "host001\t-f /tmp/evil.cf",     /* tab */
        "host001 -D attacker_class",    /* space + option */
        "'host001'",                    /* single quote */
        "\"host001\"",                  /* double quote */
        "host`id`",                     /* backtick */
        "",                             /* empty */
    };

    for (size_t i = 0; i < sizeof(injections) / sizeof(injections[0]); i++)
    {
        char *reply = RunRequest(injections[i]);
        assert_true(strstr(reply, "Malformed runagent request") != NULL);
        assert_true(strstr(reply, "\"output\"") == NULL);
        free(reply);
    }
}

/* A plain host specification must not be refused. cf-runagent is not actually
 * present in the test environment, so it is never run successfully, but the
 * reply must not be the malformed-request error. */
static void test_valid_requests_not_refused(void)
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
        char *reply = RunRequest(valids[i]);
        assert_true(strstr(reply, "Malformed runagent request") == NULL);
        free(reply);
    }
}

int main(void)
{
    PRINT_TEST_BANNER();

    /* Keep GetBinDir() (used when building the cf-runagent command) inside a
     * throwaway directory rather than the real work directory. */
    setenv("CFENGINE_TEST_OVERRIDE_WORKDIR", "/tmp", 1);

    /* cf-runagent output goes to a pipe; a failed exec in the child must not
     * knock over the test with SIGPIPE. */
    signal(SIGPIPE, SIG_IGN);

    const UnitTest tests[] = {
        unit_test(test_injection_requests_refused),
        unit_test(test_valid_requests_not_refused),
    };

    return run_tests(tests);
}
