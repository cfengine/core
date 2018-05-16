/*
   Copyright 2018 Northern.tech AS

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

#include <generic_agent.h>

#include <known_dirs.h>
#include <unix.h>
#include <eval_context.h>
#include <lastseen.h>
#include <crypto.h>
#include <files_names.h>
#include <promises.h>
#include <conversion.h>
#include <vars.h>
#include <client_code.h>
#include <communication.h>
#include <net.h>
#include <string_lib.h>
#include <rlist.h>
#include <scope.h>
#include <policy.h>
#include <audit.h>
#include <man.h>
#include <connection_info.h>
#include <addr_lib.h>
#include <loading.h>
#include <expand.h>                                 /* ProtocolVersionParse */
#include <files_hashes.h>
#include <string_lib.h>
#include <cleanup.h>

typedef enum
{
    RUNAGENT_CONTROL_HOSTS,
    RUNAGENT_CONTROL_PORT_NUMBER,
    RUNAGENT_CONTROL_FORCE_IPV4,
    RUNAGENT_CONTROL_TRUSTKEY,
    RUNAGENT_CONTROL_ENCRYPT,
    RUNAGENT_CONTROL_BACKGROUND,
    RUNAGENT_CONTROL_MAX_CHILD,
    RUNAGENT_CONTROL_OUTPUT_TO_FILE,
    RUNAGENT_CONTROL_OUTPUT_DIRECTORY,
    RUNAGENT_CONTROL_TIMEOUT,
    RUNAGENT_CONTROL_NONE
} RunagentControl;

static void ThisAgentInit(void);
static GenericAgentConfig *CheckOpts(int argc, char **argv);

static void KeepControlPromises(EvalContext *ctx, const Policy *policy);
static int HailServer(const EvalContext *ctx, const GenericAgentConfig *config,
                      char *host);
static void SendClassData(AgentConnection *conn);
static void HailExec(AgentConnection *conn, char *peer);
static FILE *NewStream(char *name);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *const CF_RUNAGENT_SHORT_DESCRIPTION =
    "activate cf-agent on a remote host";

static const char *const CF_RUNAGENT_MANPAGE_LONG_DESCRIPTION =
    "cf-runagent connects to a list of running instances of "
    "cf-serverd. It allows foregoing the usual cf-execd schedule "
    "to activate cf-agent. Additionally, a user "
    "may send classes to be defined on the remote\n"
    "host. Two kinds of classes may be sent: classes to decide "
    "on which hosts cf-agent will be started, and classes that "
    "the user requests cf-agent should define on execution. "
    "The latter type is regulated by cf-serverd's role based access control.";

static const struct option OPTIONS[] =
{
    {"help", no_argument, 0, 'h'},
    {"background", optional_argument, 0, 'b'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"log-level", required_argument, 0, 'g'},
    {"dry-run", no_argument, 0, 'n'},
    {"version", no_argument, 0, 'V'},
    {"file", required_argument, 0, 'f'},
    {"define-class", required_argument, 0, 'D'},
    {"select-class", required_argument, 0, 's'},
    {"inform", no_argument, 0, 'I'},
    {"remote-options", required_argument, 0, 'o'},
    {"diagnostic", no_argument, 0, 'x'},
    {"hail", required_argument, 0, 'H'},
    {"interactive", no_argument, 0, 'i'},
    {"timeout", required_argument, 0, 't'},
    {"color", optional_argument, 0, 'C'},
    {"timestamp", no_argument, 0, 'l'},
    /* Only long option for the rest */
    {"log-modules", required_argument, 0, 0},
    {"remote-bundles", required_argument, 0, 0},
    {NULL, 0, 0, '\0'}
};

static const char *const HINTS[] =
{
    "Print the help message",
    "Parallelize connections (50 by default)",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "Specify how detailed logs should be. Possible values: 'error', 'warning', 'notice', 'info', 'verbose', 'debug'",
    "All talk and no action mode - make no changes, only inform of promises not kept",
    "Output the version of the software",
    "Specify an alternative input file than the default. This option is overridden by FILE if supplied as argument.",
    "Define a list of comma separated classes to be sent to a remote agent",
    "Define a list of comma separated classes to be used to select remote agents by constraint",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "(deprecated)",
    "(deprecated)",
    "Hail the following comma-separated lists of hosts, overriding default list",
    "Enable interactive mode for key trust",
    "Connection timeout, seconds",
    "Enable colorized output. Possible values: 'always', 'auto', 'never'. If option is used, the default value is 'auto'",
    "Log timestamps on each line of log output",
    "Enable even more detailed debug logging for specific areas of the implementation. Use together with '-d'. Use --log-modules=help for a list of available modules",
    "Bundles to execute on the remote agent",
    NULL
};

extern const ConstraintSyntax CFR_CONTROLBODY[];

int INTERACTIVE = false; /* GLOBAL_A */
int OUTPUT_TO_FILE = false; /* GLOBAL_P */
char OUTPUT_DIRECTORY[CF_BUFSIZE] = ""; /* GLOBAL_P */
int BACKGROUND = false; /* GLOBAL_P GLOBAL_A */
int MAXCHILD = 50; /* GLOBAL_P GLOBAL_A */

const Rlist *HOSTLIST = NULL;                          /* GLOBAL_P GLOBAL_A */

char   SENDCLASSES[CF_MAXVARSIZE] = "";                         /* GLOBAL_A */
char DEFINECLASSES[CF_MAXVARSIZE] = "";                         /* GLOBAL_A */
char REMOTEBUNDLES[CF_MAXVARSIZE] = "";

/*****************************************************************************/

int main(int argc, char *argv[])
{
#if !defined(__MINGW32__)
    int count = 0;
    int status;
    int pid;
#endif

    GenericAgentConfig *config = CheckOpts(argc, argv);
    EvalContext *ctx = EvalContextNew();
    GenericAgentConfigApply(ctx, config);

    GenericAgentDiscoverContext(ctx, config);
    Policy *policy = LoadPolicy(ctx, config);

    GenericAgentPostLoadInit(ctx);
    ThisAgentInit();

    KeepControlPromises(ctx, policy);      // Set RUNATTR using copy

    if (BACKGROUND && INTERACTIVE)
    {
        Log(LOG_LEVEL_ERR, "You cannot specify background mode and interactive mode together");
        DoCleanupAndExit(EXIT_FAILURE);
    }

/* HvB */
    if (HOSTLIST)
    {
        const Rlist *rp = HOSTLIST;

        while (rp != NULL)
        {

#ifdef __MINGW32__
            if (BACKGROUND)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Windows does not support starting processes in the background - starting in foreground");
                BACKGROUND = false;
            }
#else
            if (BACKGROUND)     /* parallel */
            {
                if (count < MAXCHILD)
                {
                    if (fork() == 0)    /* child process */
                    {
                        HailServer(ctx, config, RlistScalarValue(rp));
                        DoCleanupAndExit(EXIT_SUCCESS);
                    }
                    else        /* parent process */
                    {
                        rp = rp->next;
                        count++;
                    }
                }
                else
                {
                    pid = wait(&status);
                    Log(LOG_LEVEL_DEBUG, "child = %d, child number = %d", pid, count);
                    count--;
                }
            }
            else                /* serial */
#endif /* __MINGW32__ */
            {
                HailServer(ctx, config, RlistScalarValue(rp));
                rp = rp->next;
            }
        }                       /* end while */
    }                           /* end if HOSTLIST */

#ifndef __MINGW32__
    if (BACKGROUND)
    {
        Log(LOG_LEVEL_NOTICE, "Waiting for child processes to finish");
        while (count > 0)
        {
            pid = wait(&status);
            Log(LOG_LEVEL_VERBOSE, "Child %d ended, number %d", pid, count);
            count--;
        }
    }
#endif

    PolicyDestroy(policy);
    GenericAgentFinalize(ctx, config);

    return 0;
}

/*******************************************************************/

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_RUNAGENT, GetTTYInteractive());

    DEFINECLASSES[0] = '\0';
    SENDCLASSES[0]   = '\0';
    REMOTEBUNDLES[0] = '\0';

    int longopt_idx;
    while ((c = getopt_long(argc, argv, "t:q:db::vnKhIif:g:D:VSxo:s:MH:C::l",
                            OPTIONS, &longopt_idx))
           != -1)
    {
        switch (c)
        {
        case 'f':
            GenericAgentConfigSetInputFile(config, GetInputDir(), optarg);
            MINUSF = true;
            break;

        case 'b':
            BACKGROUND = true;
            if (optarg)
            {
                MAXCHILD = StringToLongExitOnError(optarg);
            }
            break;

        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            break;

        case 'K':
            config->ignore_locks = true;
            break;

        case 's':
        {
            size_t len = strlen(SENDCLASSES);
            StrCatDelim(SENDCLASSES, sizeof(SENDCLASSES), &len,
                        optarg, ',');
            if (len >= sizeof(SENDCLASSES))
            {
                Log(LOG_LEVEL_ERR, "Argument too long (-s)");
                DoCleanupAndExit(EXIT_FAILURE);
            }
            break;
        }
        case 'D':
        {
            size_t len = strlen(DEFINECLASSES);
            StrCatDelim(DEFINECLASSES, sizeof(DEFINECLASSES), &len,
                        optarg, ',');
            if (len >= sizeof(DEFINECLASSES))
            {
                Log(LOG_LEVEL_ERR, "Argument too long (-D)");
                DoCleanupAndExit(EXIT_FAILURE);
            }
            break;
        }
        case 'H':
            HOSTLIST = RlistFromSplitString(optarg, ',');
            break;

        case 'o':
            Log(LOG_LEVEL_ERR, "Option \"-o\" has been deprecated,"
                " you can not pass arbitrary arguments to remote cf-agent");
            DoCleanupAndExit(EXIT_FAILURE);
            break;

        case 'I':
            LogSetGlobalLevel(LOG_LEVEL_INFO);
            break;

        case 'i':
            INTERACTIVE = true;
            break;

        case 'v':
            LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
            break;

        case 'g':
            LogSetGlobalLevelArgOrExit(optarg);
            break;

        case 'n':
            DONTDO = true;
            config->ignore_locks = true;
            break;

        case 't':
            CONNTIMEOUT = StringToLongExitOnError(optarg);
            break;

        case 'V':
        {
            Writer *w = FileWriter(stdout);
            GenericAgentWriteVersion(w);
            FileWriterDetach(w);
        }
        DoCleanupAndExit(EXIT_SUCCESS);

        case 'h':
        {
            Writer *w = FileWriter(stdout);
            WriterWriteHelp(w, "cf-runagent", OPTIONS, HINTS, true, NULL);
            FileWriterDetach(w);
        }
        DoCleanupAndExit(EXIT_SUCCESS);

        case 'M':
        {
            Writer *out = FileWriter(stdout);
            ManPageWrite(out, "cf-runagent", time(NULL),
                         CF_RUNAGENT_SHORT_DESCRIPTION,
                         CF_RUNAGENT_MANPAGE_LONG_DESCRIPTION,
                         OPTIONS, HINTS,
                         true);
            FileWriterDetach(out);
            DoCleanupAndExit(EXIT_SUCCESS);
        }

        case 'x':
            Log(LOG_LEVEL_ERR, "Option \"-x\" has been deprecated");
            DoCleanupAndExit(EXIT_FAILURE);

        case 'C':
            if (!GenericAgentConfigParseColor(config, optarg))
            {
                DoCleanupAndExit(EXIT_FAILURE);
            }
            break;

        case 'l':
            LoggingEnableTimestamps(true);
            break;

        /* long options only */
        case 0:

            if (strcmp(OPTIONS[longopt_idx].name, "log-modules") == 0)
            {
                bool ret = LogEnableModulesFromString(optarg);
                if (!ret)
                {
                    /* --log-modules=help prints out usage so we must exit */
                    DoCleanupAndExit(EXIT_SUCCESS);		/* return of false indicates that help was requested, so not failure */
                }
            }
            else if (strcmp(OPTIONS[longopt_idx].name, "remote-bundles") == 0)
            {
                size_t len = strlen(REMOTEBUNDLES);
                StrCatDelim(REMOTEBUNDLES, sizeof(REMOTEBUNDLES), &len,
                            optarg, ',');
                if (len >= sizeof(REMOTEBUNDLES))
                {
                    Log(LOG_LEVEL_ERR, "Argument too long (--remote-bundles)");
                    DoCleanupAndExit(EXIT_FAILURE);
                }
            }
            break;

        default:
        {
            Writer *w = FileWriter(stdout);
            WriterWriteHelp(w, "cf-runagent", OPTIONS, HINTS, true, NULL);
            FileWriterDetach(w);
        }
        DoCleanupAndExit(EXIT_FAILURE);

        }
    }

    if (!GenericAgentConfigParseArguments(config, argc - optind, argv + optind))
    {
        Log(LOG_LEVEL_ERR, "Too many arguments");
        DoCleanupAndExit(EXIT_FAILURE);
    }

    return config;
}

/*******************************************************************/

static void ThisAgentInit(void)
{
    umask(077);
}

/********************************************************************/

static int HailServer(const EvalContext *ctx, const GenericAgentConfig *config,
                      char *host)
{
    assert(host != NULL);

    AgentConnection *conn;
    char hostkey[CF_HOSTKEY_STRING_SIZE], user[CF_SMALLBUF];
    bool gotkey;
    char reply[8];
    bool trustkey = false;

    char *hostname, *port;
    ParseHostPort(host, &hostname, &port);

    if (hostname == NULL)
    {
        Log(LOG_LEVEL_INFO, "No remote hosts were specified to connect to");
        return false;
    }
    if (port == NULL)
    {
        port = "5308";
    }

    char ipaddr[CF_MAX_IP_LEN];
    if (Hostname2IPString(ipaddr, hostname, sizeof(ipaddr)) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "HailServer: ERROR, could not resolve '%s'", hostname);
        return false;
    }

    Address2Hostkey(hostkey, sizeof(hostkey), ipaddr);
    GetCurrentUserName(user, sizeof(user));

    if (INTERACTIVE)
    {
        Log(LOG_LEVEL_VERBOSE, "Using interactive key trust...");

        gotkey = HavePublicKey(user, ipaddr, hostkey) != NULL;
        if (!gotkey)
        {
            /* TODO print the hash of the connecting host. But to do that we
             * should open the connection first, and somehow pass that hash
             * here! redmine#7212 */
            printf("WARNING - You do not have a public key from host %s = %s\n",
                   hostname, ipaddr);
            printf("          Do you want to accept one on trust? (yes/no)\n\n--> ");

            while (true)
            {
                if (fgets(reply, sizeof(reply), stdin) == NULL)
                {
                    FatalError(ctx, "EOF trying to read answer from terminal");
                }

                if (Chop(reply, CF_EXPANDSIZE) == -1)
                {
                    Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
                }

                if (strcmp(reply, "yes") == 0)
                {
                    printf("Will trust the key...\n");
                    trustkey = true;
                    break;
                }
                else if (strcmp(reply, "no") == 0)
                {
                    printf("Will not trust the key...\n");
                    trustkey = false;
                    break;
                }
                else
                {
                    printf("Please reply yes or no...(%s)\n", reply);
                }
            }
        }
    }


#ifndef __MINGW32__
    if (BACKGROUND)
    {
        Log(LOG_LEVEL_INFO, "Hailing %s : %s (in the background)",
            hostname, port);
    }
    else
#endif
    {
        Log(LOG_LEVEL_INFO,
            "........................................................................");
        Log(LOG_LEVEL_INFO, "Hailing %s : %s",
            hostname, port);
        Log(LOG_LEVEL_INFO,
            "........................................................................");
    }

    ConnectionFlags connflags = {
        .protocol_version = config->protocol_version,
        .trust_server = trustkey,
        .off_the_record = false
    };
    int err = 0;
    conn = ServerConnection(hostname, port, CONNTIMEOUT, connflags, &err);

    if (conn == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to connect to host: %s", hostname);
        return false;
    }

    /* Send EXEC command. */
    HailExec(conn, hostname);

    return true;
}

/********************************************************************/
/* Level 2                                                          */
/********************************************************************/

static void KeepControlPromises(EvalContext *ctx, const Policy *policy)
{
    Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_RUNAGENT);
    if (constraints)
    {
        for (size_t i = 0; i < SeqLength(constraints); i++)
        {
            Constraint *cp = SeqAt(constraints, i);

            if (!IsDefinedClass(ctx, cp->classes))
            {
                continue;
            }

            VarRef *ref = VarRefParseFromScope(cp->lval, "control_runagent");
            DataType value_type;
            const void *value = EvalContextVariableGet(ctx, ref, &value_type);
            VarRefDestroy(ref);

            /* If var not found, or if it's an empty list. */
            if (value_type == CF_DATA_TYPE_NONE || value == NULL)
            {
                Log(LOG_LEVEL_ERR, "Unknown lval '%s' in runagent control body", cp->lval);
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_FORCE_IPV4].lval) == 0)
            {
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_TRUSTKEY].lval) == 0)
            {
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_ENCRYPT].lval) == 0)
            {
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_PORT_NUMBER].lval) == 0)
            {
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_BACKGROUND].lval) == 0)
            {
                /*
                 * Only process this option if are is no -b or -i options specified on
                 * command line.
                 */
                if (BACKGROUND || INTERACTIVE)
                {
                    Log(LOG_LEVEL_WARNING,
                        "'background_children' setting from 'body runagent control' is overridden by command-line option.");
                }
                else
                {
                    BACKGROUND = BooleanFromString(value);
                }
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_MAX_CHILD].lval) == 0)
            {
                MAXCHILD = (short) IntFromString(value);
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_OUTPUT_TO_FILE].lval) == 0)
            {
                OUTPUT_TO_FILE = BooleanFromString(value);
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_OUTPUT_DIRECTORY].lval) == 0)
            {
                if (IsAbsPath(value))
                {
                    strlcpy(OUTPUT_DIRECTORY, value, CF_BUFSIZE);
                    Log(LOG_LEVEL_VERBOSE, "Setting output direcory to '%s'", OUTPUT_DIRECTORY);
                }
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_TIMEOUT].lval) == 0)
            {
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_HOSTS].lval) == 0)
            {
                if (HOSTLIST == NULL)       // Don't override if command line setting
                {
                    HOSTLIST = value;
                }

                continue;
            }
        }
    }

    const char *expire_after = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_LASTSEEN_EXPIRE_AFTER);
    if (expire_after)
    {
        LASTSEENEXPIREAFTER = IntFromString(expire_after) * 60;
    }

}

static void SendClassData(AgentConnection *conn)
{
    Rlist *classes, *rp;

    classes = RlistFromSplitRegex(SENDCLASSES, "[,: ]", 99, false);

    for (rp = classes; rp != NULL; rp = rp->next)
    {
        if (SendTransaction(conn->conn_info, RlistScalarValue(rp), 0, CF_DONE) == -1)
        {
            Log(LOG_LEVEL_ERR, "Transaction failed. (send: %s)", GetErrorStr());
            return;
        }
    }

    if (SendTransaction(conn->conn_info, CFD_TERMINATOR, 0, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Transaction failed. (send: %s)", GetErrorStr());
        return;
    }
}

/********************************************************************/

static void HailExec(AgentConnection *conn, char *peer)
{
    char sendbuf[CF_BUFSIZE - CF_INBAND_OFFSET] = "EXEC";
    size_t sendbuf_len = strlen(sendbuf);

    if (!NULL_OR_EMPTY(DEFINECLASSES))
    {
        StrCat(sendbuf, sizeof(sendbuf), &sendbuf_len, " -D", 0);
        StrCat(sendbuf, sizeof(sendbuf), &sendbuf_len, DEFINECLASSES, 0);
    }
    if (!NULL_OR_EMPTY(REMOTEBUNDLES))
    {
        StrCat(sendbuf, sizeof(sendbuf), &sendbuf_len, " -b ", 0);
        StrCat(sendbuf, sizeof(sendbuf), &sendbuf_len, REMOTEBUNDLES, 0);
    }

    if (sendbuf_len >= sizeof(sendbuf))
    {
        Log(LOG_LEVEL_ERR, "Command longer than maximum transaction packet");
        DisconnectServer(conn);
        return;
    }

    if (SendTransaction(conn->conn_info, sendbuf, 0, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Transmission rejected. (send: %s)", GetErrorStr());
        DisconnectServer(conn);
        return;
    }

    /* TODO we are sending class data right after EXEC, when the server might
     * have already rejected us with BAD reply. So this class data with the
     * CFD_TERMINATOR will be interpreted by the server as a new, bogus
     * protocol command, and the server will complain. */
    SendClassData(conn);

    char recvbuffer[CF_BUFSIZE];
    FILE *fp = NewStream(peer);
    while (true)
    {
        memset(recvbuffer, 0, sizeof(recvbuffer));

        if (ReceiveTransaction(conn->conn_info, recvbuffer, NULL) == -1)
        {
            break;
        }
        if (strncmp(recvbuffer, CFD_TERMINATOR, strlen(CFD_TERMINATOR)) == 0)
        {
            break;
        }

        const size_t recv_len = strlen(recvbuffer);
        const char   *ipaddr  = conn->remoteip;

        if (strncmp(recvbuffer, "BAD:", 4) == 0)
        {
            fprintf(fp, "%s> !! %s\n", ipaddr, recvbuffer + 4);
        }
        /* cf-serverd >= 3.7 quotes command output with "> ". */
        else if (strncmp(recvbuffer, "> ", 2) == 0)
        {
            fprintf(fp, "%s> -> %s", ipaddr, &recvbuffer[2]);
        }
        else
        {
            fprintf(fp, "%s> %s", ipaddr, recvbuffer);
        }

        if (recv_len > 0 && recvbuffer[recv_len - 1] != '\n')
        {
            /* We'll be printing double newlines here with new cf-serverd
             * versions, so check for already trailing newlines. */
            /* TODO deprecate this path in a couple of versions. cf-serverd is
             * supposed to munch the newlines so we must always append one. */
            fputc('\n', fp);
        }
    }

    if (fp != stdout)
    {
        fclose(fp);
    }
    DisconnectServer(conn);
}

/********************************************************************/
/* Level                                                            */
/********************************************************************/

static FILE *NewStream(char *name)
{
    FILE *fp;
    char filename[CF_BUFSIZE];

    if (OUTPUT_DIRECTORY[0] != '\0')
    {
        snprintf(filename, CF_BUFSIZE, "%s/%s_runagent.out", OUTPUT_DIRECTORY, name);
    }
    else
    {
        snprintf(filename, CF_BUFSIZE, "%s%coutputs%c%s_runagent.out",
                 GetWorkDir(), FILE_SEPARATOR, FILE_SEPARATOR, name);
    }

    if (OUTPUT_TO_FILE)
    {
        printf("Opening file... %s\n", filename);

        if ((fp = fopen(filename, "w")) == NULL)
        {
            Log(LOG_LEVEL_ERR, "Unable to open file '%s' (fopen: %s)", filename, GetErrorStr());
            fp = stdout;
        }
    }
    else
    {
        fp = stdout;
    }

    return fp;
}
