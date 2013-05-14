#include "test.h"

#include "exec-config.h"

#include "parser.h"
#include "env_context.h"
#include "generic_agent.h"

static Policy *LoadPolicy(const char *filename)
{
    char path[1024];
    sprintf(path, "%s/%s", TESTDATADIR, filename);

    return ParserParseFile(path, PARSER_WARNING_ALL, PARSER_WARNING_ALL);
}

static void TestCheckConfigIsDefault(ExecConfig *c)
{
    assert_int_equal(10800, c->agent_expireafter);
    assert_string_equal("", c->exec_command);
    assert_string_equal("LOG_USER",c->log_facility);
    assert_string_equal("",c->mail_from_address);
    assert_int_equal(30, c->mail_max_lines);
    assert_string_equal("", c->mail_server);
    assert_string_equal("",c->mail_to_address);
    assert_int_equal(0, c->splay_time);

    assert_int_equal(12, StringSetSize(c->schedule));
}

static void test_new_destroy(void)
{
    ExecConfig *c = ExecConfigNewDefault(true, "host", "ip");
    TestCheckConfigIsDefault(c);
    ExecConfigDestroy(c);
}

static void test_load(void)
{
    GenericAgentConfig *agent_config = GenericAgentConfigNewDefault(AGENT_TYPE_EXECUTOR);
    ExecConfig *c = ExecConfigNewDefault(true, "host", "ip");

    assert_true(c->scheduled_run);
    assert_string_equal("host", c->fq_name);
    assert_string_equal("ip", c->ip_address);

    TestCheckConfigIsDefault(c);

    EvalContext *ctx = EvalContextNew();
    {
        VarRef lval = VarRefParse("g.host");
        EvalContextVariablePut(ctx, lval, (Rval) { "snookie", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }

    // provide a full body executor control and check that all options are collected
    {
        Policy *p = LoadPolicy("body_executor_control_full.cf");
        HashControls(ctx, p, agent_config);

        ExecConfigUpdate(ctx, p, c);

        assert_true(c->scheduled_run);
        assert_string_equal("host", c->fq_name);
        assert_string_equal("ip", c->ip_address);

        assert_int_equal(120, c->agent_expireafter);
        assert_string_equal("/bin/echo", c->exec_command);
        assert_string_equal("LOG_LOCAL6",c->log_facility);
        assert_string_equal("cfengine@snookie.example.org",c->mail_from_address);
        assert_int_equal(50, c->mail_max_lines);
        assert_string_equal("localhost", c->mail_server);
        assert_string_equal("cfengine_mail@example.org",c->mail_to_address);

        // splay time hard to test (pseudo random)

        assert_int_equal(2, StringSetSize(c->schedule));
        assert_true(StringSetContains(c->schedule, "Min00_05"));
        assert_true(StringSetContains(c->schedule, "Min05_10"));

        PolicyDestroy(p);
    }

    // provide a small policy and check that missing settings are being reverted to default
    {
        {
            Policy *p = LoadPolicy("body_executor_control_agent_expireafter_only.cf");
            HashControls(ctx, p, agent_config);

            ExecConfigUpdate(ctx, p, c);

            assert_true(c->scheduled_run);
            assert_string_equal("host", c->fq_name);
            assert_string_equal("ip", c->ip_address);

            assert_int_equal(121, c->agent_expireafter);

            // rest should be default
            assert_string_equal("", c->exec_command);
            assert_string_equal("LOG_USER",c->log_facility);
            assert_string_equal("",c->mail_from_address);
            assert_int_equal(30, c->mail_max_lines);
            assert_string_equal("", c->mail_server);
            assert_string_equal("",c->mail_to_address);
            assert_int_equal(0, c->splay_time);

            assert_int_equal(12, StringSetSize(c->schedule));

            PolicyDestroy(p);
        }
    }

    EvalContextDestroy(ctx);
    GenericAgentConfigDestroy(agent_config);

}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_new_destroy),
        unit_test(test_load)
    };

    return run_tests(tests);
}

// STUBS
