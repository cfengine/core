#include <lmdump.h>
#include <diagnose.h>
#include <backup.h>
#include <string_lib.h>

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
        "\tversion - Print version information\n"
        "\thelp - Print this help menu\n"
        "\n"
        "Usage:\n"
        "\t$ cf-check command [options]\n"
        "\n"
        "Examples:\n"
        "\t$ cf-check dump -a /var/cfengine/state/cf_lastseen.lmdb\n"
        "\t$ cf-check diagnose /var/cfengine/state/*.lmdb\n"
        "\n"
    );
}

int main(int argc, char **argv)
{
    if (StringEndsWith(argv[0], "lmdump"))
    {
        // Compatibility mode; act like lmdump if symlinked or renamed:
        return lmdump_main(argc, argv);
    }

    if (argc < 2)
    {
        print_help();
        printf("No command given.\n");
        return 1;
    }

    const int cmd_argc = argc - 1;
    char **cmd_argv = argv + 1;
    char *command = cmd_argv[0];

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

    if (StringSafeEqual_IgnoreCase(command, "help") ||
        StringSafeEqual_IgnoreCase(command, "--help") ||
        StringSafeEqual_IgnoreCase(command, "-h"))
    {
        print_help();
        return 0;
    }

    if (StringSafeEqual_IgnoreCase(command, "version") ||
        StringSafeEqual_IgnoreCase(command, "--version") ||
        StringSafeEqual_IgnoreCase(command, "-V"))
    {
        print_version();
        return 0;
    }

    print_help();
    printf("Unrecognized command: '%s'\n", command);
    return 1;
}
