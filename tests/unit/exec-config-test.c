#include <test.h>

#include <exec-config.h>
#include <execd-config.h>

#include <parser.h>
#include <eval_context.h>
#include <expand.h>
#include <misc_lib.h>                                          /* xsnprintf */


static Policy *TestParsePolicy(const char *filename)
{
    char path[PATH_MAX];
    xsnprintf(path, sizeof(path), "%s/%s", TESTDATADIR, filename);

    return ParserParseFile(AGENT_TYPE_COMMON, path, PARSER_WARNING_ALL, PARSER_WARNING_ALL);
}

typedef void (*TestFn)(const EvalContext *ctx, const Policy *policy);

static void run_test_in_policy(const char *policy_filename, TestFn fn)
{
    GenericAgentConfig *agent_config = GenericAgentConfigNewDefault(
        AGENT_TYPE_EXECUTOR);
    EvalContext *ctx = EvalContextNew();
    Policy *policy = TestParsePolicy(policy_filename);
    PolicyResolve(ctx, policy, agent_config);

    /* Setup global environment */
    strcpy(VFQNAME, "localhost.localdomain");
    strcpy(VIPADDRESS, "127.0.0.100");
    EvalContextAddIpAddress(ctx, "127.0.0.100");
    EvalContextAddIpAddress(ctx, "127.0.0.101");

    fn(ctx, policy);

    PolicyDestroy(policy);
    GenericAgentFinalize(ctx, agent_config);
}

static void execd_config_empty_cb(const EvalContext *ctx, const Policy *policy)
{
    ExecdConfig *config = ExecdConfigNew(ctx, policy);

    assert_int_equal(12, StringSetSize(config->schedule));
    assert_int_equal(true, StringSetContains(config->schedule, "Min00"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min05"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min10"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min15"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min20"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min25"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min30"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min35"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min40"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min45"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min50"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min55"));
    assert_int_equal(0, config->splay_time);
    assert_string_equal("LOG_USER", config->log_facility);

    ExecdConfigDestroy(config);
}

static void test_execd_config_empty(void)
{
    run_test_in_policy("body_executor_control_empty.cf", &execd_config_empty_cb);
}

static void execd_config_full_cb(const EvalContext *ctx, const Policy *policy)
{
    ExecdConfig *config = ExecdConfigNew(ctx, policy);

    assert_int_equal(2, StringSetSize(config->schedule));
    assert_int_equal(true, StringSetContains(config->schedule, "Min00_05"));
    assert_int_equal(true, StringSetContains(config->schedule, "Min05_10"));
    /* Splay calculation uses FQNAME and getuid(), so can't predict
       actual splay value */
    assert_int_equal(true, config->splay_time>=0 && config->splay_time<60);
    assert_string_equal("LOG_LOCAL6", config->log_facility);

    ExecdConfigDestroy(config);
}

static void test_execd_config_full(void)
{
    run_test_in_policy("body_executor_control_full.cf", &execd_config_full_cb);
}

#define THREE_HOURS (3*60*60)

static void exec_config_empty_cb(const EvalContext *ctx, const Policy *policy)
{
    ExecConfig *config = ExecConfigNew(false, ctx, policy);

    assert_int_equal(false, config->scheduled_run);
    /* FIXME: exec-config should provide default exec_command */
    assert_string_equal("", config->exec_command);
    assert_int_equal(THREE_HOURS, config->agent_expireafter);
    assert_string_equal("", config->mail_server);
    /* FIXME: exec-config should provide default from address */
    assert_string_equal("", config->mail_from_address);
    assert_string_equal("", config->mail_to_address);
    /* FIXME: exec-config should provide default subject */
    assert_string_equal("", config->mail_subject);
    assert_int_equal(30, config->mail_max_lines);
    assert_string_equal("localhost.localdomain", config->fq_name);
    assert_string_equal("127.0.0.100", config->ip_address);
    assert_string_equal("127.0.0.100 127.0.0.101", config->ip_addresses);

    ExecConfigDestroy(config);
}

static void test_exec_config_empty(void)
{
    run_test_in_policy("body_executor_control_empty.cf", &exec_config_empty_cb);
}

static void CheckFullExecConfig(const ExecConfig *config)
{
    assert_int_equal(true, config->scheduled_run);
    assert_string_equal("/bin/echo", config->exec_command);
    assert_int_equal(120, config->agent_expireafter);
    assert_string_equal("localhost", config->mail_server);
    assert_string_equal("cfengine@example.org", config->mail_from_address);
    assert_string_equal("cfengine_mail@example.org", config->mail_to_address);
    assert_string_equal("Test [localhost/127.0.0.1]", config->mail_subject);
    assert_int_equal(50, config->mail_max_lines);
    assert_string_equal("localhost.localdomain", config->fq_name);
    assert_string_equal("127.0.0.100", config->ip_address);
    assert_string_equal("127.0.0.100 127.0.0.101", config->ip_addresses);
}

static void exec_config_full_cb(const EvalContext *ctx, const Policy *policy)
{
    ExecConfig *config = ExecConfigNew(true, ctx, policy);
    CheckFullExecConfig(config);
    ExecConfigDestroy(config);
}

static void test_exec_config_full(void)
{
    run_test_in_policy("body_executor_control_full.cf", &exec_config_full_cb);
}

static void exec_config_copy_cb(const EvalContext *ctx, const Policy *policy)
{
    ExecConfig *config = ExecConfigNew(true, ctx, policy);
    ExecConfig *config2 = ExecConfigCopy(config);
    ExecConfigDestroy(config);

    CheckFullExecConfig(config2);

    ExecConfigDestroy(config2);
}

static void test_exec_config_copy(void)
{
    run_test_in_policy("body_executor_control_full.cf", &exec_config_copy_cb);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_execd_config_empty),
        unit_test(test_execd_config_full),
        unit_test(test_exec_config_empty),
        unit_test(test_exec_config_full),
        unit_test(test_exec_config_copy),
    };

    return run_tests(tests);
}
