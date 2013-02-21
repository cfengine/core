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
#include "cf-execd-runner.h"

#include "bootstrap.h"
#include "sysinfo.h"
#include "env_context.h"
#include "promises.h"
#include "vars.h"
#include "item_lib.h"
#include "conversion.h"
#include "reporting.h"
#include "scope.h"
#include "hashes.h"
#include "unix.h"
#include "cfstream.h"
#include "string_lib.h"
#include "verify_processes.h"
#include "signals.h"
#include "transaction.h"
#include "logging.h"
#include "exec_tools.h"
#include "rlist.h"

#define CF_EXEC_IFELAPSED 0
#define CF_EXEC_EXPIREAFTER 1

static int NO_FORK;
static int ONCE;
static int WINSERVICE = true;

static Item *SCHEDULE;
static int SPLAYTIME = 0;

static pthread_attr_t threads_attrs;

/*******************************************************************/

static GenericAgentConfig *CheckOpts(int argc, char **argv);
static void ThisAgentInit(void);
static bool ScheduleRun(Policy **policy, GenericAgentConfig *config, ExecConfig *exec_config, const ReportContext *report_context);
static void Apoptosis(void);

static bool LocalExecInThread(const ExecConfig *config);

void StartServer(Policy *policy, GenericAgentConfig *config, ExecConfig *exec_config, const ReportContext *report_context);
void KeepPromises(Policy *policy, ExecConfig *config);

static ExecConfig *CopyExecConfig(const ExecConfig *config);
static void DestroyExecConfig(ExecConfig *config);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *ID = "The executor daemon is a scheduler and wrapper for\n"
    "execution of cf-agent. It collects the output of the\n"
    "agent and can email it to a specified address. It can\n"
    "splay the start time of executions across the network\n" "and work as a class-based clock for scheduling.";

static const struct option OPTIONS[] =
{
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"dry-run", no_argument, 0, 'n'},
    {"version", no_argument, 0, 'V'},
    {"file", required_argument, 0, 'f'},
    {"define", required_argument, 0, 'D'},
    {"negate", required_argument, 0, 'N'},
    {"no-lock", no_argument, 0, 'K'},
    {"inform", no_argument, 0, 'I'},
    {"diagnostic", no_argument, 0, 'x'},
    {"no-fork", no_argument, 0, 'F'},
    {"once", no_argument, 0, 'O'},
    {"no-winsrv", no_argument, 0, 'W'},
    {"ld-library-path", required_argument, 0, 'L'},
    {NULL, 0, 0, '\0'}
};

static const char *HINTS[sizeof(OPTIONS)/sizeof(OPTIONS[0])] =
{
    "Print the help message",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "All talk and no action mode - make no changes, only inform of promises not kept",
    "Output the version of the software",
    "Specify an alternative input file than the default",
    "Define a list of comma separated classes to be defined at the start of execution",
    "Define a list of comma separated classes to be undefined at the start of execution",
    "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Activate internal diagnostics (developers only)",
    "Run as a foreground processes (do not fork)",
    "Run once and then exit (implies no-fork)",
    "Do not run as a service on windows - use this when running from a command shell (Cfengine Nova only)",
    "Set the internal value of LD_LIBRARY_PATH for child processes",
    NULL
};

/*****************************************************************************/

int main(int argc, char *argv[])
{
    GenericAgentConfig *config = CheckOpts(argc, argv);

    ReportContext *report_context = OpenReports(config->agent_type);
    GenericAgentDiscoverContext(config, report_context);

    Policy *policy = NULL;
    if (GenericAgentCheckPolicy(config, false))
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

    ExecConfig exec_config = {
        .scheduled_run = !ONCE,
        .exec_command = SafeStringDuplicate(""),
        .mail_server = SafeStringDuplicate(""),
        .mail_from_address = SafeStringDuplicate(""),
        .mail_to_address = SafeStringDuplicate(""),
        .mail_max_lines = 30,
        .fq_name = VFQNAME,
        .ip_address = VIPADDRESS,
        .agent_expireafter = 10080,
    };

    KeepPromises(policy, &exec_config);

#ifdef __MINGW32__
    if (WINSERVICE)
    {
        NovaWin_StartExecService();
    }
    else
#endif /* __MINGW32__ */
    {
        StartServer(policy, config, &exec_config, report_context);
    }

    ReportContextDestroy(report_context);
    GenericAgentConfigDestroy(config);

    return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    int c;
    char ld_library_path[CF_BUFSIZE];
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_EXECUTOR);

    while ((c = getopt_long(argc, argv, "dvnKIf:D:N:VxL:hFOV1gMW", OPTIONS, &optindex)) != EOF)
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

        case 'd':
            HardClass("opt_debug");
            DEBUG = true;
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
            NO_FORK = true;
            break;

        case 'n':
            DONTDO = true;
            IGNORELOCK = true;
            HardClass("opt_dry_run");
            break;

        case 'L':
            snprintf(ld_library_path, CF_BUFSIZE - 1, "LD_LIBRARY_PATH=%s", optarg);
            if (putenv(xstrdup(ld_library_path)) != 0)
            {
            }
            break;

        case 'W':
            WINSERVICE = false;
            break;

        case 'F':
            NO_FORK = true;
            break;

        case 'O':
            ONCE = true;
            NO_FORK = true;
            break;

        case 'V':
            PrintVersionBanner("cf-execd");
            exit(0);

        case 'h':
            Syntax("cf-execd - cfengine's execution agent", OPTIONS, HINTS, ID);
            exit(0);

        case 'M':
            ManPage("cf-execd - cfengine's execution agent", OPTIONS, HINTS, ID);
            exit(0);

        case 'x':
            CfOut(OUTPUT_LEVEL_ERROR, "", "Self-diagnostic functionality is retired.");
            exit(0);

        default:
            Syntax("cf-execd - cfengine's execution agent", OPTIONS, HINTS, ID);
            exit(1);

        }
    }

    if (argv[optind] != NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unexpected argument with no preceding option: %s\n", argv[optind]);
    }

    return config;
}

/*****************************************************************************/

static void LoadDefaultSchedule(void)
{
    CfDebug("Loading default schedule...\n");
    DeleteItemList(SCHEDULE);
    SCHEDULE = NULL;
    AppendItem(&SCHEDULE, "Min00", NULL);
    AppendItem(&SCHEDULE, "Min05", NULL);
    AppendItem(&SCHEDULE, "Min10", NULL);
    AppendItem(&SCHEDULE, "Min15", NULL);
    AppendItem(&SCHEDULE, "Min20", NULL);
    AppendItem(&SCHEDULE, "Min25", NULL);
    AppendItem(&SCHEDULE, "Min30", NULL);
    AppendItem(&SCHEDULE, "Min35", NULL);
    AppendItem(&SCHEDULE, "Min40", NULL);
    AppendItem(&SCHEDULE, "Min45", NULL);
    AppendItem(&SCHEDULE, "Min50", NULL);
    AppendItem(&SCHEDULE, "Min55", NULL);
}

static void ThisAgentInit(void)
{
    umask(077);

    if (SCHEDULE == NULL)
    {
        LoadDefaultSchedule();
    }
}

/*****************************************************************************/

static double GetSplay(void)
{
    char splay[CF_BUFSIZE];

    snprintf(splay, CF_BUFSIZE, "%s+%s+%ju", VFQNAME, VIPADDRESS, (uintmax_t)getuid());

    return ((double) GetHash(splay, CF_HASHTABLESIZE)) / CF_HASHTABLESIZE;
}

/*****************************************************************************/
/* Might be called back from NovaWin_StartExecService */

void KeepPromises(Policy *policy, ExecConfig *config)
{
    bool schedule_is_specified = false;

    Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_EXECUTOR);
    if (constraints)
    {
        for (size_t i = 0; i < SeqLength(constraints); i++)
        {
            Constraint *cp = SeqAt(constraints, i);

            if (IsExcluded(cp->classes, NULL))
            {
                continue;
            }

            Rval retval;
            if (GetVariable("control_executor", cp->lval, &retval) == DATA_TYPE_NONE)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Unknown lval %s in exec control body", cp->lval);
                continue;
            }

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILFROM].lval) == 0)
            {
                free(config->mail_from_address);
                config->mail_from_address = SafeStringDuplicate(retval.item);
                CfDebug("mailfrom = %s\n", config->mail_from_address);
            }

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILTO].lval) == 0)
            {
                free(config->mail_to_address);
                config->mail_to_address = SafeStringDuplicate(retval.item);
                CfDebug("mailto = %s\n", config->mail_to_address);
            }

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_SMTPSERVER].lval) == 0)
            {
                free(config->mail_server);
                config->mail_server = SafeStringDuplicate(retval.item);
                CfDebug("smtpserver = %s\n", config->mail_server);
            }

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_EXECCOMMAND].lval) == 0)
            {
                free(config->exec_command);
                config->exec_command = SafeStringDuplicate(retval.item);
                CfDebug("exec_command = %s\n", config->exec_command);
            }

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_AGENT_EXPIREAFTER].lval) == 0)
            {
                config->agent_expireafter = IntFromString(retval.item);
                CfDebug("agent_expireafter = %d\n", config->agent_expireafter);
            }

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_EXECUTORFACILITY].lval) == 0)
            {
                SetFacility(retval.item);
                continue;
            }

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILMAXLINES].lval) == 0)
            {
                config->mail_max_lines = IntFromString(retval.item);
                CfDebug("maxlines = %d\n", config->mail_max_lines);
            }

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_SPLAYTIME].lval) == 0)
            {
                int time = IntFromString(RvalScalarValue(retval));

                SPLAYTIME = (int) (time * SECONDS_PER_MINUTE * GetSplay());
            }

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_SCHEDULE].lval) == 0)
            {
                CfDebug("Loading user-defined schedule...\n");
                DeleteItemList(SCHEDULE);
                SCHEDULE = NULL;
                schedule_is_specified = true;

                for (const Rlist *rp = retval.item; rp; rp = rp->next)
                {
                    if (!IsItemIn(SCHEDULE, rp->item))
                    {
                        AppendItem(&SCHEDULE, rp->item, NULL);
                    }
                }
            }
        }
    }

    if (!schedule_is_specified)
    {
        LoadDefaultSchedule();
    }
}

/*****************************************************************************/

/* Might be called back from NovaWin_StartExecService */
void StartServer(Policy *policy, GenericAgentConfig *config, ExecConfig *exec_config, const ReportContext *report_context)
{
#if !defined(__MINGW32__)
    time_t now = time(NULL);
#endif
    Promise *pp = NewPromise("exec_cfengine", "the executor agent");
    Attributes dummyattr;
    CfLock thislock;

    pthread_attr_init(&threads_attrs);
    pthread_attr_setdetachstate(&threads_attrs, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&threads_attrs, (size_t)2048*1024);

    Banner("Starting executor");
    memset(&dummyattr, 0, sizeof(dummyattr));

    dummyattr.restart_class = "nonce";
    dummyattr.transaction.ifelapsed = CF_EXEC_IFELAPSED;
    dummyattr.transaction.expireafter = CF_EXEC_EXPIREAFTER;

    if (!ONCE)
    {
        thislock = AcquireLock(pp->promiser, VUQNAME, CFSTARTTIME, dummyattr, pp, false);

        if (thislock.lock == NULL)
        {
            PromiseDestroy(pp);
            return;
        }

        /* Kill previous instances of cf-execd if those are still running */
        Apoptosis();

        /* FIXME: kludge. This code re-sets "last" lock to the one we have
           acquired a few lines before. If the cf-execd is terminated, this lock
           will be removed, and subsequent restart of cf-execd won't fail.

           The culprit is Apoptosis(), which creates a promise and executes it,
           taking locks during it, so CFLOCK/CFLAST/CFLOG get reset.

           Proper fix is to keep all the taken locks in the memory, and release
           all of them during process termination.
         */
        strcpy(CFLOCK, thislock.lock);
        strcpy(CFLAST, thislock.last ? thislock.last : "");
        strcpy(CFLOG, thislock.log ? thislock.log : "");
    }

#ifdef __MINGW32__

    if (!NO_FORK)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Windows does not support starting processes in the background - starting in foreground");
    }

#else /* !__MINGW32__ */

    if ((!NO_FORK) && (fork() != 0))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "cf-execd starting %.24s\n", cf_ctime(&now));
        _exit(0);
    }

    if (!NO_FORK)
    {
        ActAsDaemon(0);
    }

#endif /* !__MINGW32__ */

    WritePID("cf-execd.pid");
    signal(SIGINT, HandleSignalsForDaemon);
    signal(SIGTERM, HandleSignalsForDaemon);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignalsForDaemon);
    signal(SIGUSR2, HandleSignalsForDaemon);

    umask(077);

    if (ONCE)
    {
        LocalExec(exec_config);
        CloseLog();
    }
    else
    {
        while (!IsPendingTermination())
        {
            if (ScheduleRun(&policy, config, exec_config, report_context))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Sleeping for splaytime %d seconds\n\n", SPLAYTIME);
                sleep(SPLAYTIME);

                if (!LocalExecInThread(exec_config))
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", "Unable to run agent in thread, falling back to blocking execution");
                    LocalExec(exec_config);
                }
            }
        }

        YieldCurrentLock(thislock);
    }
}

/*****************************************************************************/

static void *LocalExecThread(void *param)
{
#if !defined(__MINGW32__)
    sigset_t sigmask;
    sigemptyset(&sigmask);
    pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
#endif

    ExecConfig *config = (ExecConfig *)param;
    LocalExec(config);
    DestroyExecConfig(config);

    return NULL;
}

static bool LocalExecInThread(const ExecConfig *config)
{
    ExecConfig *thread_config = CopyExecConfig(config);

    pthread_t tid;

    if (pthread_create(&tid, &threads_attrs, LocalExecThread, thread_config) == 0)
    {
        return true;
    }
    else
    {
        DestroyExecConfig(thread_config);
        CfOut(OUTPUT_LEVEL_INFORM, "pthread_create", "Can't create thread!");
        return false;
    }
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static void Apoptosis()
{
    Promise pp = { 0 };
    Rlist *signals = NULL, *owners = NULL;
    char mypid[32];
    static char promiser_buf[CF_SMALLBUF];

#if defined(_WIN32)
    return;
#endif

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Programmed pruning of the scheduler cluster");

#ifdef __MINGW32__
    snprintf(promiser_buf, sizeof(promiser_buf), "cf-execd");     // using '\' causes regexp problems
#else
    snprintf(promiser_buf, sizeof(promiser_buf), "%s/bin/cf-execd", CFWORKDIR);
#endif

    pp.promiser = promiser_buf;
    pp.promisee = (Rval) {"cfengine", RVAL_TYPE_SCALAR};
    pp.classes = "any";
    pp.offset.line = 0;
    pp.audit = NULL;
    pp.conlist = SeqNew(100, ConstraintDestroy);

    pp.bundletype = "agent";
    pp.bundle = "exec_apoptosis";
    pp.ref = "Programmed death";
    pp.agentsubtype = "processes";
    pp.done = false;
    pp.cache = NULL;
    pp.inode_cache = NULL;
    pp.this_server = NULL;
    pp.donep = &(pp.done);
    pp.conn = NULL;

    GetCurrentUserName(mypid, 31);

    RlistPrepend(&signals, "term", RVAL_TYPE_SCALAR);
    RlistPrepend(&owners, mypid, RVAL_TYPE_SCALAR);

    PromiseAppendConstraint(&pp, "signals", (Rval) {signals, RVAL_TYPE_LIST }, "any", false);
    PromiseAppendConstraint(&pp, "process_select", (Rval) {xstrdup("true"), RVAL_TYPE_SCALAR}, "any", false);
    PromiseAppendConstraint(&pp, "process_owner", (Rval) {owners, RVAL_TYPE_LIST }, "any", false);
    PromiseAppendConstraint(&pp, "ifelapsed", (Rval) {xstrdup("0"), RVAL_TYPE_SCALAR}, "any", false);
    PromiseAppendConstraint(&pp, "process_count", (Rval) {xstrdup("true"), RVAL_TYPE_SCALAR}, "any", false);
    PromiseAppendConstraint(&pp, "match_range", (Rval) {xstrdup("0,2"), RVAL_TYPE_SCALAR}, "any", false);
    PromiseAppendConstraint(&pp, "process_result", (Rval) {xstrdup("process_owner.process_count"), RVAL_TYPE_SCALAR}, "any", false);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Looking for cf-execd processes owned by %s", mypid);

    if (LoadProcessTable(&PROCESSTABLE))
    {
        VerifyProcessesPromise(&pp);
    }

    DeleteItemList(PROCESSTABLE);

    SeqDestroy(pp.conlist);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Pruning complete");
}

/*****************************************************************************/

typedef enum
{
    RELOAD_ENVIRONMENT,
    RELOAD_FULL
} Reload;

static Reload CheckNewPromises(const char *input_file, const Rlist *input_files, const ReportContext *report_context)
{
    if (NewPromiseProposals(input_file, input_files))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> New promises detected...\n");

        if (CheckPromises(input_file))
        {
            return RELOAD_FULL;
        }
        else
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", " !! New promises file contains syntax errors -- ignoring");
            PROMISETIME = time(NULL);
        }
    }
    else
    {
        CfDebug(" -> No new promises found\n");
    }

    return RELOAD_ENVIRONMENT;
}

static bool ScheduleRun(Policy **policy, GenericAgentConfig *config, ExecConfig *exec_config, const ReportContext *report_context)
{
    Item *ip;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Sleeping...\n");
    sleep(CFPULSETIME);         /* 1 Minute resolution is enough */

// recheck license (in case of license updates or expiry)

    if (EnterpriseExpiry())
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Cfengine - autonomous configuration engine. This enterprise license is invalid.\n");
        exit(1);
    }

    /*
     * FIXME: this logic duplicates the one from cf-serverd.c. Unify ASAP.
     */

    if (CheckNewPromises(config->input_file, InputFiles(*policy), report_context) == RELOAD_FULL)
    {
        /* Full reload */

        CfOut(OUTPUT_LEVEL_INFORM, "", "Re-reading promise file %s..\n", config->input_file);

        DeleteAlphaList(&VHEAP);
        InitAlphaList(&VHEAP);
        DeleteAlphaList(&VHARDHEAP);
        InitAlphaList(&VHARDHEAP);
        DeleteAlphaList(&VADDCLASSES);
        InitAlphaList(&VADDCLASSES);

        DeleteItemList(IPADDRESSES);
        IPADDRESSES = NULL;

        DeleteItemList(VNEGHEAP);

        DeleteAllScope();

        strcpy(VDOMAIN, "undefinded.domain");
        POLICY_SERVER[0] = '\0';

        VNEGHEAP = NULL;

        PolicyDestroy(*policy);
        *policy = NULL;

        ERRORCOUNT = 0;

        NewScope("sys");

        SetPolicyServer(POLICY_SERVER);
        NewScalar("sys", "policy_hub", POLICY_SERVER, DATA_TYPE_STRING);

        NewScope("const");
        NewScope("this");
        NewScope("mon");
        NewScope("control_server");
        NewScope("control_common");
        NewScope("remote_access");

        GetNameInfo3();
        GetInterfacesInfo(AGENT_TYPE_EXECUTOR);
        Get3Environment();
        BuiltinClasses();
        OSClasses();

        HardClass(CF_AGENTTYPES[THIS_AGENT_TYPE]);

        SetReferenceTime(true);

        GenericAgentConfigSetBundleSequence(config, NULL);

        *policy = GenericAgentLoadPolicy(AGENT_TYPE_EXECUTOR, config, report_context);
        KeepPromises(*policy, exec_config);
    }
    else
    {
        /* Environment reload */

        DeleteAlphaList(&VHEAP);
        InitAlphaList(&VHEAP);
        DeleteAlphaList(&VADDCLASSES);
        InitAlphaList(&VADDCLASSES);
        DeleteAlphaList(&VHARDHEAP);
        InitAlphaList(&VHARDHEAP);

        DeleteItemList(IPADDRESSES);
        IPADDRESSES = NULL;


        DeleteScope("this");
        DeleteScope("mon");
        DeleteScope("sys");
        NewScope("this");
        NewScope("mon");
        NewScope("sys");

        GetInterfacesInfo(AGENT_TYPE_EXECUTOR);
        Get3Environment();
        BuiltinClasses();
        OSClasses();
        SetReferenceTime(true);
    }

    for (ip = SCHEDULE; ip != NULL; ip = ip->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking schedule %s...\n", ip->name);

        if (IsDefinedClass(ip->name, NULL))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Waking up the agent at %s ~ %s \n", cf_ctime(&CFSTARTTIME), ip->name);
            return true;
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Nothing to do at %s\n", cf_ctime(&CFSTARTTIME));
    return false;
}

/*************************************************************************/

ExecConfig *CopyExecConfig(const ExecConfig *config)
{
    ExecConfig *copy = xcalloc(1, sizeof(ExecConfig));
    copy->scheduled_run = config->scheduled_run;
    copy->exec_command = xstrdup(config->exec_command);
    copy->mail_server = xstrdup(config->mail_server);
    copy->mail_from_address = xstrdup(config->mail_from_address);
    copy->mail_to_address = xstrdup(config->mail_to_address);
    copy->fq_name = xstrdup(config->fq_name);
    copy->ip_address = xstrdup(config->ip_address);
    copy->mail_max_lines = config->mail_max_lines;
    copy->agent_expireafter = config->agent_expireafter;

    return copy;
}

void DestroyExecConfig(ExecConfig *config)
{
    free(config->exec_command);
    free(config->mail_server);
    free(config->mail_from_address);
    free(config->mail_to_address);
    free(config->fq_name);
    free(config->ip_address);
    free(config);
}
