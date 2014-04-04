#include <test.h>

#include <stdlib.h>
#include <cmockery.h>
#include <configuration.h>
#include <command_line.h>

static void test_parse(void)
{
    Configuration *configuration = NULL;
    char *empty_command_line[] = { "this" };
    assert_int_equal (-1, parse(1, empty_command_line, &configuration));
    assert_true (configuration == NULL);

    char *common_command_line[] = { "this", "-b", "backup.sh", "-s",
                                    "backup.tar.gz", "-i", "rpm", "-ivh",
                                    "package.rpm"};
    assert_int_equal(0, parse(9, common_command_line, &configuration));
    assert_true(configuration != NULL);
    assert_string_equal("backup.sh", ConfigurationBackupTool(configuration));
    assert_string_equal("backup.tar.gz", ConfigurationBackupPath(configuration));
    assert_string_equal("rpm", ConfigurationCommand(configuration));
    assert_int_equal(3, ConfigurationNumberOfArguments(configuration));
    ConfigurationDestroy(&configuration);
    assert_true(configuration == NULL);

    char *full_command_line[] = { "this", "-b", "backup.sh", "-s",
                                  "backup.tar.gz", "-c", "copy", "-f",
                                  "/opt/cfengine", "-i", "dpkg", "--install",
                                  "package.deb"
    };
    assert_int_equal(0, parse(13, full_command_line, &configuration));
    assert_true(configuration != NULL);
    assert_string_equal("backup.sh", ConfigurationBackupTool(configuration));
    assert_string_equal("backup.tar.gz", ConfigurationBackupPath(configuration));
    assert_string_equal("copy", ConfigurationCopy(configuration));
    assert_string_equal("/opt/cfengine", ConfigurationCFEnginePath(configuration));
    assert_string_equal("this", ConfigurationCFUpgrade(configuration));
    assert_string_equal("dpkg", ConfigurationCommand(configuration));
    assert_int_equal(3, ConfigurationNumberOfArguments(configuration));
    ConfigurationDestroy(&configuration);
    assert_true(configuration == NULL);
}

static void test_configuration(void)
{
    Configuration *configuration = ConfigurationNew();
    assert_true(configuration != NULL);
    assert_true(ConfigurationBackupTool(configuration) == NULL);
    assert_true(ConfigurationBackupPath(configuration) == NULL);
    assert_string_equal("/tmp/cf-upgrade", ConfigurationCopy(configuration));
    assert_string_equal("/var/cfengine/", ConfigurationCFEnginePath(configuration));
    assert_true(ConfigurationCommand(configuration) == NULL);
    assert_true(ConfigurationCFUpgrade(configuration) == NULL);
    assert_int_equal(0, ConfigurationNumberOfArguments(configuration));
    assert_false(ConfigurationPerformUpdate(configuration));
    ConfigurationDestroy(&configuration);
    assert_true(configuration == NULL);

    configuration = ConfigurationNew();
    char backup_tool[] = "backup.sh";
    char backup_path[] = "backup.tar.gz";
    char cfupgrade_copy[] = "/tmp/copy";
    char cfupgrade[] = "cf-upgrade";
    char command[] = "dpkg";
    char argument[] = "-ivh";
    char cfengine[] = "/opt/cfengine";
    ConfigurationSetBackupTool(configuration, backup_tool);
    assert_string_equal(ConfigurationBackupTool(configuration), backup_tool);
    ConfigurationSetBackupPath(configuration, backup_path);
    assert_string_equal(ConfigurationBackupPath(configuration), backup_path);
    ConfigurationSetCopy(configuration, cfupgrade_copy);
    assert_string_equal(ConfigurationCopy(configuration), cfupgrade_copy);
    ConfigurationSetCFUpgrade(configuration, cfupgrade);
    assert_string_equal(ConfigurationCFUpgrade(configuration), cfupgrade);
    ConfigurationAddArgument(configuration, command);
    assert_string_equal(ConfigurationCommand(configuration), command);
    assert_string_equal(ConfigurationArgument(configuration, 0), command);
    assert_int_equal(1, ConfigurationNumberOfArguments(configuration));
    ConfigurationAddArgument(configuration, argument);
    assert_string_equal(ConfigurationArgument(configuration, 0), command);
    assert_string_equal(ConfigurationArgument(configuration, 1), argument);
    assert_int_equal(2, ConfigurationNumberOfArguments(configuration));
    ConfigurationSetCFEnginePath(configuration, cfengine);
    assert_string_equal(ConfigurationCFEnginePath(configuration), cfengine);
    assert_int_equal(2, ConfigurationNumberOfArguments(configuration));
    ConfigurationDestroy(&configuration);
    assert_true(configuration == NULL);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_parse),
        unit_test(test_configuration)
    };

    return run_tests(tests);
}

