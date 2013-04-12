#include "test.h"

#include "generic_agent.h"
#include "env_context.h"

void test_load_masterfiles(void **state)
{
    EvalContext *ctx = EvalContextNew();
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON);

    MINUSF = true;
    GenericAgentConfigSetInputFile(config, "../../masterfiles/promises.cf");

    Policy *masterfiles = GenericAgentLoadPolicy(ctx, config);
    assert_true(masterfiles);

    PolicyDestroy(masterfiles);
    GenericAgentConfigDestroy(config);
    EvalContextDestroy(ctx);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_load_masterfiles),
    };

    return run_tests(tests);
}
