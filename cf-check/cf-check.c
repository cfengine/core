#include <lmdump.h>
#include <dump.h>
#include <diagnose.h>
#include <backup.h>
#include <repair.h>
#include <string_lib.h>
#include <logging.h>
#include <man.h>
#include <cleanup.h>            /* CallCleanupFunctions() */

static void print_version()
{
    printf("cf-check BETA version %s\n", VERSION);
}

static void print_help()
{
    printf(
        "\n"
        "cf-check:\n"
        "\tUtility for diagnosis and repair of local CFEngine databases.\n"
        "\tThis BETA version of the tool is for testing purposes only.\n"
        "\n"
        "Commands:\n"
        "\tdump - Print the contents of a database file\n"
        "\tdiagnose - Assess the health of one or more database files\n"
        "\tbackup - Copy database files to a timestamped folder\n"
        "\trepair - Diagnose, then backup and delete any corrupt databases\n"
        "\tversion - Print version information\n"
        "\thelp - Print this help menu\n"
        "\n"
        "Usage:\n"
        "\t$ cf-check <command> [options] [file ...]\n"
        "\n"
        "Examples:\n"
        "\t$ cf-check dump " WORKDIR "/state/cf_lastseen.lmdb\n"
        "\t$ cf-check lmdump -a " WORKDIR "/state/cf_lastseen.lmdb\n"
        "\t$ cf-check diagnose\n"
        "\t$ cf-check repair\n"
        "\n");
}

static const char *const CF_CHECK_SHORT_DESCRIPTION =
    "Utility for diagnosis and repair of local CFEngine databases.";

static const char *const CF_CHECK_MANPAGE_LONG_DESCRIPTION =
    "cf-check does not evaluate policy or rely on the integrity of the\n"
    "databases. It is intended to be able to detect and repair a corrupt\n"
    "database.";

static const Description COMMANDS[] =
{
    {"help",     "Prints general help or per topic",
                 "cf-check help [command]"},
    {"diagnose", "Assess the health of one or more database files",
                 "cf-check diagnose"},
    {"backup",   "Backup database files to a timestamped folder",
                 "cf-check backup"},
    {"repair",   "Diagnose, then backup and delete any corrupt databases",
                 "cf-check repair"},
    {"dump",     "Print the contents of a database file",
                 "cf-check dump " WORKDIR "/state/cf_lastseen.lmdb"},
    {"lmdump",   "LMDB database dumper (deprecated)",
                 "cf-check lmdump -a " WORKDIR "/state/cf_lastseen.lmdb"},
    {NULL, NULL, NULL}
};

static const struct option OPTIONS[] =
{
    {"help",        optional_argument,  0, 'h'},
    {"manpage",     no_argument,        0, 'M'},
    {"version",     no_argument,        0, 'V'},
    {"debug",       no_argument,        0, 'd'},
    {"verbose",     no_argument,        0, 'v'},
    {"log-level",   required_argument,  0, 'g'},
    {"inform",      no_argument,        0, 'I'},
    {NULL,          0,                  0, '\0'}
};

static const char *const HINTS[] =
{
    "Print the help message",
    "Print the man page",
    "Output the version of the software",
    "Enable debugging output",
    "Enable verbose output",
    "Specify how detailed logs should be. Possible values: 'error', 'warning', 'notice', 'info', 'verbose', 'debug'",
    "Enable basic information output",
    NULL
};

static int CFCheckHelpTopic(const char *topic)
{
    assert(topic != NULL);
    bool found = false;
    for (int i = 0; COMMANDS[i].name != NULL; ++i)
    {
        if (strcmp(COMMANDS[i].name, topic) == 0)
        {
            printf("Command:     %s\n", COMMANDS[i].name);
            printf("Usage:       %s\n", COMMANDS[i].usage);
            printf("Description: %s\n", COMMANDS[i].description);
            found = true;
            break;
        }
    }

    // Add more detailed explanation here if necessary:
    if (strcmp("help", topic) == 0)
    {
        printf("\nYou did it, you used the help command!\n");
    }
    else
    {
        if (!found)
        {
            printf("Unknown help topic: '%s'\n", topic);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int main(int argc, const char *const *argv)
{
    if (StringEndsWith(argv[0], "lmdump"))
    {
        // Compatibility mode; act like lmdump if symlinked or renamed:
        int ret = lmdump_main(argc, argv);
        CallCleanupFunctions();
        return ret;
    }

    // When run separately it makes sense for cf-check to have INFO messages
    LogSetGlobalLevel(LOG_LEVEL_INFO);
    // In agent, NOTICE log level is default, and cf-check functions
    // will print less information

    if (argc < 2)
    {
        print_help();
        Log(LOG_LEVEL_ERR, "No command given");
        CallCleanupFunctions();
        return EXIT_FAILURE;
    }

    int c = 0;
    int start_index = 1;
    const char *optstr = "+hMg:dvI"; // + means stop for non opt arg. :)
    while ((c = getopt_long(argc, (char *const *) argv, optstr, OPTIONS, &start_index))
           != -1)
    {
        switch (c)
        {
        case 'd':
        {
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            break;
        }
        case 'v':
        {
            LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
            break;
        }
        case 'I':
        {
            LogSetGlobalLevel(LOG_LEVEL_INFO);
            break;
        }
        case 'g':
        {
            LogSetGlobalLevelArgOrExit(optarg);
            break;
        }
        case 'V':
        {
            print_version();
            CallCleanupFunctions();
            return EXIT_SUCCESS;
            break;
        }
        case 'h':
        {
            print_help();
            CallCleanupFunctions();
            return EXIT_SUCCESS;
            break;
        }
        case 'M':
        {
            Writer *out = FileWriter(stdout);
            ManPageWrite(out, "cf-check", time(NULL),
                         CF_CHECK_SHORT_DESCRIPTION,
                         CF_CHECK_MANPAGE_LONG_DESCRIPTION,
                         OPTIONS, HINTS,
                         NULL, false,
                         true);
            FileWriterDetach(out);
            CallCleanupFunctions();
            return EXIT_SUCCESS;
            break;
        }
        default:
        {
            CallCleanupFunctions();
            return EXIT_FAILURE;
            break;
        }
        }
    }
    const char *const *const cmd_argv = argv + optind;
    int cmd_argc = argc - optind;
    const char *command = cmd_argv[0];

    if (StringEqual_IgnoreCase(command, "lmdump"))
    {
        int ret = lmdump_main(cmd_argc, cmd_argv);
        CallCleanupFunctions();
        return ret;
    }
    if (StringEqual_IgnoreCase(command, "dump"))
    {
        int ret = dump_main(cmd_argc, cmd_argv);
        CallCleanupFunctions();
        return ret;
    }
    if (StringEqual_IgnoreCase(command, "diagnose"))
    {
        int ret = diagnose_main(cmd_argc, cmd_argv);
        CallCleanupFunctions();
        return ret;
    }
    if (StringEqual_IgnoreCase(command, "backup"))
    {
        int ret = backup_main(cmd_argc, cmd_argv);
        CallCleanupFunctions();
        return ret;
    }
    if (StringEqual_IgnoreCase(command, "repair") ||
        StringEqual_IgnoreCase(command, "remediate"))
    {
        int ret = repair_main(cmd_argc, cmd_argv);
        CallCleanupFunctions();
        return ret;
    }
    if (StringEqual_IgnoreCase(command, "help"))
    {
        if (cmd_argc > 2)
        {
            Log(LOG_LEVEL_ERR, "help takes exactly 0 or 1 arguments");
            CallCleanupFunctions();
            return EXIT_FAILURE;
        }
        else if (cmd_argc <= 1)
        {
            print_help();
        }
        else
        {
            assert(cmd_argc == 2);
            CFCheckHelpTopic(cmd_argv[1]);
        }
        CallCleanupFunctions();
        return EXIT_SUCCESS;
    }
    if (StringEqual_IgnoreCase(command, "version"))
    {
        print_version();
        CallCleanupFunctions();
        return EXIT_SUCCESS;
    }

    print_help();
    Log(LOG_LEVEL_ERR, "Unrecognized command: '%s'", command);
    CallCleanupFunctions();
    return EXIT_FAILURE;
}
