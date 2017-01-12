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

#include <cf-net.h>
#include <logging.h>            // Log, LogSetGlobalLevel

#include <man.h>                // ManPageWrite
#include <client_code.h>        // ServerConnection
#include <crypto.h>             // CryptoInitialize
#include <addr_lib.h>           // ParseHostPort
#include <net.h>                // SocketConnect
#include <time.h>               // time_t, time, difftime
#include <stat_cache.h>         // cf_remote_stat
#include <string_lib.h>         // ToLowerStrInplace
#include <writer.h>
#include <policy_server.h>      // PolicyServerReadFile

/*
#include <unix.h>
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


static const char *const CF_NET_SHORT_DESCRIPTION =
    "Command-Line Interface (CLI) for libcfnet.";

static const char *const CF_NET_MANPAGE_LONG_DESCRIPTION =
    "TODO - this man page is not written yet.";

static const Description COMMANDS[] =
{
    {"help",    "Prints general help or per topic",             "cf-net help [command]"},
    {"connect", "Checks if host(s) is available by connecting", "cf-net -H 192.168.50.50 connect"},
    {"stat",    "Look at type of file",                         "cf-net -H 192.168.50.50 stat /var/cfengine/masterfiles/update.cf"},
    {NULL, NULL, NULL}
};

static const struct option OPTIONS[] =
{
    {"help",        no_argument,        0, 'h'},
    {"manpage",     no_argument,        0, 'M'},
    {"host",        required_argument,  0, 'H'},
    {"debug",       no_argument,        0, 'd'},
    {"verbose",     no_argument,        0, 'v'},
    {"inform",      no_argument,        0, 'I'},
    {NULL,          0,                  0, '\0'}
};

static const char *const HINTS[] =
{
    "Print the help message",
    "Print the man page",
    "Host to use (defaults to policy_server.dat)",
    "Enable debugging output",
    "Enable verbose output",
    "Enable basic information output",
    NULL
};

//*******************************************************************
// MAIN:
//*******************************************************************

int main(int argc, char **argv)
{
    CFNetOptions opts;
    char *cmd;
    char **args;
    char *hostnames = NULL;
    int ret = CFNetParse(argc, argv,                      // Inputs
                         &opts, &cmd, &args, &hostnames); // Outputs
    if (ret != 0)
    {
        exit(EXIT_FAILURE);
    }
    ret = CFNetRun(&opts, cmd, args, hostnames);  // Commands return exit code
    return ret;
}

//*******************************************************************
// INIT:
//*******************************************************************

void CFNetSetDefault(CFNetOptions *opts){
    assert(opts != NULL);
    opts->debug       = false;
    opts->verbose     = false;
    opts->inform      = false;
}

void CFNetInit()
{
    CryptoInitialize();
    LoadSecretKeys();
    cfnet_init(NULL, NULL);
}

//*******************************************************************
// MAIN LOGIC:
//*******************************************************************

void print_argv(char ** argv)
{
    for (int i = 0; argv[i] != NULL; ++i)
    {
        printf("argv[%i]: '%s'\n", i, argv[i]);
    }
}

char* RequireHostname(char *hostnames)
{
    if (hostnames == NULL)
    {
        char* policy_server = PolicyServerReadFile("/var/cfengine/");
        if (policy_server == NULL)
        {
            printf("Error: no host name (and no policy_server.dat)");
            exit(EXIT_FAILURE);
        }
        return policy_server;
    }
    return hostnames;
}


int CFNetParse(int argc, char **argv,
               CFNetOptions *opts, char **cmd, char ***args, char **hostnames)
{
    assert(opts != NULL);
    if (argc <= 1)
    {
        CFNetHelp(NULL);
        return 0;
    }
    extern int optind;
    extern char *optarg;
    CFNetSetDefault(opts);
    *hostnames = NULL;
    int c = 0;
    int start_index = 1;
    const char *optstr = "+hMH:dvI"; // + means stop for non opt arg. :)
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
                exit(EXIT_SUCCESS);
                break;
            }
            case 'H':
            {
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
            default:
            {
                printf("Default optarg = '%s', c = '%c' = %i", optarg, c, (int)c);
                break;
            }
        }
    }
    (*cmd) = argv[optind];
    (*args) = &(argv[optind+1]);
    return 0;
}

#define CFNetIfMatchRun(cmd_string, comparison, functioncall)\
    if (strcmp(cmd_string, comparison) == 0)\
    {\
        return functioncall;\
    }\

// ^ Macro returns on match(!)
int CFNetCommandSwitch(CFNetOptions *opts, const char *hostname, char *cmd, char **args)
{
    CFNetIfMatchRun(cmd, "connect",  CFNetConnect(opts, hostname, args));
    CFNetIfMatchRun(cmd, "stat",     CFNetStat(opts, hostname, args));
    CFNetIfMatchRun(cmd, "multi",    CFNetMulti(hostname));
    CFNetIfMatchRun(cmd, "multitls", CFNetMultiTLS(hostname));
    printf("'%s' is not a valid cf-net command.\n", cmd);
    return 1;
}

int CFNetRun(CFNetOptions *opts, const char *CMD, char **args, char *hostnames)
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

    if (strcmp(cmd, "help") == 0)
    {
        return CFNetHelp(args[0]);
    }

    CFNetInit();
    char *hosts = RequireHostname(hostnames);
    int ret = 0;
    char *hostname = strtok(hosts, ",");
    while (hostname != NULL){
        CFNetCommandSwitch(opts, hostname, cmd, args);
        hostname = strtok(NULL, ",");
    }
    free(hosts);
    free(cmd);
    return ret;
}

//*******************************************************************
// PROTOCOL:
//*******************************************************************

AgentConnection* CFNetOpenConnection(const char *server)
{
    AgentConnection *conn = NULL;
    ConnectionFlags connflags =
    {
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
        printf("Failed to connect to '%s'.\n", server);
        return NULL;
    }
    return conn;
}

int JustConnect(const char *server, char *port)
{
    char txtaddr[CF_MAX_IP_LEN] = "";
    return SocketConnect(server, port, 100, true,
                         txtaddr, sizeof(txtaddr));
}

//*******************************************************************
// COMMANDS:
//*******************************************************************

int CFNetHelpTopic(const char *topic)
{
    assert(topic != NULL);
    bool found = false;
    for (int i = 0; COMMANDS[i].name != NULL; ++i)
    {
        if (strcmp(COMMANDS[i].name, topic) == 0)
        {
            printf("Command:     %s\n",  COMMANDS[i].name);
            printf("Usage:       %s\n",  COMMANDS[i].usage);
            printf("Description: %s.\n", COMMANDS[i].description);
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
    else
    {
        if (found == false)
        {
            printf("Unknown help topic: '%s'\n", topic);
            return 1;
        }
    }
    return 0;
}

int CFNetHelp(const char *topic)
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
        exit(EXIT_SUCCESS);
    }
    return 0;
}

int CFNetConnectSingle(const char *server, bool print)
{
    AgentConnection *conn = CFNetOpenConnection(server);
    if (conn == NULL)
    {
        return 1;
    }
    if (print == true)
    {
        printf("Connected & authenticated successfully to '%s'.\n", server);
    }
    DeleteAgentConn(conn);
    return 0;
}

int CFNetConnect(CFNetOptions *opts, const char* hostname, char **args)
{
    if (hostname != NULL)
    {
        CFNetConnectSingle(hostname, true); // FIXME
        return 0;
    }
    // TODO
    if (args == NULL)
    {
        return 0;
    }
    if (opts->used_default)
    {
        return 0;
    }
    for (int i = 0; args[i] != NULL; ++i)
    {
        CFNetConnectSingle(args[i], true);
    }
    return 0;
}

void CFNetStatPrint(const char *file, int st_mode, const char *server)
{
    printf("%s:", server);
    if (S_ISDIR(st_mode))
    {
        printf("'%s' is a directory.\n", file);
    }
    else if (S_ISREG(st_mode))
    {
        printf("'%s' is a regular file.\n", file);
    }
    else if (S_ISSOCK(st_mode))
    {
        printf("'%s' is a socket.\n", file);
    }
    else if (S_ISCHR(st_mode))
    {
        printf("'%s' is a character device file.\n", file);
    }
    else if (S_ISBLK(st_mode))
    {
        printf("'%s' is a block device file.\n", file);
    }
    else if (S_ISFIFO(st_mode))
    {
        printf("'%s' is a named pipe (FIFO).\n", file);
    }
    else if (S_ISLNK(st_mode))
    {
        printf("'%s' is a symbolic link.\n", file);
    }
    else
    {
        printf("'%s' has an unrecognized st_mode.\n", file);
    }
}

int CFNetStat(CFNetOptions *opts, const char* hostname, char **args)
{
    assert(opts);
    char *file = args[0];
    AgentConnection *conn = CFNetOpenConnection(hostname);// FIXME
    if (conn == NULL)
    {
        return 1;
    }
    bool encrypt = true;
    struct stat sb;
    int r = cf_remote_stat(conn, encrypt, file, &sb, "file");
    if (r!= 0)
    {
        printf("Could not stat: '%s'.\n", file);
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
    DeleteAgentConn(conn);
    return 0;
}

int CFNetMulti(const char *server)
{
    time_t start;
    time(&start);

    int ret = 0;
    int attempts = 0;

    printf("Connecting repeatedly to '%s' without handshakes.\n",server);
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
    printf("Server unavailable after %i attempts.\n", attempts);

    time_t stop;
    time(&stop);
    double seconds = difftime(stop,start);
    printf("%s(): '%s' unavailable after %.f seconds (%i attempts).\n", __FUNCTION__, server, seconds, attempts);
    return 0;
}

int CFNetMultiTLS(const char *server)
{
    time_t start;
    time(&start);

    printf("Multiple handshakes to '%s'.\n", server);
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
    printf("%s(): '%s' unavailable after %.f seconds (%i attempts).\n", __FUNCTION__, server, seconds, attempts);
    CFNetMulti(server);
    return 0;
}
