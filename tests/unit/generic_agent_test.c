#include <test.h>

#include <generic_agent.h>
#include <known_dirs.h>
#include <eval_context.h>
#include <sysinfo_priv.h>
#include <loading.h>
#include <file_lib.h>
#include <misc_lib.h>                                          /* xsnprintf */
#include <crypto.h>
#include <string_lib.h>

char TEMPDIR[] = "/tmp/generic_agent_test_XXXXXX";

void test_have_tty_interactive_failsafe_is_not_created(void)
{
     CryptoInitialize();

     bool simulate_tty_interactive = true;

    EvalContext *ctx = EvalContextNew();
    GenericAgentConfig *config =
            GenericAgentConfigNewDefault(AGENT_TYPE_COMMON,
                                         simulate_tty_interactive);

    /* Just make sure that file doesn't exist. */
    GenericAgentConfigSetInputFile(config, NULL,
                                   "/masterfiles/non_existing.cf");

    /* This is where failsafe.cf will be created. */
    char *failsafe_file = StringFormat("%s%c%s",
                                       GetInputDir(),
                                       FILE_SEPARATOR,
                                       "failsafe.cf");
    Policy *policy = SelectAndLoadPolicy(config, ctx, false, false);
    struct stat buf;

    /* failsafe.cf shouldn't be created as we have tty_interactive. */
    assert_int_equal(stat(failsafe_file, &buf), -1);

    free(failsafe_file);

    PolicyDestroy(policy);
    GenericAgentFinalize(ctx, config);

}

void test_dont_have_tty_interactive_failsafe_is_created(void)
{
     CryptoInitialize();

     bool simulate_tty_interactive = false;

    EvalContext *ctx = EvalContextNew();
    GenericAgentConfig *config =
        GenericAgentConfigNewDefault(AGENT_TYPE_COMMON,
                                     simulate_tty_interactive);

    /* Just make sure that file doesn't exist. */
    GenericAgentConfigSetInputFile(config,
                                   NULL,
                                   "/masterfiles/non_existing.cf");

    /* This is where failsafe.cf will be created. */
    char *failsafe_file =
        StringFormat("%s%c%s", GetInputDir(), FILE_SEPARATOR,  "failsafe.cf");
    Policy *policy = SelectAndLoadPolicy(config, ctx, false, false);
    struct stat buf;

    /* failsafe.cf should be created as we don't have tty_interactive. */
    assert_int_equal(stat(failsafe_file, &buf), 0);

    unlink(failsafe_file);
    free(failsafe_file);

    PolicyDestroy(policy);
    GenericAgentFinalize(ctx, config);

}

void test_resolve_absolute_input_path(void)
{
    assert_string_equal("/abs/aux.cf", GenericAgentResolveInputPath(NULL, "/abs/aux.cf"));
}

void test_resolve_non_anchored_base_path(void)
{
    char inputdir[CF_BUFSIZE] = "";

    strlcpy(inputdir, GetInputDir(), sizeof(inputdir));

    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON, false);
    GenericAgentConfigSetInputFile(config, inputdir, "promises.cf");

    char testpath[CF_BUFSIZE];

    xsnprintf(testpath, sizeof(testpath), "%s%s", TEMPDIR, "/inputs");
    assert_string_equal(testpath, config->input_dir);
    xsnprintf(testpath, sizeof(testpath), "%s%s", TEMPDIR, "/inputs/promises.cf");
    assert_string_equal(testpath, config->input_file);

    xsnprintf(testpath, sizeof(testpath), "%s%s", TEMPDIR, "/inputs/aux.cf");
    assert_string_equal(testpath, GenericAgentResolveInputPath(config, "aux.cf"));
    xsnprintf(testpath, sizeof(testpath), "%s%s", TEMPDIR, "/inputs/rel/aux.cf");
    assert_string_equal(testpath, GenericAgentResolveInputPath(config, "rel/aux.cf"));
    xsnprintf(testpath, sizeof(testpath), "%s%s", TEMPDIR, "/inputs/./aux.cf");
    assert_string_equal(testpath, GenericAgentResolveInputPath(config, "./aux.cf"));
    xsnprintf(testpath, sizeof(testpath), "%s%s", TEMPDIR, "/inputs/./rel/aux.cf");
    assert_string_equal(testpath, GenericAgentResolveInputPath(config, "./rel/aux.cf"));

    GenericAgentConfigDestroy(config);
}

void test_resolve_relative_base_path(void)
{
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON, false);
    GenericAgentConfigSetInputFile(config, GetWorkDir(), "./inputs/promises.cf");

    assert_string_equal("./inputs/aux.cf", GenericAgentResolveInputPath(config, "aux.cf"));
    assert_string_equal("./inputs/rel/aux.cf", GenericAgentResolveInputPath(config, "rel/aux.cf"));
    assert_string_equal("./inputs/./aux.cf", GenericAgentResolveInputPath(config, "./aux.cf"));
    assert_string_equal("./inputs/./rel/aux.cf", GenericAgentResolveInputPath(config, "./rel/aux.cf"));

    GenericAgentConfigDestroy(config);
}

int main()
{
    if (mkdtemp(TEMPDIR) == NULL)
    {
        fprintf(stderr, "Could not create temporary directory\n");
        return 1;
    }
    char *inputs = NULL;
    xasprintf(&inputs, "%s/inputs", TEMPDIR);
    mkdir(inputs, 0755);
    free(inputs);

    char *env_var = NULL;
    xasprintf(&env_var, "CFENGINE_TEST_OVERRIDE_WORKDIR=%s", TEMPDIR);
    // Will leak, but that's how crappy putenv() is.
    putenv(env_var);

    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_resolve_absolute_input_path),
        unit_test(test_resolve_non_anchored_base_path),
        unit_test(test_resolve_relative_base_path),
        unit_test(test_have_tty_interactive_failsafe_is_not_created),
        unit_test(test_dont_have_tty_interactive_failsafe_is_created),
    };

    int ret = run_tests(tests);

    char rm_rf[] = "rm -rf ";
    char cmd[sizeof(rm_rf) + sizeof(TEMPDIR)];
    xsnprintf(cmd, sizeof(cmd), "%s%s", rm_rf, TEMPDIR);
    ARG_UNUSED int ignore = system(cmd);

    return ret;
}
