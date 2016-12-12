/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

//#include <unix.h>
#include <logging.h>            // Log, LogSetGlobalLevel

#include <man.h>                // ManPageWrite
#include <client_code.h>        // ServerConnection
#include <crypto.h>             // CryptoInitialize
#include <addr_lib.h>           // ParseHostPort
#include <net.h>                // SocketConnect
#include <time.h>               // time_t, time, difftime
#include <stat_cache.h>         // cf_remote_stat
#include <string_lib.h>         // ToLowerStrInplace
// TODO: MOVE addr_lib out of libpromises....
/*
#include <generic_agent.h>
#include <known_dirs.h>
#include <eval_context.h>
#include <lastseen.h>
#include <files_names.h>
#include <promises.h>
#include <conversion.h>
#include <vars.h>
#include <communication.h>
#include <rlist.h>
#include <scope.h>
#include <policy.h>
#include <audit.h>
#include <connection_info.h>
#include <loading.h>
#include <expand.h>
#include <files_hashes.h>
*/
#define CF_NET_VERSION "0.0.0"

static bool cfnet_silent = false;
#define nprint(fmt, args...); {if(!cfnet_silent) printf(fmt, ##args);}

// TODO: Add usage and list of commands for usability
static const char *const CF_NET_SHORT_DESCRIPTION =
    "command line interface (cli) for libcfnet";

static const char *const CF_NET_MANPAGE_LONG_DESCRIPTION =
    "TODO - this man page is not written yet." //TODO
    "...";

// TODO: Check that these are correct later
static const struct option OPTIONS[] =
{
    {"help",        no_argument,        0, 'h'}, // TODO Use flags here instead
    {"manpage",     no_argument,        0, 'M'},
    {"debug",       no_argument,        0, 'd'},
    {"verbose",     no_argument,        0, 'v'},
    {"inform",      no_argument,        0, 'I'},
    {"dry-run",     no_argument,        0, 'n'},
    {"version",     no_argument,        0, 'V'},
    {"interactive", no_argument,        0, 'i'},
    {"timestamp",   no_argument,        0, 'l'},
    {NULL,          0,                  0, '\0'}
};

// TODO: Check that these are correct later
static const char *const HINTS[] =
{
    "Print the help message",
    "Print the man page",
    "Enable debugging output",
    "Enable verbose output",
    "Enable basic information output",
    "Do not perform action, only output what to do",
    "Output the version of the software",
    "Enable interactive mode",
    "Log timestamps on each line of log output",
    NULL
};

// TODO: Consider global variables for performance and code style(?)
typedef struct
{
    bool debug;
    bool verbose;
    bool inform;
    bool dry_run;
    bool version;
    bool interactive;
    bool timestamp;
    int background;
    int timeout;
} CFNetOptions;

void CFNetSetDefault(CFNetOptions *opts);
int CFNetRun(CFNetOptions *opts, const char *CMD,char **args);
int CFNetParse(int argc, char **argv,                        // Inputs
               CFNetOptions *opts, char **cmd, char ***args); // Outputs

int main(int argc, char **argv)
{
    CFNetOptions opts;
    char *cmd;
    char **args;

    int ret = CFNetParse(argc, argv,          // Inputs
                         &opts, &cmd, &args); // Outputs
    if (ret == -1)
    {
        exit(EXIT_FAILURE);
    }
    ret = CFNetRun(&opts, cmd, args);
    return ret;
}

void CFNetSetDefault(CFNetOptions *opts){
    assert(opts != NULL);
    opts->debug       = false;
    opts->verbose     = false;
    opts->inform      = false;
    opts->dry_run     = false;
    opts->version     = false;
    opts->interactive = false;
    opts->timestamp   = false;
    opts->background  = 0;
    opts->timeout     = 0;
}

int CFNetHelp(const char* topic){
    printf("Unused topic: %s.\n", topic);
    Writer *w = FileWriter(stdout);
    WriterWriteHelp(w, "cf-net", OPTIONS, HINTS, true);
    FileWriterDetach(w);
    exit(EXIT_SUCCESS);
}

int CFNetParse(int argc, char **argv,
               CFNetOptions *opts, char **cmd, char ***args)
{
    assert(opts != NULL);
    extern char *optarg;
    extern int optind;//, opterr, optopt;
    CFNetSetDefault(opts);
    int c = 0;
    int start_index = 1;
    while ((c = getopt_long(argc, argv, "hMdvInVilb:t:",
                            OPTIONS, &start_index))
            != -1)
    {
        switch (c)
        {
            case 'h':
            {
                CFNetHelp(NULL);
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
            case 'n':
            {
                opts->dry_run = true; // TODO: Not in use
                break;
            }
            case 'V':
            {
                printf("cf-net version number "CF_NET_VERSION"\n");
                exit(EXIT_SUCCESS);
                break;
            }
            case 'i':
            {
                opts->interactive = true; // TODO: Not in use
                break;
            }
            case 'l':
            {
                opts->timestamp = true; // TODO: Not in use
                break;
            }
            case 'b':
            {
                opts->background = atoi(optarg); // TODO: Not in use
                break;
            }
            case 't':
            {
                opts->timeout = atoi(optarg); // TODO: Not in use
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
                exit(EXIT_SUCCESS);
                break;
            }
            default:
            {
                break;
            }
        }
    }
    (*cmd) = argv[optind];
    (*args) = &(argv[optind+1]);
    return 0;
}

// Just for testing argument parsing
int CFNetPrint(char **args)
{
    for (int i = 0; args[i] != NULL; ++i)
    {
        printf("%s ", args[i]);
    }
    printf("\n");
    return 0;
}

void CFNetInit()
{
    CryptoInitialize();
    LoadSecretKeys();
    cfnet_init(NULL, NULL);
}

AgentConnection* CFNetOpenConnection(char *server)
{

    AgentConnection *conn = NULL;
    ConnectionFlags connflags = {
        .protocol_version = CF_PROTOCOL_LATEST,
        .trust_server = true
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
        nprint("Failed to connect to '%s'.\n", server);
        return NULL;
    }
    return conn;
}

int CFNetConnectSingle(char *server)
{
    AgentConnection *conn = CFNetOpenConnection(server);
    if (conn == NULL)
    {
        return -1;
    }
    nprint("Connected & authenticated successfully to '%s'.\n", server);
    DeleteAgentConn(conn);
    return 0;
}

int CFNetConnect(char **args)
{
    for (int i = 0; args[i] != NULL; ++i)
    {
        cfnet_silent = false;
        CFNetConnectSingle(args[i]);
        cfnet_silent = true;
    }
    return 0;
}

void CFNetStatPrint(const char* file, int st_mode)
{
    if (S_ISDIR(st_mode))
    {
        nprint("'%s' is a directory.\n", file);
    }
    else if (S_ISREG(st_mode))
    {
        nprint("'%s' is a regular file.\n", file);
    }
    else if (S_ISSOCK(st_mode))
    {
        nprint("'%s' is a socket.\n", file);
    }
    else if (S_ISCHR(st_mode))
    {
        nprint("'%s' is a character device file.\n", file);
    }
    else if (S_ISBLK(st_mode))
    {
        nprint("'%s' is a block device file.\n", file);
    }
    else if (S_ISFIFO(st_mode))
    {
        nprint("'%s' is a named pipe (FIFO).\n", file);
    }
    else if (S_ISLNK(st_mode))
    {
        nprint("'%s' is a symbolic link.\n", file);
    }
    else
    {
        nprint("'%s' has an unrecognized st_mode.\n", file);
    }
}

int CFNetStat(char **args)
{
    char *server = args[0];
    char *file = args[1];
    AgentConnection *conn = CFNetOpenConnection(server);
    if (conn == NULL)
    {
        return -1;
    }
    bool encrypt = true;
    struct stat sb;
    int r = cf_remote_stat(conn, encrypt, file, &sb, "file");
    if (r!= 0)
    {
        nprint("Could not stat: '%s'.\n", file);
        // nprint("(Check cf-serverd output for more info)\n");
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
        CFNetStatPrint(file, sb.st_mode);
    }
    DeleteAgentConn(conn);
    return 0;
}

int JustConnect(char *server, char* port)
{
    char txtaddr[CF_MAX_IP_LEN] = "";
    return SocketConnect(server, port, 100, true,
                         txtaddr, sizeof(txtaddr));
}

int CFNetMulti(char *server)
{
    time_t start;
    time(&start);

    int ret = 0;
    int attempts = 0;

    nprint("Connecting repeatedly to '%s' without handshakes.\n",server);
    attempts = 0;

    char *buf = xstrdup(server);
    char *host, *port;
    ParseHostPort(buf, &host, &port);
    if (port == NULL)
    {
        port = CFENGINE_PORT_STR;
    }

    ret = 0;
    while(ret != -1)
    {
        ret = JustConnect(host, port);
        attempts++;
    }

    free(buf);
    nprint("Server unavailable after %i attempts.\n", attempts);

    time_t stop;
    time(&stop);
    double seconds = difftime(stop,start);
    nprint("%s(): '%s' unavailable after %.f seconds (%i attempts).\n", __FUNCTION__, server, seconds, attempts);
    return 0;
}

int CFNetMultiTLS(char *server)
{
    time_t start;
    time(&start);

    nprint("Multiple handshakes to '%s'.\n", server);
    cfnet_silent = true;
    int ret = 0;
    int attempts = 0;
    while(ret == 0)
    {
        ret = CFNetConnectSingle(server);
        ++attempts;
    }
    cfnet_silent = false;
    time_t stop;
    time(&stop);

    double seconds = difftime(stop,start);
    nprint("%s(): '%s' unavailable after %.f seconds (%i attempts).\n", __FUNCTION__, server, seconds, attempts);
    CFNetMulti(server);
    return 0;
}

#define CFNetIfMatchRun(cmd_string, comparison, functioncall)\
    if (strcmp(cmd_string, comparison) == 0)\
    {\
        return functioncall;\
    }\

// Macro returns on match(!)
int CFNetCommandSwitch(char *cmd, char **args)
{
    CFNetIfMatchRun(cmd, "print",    CFNetPrint(args));
    CFNetIfMatchRun(cmd, "connect",  CFNetConnect(args));
    CFNetIfMatchRun(cmd, "stat",     CFNetStat(args));
    CFNetIfMatchRun(cmd, "multi",    CFNetMulti(args[0]));
    CFNetIfMatchRun(cmd, "multitls", CFNetMultiTLS(args[0]));
    CFNetIfMatchRun(cmd, "help",     CFNetHelp(args[0]));
    printf("'%s' is not a valid cf-net command.\n", cmd);
    return 1;
}

int CFNetRun(CFNetOptions *opts, const char *CMD, char **args)
{
    char* cmd = xstrdup(CMD);

    ToLowerStrInplace(cmd);

    if(opts->inform)
    {
        LogSetGlobalLevel(LOG_LEVEL_INFO);
    }
    if(opts->verbose)
    {
        LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
    }
    if(opts->debug)
    {
        LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
    }

    Log(LOG_LEVEL_VERBOSE, "Running command '%s' with argument(s):\n", cmd);
    for (int i = 0; args[i] != NULL; ++i)
    {
        Log(LOG_LEVEL_VERBOSE, "%s\n", args[i]);
    }
    CFNetInit();
    int ret = CFNetCommandSwitch(cmd, args);
    free(cmd);
    return ret;
}
