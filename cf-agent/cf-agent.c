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

#include "env_context.h"
#include "verify_databases.h"
#include "verify_environments.h"
#include "verify_exec.h"
#include "verify_methods.h"
#include "verify_processes.h"
#include "verify_packages.h"
#include "verify_outputs.h"
#include "verify_services.h"
#include "verify_storage.h"
#include "verify_files_utils.h"
#include "addr_lib.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_repository.h"
#include "files_edit.h"
#include "files_properties.h"
#include "item_lib.h"
#include "vars.h"
#include "conversion.h"
#include "expand.h"
#include "transaction.h"
#include "scope.h"
#include "matching.h"
#include "instrumentation.h"
#include "unix.h"
#include "attributes.h"
#include "cfstream.h"
#include "communication.h"
#include "signals.h"
#include "logging.h"
#include "nfs.h"
#include "processes_select.h"
#include "list.h"
#include "fncall.h"
#include "rlist.h"

typedef enum
{
    TYPE_SEQUENCE_META,
    TYPE_SEQUENCE_VARS,
    TYPE_SEQUENCE_DEFAULTS,
    TYPE_SEQUENCE_CONTEXTS,
    TYPE_SEQUENCE_OUTPUTS,
    TYPE_SEQUENCE_INTERFACES,
    TYPE_SEQUENCE_FILES,
    TYPE_SEQUENCE_PACKAGES,
    TYPE_SEQUENCE_ENVIRONMENTS,
    TYPE_SEQUENCE_METHODS,
    TYPE_SEQUENCE_PROCESSES,
    TYPE_SEQUENCE_SERVICES,
    TYPE_SEQUENCE_COMMANDS,
    TYPE_SEQUENCE_STORAGE,
    TYPE_SEQUENCE_DATABASES,
    TYPE_SEQUENCE_REPORTS,
    TYPE_SEQUENCE_NONE
} TypeSequence;

#ifdef HAVE_AVAHI_CLIENT_CLIENT_H
#ifdef HAVE_AVAHI_COMMON_ADDRESS_H
#include "findhub.h"
#endif
#endif

#ifdef HAVE_NOVA
#include "cf.nova.h"
#include "agent_reports.h"
#else
#include "reporting.h"
#endif

extern int PR_KEPT;
extern int PR_REPAIRED;
extern int PR_NOTKEPT;

static bool ALLCLASSESREPORT;
static bool ALWAYS_VALIDATE;
static bool CFPARANOID = false;

static Rlist *ACCESSLIST;

static int CFA_BACKGROUND = 0;
static int CFA_BACKGROUND_LIMIT = 1;

static Item *PROCESSREFRESH;

static const char *AGENT_TYPESEQUENCE[] =
{
    "meta",
    "vars",
    "defaults",
    "classes",                  /* Maelstrom order 2 */
    "outputs",
    "interfaces",
    "files",
    "packages",
    "guest_environments",
    "methods",
    "processes",
    "services",
    "commands",
    "storage",
    "databases",
    "reports",
    NULL
};

/*******************************************************************/
/* Agent specific variables                                        */
/*******************************************************************/

static void ThisAgentInit(void);
static GenericAgentConfig *CheckOpts(int argc, char **argv);
static void CheckAgentAccess(Rlist *list, const Rlist *input_files);
static void KeepControlPromises(Policy *policy);
static void KeepAgentPromise(Promise *pp, const ReportContext *report_context);
static int NewTypeContext(TypeSequence type);
static void DeleteTypeContext(Policy *policy, TypeSequence type, const ReportContext *report_context);
static void ClassBanner(TypeSequence type);
static void ParallelFindAndVerifyFilesPromises(Promise *pp, const ReportContext *report_context);
static bool VerifyBootstrap(void);
static void KeepPromiseBundles(Policy *policy, GenericAgentConfig *config, const ReportContext *report_context);
static void KeepPromises(Policy *policy, GenericAgentConfig *config, const ReportContext *report_context);
static int NoteBundleCompliance(const Bundle *bundle, int save_pr_kept, int save_pr_repaired, int save_pr_notkept);
#ifdef HAVE_AVAHI_CLIENT_CLIENT_H
#ifdef HAVE_AVAHI_COMMON_ADDRESS_H
static int AutomaticBootstrap();
#endif
#endif

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *ID = "The main Cfengine agent is the instigator of change\n"
    "in the system. In that sense it is the most important\n" "part of the CFEngine suite.\n";

static const struct option OPTIONS[15] =
{
    {"bootstrap", no_argument, 0, 'B'},
    {"bundlesequence", required_argument, 0, 'b'},
    {"debug", no_argument, 0, 'd'},
    {"define", required_argument, 0, 'D'},
    {"diagnostic", optional_argument, 0, 'x'},
    {"dry-run", no_argument, 0, 'n'},
    {"file", required_argument, 0, 'f'},
    {"help", no_argument, 0, 'h'},
    {"inform", no_argument, 0, 'I'},
    {"negate", required_argument, 0, 'N'},
    {"no-lock", no_argument, 0, 'K'},
    {"policy-server", required_argument, 0, 's'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, '\0'}
};

static const char *HINTS[15] =
{
    "Bootstrap/repair a cfengine configuration from failsafe file in the WORKDIR else in current directory",
    "Set or override bundlesequence from command line",
    "Enable debugging output",
    "Define a list of comma separated classes to be defined at the start of execution",
    "Do internal diagnostic (developers only) level in optional argument",
    "All talk and no action mode - make no changes, only inform of promises not kept",
    "Specify an alternative input file than the default",
    "Print the help message",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Define a list of comma separated classes to be undefined at the start of execution",
    "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
    "Define the server name or IP address of the a policy server (for use with bootstrap)",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    NULL
};

#ifndef HAVE_NOVA
void NoteEfficiency(double e)
{
}
#endif

/*******************************************************************/

int main(int argc, char *argv[])
{
    int ret = 0;

    GenericAgentConfig *config = CheckOpts(argc, argv);
#ifdef HAVE_AVAHI_CLIENT_CLIENT_H
#ifdef HAVE_AVAHI_COMMON_ADDRESS_H
    if (NULL_OR_EMPTY(POLICY_SERVER) && BOOTSTRAP)
    {
        int ret = AutomaticBootstrap();

        if (ret < 0)
        {
            return 1;
        }
    }
#endif
#endif
    ReportContext *report_context = OpenReports(config->agent_type);

    GenericAgentDiscoverContext(config, report_context);

    Policy *policy = NULL;
    if (GenericAgentCheckPolicy(config, ALWAYS_VALIDATE))
    {
        policy = GenericAgentLoadPolicy(config->agent_type, config, report_context);
    }
    else if (config->tty_interactive)
    {
        FatalError("CFEngine was not able to get confirmation of promises from cf-promises, please verify input file\n");
    }
    else
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "CFEngine was not able to get confirmation of promises from cf-promises, so going to failsafe\n");
        HardClass("failsafe_fallback");
        GenericAgentConfigSetInputFile(config, "failsafe.cf");
        policy = GenericAgentLoadPolicy(config->agent_type, config, report_context);
    }

    CheckLicenses();

    ThisAgentInit();
    BeginAudit();
    KeepPromises(policy, config, report_context);
    CloseReports("agent", report_context);

    // only note class usage when default policy is run
    if (!config->input_file)
    {
        NoteClassUsage(VHEAP, true);
        NoteClassUsage(VHARDHEAP, true);
    }
#ifdef HAVE_NOVA
    Nova_NoteVarUsageDB();
    Nova_TrackExecution(config->input_file);
#endif
    PurgeLocks();

    if (BOOTSTRAP && !VerifyBootstrap())
    {
        ret = 1;
    }

    EndAudit(CFA_BACKGROUND);
    GenericAgentConfigDestroy(config);

    return ret;
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    char *sp;
    int optindex = 0;
    int c, alpha = false, v6 = false;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_AGENT);

/* Because of the MacOS linker we have to call this from each agent
   individually before Generic Initialize */

    POLICY_SERVER[0] = '\0';

    while ((c = getopt_long(argc, argv, "rdvnKIf:D:N:Vs:x:MBb:h", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'f':
            if (optarg && strlen(optarg) < 5)
            {
                FatalError(" -f used but argument \"%s\" incorrect", optarg);
            }

            GenericAgentConfigSetInputFile(config, optarg);
            MINUSF = true;
            break;

        case 'b':
            if (optarg)
            {
                config->bundlesequence = RlistFromSplitString(optarg, ',');
                CBUNDLESEQUENCE_STR = optarg;
            }
            break;

        case 'd':
            HardClass("opt_debug");
            DEBUG = true;
            break;

        case 'B':
            BOOTSTRAP = true;
            MINUSF = true;
            GenericAgentConfigSetInputFile(config, "promises.cf");
            IGNORELOCK = true;
            HardClass("bootstrap_mode");
            break;

        case 's':
            
            if(IsLoopbackAddress(optarg))
            {
                FatalError("Use a non-loopback address when bootstrapping");
            }

            // temporary assure that network functions are working
            OpenNetwork();

            strncpy(POLICY_SERVER, Hostname2IPString(optarg), CF_BUFSIZE - 1);

            CloseNetwork();

            for (sp = POLICY_SERVER; *sp != '\0'; sp++)
            {
                if (isalpha((int)*sp))
                {
                    alpha = true;
                }

                if (ispunct((int)*sp) && *sp != ':' && *sp != '.')
                {
                    alpha = true;
                }

                if (*sp == ':')
                {
                    v6 = true;
                }
            }

            if (alpha && !v6)
            {
                FatalError
                    ("Error specifying policy server. The policy server's IP address could not be looked up. Please use the IP address instead if there is no error.");
            }

            break;

        case 'K':
            IGNORELOCK = true;
            break;

        case 'D':
            NewClassesFromString(optarg);
            break;

        case 'N':
            NegateClassesFromString(optarg);
            break;

        case 'I':
            INFORM = true;
            break;

        case 'v':
            VERBOSE = true;
            break;

        case 'n':
            DONTDO = true;
            IGNORELOCK = true;
            HardClass("opt_dry_run");
            break;

        case 'V':
            PrintVersionBanner("cf-agent");
            exit(0);

        case 'h':
            Syntax("cf-agent - cfengine's change agent", OPTIONS, HINTS, ID);
            exit(0);

        case 'M':
            ManPage("cf-agent - cfengine's change agent", OPTIONS, HINTS, ID);
            exit(0);

        case 'x':
            CfOut(OUTPUT_LEVEL_ERROR, "", "Self-diagnostic functionality is retired");
            exit(0);

        case 'r':
            SHOWREPORTS = true;
            break;

        default:
            Syntax("cf-agent - cfengine's change agent", OPTIONS, HINTS, ID);
            exit(1);
        }
    }

    if (argv[optind] != NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unexpected argument with no preceding option: %s\n", argv[optind]);
        FatalError("Aborted");
    }

    CfDebug("Set debugging\n");

    return config;
}

/*******************************************************************/

static void ThisAgentInit(void)
{
    FILE *fp;
    char filename[CF_BUFSIZE];

#ifdef HAVE_SETSID
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Immunizing against parental death");
    setsid();
#endif

    signal(SIGINT, HandleSignalsForAgent);
    signal(SIGTERM, HandleSignalsForAgent);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignalsForAgent);
    signal(SIGUSR2, HandleSignalsForAgent);

    CFA_MAXTHREADS = 30;
    EDITFILESIZE = 100000;

/*
  do not set signal(SIGCHLD,SIG_IGN) in agent near
  popen() - or else pclose will fail to return
  status which we need for setting returns
*/

    snprintf(filename, CF_BUFSIZE, "%s/cfagent.%s.log", CFWORKDIR, VSYSNAME.nodename);
    MapName(filename);

    if ((fp = fopen(filename, "a")) != NULL)
    {
        fclose(fp);
    }
}

/*******************************************************************/

static void KeepPromises(Policy *policy, GenericAgentConfig *config, const ReportContext *report_context)
{
    double efficiency, model;

    KeepControlPromises(policy);
    KeepPromiseBundles(policy, config, report_context);

// TOPICS counts the number of currently defined promises
// OCCUR counts the number of objects touched while verifying config

    efficiency = 100.0 * CF_OCCUR / (double) (CF_OCCUR + CF_TOPICS);
    model = 100.0 * (1.0 - CF_TOPICS / (double)(PR_KEPT + PR_NOTKEPT + PR_REPAIRED));
    
    NoteEfficiency(efficiency);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Checked %d objects with %d promises, i.e. model efficiency %.2lf%%", CF_OCCUR, CF_TOPICS, efficiency);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> The %d declared promise patterns actually expanded into %d individual promises, i.e. declaration efficiency %.2lf%%", (int) CF_TOPICS, PR_KEPT + PR_NOTKEPT + PR_REPAIRED, model);

}

/*******************************************************************/
/* Level 2                                                         */
/*******************************************************************/

void KeepControlPromises(Policy *policy)
{
    Rval retval;
    Rlist *rp;

    Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_AGENT);
    if (constraints)
    {
        for (size_t i = 0; i < SeqLength(constraints); i++)
        {
            Constraint *cp = SeqAt(constraints, i);

            if (IsExcluded(cp->classes, NULL))
            {
                continue;
            }

            if (GetVariable("control_common", cp->lval, &retval) != DATA_TYPE_NONE)
            {
                /* Already handled in generic_agent */
                continue;
            }

            if (GetVariable("control_agent", cp->lval, &retval) == DATA_TYPE_NONE)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Unknown lval %s in agent control body", cp->lval);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_MAXCONNECTIONS].lval) == 0)
            {
                CFA_MAXTHREADS = (int) IntFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET maxconnections = %d\n", CFA_MAXTHREADS);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_CHECKSUM_ALERT_TIME].lval) == 0)
            {
                CF_PERSISTENCE = (int) IntFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET checksum_alert_time = %d\n", CF_PERSISTENCE);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_AGENTFACILITY].lval) == 0)
            {
                SetFacility(retval.item);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_AGENTACCESS].lval) == 0)
            {
                ACCESSLIST = (Rlist *) retval.item;
                CheckAgentAccess(ACCESSLIST, InputFiles(policy));
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_REFRESH_PROCESSES].lval) == 0)
            {
                Rlist *rp;

                if (VERBOSE)
                {
                    printf("%s> SET refresh_processes when starting: ", VPREFIX);

                    for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                    {
                        printf(" %s", (char *) rp->item);
                        PrependItem(&PROCESSREFRESH, rp->item, NULL);
                    }

                    printf("\n");
                }

                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ABORTCLASSES].lval) == 0)
            {
                Rlist *rp;

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET Abort classes from ...\n");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                {
                    char name[CF_MAXVARSIZE] = "";

                    strncpy(name, rp->item, CF_MAXVARSIZE - 1);

                    AddAbortClass(name, cp->classes);
                }

                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ABORTBUNDLECLASSES].lval) == 0)
            {
                Rlist *rp;

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET Abort bundle classes from ...\n");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                {
                    char name[CF_MAXVARSIZE] = "";

                    strncpy(name, rp->item, CF_MAXVARSIZE - 1);

                    if (!IsItemIn(ABORTBUNDLEHEAP, name))
                    {
                        AppendItem(&ABORTBUNDLEHEAP, name, cp->classes);
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ADDCLASSES].lval) == 0)
            {
                Rlist *rp;

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "-> Add classes ...\n");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> ... %s\n", RlistScalarValue(rp));
                    NewClass(rp->item, NULL);
                }

                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_AUDITING].lval) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "This option does nothing and is retained for compatibility reasons");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ALWAYSVALIDATE].lval) == 0)
            {
                ALWAYS_VALIDATE = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET alwaysvalidate = %d\n", ALWAYS_VALIDATE);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ALLCLASSESREPORT].lval) == 0)
            {
                ALLCLASSESREPORT = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET allclassesreport = %d\n", ALLCLASSESREPORT);
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_SECUREINPUT].lval) == 0)
            {
                CFPARANOID = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET secure input = %d\n", CFPARANOID);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_BINARYPADDINGCHAR].lval) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "binarypaddingchar is obsolete and does nothing\n");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_BINDTOINTERFACE].lval) == 0)
            {
                strncpy(BINDINTERFACE, retval.item, CF_BUFSIZE - 1);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET bindtointerface = %s\n", BINDINTERFACE);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_HASHUPDATES].lval) == 0)
            {
                bool enabled = BooleanFromString(retval.item);

                SetChecksumUpdates(enabled);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET ChecksumUpdates %d\n", enabled);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_EXCLAMATION].lval) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "exclamation control is deprecated and does not do anything\n");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_CHILDLIBPATH].lval) == 0)
            {
                char output[CF_BUFSIZE];

                snprintf(output, CF_BUFSIZE, "LD_LIBRARY_PATH=%s", (char *) retval.item);
                if (putenv(xstrdup(output)) == 0)
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Setting %s\n", output);
                }
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_DEFAULTCOPYTYPE].lval) == 0)
            {
                DEFAULT_COPYTYPE = (char *) retval.item;
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET defaultcopytype = %s\n", DEFAULT_COPYTYPE);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_FSINGLECOPY].lval) == 0)
            {
                SINGLE_COPY_LIST = (Rlist *) retval.item;
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET file single copy list\n");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_FAUTODEFINE].lval) == 0)
            {
                SetFileAutoDefineList(RvalRlistValue(retval));
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET file auto define list\n");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_DRYRUN].lval) == 0)
            {
                DONTDO = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET dryrun = %c\n", DONTDO);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_INFORM].lval) == 0)
            {
                INFORM = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET inform = %c\n", INFORM);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_VERBOSE].lval) == 0)
            {
                VERBOSE = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET inform = %c\n", VERBOSE);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_REPOSITORY].lval) == 0)
            {
                SetRepositoryLocation(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET repository = %s\n", RvalScalarValue(retval));
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_SKIPIDENTIFY].lval) == 0)
            {
                bool enabled = BooleanFromString(retval.item);

                SetSkipIdentify(enabled);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET skipidentify = %d\n", (int) enabled);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_SUSPICIOUSNAMES].lval) == 0)
            {

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                {
                    AddFilenameToListOfSuspicious(RlistScalarValue(rp));
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "-> Considering %s as suspicious file", RlistScalarValue(rp));
                }

                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_REPCHAR].lval) == 0)
            {
                char c = *(char *) retval.item;

                SetRepositoryChar(c);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET repchar = %c\n", c);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_MOUNTFILESYSTEMS].lval) == 0)
            {
                CF_MOUNTALL = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET mountfilesystems = %d\n", CF_MOUNTALL);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_EDITFILESIZE].lval) == 0)
            {
                EDITFILESIZE = IntFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET EDITFILESIZE = %d\n", EDITFILESIZE);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_IFELAPSED].lval) == 0)
            {
                VIFELAPSED = IntFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET ifelapsed = %d\n", VIFELAPSED);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_EXPIREAFTER].lval) == 0)
            {
                VEXPIREAFTER = IntFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET ifelapsed = %d\n", VEXPIREAFTER);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_TIMEOUT].lval) == 0)
            {
                CONNTIMEOUT = IntFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET timeout = %jd\n", (intmax_t) CONNTIMEOUT);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_MAX_CHILDREN].lval) == 0)
            {
                CFA_BACKGROUND_LIMIT = IntFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET MAX_CHILDREN = %d\n", CFA_BACKGROUND_LIMIT);
                if (CFA_BACKGROUND_LIMIT > 10)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "Silly value for max_children in agent control promise (%d > 10)",
                          CFA_BACKGROUND_LIMIT);
                    CFA_BACKGROUND_LIMIT = 1;
                }
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_SYSLOG].lval) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET syslog = %d\n", BooleanFromString(retval.item));
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ENVIRONMENT].lval) == 0)
            {
                Rlist *rp;

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET environment variables from ...\n");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                {
                    if (putenv(rp->item) != 0)
                    {
                        CfOut(OUTPUT_LEVEL_ERROR, "putenv", "Failed to set environment variable %s", RlistScalarValue(rp));
                    }
                }

                continue;
            }
        }
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[COMMON_CONTROL_LASTSEEN_EXPIRE_AFTER].lval, &retval) != DATA_TYPE_NONE)
    {
        LASTSEENEXPIREAFTER = IntFromString(retval.item) * 60;
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[COMMON_CONTROL_FIPS_MODE].lval, &retval) != DATA_TYPE_NONE)
    {
        FIPS_MODE = BooleanFromString(retval.item);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET FIPS_MODE = %d\n", FIPS_MODE);
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[COMMON_CONTROL_SYSLOG_PORT].lval, &retval) != DATA_TYPE_NONE)
    {
        SetSyslogPort(IntFromString(retval.item));
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET syslog_port to %s", RvalScalarValue(retval));
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[COMMON_CONTROL_SYSLOG_HOST].lval, &retval) != DATA_TYPE_NONE)
    {
        SetSyslogHost(Hostname2IPString(retval.item));
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET syslog_host to %s", Hostname2IPString(retval.item));
    }

#ifdef HAVE_NOVA
    Nova_Initialize();
#endif
}

/*********************************************************************/

static void KeepPromiseBundles(Policy *policy, GenericAgentConfig *config, const ReportContext *report_context)
{
    Bundle *bp;
    Rlist *rp, *params;
    FnCall *fp;
    char *name;
    Rval retval;
    int ok = true;

    if (config->bundlesequence)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", " >> Using command line specified bundlesequence");
        retval = (Rval) { config->bundlesequence, RVAL_TYPE_LIST };
    }
    else if (GetVariable("control_common", "bundlesequence", &retval) == DATA_TYPE_NONE)
    {
        // TODO: somewhat frenzied way of telling user about an error
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! No bundlesequence in the common control body");
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        exit(1);
    }

    // TODO: should've been checked a long time ago, remove?
    if (retval.type != RVAL_TYPE_LIST)
    {
        FatalError("Promised bundlesequence was not a list");
    }

    for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case RVAL_TYPE_SCALAR:
            name = (char *) rp->item;
            params = NULL;

            if (strcmp(name, CF_NULL_VALUE) == 0)
            {
                continue;
            }

            break;
        case RVAL_TYPE_FNCALL:
            fp = (FnCall *) rp->item;
            name = (char *) fp->name;
            params = (Rlist *) fp->args;
            break;

        default:
            name = NULL;
            params = NULL;
            CfOut(OUTPUT_LEVEL_ERROR, "", "Illegal item found in bundlesequence: ");
            RvalShow(stdout, (Rval) {rp->item, rp->type});
            printf(" = %c\n", rp->type);
            ok = false;
            break;
        }

        if (!config->ignore_missing_bundles)
        {
            if (!(PolicyGetBundle(policy, NULL, "agent", name) || (PolicyGetBundle(policy, NULL, "common", name))))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Bundle \"%s\" listed in the bundlesequence was not found\n", name);
                ok = false;
            }
        }
    }

    if (!ok)
    {
        FatalError("Errors in agent bundles");
    }

    if (VERBOSE || DEBUG)
    {
        printf("%s> -> Bundlesequence => ", VPREFIX);
        RvalShow(stdout, retval);
        printf("\n");
    }

/* If all is okay, go ahead and evaluate */

    for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case RVAL_TYPE_FNCALL:
            fp = (FnCall *) rp->item;
            name = (char *) fp->name;
            params = (Rlist *) fp->args;
            break;
        default:
            name = (char *) rp->item;
            params = NULL;
            break;
        }

        if ((bp = PolicyGetBundle(policy, NULL, "agent", name)) || (bp = PolicyGetBundle(policy, NULL, "common", name)))
        {
            char ns[CF_BUFSIZE];
            snprintf(ns,CF_BUFSIZE,"%s_meta", name);
            NewScope(ns);

            SetBundleOutputs(bp->name);
            AugmentScope(bp->name, bp->ns, bp->args, params);
            BannerBundle(bp, params);
            THIS_BUNDLE = bp->name;
            DeletePrivateClassContext();        // Each time we change bundle
            ScheduleAgentOperations(bp, report_context);
            ResetBundleOutputs(bp->name);
        }
    }
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

int ScheduleAgentOperations(Bundle *bp, const ReportContext *report_context)
// NB - this function can be called recursively through "methods"
{
    SubType *sp;
    TypeSequence type;
    int pass;
    int save_pr_kept = PR_KEPT;
    int save_pr_repaired = PR_REPAIRED;
    int save_pr_notkept = PR_NOTKEPT;

    if (PROCESSREFRESH == NULL || (PROCESSREFRESH && IsRegexItemIn(PROCESSREFRESH, bp->name)))
    {
        DeleteItemList(PROCESSTABLE);
        PROCESSTABLE = NULL;
    }

    for (pass = 1; pass < CF_DONEPASSES; pass++)
    {
        for (type = 0; AGENT_TYPESEQUENCE[type] != NULL; type++)
        {
            ClassBanner(type);

            if ((sp = BundleGetSubType(bp, AGENT_TYPESEQUENCE[type])) == NULL)
            {
                continue;
            }

            BannerSubType(bp->name, sp->name, pass);
            SetScope(bp->name);

            if (!NewTypeContext(type))
            {
                continue;
            }

            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                Promise *pp = SeqAt(sp->promises, ppi);

                if (ALLCLASSESREPORT)
                {
                    SaveClassEnvironment();
                }

                if (pass == 1)  // Count the number of promises modelled for efficiency
                {
                    CF_TOPICS++;
                }

                ExpandPromise(AGENT_TYPE_AGENT, bp->name, pp, KeepAgentPromise, report_context);

                if (Abort())
                {
                    NoteClassUsage(VADDCLASSES, false);
                    DeleteTypeContext(bp->parent_policy, type, report_context);
                    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept);
                    return false;
                }
            }

            DeleteTypeContext(bp->parent_policy, type, report_context);
        }
    }

    NoteClassUsage(VADDCLASSES, false);
    return NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept);
}

/*********************************************************************/

#ifdef __MINGW32__

static void CheckAgentAccess(Rlist *list)
{
}

#else

static void CheckAgentAccess(Rlist *list, const Rlist *input_files)
{
    struct stat sb;
    uid_t uid;
    int access = false;

    uid = getuid();

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (Str2Uid(rp->item, NULL, NULL) == uid)
        {
            return;
        }
    }

    for (const Rlist *rp = input_files; rp != NULL; rp = rp->next)
    {
        cfstat(rp->item, &sb);

        if (ACCESSLIST)
        {
            for (const Rlist *rp2 = ACCESSLIST; rp2 != NULL; rp2 = rp2->next)
            {
                if (Str2Uid(rp2->item, NULL, NULL) == sb.st_uid)
                {
                    access = true;
                    break;
                }
            }

            if (!access)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "File %s is not owned by an authorized user (security exception)",
                      RlistScalarValue(rp));
                exit(1);
            }
        }
        else if (CFPARANOID && IsPrivileged())
        {
            if (sb.st_uid != getuid())
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "File %s is not owned by uid %ju (security exception)", RlistScalarValue(rp),
                      (uintmax_t)getuid());
                exit(1);
            }
        }
    }

    FatalError("You are denied access to run this policy");
}
#endif /* !__MINGW32__ */

/*********************************************************************/

/**************************************************************/

static void DefaultVarPromise(const Promise *pp)
{
    char *regex = ConstraintGetRvalValue("if_match_regex", pp, RVAL_TYPE_SCALAR);
    Rval rval;
    DataType dt;
    Rlist *rp;
    bool okay = true;

    dt = GetVariable("this", pp->promiser, &rval);

    switch (dt)
       {
       case DATA_TYPE_STRING:
       case DATA_TYPE_INT:
       case DATA_TYPE_REAL:

           if (regex && !FullTextMatch(regex,rval.item))
              {
              return;
              }

           if (regex == NULL)
              {
              return;
              }

           break;

       case DATA_TYPE_STRING_LIST:
       case DATA_TYPE_INT_LIST:
       case DATA_TYPE_REAL_LIST:

           if (regex)
              {
              for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
                 {
                 if (FullTextMatch(regex,rp->item))
                    {
                    okay = false;
                    break;
                    }
                 }

              if (okay)
                 {
                 return;
                 }
              }

       break;

       default:
           break;
       }

    DeleteScalar(pp->bundle, pp->promiser);
    ConvergeVarHashPromise(pp->bundle, pp, true);
}

static void KeepAgentPromise(Promise *pp, const ReportContext *report_context)
{
    char *sp = NULL;
    struct timespec start = BeginMeasure();

    if (!IsDefinedClass(pp->classes, pp->ns))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping whole next promise (%s), as context %s is not relevant\n", pp->promiser,
              pp->classes);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }

    if (pp->done)
    {
        return;
    }

    if (VarClassExcluded(pp, &sp))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping whole next promise (%s), as var-context %s is not relevant\n", pp->promiser,
              sp);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }


    if (MissingDependencies(pp))
    {
        return;
    }
    
// Record promises examined for efficiency calc

    if (strcmp("meta", pp->agentsubtype) == 0)
    {
        char ns[CF_BUFSIZE];
        snprintf(ns,CF_BUFSIZE,"%s_meta",pp->bundle);
        NewScope(ns);
        ConvergeVarHashPromise(ns, pp, true);
        return;
    }

    if (strcmp("vars", pp->agentsubtype) == 0)
    {
        ConvergeVarHashPromise(pp->bundle, pp, true);
        return;
    }

    if (strcmp("defaults", pp->agentsubtype) == 0)
    {
        DefaultVarPromise(pp);
        return;
    }

    
    if (strcmp("classes", pp->agentsubtype) == 0)
    {
        KeepClassContextPromise(pp);
        return;
    }

    if (strcmp("outputs", pp->agentsubtype) == 0)
    {
        VerifyOutputsPromise(pp);
        return;
    }

    SetPromiseOutputs(pp);

    if (strcmp("interfaces", pp->agentsubtype) == 0)
    {
        VerifyInterfacesPromise(pp);
        return;
    }

    if (strcmp("processes", pp->agentsubtype) == 0)
    {
        VerifyProcessesPromise(pp);
        return;
    }

    if (strcmp("storage", pp->agentsubtype) == 0)
    {
        FindAndVerifyStoragePromises(pp, report_context);
        EndMeasurePromise(start, pp);
        return;
    }

    if (strcmp("packages", pp->agentsubtype) == 0)
    {
        VerifyPackagesPromise(pp);
        EndMeasurePromise(start, pp);
        return;
    }

    if (strcmp("files", pp->agentsubtype) == 0)
    {
        if (PromiseGetConstraintAsBoolean("background", pp))
        {
            ParallelFindAndVerifyFilesPromises(pp, report_context);
        }
        else
        {
            FindAndVerifyFilesPromises(pp, report_context);
        }

        EndMeasurePromise(start, pp);
        return;
    }

    if (strcmp("commands", pp->agentsubtype) == 0)
    {
        VerifyExecPromise(pp);
        EndMeasurePromise(start, pp);
        return;
    }

    if (strcmp("databases", pp->agentsubtype) == 0)
    {
        VerifyDatabasePromises(pp);
        EndMeasurePromise(start, pp);
        return;
    }

    if (strcmp("methods", pp->agentsubtype) == 0)
    {
        VerifyMethodsPromise(pp, report_context);
        EndMeasurePromise(start, pp);
        return;
    }

    if (strcmp("services", pp->agentsubtype) == 0)
    {
        VerifyServicesPromise(pp, report_context);
        EndMeasurePromise(start, pp);
        return;
    }

    if (strcmp("guest_environments", pp->agentsubtype) == 0)
    {
        VerifyEnvironmentsPromise(pp);
        EndMeasurePromise(start, pp);
        return;
    }

    if (strcmp("reports", pp->agentsubtype) == 0)
    {
        VerifyReportPromise(pp);
        return;
    }
}

/*********************************************************************/
/* Type context                                                      */
/*********************************************************************/

static int NewTypeContext(TypeSequence type)
{
// get maxconnections

    switch (type)
    {
    case TYPE_SEQUENCE_ENVIRONMENTS:
        NewEnvironmentsContext();
        break;

    case TYPE_SEQUENCE_FILES:

        ConnectionsInit();
        break;

    case TYPE_SEQUENCE_PROCESSES:

        if (!LoadProcessTable(&PROCESSTABLE))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to read the process table - cannot keep process promises\n");
            return false;
        }
        break;

    case TYPE_SEQUENCE_STORAGE:

#ifndef __MINGW32__                   // TODO: Run if implemented on Windows
        if (MOUNTEDFSLIST != NULL)
        {
            DeleteMountInfo(MOUNTEDFSLIST);
            MOUNTEDFSLIST = NULL;
        }
#endif /* !__MINGW32__ */
        break;

    default:

        /* Initialization is not required */
        ;
    }

    return true;
}

/*********************************************************************/

static void DeleteTypeContext(Policy *policy, TypeSequence type, const ReportContext *report_context)
{
    switch (type)
    {
    case TYPE_SEQUENCE_CONTEXTS:
        HashVariables(policy, THIS_BUNDLE, report_context);
        break;

    case TYPE_SEQUENCE_ENVIRONMENTS:
        DeleteEnvironmentsContext();
        break;

    case TYPE_SEQUENCE_FILES:

        ConnectionsCleanup();
        break;

    case TYPE_SEQUENCE_PROCESSES:
        break;

    case TYPE_SEQUENCE_STORAGE:
#ifndef __MINGW32__
    {
        Attributes a = { {0} };
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Number of changes observed in %s is %d\n", VFSTAB[VSYSTEMHARDCLASS], FSTAB_EDITS);

        if (FSTAB_EDITS && FSTABLIST && !DONTDO)
        {
            if (FSTABLIST)
            {
                SaveItemListAsFile(FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], a, NULL);
                DeleteItemList(FSTABLIST);
                FSTABLIST = NULL;
            }
            FSTAB_EDITS = 0;
        }

        if (!DONTDO && CF_MOUNTALL)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Mounting all filesystems\n");
            MountAll();
        }
    }
#endif /* !__MINGW32__ */
        break;

    case TYPE_SEQUENCE_PACKAGES:

        ExecuteScheduledPackages();

        CleanScheduledPackages();
        break;

    default:

        /* Deinitialization is not required */
        ;

    }
}

/**************************************************************/

static void ClassBanner(TypeSequence type)
{
    const Item *ip;

    if (type != TYPE_SEQUENCE_INTERFACES)  /* Just parsed all local classes */
    {
        return;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "     +  Private classes augmented:\n");

    AlphaListIterator it = AlphaListIteratorInit(&VADDCLASSES);

    for (ip = AlphaListIteratorNext(&it); ip != NULL; ip = AlphaListIteratorNext(&it))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "     +       %s\n", ip->name);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "     -  Private classes diminished:\n");

    for (ip = VNEGHEAP; ip != NULL; ip = ip->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "     -       %s\n", ip->name);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");

    CfDebug("     ?  Public class context:\n");

    it = AlphaListIteratorInit(&VHEAP);
    for (ip = AlphaListIteratorNext(&it); ip != NULL; ip = AlphaListIteratorNext(&it))
    {
        CfDebug("     ?       %s\n", ip->name);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
}

/**************************************************************/
/* Thread context                                             */
/**************************************************************/

#ifdef __MINGW32__

static void ParallelFindAndVerifyFilesPromises(Promise *pp, const ReportContext *report_context)
{
    int background = GetBooleanConstraint("background", pp);

    if (background)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Background processing of files promises is not supported on Windows");
    }

    FindAndVerifyFilesPromises(pp, report_context);
}

#else /* !__MINGW32__ */

static void ParallelFindAndVerifyFilesPromises(Promise *pp, const ReportContext *report_context)
{
    int background = PromiseGetConstraintAsBoolean("background", pp);
    pid_t child = 1;

    if (background && (CFA_BACKGROUND < CFA_BACKGROUND_LIMIT))
    {
        CFA_BACKGROUND++;
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Spawning new process...\n");
        child = fork();

        if (child == 0)
        {
            ALARM_PID = -1;
            AM_BACKGROUND_PROCESS = true;
        }
        else
        {
            AM_BACKGROUND_PROCESS = false;
        }
    }
    else if (CFA_BACKGROUND >= CFA_BACKGROUND_LIMIT)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "",
              " !> Promised parallel execution promised but exceeded the max number of promised background tasks, so serializing");
        background = 0;
    }

    if (child == 0 || !background)
    {
        FindAndVerifyFilesPromises(pp, report_context);
    }
}

#endif /* !__MINGW32__ */

/**************************************************************/

static bool VerifyBootstrap(void)
{
    struct stat sb;
    char filePath[CF_MAXVARSIZE];

    if (NULL_OR_EMPTY(POLICY_SERVER))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Bootstrapping failed, no policy server is specified");
        return false;
    }

    // we should at least have gotten promises.cf from the policy hub
    snprintf(filePath, sizeof(filePath), "%s/inputs/promises.cf", CFWORKDIR);
    MapName(filePath);

    if (cfstat(filePath, &sb) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Bootstrapping failed, no input file at %s after bootstrap", filePath);
        return false;
    }

    // embedded failsafe.cf (bootstrap.c) contains a promise to start cf-execd (executed while running this cf-agent)
    DeleteItemList(PROCESSTABLE);
    PROCESSTABLE = NULL;
    LoadProcessTable(&PROCESSTABLE);

    if (!IsProcessNameRunning(".*cf-execd.*"))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Bootstrapping failed, cf-execd is not running");
        return false;
    }

    CfOut(OUTPUT_LEVEL_CMDOUT, "", "-> Bootstrap to %s completed successfully", POLICY_SERVER);

    return true;
}

/**************************************************************/
/* Compliance comp                                            */
/**************************************************************/

static int NoteBundleCompliance(const Bundle *bundle, int save_pr_kept, int save_pr_repaired, int save_pr_notkept)
{
    double delta_pr_kept, delta_pr_repaired, delta_pr_notkept;
    double bundle_compliance = 0.0;
        
    delta_pr_kept = (double) (PR_KEPT - save_pr_kept);
    delta_pr_notkept = (double) (PR_NOTKEPT - save_pr_notkept);
    delta_pr_repaired = (double) (PR_REPAIRED - save_pr_repaired);

    if (delta_pr_kept + delta_pr_notkept + delta_pr_repaired <= 0)
       {
       CfOut(OUTPUT_LEVEL_VERBOSE, "", " ==> Zero promises executed for bundle \"%s\"", bundle->name);
       return CF_NOP;
       }

    CfOut(OUTPUT_LEVEL_VERBOSE,""," ==> == Bundle Accounting Summary for \"%s\" ==", bundle->name);
    CfOut(OUTPUT_LEVEL_VERBOSE,""," ==> Promises kept in \"%s\" = %.0lf", bundle->name, delta_pr_kept);
    CfOut(OUTPUT_LEVEL_VERBOSE,""," ==> Promises not kept in \"%s\" = %.0lf", bundle->name, delta_pr_notkept);
    CfOut(OUTPUT_LEVEL_VERBOSE,""," ==> Promises repaired in \"%s\" = %.0lf", bundle->name, delta_pr_repaired);
    
    bundle_compliance = (delta_pr_kept + delta_pr_repaired) / (delta_pr_kept + delta_pr_notkept + delta_pr_repaired);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " ==> Aggregate compliance (promises kept/repaired) for bundle \"%s\" = %.1lf%%",
          bundle->name, bundle_compliance * 100.0);
    LastSawBundle(bundle, bundle_compliance);

    // return the worst case for the bundle status
    
    if (delta_pr_notkept > 0)
    {
        return CF_FAIL;
    }

    if (delta_pr_repaired > 0)
    {
        return CF_CHG;
    }

    return CF_NOP;
}

#ifdef HAVE_AVAHI_CLIENT_CLIENT_H
#ifdef HAVE_AVAHI_COMMON_ADDRESS_H
static int AutomaticBootstrap()
{
    List *foundhubs = NULL;
    int hubcount = ListHubs(&foundhubs);
    
    switch(hubcount)
    {
    case -1:
        CfOut(OUTPUT_LEVEL_ERROR, "", "Error while trying to find a Policy Server");
        ListDestroy(&foundhubs);
        return -1;
    case 0:
        CfOut(OUTPUT_LEVEL_REPORTING, "", "No hubs were found. Exiting.");
        ListDestroy(&foundhubs);
        return -1;
    case 1:
        CfOut(OUTPUT_LEVEL_REPORTING, "", "Found hub installed on:"
                                                      "Hostname: %s"
                                                      "IP Address: %s",
                                                      ((HostProperties*)foundhubs)->Hostname,
                                                      ((HostProperties*)foundhubs)->IPAddress);
        strncpy(POLICY_SERVER, ((HostProperties*)foundhubs)->IPAddress, CF_BUFSIZE);
        dlclose(avahi_handle);
        break;
    default:
        CfOut(OUTPUT_LEVEL_REPORTING, "", "Found more than one hub registered in the network.\n"
                                                      "Please bootstrap manually using IP from the list below:");
        PrintList(foundhubs);
        dlclose(avahi_handle);
        ListDestroy(&foundhubs);
        return -1;
    };

    ListDestroy(&foundhubs);

    return 0;
}
#endif
#endif
