#include "test.h"

#include "generic_agent.h"
#include "env_context.h"
#include "sysinfo.h"

void test_load_masterfiles(void)
{
    EvalContext *ctx = EvalContextNew();
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON);

    GenericAgentConfigSetInputFile(config, NULL,
                                   ABS_TOP_SRCDIR "/masterfiles/promises.cf");

    Policy *masterfiles = GenericAgentLoadPolicy(ctx, config);
    assert_true(masterfiles);

    PolicyDestroy(masterfiles);
    GenericAgentConfigDestroy(config);
    EvalContextDestroy(ctx);
}

void test_resolve_absolute_input_path(void)
{
    assert_string_equal("/abs/aux.cf", GenericAgentResolveInputPath(NULL, "/abs/aux.cf"));
}

void test_resolve_non_anchored_base_path(void)
{
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON);
    GenericAgentConfigSetInputFile(config, CFWORKDIR, "promises.cf");

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
        unit_test(test_load_masterfiles),
        unit_test(test_resolve_absolute_input_path),
        unit_test(test_resolve_non_anchored_base_path),
        unit_test(test_resolve_relative_base_path),
    };

    return run_tests(tests);
}
