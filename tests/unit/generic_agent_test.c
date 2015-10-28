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

char CFWORKDIR[CF_BUFSIZE];

void test_load_masterfiles(void)
{
    EvalContext *ctx = EvalContextNew();
    DiscoverVersion(ctx);

    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON, false);

    GenericAgentConfigSetInputFile(config, NULL,
                                   ABS_TOP_SRCDIR "/masterfiles/promises.cf");

    Policy *masterfiles = LoadPolicy(ctx, config);
    assert_true(masterfiles);

    PolicyDestroy(masterfiles);
    GenericAgentFinalize(ctx, config);
}

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
    SelectAndLoadPolicy(config, ctx, false);
    struct stat buf;
    
    /* failsafe.cf shouldn't be created as we have tty_interactive. */
    assert_int_equal(stat(failsafe_file, &buf), -1);
    
    free(failsafe_file);
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
    SelectAndLoadPolicy(config, ctx, false);
    struct stat buf;
 
    /* failsafe.cf should be created as we don't have tty_interactive. */
    assert_int_equal(stat(failsafe_file, &buf), 0);
    
    unlink(failsafe_file);
    free(failsafe_file);
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

    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON, false);
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
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON, false);
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
        unit_test(test_have_tty_interactive_failsafe_is_not_created),
        unit_test(test_dont_have_tty_interactive_failsafe_is_created),
    };

    return run_tests(tests);
}
