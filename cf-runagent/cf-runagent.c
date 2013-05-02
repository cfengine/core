/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "generic_agent.h"

#include "sysinfo.h"
#include "env_context.h"
#include "lastseen.h"
#include "crypto.h"
#include "files_names.h"
#include "promises.h"
#include "conversion.h"
#include "vars.h"
#include "logging_old.h"
#include "logging.h"
#include "client_code.h"
#include "communication.h"
#include "net.h"
#include "string_lib.h"
#include "rlist.h"
#include "scope.h"
#include "policy.h"
#include "audit.h"

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
static GenericAgentConfig *CheckOpts(EvalContext *ctx, int argc, char **argv);

static void KeepControlPromises(EvalContext *ctx, Policy *policy);
static int HailServer(EvalContext *ctx, char *host);
static int ParseHostname(char *hostname, char *new_hostname);
static void SendClassData(AgentConnection *conn);
static void HailExec(AgentConnection *conn, char *peer, char *recvbuffer, char *sendbuffer);
static FILE *NewStream(char *name);
static void DeleteStream(FILE *fp);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *ID = "The run agent connects to a list of running instances of\n"
    "the cf-serverd service. The agent allows a user to\n"
    "forego the usual scheduling interval for the agent and\n"
    "activate cf-agent on a remote host. Additionally, a user\n"
    "can send additional classes to be defined on the remote\n"
    "host. Two kinds of classes may be sent: classes to decide\n"
    "on which hosts the agent will be started, and classes that\n"
    "the user requests the agent should define on execution.\n"
    "The latter type is regulated by cf-serverd's role based\n" "access control.";

static const struct option OPTIONS[17] =
{
    {"help", no_argument, 0, 'h'},
    {"background", optional_argument, 0, 'b'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
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
    {NULL, 0, 0, '\0'}
};

static const char *HINTS[17] =
{
    "Print the help message",
    "Parallelize connections (50 by default)",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "All talk and no action mode - make no changes, only inform of promises not kept",
    "Output the version of the software",
    "Specify an alternative input file than the default",
    "Define a list of comma separated classes to be sent to a remote agent",
    "Define a list of comma separated classes to be used to select remote agents by constraint",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Pass options to a remote server process",
    "Activate internal diagnostics (developers only)",
    "Hail the following comma-separated lists of hosts, overriding default list",
    "Enable interactive mode for key trust",
    "Connection timeout, seconds",
    NULL
};

extern const ConstraintSyntax CFR_CONTROLBODY[];

int INTERACTIVE = false;
int OUTPUT_TO_FILE = false;
char OUTPUT_DIRECTORY[CF_BUFSIZE];
int BACKGROUND = false;
int MAXCHILD = 50;
char REMOTE_AGENT_OPTIONS[CF_MAXVARSIZE];
Attributes RUNATTR = { {0} };

Rlist *HOSTLIST = NULL;
char SENDCLASSES[CF_MAXVARSIZE];
char DEFINECLASSES[CF_MAXVARSIZE];

/*****************************************************************************/

int main(int argc, char *argv[])
{
    Rlist *rp;
#if !defined(__MINGW32__)
    int count = 0;
    int status;
    int pid;
#endif

    EvalContext *ctx = EvalContextNew();

    GenericAgentConfig *config = CheckOpts(ctx, argc, argv);
    GenericAgentConfigApply(ctx, config);

    GenericAgentDiscoverContext(ctx, config);
    Policy *policy = GenericAgentLoadPolicy(ctx, config);

    CheckForPolicyHub(ctx);

    ThisAgentInit();
    KeepControlPromises(ctx, policy);      // Set RUNATTR using copy

    if (BACKGROUND && INTERACTIVE)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! You cannot specify background mode and interactive mode together");
        exit(1);
    }

/* HvB */
    if (HOSTLIST)
    {
        rp = HOSTLIST;

        while (rp != NULL)
        {

#ifdef __MINGW32__
            if (BACKGROUND)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "",
                      "Windows does not support starting processes in the background - starting in foreground");
                BACKGROUND = false;
            }
#else
            if (BACKGROUND)     /* parallel */
            {
                if (count <= MAXCHILD)
                {
                    if (fork() == 0)    /* child process */
                    {
                        HailServer(ctx, rp->item);
                        exit(0);
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
                    CfDebug("child = %d, child number = %d\n", pid, count);
                    count--;
                }
            }
            else                /* serial */
#endif /* __MINGW32__ */
            {
                HailServer(ctx, rp->item);
                rp = rp->next;
            }
        }                       /* end while */
    }                           /* end if HOSTLIST */

#ifndef __MINGW32__
    if (BACKGROUND)
    {
        printf("Waiting for child processes to finish\n");
        while (count > 1)
        {
            pid = wait(&status);
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Child = %d ended, number = %d\n", pid, count);
            count--;
        }
    }
#endif

    GenericAgentConfigDestroy(config);

    return 0;
}

/*******************************************************************/

static GenericAgentConfig *CheckOpts(EvalContext *ctx, int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_RUNAGENT);

    DEFINECLASSES[0] = '\0';
    SENDCLASSES[0] = '\0';

    while ((c = getopt_long(argc, argv, "t:q:db:vnKhIif:D:VSxo:s:MH:", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'f':
            GenericAgentConfigSetInputFile(config, GetWorkDir(), optarg);
            MINUSF = true;
            break;

        case 'b':
            BACKGROUND = true;
            if (optarg)
            {
                MAXCHILD = atoi(optarg);
            }
            break;

        case 'd':
            config->debug_mode = true;
            break;

        case 'K':
            IGNORELOCK = true;
            break;

        case 's':
            strncpy(SENDCLASSES, optarg, CF_MAXVARSIZE);

            if (strlen(optarg) > CF_MAXVARSIZE)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Argument too long\n");
                exit(EXIT_FAILURE);
            }
            break;

        case 'D':
            strncpy(DEFINECLASSES, optarg, CF_MAXVARSIZE);

            if (strlen(optarg) > CF_MAXVARSIZE)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Argument too long\n");
                exit(EXIT_FAILURE);
            }
            break;

        case 'H':
            HOSTLIST = RlistFromSplitString(optarg, ',');
            break;

        case 'o':
            strncpy(REMOTE_AGENT_OPTIONS, optarg, CF_MAXVARSIZE);
            break;

        case 'I':
            INFORM = true;
            break;

        case 'i':
            INTERACTIVE = true;
            break;

        case 'v':
            VERBOSE = true;
            break;

        case 'n':
            DONTDO = true;
            IGNORELOCK = true;
            EvalContextHeapAddHard(ctx, "opt_dry_run");
            break;

        case 't':
            CONNTIMEOUT = atoi(optarg);
            break;

        case 'V':
            PrintVersion();
            exit(0);

        case 'h':
            Syntax("cf-runagent", OPTIONS, HINTS, ID, true);
            exit(0);

        case 'M':
            ManPage("cf-runagent - Run agent", OPTIONS, HINTS, ID);
            exit(0);

        case 'x':
            CfOut(OUTPUT_LEVEL_ERROR, "", "Self-diagnostic functionality is retired.");
            exit(0);

        default:
            Syntax("cf-runagent", OPTIONS, HINTS, ID, true);
            exit(1);

        }
    }

    if (!GenericAgentConfigParseArguments(config, argc - optind, argv + optind))
    {
        Log(LOG_LEVEL_ERR, "Too many arguments");
        exit(EXIT_FAILURE);
    }

    return config;
}

/*******************************************************************/

static void ThisAgentInit(void)
{
    umask(077);

    if (strstr(REMOTE_AGENT_OPTIONS, "--file") || strstr(REMOTE_AGENT_OPTIONS, "-f"))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              "The specified remote options include a useless --file option. The remote server has promised to ignore this, thus it is disallowed.\n");
        exit(1);
    }
}

/********************************************************************/

static int HailServer(EvalContext *ctx, char *host)
{
    AgentConnection *conn;
    char sendbuffer[CF_BUFSIZE], recvbuffer[CF_BUFSIZE], peer[CF_MAXVARSIZE],
        digest[CF_MAXVARSIZE], user[CF_SMALLBUF];
    bool gotkey;
    char reply[8];

    FileCopy fc = {
        .portnumber = (short) ParseHostname(host, peer),
    };

    char ipaddr[CF_MAX_IP_LEN];
    if (Hostname2IPString(ipaddr, peer, sizeof(ipaddr)) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
            "HailServer: ERROR, could not resolve %s", peer);
        return false;
    }

    Address2Hostkey(ipaddr, digest);
    GetCurrentUserName(user, CF_SMALLBUF);

    if (INTERACTIVE)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Using interactive key trust...\n");

        gotkey = HavePublicKey(user, peer, digest) != NULL;

        if (!gotkey)
        {
            gotkey = HavePublicKey(user, ipaddr, digest) != NULL;
        }

        if (!gotkey)
        {
            printf("WARNING - You do not have a public key from host %s = %s\n",
                   host, ipaddr);
            printf("          Do you want to accept one on trust? (yes/no)\n\n--> ");

            while (true)
            {
                if (fgets(reply, 8, stdin) == NULL)
                {
                    FatalError(ctx, "EOF trying to read answer from terminal");
                }

                if (Chop(reply, CF_EXPANDSIZE) == -1)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
                }

                if (strcmp(reply, "yes") == 0)
                {
                    printf(" -> Will trust the key...\n");
                    fc.trustkey = true;
                    break;
                }
                else if (strcmp(reply, "no") == 0)
                {
                    printf(" -> Will not trust the key...\n");
                    fc.trustkey = false;
                    break;
                }
                else
                {
                    printf(" !! Please reply yes or no...(%s)\n", reply);
                }
            }
        }
    }

/* Continue */

#ifdef __MINGW32__

    CfOut(OUTPUT_LEVEL_INFORM, "", "...........................................................................\n");
    CfOut(OUTPUT_LEVEL_INFORM, "", " * Hailing %s : %u, with options \"%s\" (serial)\n", peer, fc.portnumber,
          REMOTE_AGENT_OPTIONS);
    CfOut(OUTPUT_LEVEL_INFORM, "", "...........................................................................\n");

#else /* !__MINGW32__ */

    if (BACKGROUND)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Hailing %s : %u, with options \"%s\" (parallel)\n", peer, fc.portnumber,
              REMOTE_AGENT_OPTIONS);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "...........................................................................\n");
        CfOut(OUTPUT_LEVEL_INFORM, "", " * Hailing %s : %u, with options \"%s\" (serial)\n", peer, fc.portnumber,
              REMOTE_AGENT_OPTIONS);
        CfOut(OUTPUT_LEVEL_INFORM, "", "...........................................................................\n");
    }

#endif /* !__MINGW32__ */

    fc.servers = RlistFromSplitString(peer, '*');

    if (fc.servers == NULL || strcmp(fc.servers->item, "localhost") == 0)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "No hosts are registered to connect to");
        return false;
    }
    else
    {
        int err = 0;
        conn = NewServerConnection(fc, false, &err);

        if (conn == NULL)
        {
            RlistDestroy(fc.servers);
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> No suitable server responded to hail\n");
            return false;
        }
    }

/* Check trust interaction*/

    HailExec(conn, peer, recvbuffer, sendbuffer);

    RlistDestroy(fc.servers);

    return true;
}

/********************************************************************/
/* Level 2                                                          */
/********************************************************************/

static void KeepControlPromises(EvalContext *ctx, Policy *policy)
{
    Rval retval;

    RUNATTR.copy.trustkey = false;
    RUNATTR.copy.encrypt = true;
    RUNATTR.copy.force_ipv4 = false;
    RUNATTR.copy.portnumber = SHORT_CFENGINEPORT;

/* Keep promised agent behaviour - control bodies */

    Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_RUNAGENT);
    if (constraints)
    {
        for (size_t i = 0; i < SeqLength(constraints); i++)
        {
            Constraint *cp = SeqAt(constraints, i);

            if (!IsDefinedClass(ctx, cp->classes, NULL))
            {
                continue;
            }

            if (!EvalContextVariableGet(ctx, (VarRef) { NULL, "control_runagent", cp->lval }, &retval, NULL))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Unknown lval %s in runagent control body", cp->lval);
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_FORCE_IPV4].lval) == 0)
            {
                RUNATTR.copy.force_ipv4 = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET force_ipv4 = %d\n", RUNATTR.copy.force_ipv4);
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_TRUSTKEY].lval) == 0)
            {
                RUNATTR.copy.trustkey = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET trustkey = %d\n", RUNATTR.copy.trustkey);
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_ENCRYPT].lval) == 0)
            {
                RUNATTR.copy.encrypt = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET encrypt = %d\n", RUNATTR.copy.encrypt);
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_PORT_NUMBER].lval) == 0)
            {
                RUNATTR.copy.portnumber = (short) IntFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET default portnumber = %u\n", (int) RUNATTR.copy.portnumber);
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
                    CfOut(OUTPUT_LEVEL_ERROR, "",
                          "Warning: 'background_children' setting from 'body runagent control' is overriden by command-line option.");
                }
                else
                {
                    BACKGROUND = BooleanFromString(retval.item);
                }
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_MAX_CHILD].lval) == 0)
            {
                MAXCHILD = (short) IntFromString(retval.item);
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_OUTPUT_TO_FILE].lval) == 0)
            {
                OUTPUT_TO_FILE = BooleanFromString(retval.item);
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_OUTPUT_DIRECTORY].lval) == 0)
            {
                if (IsAbsPath(retval.item))
                {
                    strncpy(OUTPUT_DIRECTORY, retval.item, CF_BUFSIZE - 1);
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET output direcory to = %s\n", OUTPUT_DIRECTORY);
                }
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_TIMEOUT].lval) == 0)
            {
                RUNATTR.copy.timeout = (short) IntFromString(retval.item);
                continue;
            }

            if (strcmp(cp->lval, CFR_CONTROLBODY[RUNAGENT_CONTROL_HOSTS].lval) == 0)
            {
                if (HOSTLIST == NULL)       // Don't override if command line setting
                {
                    HOSTLIST = retval.item;
                }

                continue;
            }
        }
    }

    if (EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_LASTSEEN_EXPIRE_AFTER, &retval))
    {
        LASTSEENEXPIREAFTER = IntFromString(retval.item) * 60;
    }

}

/********************************************************************/

static int ParseHostname(char *name, char *hostname)
{
    int port = ntohs(SHORT_CFENGINEPORT);

    if (strchr(name, ':'))
    {
        sscanf(name, "%250[^:]:%d", hostname, &port);
    }
    else
    {
        strncpy(hostname, name, CF_MAXVARSIZE);
    }

    return (port);
}

/********************************************************************/

static void SendClassData(AgentConnection *conn)
{
    Rlist *classes, *rp;
    char sendbuffer[CF_BUFSIZE];

    classes = RlistFromSplitRegex(SENDCLASSES, "[,: ]", 99, false);

    for (rp = classes; rp != NULL; rp = rp->next)
    {
        if (SendTransaction(conn->sd, rp->item, 0, CF_DONE) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "send", "Transaction failed");
            return;
        }
    }

    snprintf(sendbuffer, CF_MAXVARSIZE, "%s", CFD_TERMINATOR);

    if (SendTransaction(conn->sd, sendbuffer, 0, CF_DONE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "send", "Transaction failed");
        return;
    }
}

/********************************************************************/

static void HailExec(AgentConnection *conn, char *peer, char *recvbuffer, char *sendbuffer)
{
    FILE *fp = stdout;
    char *sp;
    int n_read;

    if (strlen(DEFINECLASSES))
    {
        snprintf(sendbuffer, CF_BUFSIZE, "EXEC %s -D%s", REMOTE_AGENT_OPTIONS, DEFINECLASSES);
    }
    else
    {
        snprintf(sendbuffer, CF_BUFSIZE, "EXEC %s", REMOTE_AGENT_OPTIONS);
    }

    if (SendTransaction(conn->sd, sendbuffer, 0, CF_DONE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "send", "Transmission rejected");
        DisconnectServer(conn);
        return;
    }

    fp = NewStream(peer);
    SendClassData(conn);

    while (true)
    {
        memset(recvbuffer, 0, CF_BUFSIZE);

        if ((n_read = ReceiveTransaction(conn->sd, recvbuffer, NULL)) == -1)
        {
            return;
        }

        if (n_read == 0)
        {
            break;
        }

        if (strlen(recvbuffer) == 0)
        {
            continue;
        }

        if ((sp = strstr(recvbuffer, CFD_TERMINATOR)) != NULL)
        {
            break;
        }

        if ((sp = strstr(recvbuffer, "BAD:")) != NULL)
        {
            fprintf(fp, "%s> !! %s\n", VPREFIX, recvbuffer + 4);
            continue;
        }

        if (strstr(recvbuffer, "too soon"))
        {
            fprintf(fp, "%s> !! %s\n", VPREFIX, recvbuffer);
            continue;
        }

        fprintf(fp, "%s> -> %s", VPREFIX, recvbuffer);
    }

    DeleteStream(fp);
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
        snprintf(filename, CF_BUFSIZE, "%s/outputs/%s_runagent.out", CFWORKDIR, name);
    }

    if (OUTPUT_TO_FILE)
    {
        printf("Opening file...%s\n", filename);

        if ((fp = fopen(filename, "w")) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to open file %s\n", filename);
            fp = stdout;
        }
    }
    else
    {
        fp = stdout;
    }

    return fp;
}

/********************************************************************/

static void DeleteStream(FILE *fp)
{
    if (fp != stdout)
    {
        fclose(fp);
    }
}
