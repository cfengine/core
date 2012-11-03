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
#include "constraints.h"
#include "verify_environments.h"
#include "verify_processes.h"
#include "addr_lib.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_repository.h"
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

#ifdef HAVE_NOVA
#include "nova-reporting.h"
#else
#include "reporting.h"
#endif

extern int PR_KEPT;
extern int PR_REPAIRED;
extern int PR_NOTKEPT;

/*******************************************************************/
/* Agent specific variables                                        */
/*******************************************************************/

static void ThisAgentInit(void);
static GenericAgentConfig CheckOpts(int argc, char **argv);
static void CheckAgentAccess(Rlist *list);
static void KeepAgentPromise(Promise *pp, const ReportContext *report_context);
static int NewTypeContext(enum typesequence type);
static void DeleteTypeContext(Policy *policy, enum typesequence type, const ReportContext *report_context);
static void ClassBanner(enum typesequence type);
static void ParallelFindAndVerifyFilesPromises(Promise *pp, const ReportContext *report_context);
static bool VerifyBootstrap(void);
static void KeepPromiseBundles(Policy *policy, Rlist *bundlesequence, const ReportContext *report_context);
static void KeepPromises(Policy *policy, GenericAgentConfig config, const ReportContext *report_context);
static int NoteBundleCompliance(const Bundle *bundle, int save_pr_kept, int save_pr_repaired, int save_pr_notkept);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *ID = "The main Cfengine agent is the instigator of change\n"
    "in the system. In that sense it is the most important\n" "part of the Cfengine suite.\n";

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

/*******************************************************************/

int main(int argc, char *argv[])
{
    int ret = 0;

    GenericAgentConfig config = CheckOpts(argc, argv);

    ReportContext *report_context = OpenReports("agent");
    Policy *policy = GenericInitialize("agent", config, report_context);
    ThisAgentInit();
    KeepPromises(policy, config, report_context);
    CloseReports("agent", report_context);
    NoteClassUsage(VHEAP, true);
    NoteClassUsage(VHARDHEAP, true);
#ifdef HAVE_NOVA
    Nova_NoteVarUsageDB();
    Nova_TrackExecution();
#endif
    PurgeLocks();

    if (BOOTSTRAP && !VerifyBootstrap())
    {
        ret = 1;
    }

    return ret;
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

static GenericAgentConfig CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    char *sp;
    int optindex = 0;
    int c, alpha = false, v6 = false;
    GenericAgentConfig config = GenericAgentDefaultConfig(AGENT_TYPE_AGENT);

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

            SetInputFile(optarg);
            MINUSF = true;
            break;

        case 'b':
            if (optarg)
            {
                config.bundlesequence = SplitStringAsRList(optarg, ',');
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
            AgentDiagnostic();
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
        CfOut(cf_error, "", "Unexpected argument with no preceding option: %s\n", argv[optind]);
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
    CfOut(cf_verbose, "", " -> Immunizing against parental death");
    setsid();
#endif

    signal(SIGINT, HandleSignals);
    signal(SIGTERM, HandleSignals);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignals);
    signal(SIGUSR2, HandleSignals);

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

static void KeepPromises(Policy *policy, GenericAgentConfig config, const ReportContext *report_context)
{
 double efficiency, model;

    if (THIS_AGENT_TYPE == AGENT_TYPE_AGENT)
    {
        BeginAudit();
    }

    KeepControlPromises(policy);
    KeepPromiseBundles(policy, config.bundlesequence, report_context);

// TOPICS counts the number of currently defined promises
// OCCUR counts the number of objects touched while verifying config

    efficiency = 100.0 * CF_OCCUR / (double) (CF_OCCUR + CF_TOPICS);
    model = 100.0 * (1.0 - CF_TOPICS / (double)(PR_KEPT + PR_NOTKEPT + PR_REPAIRED));
    
    NoteEfficiency(efficiency);

    CfOut(cf_verbose, "", " -> Checked %d objects with %d promises, i.e. model efficiency %.2lf%%", CF_OCCUR, CF_TOPICS, efficiency);
    CfOut(cf_verbose, "", " -> The %d declared promise patterns actually expanded into %d individual promises, i.e. declaration efficiency %.2lf%%", (int) CF_TOPICS, PR_KEPT + PR_NOTKEPT + PR_REPAIRED, model);

}

/*******************************************************************/
/* Level 2                                                         */
/*******************************************************************/

void KeepControlPromises(Policy *policy)
{
    Rval retval;
    Rlist *rp;

    for (Constraint *cp = ControlBodyConstraints(policy, AGENT_TYPE_AGENT); cp != NULL; cp = cp->next)
    {
        if (IsExcluded(cp->classes, NULL))
        {
            continue;
        }

        if (GetVariable("control_common", cp->lval, &retval) != cf_notype)
        {
            /* Already handled in generic_agent */
            continue;
        }

        if (GetVariable("control_agent", cp->lval, &retval) == cf_notype)
        {
            CfOut(cf_error, "", "Unknown lval %s in agent control body", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_maxconnections].lval) == 0)
        {
            CFA_MAXTHREADS = (int) Str2Int(retval.item);
            CfOut(cf_verbose, "", "SET maxconnections = %d\n", CFA_MAXTHREADS);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_checksum_alert_time].lval) == 0)
        {
            CF_PERSISTENCE = (int) Str2Int(retval.item);
            CfOut(cf_verbose, "", "SET checksum_alert_time = %d\n", CF_PERSISTENCE);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_agentfacility].lval) == 0)
        {
            SetFacility(retval.item);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_agentaccess].lval) == 0)
        {
            ACCESSLIST = (Rlist *) retval.item;
            CheckAgentAccess(ACCESSLIST);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_refresh_processes].lval) == 0)
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

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_abortclasses].lval) == 0)
        {
            Rlist *rp;

            CfOut(cf_verbose, "", "SET Abort classes from ...\n");

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                char name[CF_MAXVARSIZE] = "";

                strncpy(name, rp->item, CF_MAXVARSIZE - 1);
                CanonifyNameInPlace(name);

                AddAbortClass(name, cp->classes);
            }

            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_abortbundleclasses].lval) == 0)
        {
            Rlist *rp;

            CfOut(cf_verbose, "", "SET Abort bundle classes from ...\n");

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                char name[CF_MAXVARSIZE] = "";

                strncpy(name, rp->item, CF_MAXVARSIZE - 1);
                CanonifyNameInPlace(name);

                if (!IsItemIn(ABORTBUNDLEHEAP, name))
                {
                    AppendItem(&ABORTBUNDLEHEAP, name, cp->classes);
                }
            }

            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_addclasses].lval) == 0)
        {
            Rlist *rp;

            CfOut(cf_verbose, "", "-> Add classes ...\n");

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                CfOut(cf_verbose, "", " -> ... %s\n", ScalarValue(rp));
                NewClass(rp->item, NULL);
            }

            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_auditing].lval) == 0)
        {
            AUDIT = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET auditing = %d\n", AUDIT);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_alwaysvalidate].lval) == 0)
        {
            ALWAYS_VALIDATE = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET alwaysvalidate = %d\n", ALWAYS_VALIDATE);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_allclassesreport].lval) == 0)
        {
            ALLCLASSESREPORT = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET allclassesreport = %d\n", ALLCLASSESREPORT);
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_secureinput].lval) == 0)
        {
            CFPARANOID = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET secure input = %d\n", CFPARANOID);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_binarypaddingchar].lval) == 0)
        {
            PADCHAR = *(char *) retval.item;
            CfOut(cf_verbose, "", "SET binarypaddingchar = %c\n", PADCHAR);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_bindtointerface].lval) == 0)
        {
            strncpy(BINDINTERFACE, retval.item, CF_BUFSIZE - 1);
            CfOut(cf_verbose, "", "SET bindtointerface = %s\n", BINDINTERFACE);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_hashupdates].lval) == 0)
        {
            bool enabled = GetBoolean(retval.item);

            SetChecksumUpdates(enabled);
            CfOut(cf_verbose, "", "SET ChecksumUpdates %d\n", enabled);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_exclamation].lval) == 0)
        {
            EXCLAIM = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET exclamation %d\n", EXCLAIM);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_childlibpath].lval) == 0)
        {
            char output[CF_BUFSIZE];

            snprintf(output, CF_BUFSIZE, "LD_LIBRARY_PATH=%s", (char *) retval.item);
            if (putenv(xstrdup(output)) == 0)
            {
                CfOut(cf_verbose, "", "Setting %s\n", output);
            }
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_defaultcopytype].lval) == 0)
        {
            DEFAULT_COPYTYPE = (char *) retval.item;
            CfOut(cf_verbose, "", "SET defaultcopytype = %s\n", DEFAULT_COPYTYPE);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_fsinglecopy].lval) == 0)
        {
            SINGLE_COPY_LIST = (Rlist *) retval.item;
            CfOut(cf_verbose, "", "SET file single copy list\n");
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_fautodefine].lval) == 0)
        {
            AUTO_DEFINE_LIST = (Rlist *) retval.item;
            CfOut(cf_verbose, "", "SET file auto define list\n");
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_dryrun].lval) == 0)
        {
            DONTDO = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET dryrun = %c\n", DONTDO);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_inform].lval) == 0)
        {
            INFORM = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET inform = %c\n", INFORM);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_verbose].lval) == 0)
        {
            VERBOSE = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET inform = %c\n", VERBOSE);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_repository].lval) == 0)
        {
            SetRepositoryLocation(retval.item);
            CfOut(cf_verbose, "", "SET repository = %s\n", ScalarRvalValue(retval));
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_skipidentify].lval) == 0)
        {
            bool enabled = GetBoolean(retval.item);

            SetSkipIdentify(enabled);
            CfOut(cf_verbose, "", "SET skipidentify = %d\n", (int) enabled);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_suspiciousnames].lval) == 0)
        {

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                AddFilenameToListOfSuspicious(ScalarValue(rp));
                CfOut(cf_verbose, "", "-> Concidering %s as suspicious file", ScalarValue(rp));
            }

            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_repchar].lval) == 0)
        {
            char c = *(char *) retval.item;

            SetRepositoryChar(c);
            CfOut(cf_verbose, "", "SET repchar = %c\n", c);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_mountfilesystems].lval) == 0)
        {
            CF_MOUNTALL = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET mountfilesystems = %d\n", CF_MOUNTALL);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_editfilesize].lval) == 0)
        {
            EDITFILESIZE = Str2Int(retval.item);
            CfOut(cf_verbose, "", "SET EDITFILESIZE = %d\n", EDITFILESIZE);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_ifelapsed].lval) == 0)
        {
            VIFELAPSED = Str2Int(retval.item);
            CfOut(cf_verbose, "", "SET ifelapsed = %d\n", VIFELAPSED);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_expireafter].lval) == 0)
        {
            VEXPIREAFTER = Str2Int(retval.item);
            CfOut(cf_verbose, "", "SET ifelapsed = %d\n", VEXPIREAFTER);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_timeout].lval) == 0)
        {
            CONNTIMEOUT = Str2Int(retval.item);
            CfOut(cf_verbose, "", "SET timeout = %jd\n", (intmax_t) CONNTIMEOUT);
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_max_children].lval) == 0)
        {
            CFA_BACKGROUND_LIMIT = Str2Int(retval.item);
            CfOut(cf_verbose, "", "SET MAX_CHILDREN = %d\n", CFA_BACKGROUND_LIMIT);
            if (CFA_BACKGROUND_LIMIT > 10)
            {
                CfOut(cf_error, "", "Silly value for max_children in agent control promise (%d > 10)",
                      CFA_BACKGROUND_LIMIT);
                CFA_BACKGROUND_LIMIT = 1;
            }
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_syslog].lval) == 0)
        {
            CfOut(cf_verbose, "", "SET syslog = %d\n", GetBoolean(retval.item));
            continue;
        }

        if (strcmp(cp->lval, CFA_CONTROLBODY[cfa_environment].lval) == 0)
        {
            Rlist *rp;

            CfOut(cf_verbose, "", "SET environment variables from ...\n");

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                if (putenv(rp->item) != 0)
                {
                    CfOut(cf_error, "putenv", "Failed to set environment variable %s", ScalarValue(rp));
                }
            }

            continue;
        }
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[cfg_lastseenexpireafter].lval, &retval) != cf_notype)
    {
        LASTSEENEXPIREAFTER = Str2Int(retval.item) * 60;
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[cfg_fips_mode].lval, &retval) != cf_notype)
    {
        FIPS_MODE = GetBoolean(retval.item);
        CfOut(cf_verbose, "", "SET FIPS_MODE = %d\n", FIPS_MODE);
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[cfg_syslog_port].lval, &retval) != cf_notype)
    {
        SetSyslogPort(Str2Int(retval.item));
        CfOut(cf_verbose, "", "SET syslog_port to %s", ScalarRvalValue(retval));
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[cfg_syslog_host].lval, &retval) != cf_notype)
    {
        SetSyslogHost(Hostname2IPString(retval.item));
        CfOut(cf_verbose, "", "SET syslog_host to %s", Hostname2IPString(retval.item));
    }

#ifdef HAVE_NOVA
    Nova_Initialize();
#endif
}

/*********************************************************************/

static void KeepPromiseBundles(Policy *policy, Rlist *bundlesequence, const ReportContext *report_context)
{
    Bundle *bp;
    Rlist *rp, *params;
    FnCall *fp;
    char *name;
    Rval retval;
    int ok = true;

    if (bundlesequence)
    {
        CfOut(cf_inform, "", " >> Using command line specified bundlesequence");
        retval = (Rval) {bundlesequence, CF_LIST};
    }
    else if (GetVariable("control_common", "bundlesequence", &retval) == cf_notype)
    {
        CfOut(cf_error, "", " !! !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        CfOut(cf_error, "", " !! No bundlesequence in the common control body");
        CfOut(cf_error, "", " !! !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        exit(1);
    }

    if (retval.rtype != CF_LIST)
    {
        FatalError("Promised bundlesequence was not a list");
    }

    for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case CF_SCALAR:
            name = (char *) rp->item;
            params = NULL;

            if (strcmp(name, CF_NULL_VALUE) == 0)
            {
                continue;
            }

            break;
        case CF_FNCALL:
            fp = (FnCall *) rp->item;
            name = (char *) fp->name;
            params = (Rlist *) fp->args;
            break;

        default:
            name = NULL;
            params = NULL;
            CfOut(cf_error, "", "Illegal item found in bundlesequence: ");
            ShowRval(stdout, (Rval) {rp->item, rp->type});
            printf(" = %c\n", rp->type);
            ok = false;
            break;
        }

        if (!IGNORE_MISSING_BUNDLES)
        {
            if (!(GetBundle(policy, name, "agent") || (GetBundle(policy, name, "common"))))
            {
                CfOut(cf_error, "", "Bundle \"%s\" listed in the bundlesequence was not found\n", name);
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
        ShowRval(stdout, retval);
        printf("\n");
    }

/* If all is okay, go ahead and evaluate */

    for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case CF_FNCALL:
            fp = (FnCall *) rp->item;
            name = (char *) fp->name;
            params = (Rlist *) fp->args;
            break;
        default:
            name = (char *) rp->item;
            params = NULL;
            break;
        }

        if ((bp = GetBundle(policy, name, "agent")) || (bp = GetBundle(policy, name, "common")))
        {
            char namespace[CF_BUFSIZE];
            snprintf(namespace,CF_BUFSIZE,"%s_meta", name);
            NewScope(namespace);

            SetBundleOutputs(bp->name);
            AugmentScope(bp->name, bp->namespace, bp->args, params);
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
    Promise *pp;
    enum typesequence type;
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

            if ((sp = GetSubTypeForBundle(AGENT_TYPESEQUENCE[type], bp)) == NULL)
            {
                continue;
            }

            BannerSubType(bp->name, sp->name, pass);
            SetScope(bp->name);

            if (!NewTypeContext(type))
            {
                continue;
            }

            for (pp = sp->promiselist; pp != NULL; pp = pp->next)
            {
                SaveClassEnvironment();

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

static void CheckAgentAccess(Rlist *list)
#ifdef MINGW
{
}
#else                           /* NOT MINGW */
{
    Rlist *rp, *rp2;
    struct stat sb;
    uid_t uid;
    int access = false;

    uid = getuid();

    for (rp = list; rp != NULL; rp = rp->next)
    {
        if (Str2Uid(rp->item, NULL, NULL) == uid)
        {
            return;
        }
    }

    if (VINPUTLIST != NULL)
    {
        for (rp = VINPUTLIST; rp != NULL; rp = rp->next)
        {
            cfstat(rp->item, &sb);

            if (ACCESSLIST)
            {
                for (rp2 = ACCESSLIST; rp2 != NULL; rp2 = rp2->next)
                {
                    if (Str2Uid(rp2->item, NULL, NULL) == sb.st_uid)
                    {
                        access = true;
                        break;
                    }
                }

                if (!access)
                {
                    CfOut(cf_error, "", "File %s is not owned by an authorized user (security exception)",
                          ScalarValue(rp));
                    exit(1);
                }
            }
            else if (CFPARANOID && IsPrivileged())
            {
                if (sb.st_uid != getuid())
                {
                    CfOut(cf_error, "", "File %s is not owned by uid %ju (security exception)", ScalarValue(rp),
                          (uintmax_t)getuid());
                    exit(1);
                }
            }
        }
    }

    FatalError("You are denied access to run this policy");
}
#endif /* NOT MINGW */

/*********************************************************************/

static void KeepAgentPromise(Promise *pp, const ReportContext *report_context)
{
    char *sp = NULL;
    struct timespec start = BeginMeasure();

    if (!IsDefinedClass(pp->classes, pp->namespace))
    {
        CfOut(cf_verbose, "", "\n");
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(cf_verbose, "", "Skipping whole next promise (%s), as context %s is not relevant\n", pp->promiser,
              pp->classes);
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }

    if (pp->done)
    {
        return;
    }

    if (VarClassExcluded(pp, &sp))
    {
        CfOut(cf_verbose, "", "\n");
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(cf_verbose, "", "Skipping whole next promise (%s), as var-context %s is not relevant\n", pp->promiser,
              sp);
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }


    if (MissingDependencies(pp))
    {
        return;
    }
    
// Record promises examined for efficiency calc

    if (strcmp("meta", pp->agentsubtype) == 0)
    {
        char namespace[CF_BUFSIZE];
        snprintf(namespace,CF_BUFSIZE,"%s_meta",pp->bundle);
        NewScope(namespace);
        ConvergeVarHashPromise(namespace, pp, true);
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
        if (GetBooleanConstraint("background", pp))
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

static int NewTypeContext(enum typesequence type)
{
// get maxconnections

    switch (type)
    {
    case kp_environments:
        NewEnvironmentsContext();
        break;

    case kp_files:

        ConnectionsInit();
        break;

    case kp_processes:

        if (!LoadProcessTable(&PROCESSTABLE))
        {
            CfOut(cf_error, "", "Unable to read the process table - cannot keep process promises\n");
            return false;
        }
        break;

    case kp_storage:

#ifndef MINGW                   // TODO: Run if implemented on Windows
        if (MOUNTEDFSLIST != NULL)
        {
            DeleteMountInfo(MOUNTEDFSLIST);
            MOUNTEDFSLIST = NULL;
        }
#endif /* NOT MINGW */
        break;

    default:

        /* Initialization is not required */
        ;
    }

    return true;
}

/*********************************************************************/

static void DeleteTypeContext(Policy *policy, enum typesequence type, const ReportContext *report_context)
{
    switch (type)
    {
    case kp_classes:
        HashVariables(policy, THIS_BUNDLE, report_context);
        break;

    case kp_environments:
        DeleteEnvironmentsContext();
        break;

    case kp_files:

        ConnectionsCleanup();
        break;

    case kp_processes:
        break;

    case kp_storage:
#ifndef MINGW
    {
        Attributes a = { {0} };
        CfOut(cf_verbose, "", " -> Number of changes observed in %s is %d\n", VFSTAB[VSYSTEMHARDCLASS], FSTAB_EDITS);

        if (FSTAB_EDITS && FSTABLIST && !DONTDO)
        {
            if (FSTABLIST)
            {
                SaveItemListAsFile(FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], a, NULL, report_context);
                DeleteItemList(FSTABLIST);
                FSTABLIST = NULL;
            }
            FSTAB_EDITS = 0;
        }

        if (!DONTDO && CF_MOUNTALL)
        {
            CfOut(cf_verbose, "", " -> Mounting all filesystems\n");
            MountAll();
        }
    }
#endif /* NOT MINGW */
        break;

    case kp_packages:

        ExecuteScheduledPackages();

        CleanScheduledPackages();
        break;

    default:

        /* Deinitialization is not required */
        ;

    }
}

/**************************************************************/

static void ClassBanner(enum typesequence type)
{
    const Item *ip;

    if (type != kp_interfaces)  /* Just parsed all local classes */
    {
        return;
    }

    CfOut(cf_verbose, "", "\n");
    CfOut(cf_verbose, "", "     +  Private classes augmented:\n");

    AlphaListIterator it = AlphaListIteratorInit(&VADDCLASSES);

    for (ip = AlphaListIteratorNext(&it); ip != NULL; ip = AlphaListIteratorNext(&it))
    {
        CfOut(cf_verbose, "", "     +       %s\n", ip->name);
    }

    CfOut(cf_verbose, "", "\n");

    CfOut(cf_verbose, "", "     -  Private classes diminished:\n");

    for (ip = VNEGHEAP; ip != NULL; ip = ip->next)
    {
        CfOut(cf_verbose, "", "     -       %s\n", ip->name);
    }

    CfOut(cf_verbose, "", "\n");

    CfDebug("     ?  Public class context:\n");

    it = AlphaListIteratorInit(&VHEAP);
    for (ip = AlphaListIteratorNext(&it); ip != NULL; ip = AlphaListIteratorNext(&it))
    {
        CfDebug("     ?       %s\n", ip->name);
    }

    CfOut(cf_verbose, "", "\n");
}

/**************************************************************/
/* Thread context                                             */
/**************************************************************/

static void ParallelFindAndVerifyFilesPromises(Promise *pp, const ReportContext *report_context)
{
    int background = GetBooleanConstraint("background", pp);

#ifdef MINGW

    if (background)
    {
        CfOut(cf_verbose, "", "Background processing of files promises is not supported on Windows");
    }

    FindAndVerifyFilesPromises(pp, report_context);

#else /* NOT MINGW */

    pid_t child = 1;

    if (background && (CFA_BACKGROUND < CFA_BACKGROUND_LIMIT))
    {
        CFA_BACKGROUND++;
        CfOut(cf_verbose, "", "Spawning new process...\n");
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
        CfOut(cf_verbose, "",
              " !> Promised parallel execution promised but exceeded the max number of promised background tasks, so serializing");
        background = 0;
    }

    if (child == 0 || !background)
    {
        FindAndVerifyFilesPromises(pp, report_context);
    }

#endif /* NOT MINGW */
}

/**************************************************************/

static bool VerifyBootstrap(void)
{
    struct stat sb;
    char filePath[CF_MAXVARSIZE];

    if (NULL_OR_EMPTY(POLICY_SERVER))
    {
        CfOut(cf_error, "", "!! Bootstrapping failed, no policy server is specified");
        return false;
    }

    // we should at least have gotten promises.cf from the policy hub
    snprintf(filePath, sizeof(filePath), "%s/inputs/promises.cf", CFWORKDIR);
    MapName(filePath);

    if (cfstat(filePath, &sb) == -1)
    {
        CfOut(cf_error, "", "!! Bootstrapping failed, no input file at %s after bootstrap", filePath);
        return false;
    }

    // embedded failsafe.cf (bootstrap.c) contains a promise to start cf-execd (executed while running this cf-agent)
    DeleteItemList(PROCESSTABLE);
    PROCESSTABLE = NULL;
    LoadProcessTable(&PROCESSTABLE);

    if (!IsProcessNameRunning(".*cf-execd.*"))
    {
        CfOut(cf_error, "", "!! Bootstrapping failed, cf-execd is not running");
        return false;
    }

    CfOut(cf_cmdout, "", "-> Bootstrap to %s completed successfully", POLICY_SERVER);

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
       CfOut(cf_verbose, "", " ==> Zero promises executed for bundle \"%s\"", bundle->name);
       return CF_NOP;
       }

    CfOut(cf_verbose,""," ==> == Bundle Accounting Summary for \"%s\" ==", bundle->name);
    CfOut(cf_verbose,""," ==> Promises kept in \"%s\" = %.0lf", bundle->name, delta_pr_kept);
    CfOut(cf_verbose,""," ==> Promises not kept in \"%s\" = %.0lf", bundle->name, delta_pr_notkept);
    CfOut(cf_verbose,""," ==> Promises repaired in \"%s\" = %.0lf", bundle->name, delta_pr_repaired);
    
    bundle_compliance = (delta_pr_kept + delta_pr_repaired) / (delta_pr_kept + delta_pr_notkept + delta_pr_repaired);

    CfOut(cf_verbose, "", " ==> Aggregate compliance (promises kept/repaired) for bundle \"%s\" = %.1lf%%",
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
