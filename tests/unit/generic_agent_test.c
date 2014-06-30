#include <test.h>

#include <generic_agent.h>
#include <known_dirs.h>
#include <eval_context.h>
#include <sysinfo_priv.h>
#include <loading.h>
#include <file_lib.h>
#include <misc_lib.h>                                          /* xsnprintf */


void test_load_masterfiles(void)
{
    EvalContext *ctx = EvalContextNew();
    DiscoverVersion(ctx);

    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON);

    GenericAgentConfigSetInputFile(config, NULL,
                                   ABS_TOP_SRCDIR "/masterfiles/promises.cf");

    Policy *masterfiles = LoadPolicy(ctx, config);
    assert_true(masterfiles);

    PolicyDestroy(masterfiles);
    GenericAgentFinalize(ctx, config);
}

void test_resolve_absolute_input_path(void)
{
    assert_string_equal("/abs/aux.cf", GenericAgentResolveInputPath(NULL, "/abs/aux.cf"));
}

void test_resolve_non_anchored_base_path(void)
{
    static char inputdir[CF_BUFSIZE] = "";

    /*
     * Can not use GetInputDir() because that will return the configured $(sys.inputdir) as
     * the environment variable CFENGINE_TEST_OVERRIDE_WORKDIR is not set.
    */
    xsnprintf(inputdir, CF_BUFSIZE, "%s%cinputs", CFWORKDIR, FILE_SEPARATOR);

    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON);
    GenericAgentConfigSetInputFile(config, inputdir, "promises.cf");

    assert_string_equal("/workdir/inputs", config->input_dir);
    assert_string_equal("/workdir/inputs/promises.cf", config->input_file);

    assert_string_equal("/workdir/inputs/aux.cf", GenericAgentResolveInputPath(config, "aux.cf"));
    assert_string_equal("/workdir/inputs/rel/aux.cf", GenericAgentResolveInputPath(config, "rel/aux.cf"));
    assert_string_equal("/workdir/inputs/./aux.cf", GenericAgentResolveInputPath(config, "./aux.cf"));
    assert_string_equal("/workdir/inputs/./rel/aux.cf", GenericAgentResolveInputPath(config, "./rel/aux.cf"));
}

void test_resolve_relative_base_path(void)
{
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON);
    GenericAgentConfigSetInputFile(config, CFWORKDIR, "./inputs/promises.cf");

    assert_string_equal("./inputs/aux.cf", GenericAgentResolveInputPath(config, "aux.cf"));
    assert_string_equal("./inputs/rel/aux.cf", GenericAgentResolveInputPath(config, "rel/aux.cf"));
    assert_string_equal("./inputs/./aux.cf", GenericAgentResolveInputPath(config, "./aux.cf"));
    assert_string_equal("./inputs/./rel/aux.cf", GenericAgentResolveInputPath(config, "./rel/aux.cf"));
}

int main()
{
    strcpy(CFWORKDIR, "/workdir");

    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        // disabled masterfiles load test for now
        /* unit_test(test_load_masterfiles),*/
        unit_test(test_resolve_absolute_input_path),
        unit_test(test_resolve_non_anchored_base_path),
        unit_test(test_resolve_relative_base_path),
    };

    return run_tests(tests);
}
