#include <lmdump.h>
#include <diagnose.h>
#include <backup.h>
#include <repair.h>
#include <string_lib.h>
#include <logging.h>
#include <man.h>

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
        "\t$ cf-check command [options]\n"
        "\n"
        "Examples:\n"
        "\t$ cf-check dump -a " WORKDIR "/state/cf_lastseen.lmdb\n"
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

// static const Description COMMANDS[] =
// {
//     {"help",     "Prints general help or per topic",
//                  "cf-check help [command]"},
//     {"diagnose", "Assess the health of one or more database files",
//                  "cf-check diagnose"},
//     {"backup",   "Copy database files to a timestamped folder",
//                  "cf-check backup"},
//     {"repair",   "Diagnose, then backup and delete any corrupt databases",
//                  "cf-check repair"},
//     {"dump",     "Print the contents of a database file",
//                  "cf-check dump -a " WORKDIR "/state/cf_lastseen.lmdb"},
//     {NULL, NULL, NULL}
// };

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

int main(int argc, const char *const *argv)
{
    if (StringEndsWith(argv[0], "lmdump"))
    {
        // Compatibility mode; act like lmdump if symlinked or renamed:
        return lmdump_main(argc, argv);
    }

    // When run separately it makes sense for cf-check to have INFO messages
    LogSetGlobalLevel(LOG_LEVEL_INFO);
    // In agent, NOTICE log level is default, and cf-check functions
    // will print less information

    if (argc < 2)
    {
        print_help();
        Log(LOG_LEVEL_ERR, "No command given");
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
                return EXIT_SUCCESS;
                break;
            }
            case 'h':
            {
                print_help();
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
                             true);
                FileWriterDetach(out);
                return EXIT_SUCCESS;
                break;
            }
            default:
            {
                return EXIT_FAILURE;
                break;
            }
        }
    }
    const char *const *const cmd_argv = argv + optind;
    int cmd_argc = argc - optind;
    const char *command = cmd_argv[0];

    if (StringSafeEqual_IgnoreCase(command, "lmdump") ||
        StringSafeEqual_IgnoreCase(command, "dump"))
    {
        return lmdump_main(cmd_argc, cmd_argv);
    }
    if (StringSafeEqual_IgnoreCase(command, "diagnose"))
    {
        return diagnose_main(cmd_argc, cmd_argv);
    }
    if (StringSafeEqual_IgnoreCase(command, "backup"))
    {
        return backup_main(cmd_argc, cmd_argv);
    }
    if (StringSafeEqual_IgnoreCase(command, "repair") ||
        StringSafeEqual_IgnoreCase(command, "remediate"))
    {
        return repair_main(cmd_argc, cmd_argv);
    }
    if (StringSafeEqual_IgnoreCase(command, "help"))
    {
        print_help();
        return EXIT_SUCCESS;
    }
    if (StringSafeEqual_IgnoreCase(command, "version"))
    {
        print_version();
        return EXIT_SUCCESS;
    }

    print_help();
    Log(LOG_LEVEL_ERR, "Unrecognized command: '%s'", command);
    return EXIT_FAILURE;
}
