#include <lmdump.h>
#include <diagnose.h>
#include <string_lib.h>

int main(int argc, char **argv)
{
    if (StringEndsWith(argv[0], "lmdump"))
    {
        // Compatibility mode; act like lmdump if symlinked or renamed:
        return lmdump_main(argc, argv);
    }

    if (argc < 2)
    {
        printf("Need to supply a command, for example 'cf-check lmdump'\n");
        return 1;
    }

    const int cmd_argc = argc - 1;
    char **cmd_argv = argv + 1;
    char *command = cmd_argv[0];

    if (StringSafeEqual_IgnoreCase(command, "lmdump"))
    {
        return lmdump_main(cmd_argc, cmd_argv);
    }
    if (StringSafeEqual_IgnoreCase(command, "diagnose"))
    {
        return diagnose_main(cmd_argc, cmd_argv);
    }

    printf("Unrecognized command: '%s'\n", command);
    return 1;
}
