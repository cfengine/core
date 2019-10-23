/*
  Copyright 2019 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; version 3.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/


#define CF_NET_VERSION "0.1.2"

#include <platform.h>
#include <libgen.h>
#include <client_code.h>        // ServerConnection
#include <logging.h>            // Log, LogSetGlobalLevel
#include <man.h>                // ManPageWrite
#include <crypto.h>             // CryptoInitialize
#include <addr_lib.h>           // ParseHostPort
#include <net.h>                // SocketConnect() SendTransaction()
#include <time.h>               // time_t, time, difftime
#include <stat_cache.h>         // cf_remote_stat
#include <string_lib.h>         // ToLowerStrInplace
#include <writer.h>
#include <policy_server.h>      // PolicyServerReadFile
#include <generic_agent.h>      // GenericAgentSetDefaultDigest TODO: rm dep
#include <cf-windows-functions.h> // TODO: move this out of libpromises
#include <known_dirs.h>           // TODO: move this 'out of libpromises
#include <cleanup.h>
#include <protocol.h>
#include <sequence.h>

#define ARG_UNUSED __attribute__((unused))

typedef struct
{
    bool debug;
    bool verbose;
    bool inform;
    bool used_default;
    char *min_tls_version;
    char *allow_ciphers;
} CFNetOptions;

//*******************************************************************
// DOCUMENTATION / GETOPT CONSTS:
//*******************************************************************

static const char *const CF_NET_SHORT_DESCRIPTION =
    "Command-Line Interface (CLI) for libcfnet.";

static const char *const CF_NET_MANPAGE_LONG_DESCRIPTION =
    "cf-net is a testing/debugging tool intended for developers as well "
    "as CFEngine users. cf-net connects to cf-serverd on a specified host "
    "and can issue arbitrary CFEngine protocol commands. Currently 4 "
    "commands are supported; connect, stat, opendir and get. This tool can "
    "be useful to check if a host is online, see if you can fetch policy or "
    "even stress test the host by running many instances of cf-net. "
    "cf-net is much more lightweight and easy to use than cf-agent, as it "
    "does not include any policy functionality. Please note that in order to "
    "connect to a host cf-net needs access to the key-pair generated by "
    "cf-key. For now, it is much easier to use cf-net on a bootstrapped "
    "host. This is due to ACLs and key-pairs and can be made easier in the "
    "future.";

static const Description COMMANDS[] =
{
    {"help",    "Prints general help or per topic",
                "cf-net help [command]"},
    {"connect", "Checks if host(s) is available by connecting",
                "cf-net -H 192.168.50.50,192.168.50.51 connect"},
    {"stat",    "Look at type of file",
                "cf-net stat masterfiles/update.cf"},
    {"get",     "Get file from server",
                "cf-net get masterfiles/update.cf -o download.cf [-jNTHREADS]\n"
                "\t\t\t(%d can be used in both the remote and output file paths when '-j' is used)"},
    {"opendir", "List files and folders in a directory",
                "cf-net opendir masterfiles"},
    {NULL, NULL, NULL}
};

static const struct option OPTIONS[] =
{
    {"help",        no_argument,        0, 'h'},
    {"manpage",     no_argument,        0, 'M'},
    {"host",        required_argument,  0, 'H'},
    {"debug",       no_argument,        0, 'd'},
    {"verbose",     no_argument,        0, 'v'},
    {"log-level",   required_argument,  0, 'g'},
    {"inform",      no_argument,        0, 'I'},
    {"tls-version", required_argument,  0, 't'},
    {"ciphers",     required_argument,  0, 'c'},
    {NULL,          0,                  0, '\0'}
};

static const char *const HINTS[] =
{
    "Print the help message",
    "Print the man page",
    "Server hostnames or IPs, comma-separated (defaults to policy server)",
    "Enable debugging output",
    "Enable verbose output",
    "Specify how detailed logs should be. Possible values: 'error', 'warning', 'notice', 'info', 'verbose', 'debug'",
    "Enable basic information output",
    "Minimum TLS version to use",
    "TLS ciphers to use (comma-separated list)",
    NULL
};

//*******************************************************************
// COMMAND ENUMS:
//*******************************************************************

#define CF_NET_COMMANDS(generator_macro) \
        generator_macro(CONNECT)         \
        generator_macro(STAT)            \
        generator_macro(GET)             \
        generator_macro(OPENDIR)         \
        generator_macro(MULTI)           \
        generator_macro(MULTITLS)        \
        generator_macro(HELP)            \
        generator_macro(INVALID)         \

#define GENERATE_ENUM(CMD_NAME) CFNET_CMD_##CMD_NAME,
#define GENERATE_STRING(CMD_NAME) #CMD_NAME,

enum command_enum
{
    CF_NET_COMMANDS(GENERATE_ENUM) command_enum_max
};

static const char *command_strings[] =
{
    CF_NET_COMMANDS(GENERATE_STRING) NULL
};


//*******************************************************************
// FUNCTION DECLARATIONS:
//*******************************************************************


// INIT:
static void CFNetSetDefault(CFNetOptions *opts);
static void CFNetInit();
static void CFNetOptionsClear(CFNetOptions *opts);

// MAIN LOGIC:
static int CFNetParse(int argc, char **argv,                        // INPUTS
               CFNetOptions *opts, char ***args, char **hostnames); // OUTPUTS
static int CFNetCommandSwitch(CFNetOptions *opts, const char *hostname,
                              char **args, enum command_enum cmd);
static int CFNetRun(CFNetOptions *opts, char **args, char *hostnames);
static char *RequireHostname(char *hostnames);

// PROTOCOL:
static AgentConnection *CFNetOpenConnection(const char *server);
static void CFNetDisconnect(AgentConnection *conn);
static int JustConnect(const char *server, char *port);

// COMMANDS:
static int CFNetHelpTopic(const char *topic);
static int CFNetHelp(const char *topic);
static int CFNetConnectSingle(const char *server, bool print);
static int CFNetConnect(const char *hostname, char **args);
static void CFNetStatPrint(const char *file, int st_mode, const char *server);
static int CFNetStat(CFNetOptions *opts, const char *hostname, char **args);
static int CFNetGet(CFNetOptions *opts, const char *hostname, char **args);
static int CFNetOpenDir(CFNetOptions *opts, const char *hostname, char **args);
static int CFNetMulti(const char *server);
static int CFNetMultiTLS(const char *server);


//*******************************************************************
// MAIN:
//*******************************************************************

int main(int argc, char **argv)
{
    CFNetOptions opts;
    CFNetSetDefault(&opts);
    char **args = NULL;
    char *hostnames = NULL;
    int ret = CFNetParse(argc, argv,                // Inputs
                         &opts, &args, &hostnames); // Outputs
    GenericAgentSetDefaultDigest(&CF_DEFAULT_DIGEST, &CF_DEFAULT_DIGEST_LEN);
    if (ret != 0)
    {
        DoCleanupAndExit(EXIT_FAILURE);
    }
    ret = CFNetRun(&opts, args, hostnames);  // Commands return exit code
    free(hostnames);
    CFNetOptionsClear(&opts);

    ret = (ret == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    DoCleanupAndExit(ret);
}

//*******************************************************************
// INIT:
//*******************************************************************

static void CFNetSetDefault(CFNetOptions *opts){
    assert(opts != NULL);
    opts->debug       = false;
    opts->verbose     = false;
    opts->inform      = false;
    opts->used_default= false;
    opts->min_tls_version = NULL;
    opts->allow_ciphers   = NULL;
}

static void CFNetOptionsClear(CFNetOptions *opts)
{
    assert(opts != NULL);
    free(opts->min_tls_version);
    free(opts->allow_ciphers);
}

static void CFNetInit(const char *min_tls_version, const char *allow_ciphers)
{
#ifdef __MINGW32__
    InitializeWindows();
    OpenNetwork();
#endif
    CryptoInitialize();
    LoadSecretKeys(NULL, NULL, NULL, NULL);
    cfnet_init(min_tls_version, allow_ciphers);
}

//*******************************************************************
// MAIN LOGIC:
//*******************************************************************

static char *RequireHostname(char *hostnames)
{
    if (hostnames == NULL)
    {
        char *policy_server = PolicyServerReadFile(GetWorkDir());
        if (policy_server == NULL)
        {
            printf("Error: no host name (and no policy_server.dat)\n");
            DoCleanupAndExit(EXIT_FAILURE);
        }
        return policy_server;
    }
    return xstrdup(hostnames);
}


static int CFNetParse(int argc, char **argv,
                      CFNetOptions *opts, char ***args, char **hostnames)
{
    assert(opts != NULL);
    if (argc <= 1)
    {
        CFNetHelp(NULL);
        return 0;
    }
    extern int optind;
    extern char *optarg;
    *hostnames = NULL;
    int c = 0;
    int start_index = 1;
    const char *optstr = "+hMg:H:dvI"; // + means stop for non opt arg. :)
    while ((c = getopt_long(argc, argv, optstr, OPTIONS, &start_index))
            != -1)
    {
        switch (c)
        {
            case 'h':
            {
                CFNetHelp(NULL);
                break;
            }
            case 'M':
            {
                // TODO: How do we actually add a man page
                Writer *out = FileWriter(stdout);
                ManPageWrite(out, "cf-net", time(NULL),
                             CF_NET_SHORT_DESCRIPTION,
                             CF_NET_MANPAGE_LONG_DESCRIPTION,
                             OPTIONS, HINTS,
                             true);
                FileWriterDetach(out);
                DoCleanupAndExit(EXIT_SUCCESS);
                break;
            }
            case 'H':
            {
                if (*hostnames != NULL)
                {
                    Log(LOG_LEVEL_INFO,
                        "Warning: multiple occurences of -H in command, "\
                        "only last one will be used.");
                    free(*hostnames);
                }
                *hostnames = xstrdup(optarg);
                break;
            }
            case 'd':
            {
                opts->debug = true;
                break;
            }
            case 'v':
            {
                opts->verbose = true;
                break;
            }
            case 'I':
            {
                opts->inform = true;
                break;
            }
            case 'g':
            {
                LogSetGlobalLevelArgOrExit(optarg);
                break;
            }
            case 't':
            {
                opts->min_tls_version = xstrdup(optarg);
                break;
            }
            case 'c':
            {
                opts->allow_ciphers = xstrdup(optarg);
                break;
            }
            default:
            {
                // printf("Default optarg = '%s', c = '%c' = %i\n",
                //        optarg, c, (int)c);
                DoCleanupAndExit(EXIT_FAILURE);
                break;
            }
        }
    }
    (*args) = &(argv[optind]);
    return 0;
}

// This converts a command string to an enum: "HELP" -> HELP
static int CFNetCommandNumber(char *command)
{
    for (int i = 0; i < command_enum_max; ++i)
    {
        if (strcmp(command, command_strings[i]) == 0)
        {
            return i;
        }
    }
    return CFNET_CMD_INVALID;
}

// ^ Macro returns on match(!)
static int CFNetCommandSwitch(CFNetOptions *opts, const char *hostname,
                              char **args, enum command_enum cmd)
{
    switch (cmd) {
        case CFNET_CMD_CONNECT:
            return CFNetConnect(hostname, args);
        case CFNET_CMD_STAT:
            return CFNetStat(opts, hostname, args);
        case CFNET_CMD_GET:
            return CFNetGet(opts, hostname, args);
        case CFNET_CMD_OPENDIR:
            return CFNetOpenDir(opts, hostname, args);
        case CFNET_CMD_MULTI:
            return CFNetMulti(hostname);
        case CFNET_CMD_MULTITLS:
            return CFNetMultiTLS(hostname);
        default:
            break;
    }
    printf("Bug: CFNetCommandSwitch() is unable to pick a command\n");
    DoCleanupAndExit(EXIT_FAILURE);
    return -1;
}

static void CFNetSetVerbosity(CFNetOptions *opts)
{
    assert(opts);
    if(opts->debug)
    {
        LogSetGlobalLevel(LOG_LEVEL_DEBUG);
        Log(LOG_LEVEL_DEBUG, "Debug log level enabled");
    }
    else if(opts->verbose)
    {
        LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
        Log(LOG_LEVEL_VERBOSE, "Verbose log level enabled");
    }
    else if(opts->inform)
    {
        LogSetGlobalLevel(LOG_LEVEL_INFO);
        Log(LOG_LEVEL_INFO, "Inform log level enabled");
    }
}

static int CFNetRun(CFNetOptions *opts, char **args, char *hostnames)
{
    assert(opts != NULL);
    assert(args != NULL);
    CFNetSetVerbosity(opts);

    char *command_name = args[0];
    if (NULL_OR_EMPTY(command_name))
    {
        printf("Error: Command missing, use cf-net --help for more info.\n");
        DoCleanupAndExit(EXIT_FAILURE);
    }
    ToUpperStrInplace(command_name);

    Log(LOG_LEVEL_VERBOSE, "Running command '%s' with argument(s):\n",
        command_name);
    for (int i = 1; args[i] != NULL; ++i)
    {
        Log(LOG_LEVEL_VERBOSE, "argv[%i]=%s\n", i, args[i]);
    }

    enum command_enum cmd = CFNetCommandNumber(command_name);

    if (cmd == CFNET_CMD_INVALID)
    {
        Log(LOG_LEVEL_ERR, "'%s' is not a valid cf-net command\n", args[0]);
        return -1;
    }
    else if (cmd == CFNET_CMD_HELP)
    {
        return CFNetHelp(args[1]);
    }

    CFNetInit(opts->min_tls_version, opts->allow_ciphers);
    char *hosts = RequireHostname(hostnames);
    int ret = 0;
    char *hostname = strtok(hosts, ",");
    while (hostname != NULL){
        CFNetCommandSwitch(opts, hostname, args, cmd);
        hostname = strtok(NULL, ",");
    }
    free(hosts);
    return ret;
}

//*******************************************************************
// PROTOCOL:
//*******************************************************************

static AgentConnection *CFNetOpenConnection(const char *server)
{
    AgentConnection *conn = NULL;
    ConnectionFlags connflags =
    {
        .protocol_version = CF_PROTOCOL_LATEST,
        .trust_server = true,
        .off_the_record = true
    };
    int err;
    char *buf = xstrdup(server);
    char *host, *port;
    ParseHostPort(buf, &host, &port);
    if (port == NULL)
    {
        port = CFENGINE_PORT_STR;
    }
    conn = ServerConnection(host, port, 30, connflags, &err);
    free(buf);
    if (conn == NULL)
    {
        printf("Failed to connect to '%s' (%d)\n", server, err);
        return NULL;
    }
    return conn;
}

static int JustConnect(const char *server, char *port)
{
    char txtaddr[CF_MAX_IP_LEN] = "";
    return SocketConnect(server, port, 100, true,
                         txtaddr, sizeof(txtaddr));
}

//*******************************************************************
// COMMANDS:
//*******************************************************************

static int CFNetHelpTopic(const char *topic)
{
    assert(topic != NULL);
    bool found = false;
    for (int i = 0; COMMANDS[i].name != NULL; ++i)
    {
        if (strcmp(COMMANDS[i].name, topic) == 0)
        {
            printf("Command:     %s\n",  COMMANDS[i].name);
            printf("Usage:       %s\n",  COMMANDS[i].usage);
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
    else if (strcmp("stat", topic) == 0)
    {
        printf("\nFor security reasons the server doesn't give any additional"
               "\ninformation if unsuccessful. 'Could not stat' message can"
               "\nindicate wrong path, the file doesn't exist, permission "
               "\ndenied, or something else.\n");
    }
    else if (strcmp("get", topic) == 0)
    {
        printf("\ncf-net get is comprised of two requests, STAT and GET."
               "\nThe CFEngine GET protocol command requires a STAT first to"
               "\ndetermine file size. By default the file is saved as its"
               "\nbasename in current working directory (cwd). Override this"
               "\nusing the -o filename option (-o - for stdout).\n");
    }
    else
    {
        if (found == false)
        {
            printf("Unknown help topic: '%s'\n", topic);
            return -1;
        }
    }
    return 0;
}

static int CFNetHelp(const char *topic)
{
    if (topic != NULL)
    {
        CFNetHelpTopic(topic);
    }
    else
    {
        Writer *w = FileWriter(stdout);
        WriterWriteHelp(w, "cf-net", OPTIONS, HINTS, false, COMMANDS);
        FileWriterDetach(w);
        DoCleanupAndExit(EXIT_SUCCESS);
    }
    return 0;
}

static int CFNetConnectSingle(const char *server, bool print)
{
    AgentConnection *conn = CFNetOpenConnection(server);
    if (conn == NULL)
    {
        return -1;
    }
    if (print == true)
    {
        printf("Connected & authenticated successfully to '%s'\n", server);
    }
    CFNetDisconnect(conn);
    return 0;
}

static int CFNetConnect(const char *hostname, char **args)
{
    assert(args != NULL);
    if (args[1] != NULL)
    {
        Log(LOG_LEVEL_ERR, "connect does not take any arguments\n"\
                           "(See cf-net --help)");
        return -1;
    }
    if (NULL_OR_EMPTY(hostname))
    {
        Log(LOG_LEVEL_ERR, "No hostname specified");
        return -1;
    }
    CFNetConnectSingle(hostname, true);
    return 0;
}

static void CFNetDisconnect(AgentConnection *conn)
{
    DisconnectServer(conn);
}

static void CFNetStatPrint(const char *file, int st_mode, const char *server)
{
    printf("%s:", server);
    if (S_ISDIR(st_mode))
    {
        printf("'%s' is a directory\n", file);
    }
    else if (S_ISREG(st_mode))
    {
        printf("'%s' is a regular file\n", file);
    }
    else if (S_ISSOCK(st_mode))
    {
        printf("'%s' is a socket\n", file);
    }
    else if (S_ISCHR(st_mode))
    {
        printf("'%s' is a character device file\n", file);
    }
    else if (S_ISBLK(st_mode))
    {
        printf("'%s' is a block device file\n", file);
    }
    else if (S_ISFIFO(st_mode))
    {
        printf("'%s' is a named pipe (FIFO)\n", file);
    }
    else if (S_ISLNK(st_mode))
    {
        printf("'%s' is a symbolic link\n", file);
    }
    else
    {
        printf("'%s' has an unrecognized st_mode\n", file);
    }
}

static int CFNetStat(ARG_UNUSED CFNetOptions *opts, const char *hostname, char **args)
{
    assert(opts);
    char *file = args[1];
    AgentConnection *conn = CFNetOpenConnection(hostname);// FIXME
    if (conn == NULL)
    {
        return -1;
    }
    bool encrypt = true;
    struct stat sb;
    int r = cf_remote_stat(conn, encrypt, file, &sb, "file");
    if (r != 0)
    {
        printf("Could not stat: '%s'\n", file);
    }
    else
    {
        Log(LOG_LEVEL_INFO, "Detailed stat output:\n"
                            "mode  = %jo, \tsize = %jd,\n"
                            "uid   = %ju, \tgid = %ju,\n"
                            "atime = %jd, \tmtime = %jd\n",
            (uintmax_t) sb.st_mode,  (intmax_t)  sb.st_size,
            (uintmax_t) sb.st_uid,   (uintmax_t) sb.st_gid,
            (intmax_t)  sb.st_atime, (intmax_t)  sb.st_mtime);
        CFNetStatPrint(file, sb.st_mode, hostname);
    }

    CFNetDisconnect(conn);
    return 0;
}

static int invalid_command(const char *cmd)
{
    Log(LOG_LEVEL_ERR, "Invalid, see: 'cf-net help %s' for more info\n", cmd);
    return -1;
}


typedef struct _GetFileData {
    const char *hostname;
    char remote_file[PATH_MAX];
    char local_file[PATH_MAX];
    int ret;
} GetFileData;

static void *CFNetGetFile(void *arg)
{
    GetFileData *data = (GetFileData *) arg;
    AgentConnection *conn = CFNetOpenConnection(data->hostname);
    if (conn == NULL)
    {
        data->ret = -1;
        return NULL;
    }

    struct stat sb;
    data->ret = cf_remote_stat(conn, true, data->remote_file, &sb, "file");
    if (data->ret != 0)
    {
        printf("Could not stat: '%s'\n", data->remote_file);
    }
    else
    {
        bool ok = CopyRegularFileNet(data->remote_file, data->local_file, sb.st_size, true, conn);
        data->ret = ok ? 0 : -1;
    }
    CFNetDisconnect(conn);
    return NULL;
}

typedef struct _CFNetThreadData {
    pthread_t    id;
    GetFileData *data;
} CFNetThreadData;

static int CFNetGet(ARG_UNUSED CFNetOptions *opts, const char *hostname, char **args)
{
    assert(opts);
    assert(hostname);
    assert(args);
    char *local_file = NULL;

    // TODO: Propagate argv and argc from main()
    int argc = 0;
    while (args[argc] != NULL)
    {
        ++argc;
    }

    static struct option longopts[] = {
         { "output",     required_argument,      NULL,           'o' },
         { "jobs",       required_argument,      NULL,           'j' },
         { NULL,         0,                      NULL,           0   }
    };
    assert(opts != NULL);
    if (argc <= 1)
    {
        return invalid_command("get");
    }
    extern int optind;
    optind = 0;
    extern char *optarg;
    int c = 0;
    // TODO: Experiment with more user friendly leading - optstring
    const char *optstr = "o:j:";
    bool specified_path = false;
    long n_threads = 1;
    while ((c = getopt_long(argc, args, optstr, longopts, NULL))
            != -1)
    {
        switch (c)
        {
            case 'o':
            {
                if (local_file != NULL)
                {
                    Log(LOG_LEVEL_INFO,
                        "Warning: multiple occurences of -o in command, "\
                        "only last one will be used.");
                    free(local_file);
                }
                local_file = xstrdup(optarg);
                specified_path = true;
                break;
            }
            case 'j':
            {
                int ret = StringToLong(optarg, &n_threads);
                if (ret != 0)
                {
                    printf("Failed to parse number of threads/jobs from '%s'\n", optarg);
                    n_threads = 1;
                }
                break;
            }
            case ':':
            {
                return invalid_command("get");
                break;
            }
            case '?':
            {
                return invalid_command("get");
                break;
            }
            default:
            {
                printf("Default optarg = '%s', c = '%c' = %i\n",
                       optarg, c, (int)c);
                break;
            }
        }
    }
    args = &(args[optind]);
    argc -= optind;
    char *remote_file = args[0];

    if(local_file == NULL)
    {
        local_file = xstrdup(basename(remote_file));
    }

    if (specified_path && strcmp(local_file, "-") == 0)
    {
         // TODO: Should rewrite CopyRegularFileNet etc. to take fd argument
         // and a simple helper function to open a file as well.
        printf("Output to stdout not yet implemented (TODO)\n");
        free(local_file);
        return -1;
    }

    CFNetThreadData **threads = (CFNetThreadData**) xcalloc((size_t) n_threads, sizeof(CFNetThreadData*));
    for (int i = 0; i < n_threads; i++)
    {
        threads[i] = (CFNetThreadData*) xcalloc(1, sizeof(CFNetThreadData));
        threads[i]->data = (GetFileData*) xcalloc(1, sizeof(GetFileData));
        threads[i]->data->hostname = hostname;
        if (n_threads > 1)
        {
            if (strstr(local_file, "%d") != NULL)
            {
                snprintf(threads[i]->data->local_file, PATH_MAX, local_file, i);
            }
            else
            {
                snprintf(threads[i]->data->local_file, PATH_MAX, "%s.%d", local_file, i);
            }

            if (strstr(remote_file, "%d") != NULL)
            {
                snprintf(threads[i]->data->remote_file, PATH_MAX, remote_file, i);
            }
            else
            {
                snprintf(threads[i]->data->remote_file, PATH_MAX, "%s", remote_file);
            }
        }
        else
        {
            snprintf(threads[i]->data->local_file, PATH_MAX, "%s", local_file);
            snprintf(threads[i]->data->remote_file, PATH_MAX, "%s", remote_file);
        }
    }

    bool failure = false;
    for (int i = 0; !failure && (i < n_threads); i++)
    {
        int ret = pthread_create(&(threads[i]->id), NULL, CFNetGetFile, threads[i]->data);
        if (ret != 0)
        {
            printf("Failed to create a new thread to get the file in: %s\n", strerror(ret));
            failure = true;
        }
    }

    for (int i = 0; i < n_threads; i++)
    {
        int ret = pthread_join(threads[i]->id, NULL);
        if (ret != 0)
        {
            printf("Failed to join the thread: %s\n", strerror(ret));
        }
        else
        {
            failure = failure && (threads[i]->data->ret != 0);
        }
    }

    for (int i = 0; i < n_threads; i++)
    {
        free(threads[i]->data);
        free(threads[i]);
    }
    free(threads);
    free(local_file);
    return failure ? -1 : 0;
}

static void PrintDirs(const Seq *list)
{
    for (size_t i = 0; i < SeqLength(list); i++)
    {
        char *dir_entry = SeqAt(list, i);
        printf("%s\n", dir_entry);
    }
}

static int CFNetOpenDir(ARG_UNUSED CFNetOptions *opts, const char *hostname, char **args)
{
    assert(opts);
    assert(hostname);
    assert(args);
    AgentConnection *conn = CFNetOpenConnection(hostname);
    if (conn == NULL)
    {
        return -1;
    }

    // TODO: Propagate argv and argc from main()
    int argc = 1;
    while (args[argc] != NULL)
    {
        ++argc;
    }
    if (argc <= 1)
    {
        return invalid_command("opendir");
    }

    const char *remote_path = args[1];

    Seq *seq = ProtocolOpenDir(conn, remote_path);
    if (seq == NULL)
    {
        return -1;
    }

    PrintDirs(seq);
    SeqDestroy(seq);
    CFNetDisconnect(conn);
    return 0;
}

static int CFNetMulti(const char *server)
{
    time_t start;
    time(&start);

    int ret = 0;
    int attempts = 0;

    printf("Connecting repeatedly to '%s' without handshakes\n",server);
    attempts = 0;

    char *buf = xstrdup(server);
    char *host, *port;
    ParseHostPort(buf, &host, &port);
    if (port == NULL)
    {
        port = CFENGINE_PORT_STR;
    }

    ret = 0;
    while(ret != 1)
    {
        ret = JustConnect(host, port);
        attempts++;
    }

    free(buf);
    printf("Server unavailable after %i attempts\n", attempts);

    time_t stop;
    time(&stop);
    double seconds = difftime(stop,start);
    printf("'%s' unavailable after %.f seconds (%i attempts)\n",
           server, seconds, attempts);
    return 0;
}

static int CFNetMultiTLS(const char *server)
{
    time_t start;
    time(&start);

    printf("Multiple handshakes to '%s'\n", server);
    int ret = 0;
    int attempts = 0;
    while(ret == 0)
    {
        ret = CFNetConnectSingle(server, false);
        ++attempts;
    }
    time_t stop;
    time(&stop);

    double seconds = difftime(stop,start);
    printf("'%s' unavailable after %.f seconds (%i attempts)\n",
           server, seconds, attempts);
    CFNetMulti(server);
    return 0;
}
