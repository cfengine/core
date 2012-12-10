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
#include "constraints.h"
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

#define CF_EXEC_IFELAPSED 0
#define CF_EXEC_EXPIREAFTER 1

static int NO_FORK;
static int ONCE;
static int WINSERVICE = true;

static Item *SCHEDULE;
static int SPLAYTIME = 0;

#if defined(HAVE_PTHREAD)
static pthread_attr_t threads_attrs;
#endif

/*******************************************************************/

static GenericAgentConfig CheckOpts(int argc, char **argv);
static void ThisAgentInit(void);
static bool ScheduleRun(Policy **policy, ExecConfig *exec_config, const ReportContext *report_context);
static void Apoptosis(void);

#if defined(HAVE_PTHREAD)
static bool LocalExecInThread(const ExecConfig *config);
#endif

void StartServer(Policy *policy, ExecConfig *config, const ReportContext *report_context);
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
    "Run once and then exit",
    "Do not run as a service on windows - use this when running from a command shell (Cfengine Nova only)",
    "Set the internal value of LD_LIBRARY_PATH for child processes",
    NULL
};

/*****************************************************************************/

int main(int argc, char *argv[])
{
    GenericAgentConfig config = CheckOpts(argc, argv);

    ReportContext *report_context = OpenReports("executor");
    Policy *policy = GenericInitialize("executor", config, report_context);
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

#ifdef MINGW
    if (WINSERVICE)
    {
        NovaWin_StartExecService();
    }
    else
#endif /* MINGW */
    {
        StartServer(policy, &exec_config, report_context);
    }

    ReportContextDestroy(report_context);

    return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

static GenericAgentConfig CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    int c;
    char ld_library_path[CF_BUFSIZE];
    GenericAgentConfig config = GenericAgentDefaultConfig(AGENT_TYPE_EXECUTOR);

    while ((c = getopt_long(argc, argv, "dvnKIf:D:N:VxL:hFOV1gMW", OPTIONS, &optindex)) != EOF)
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
            SelfDiagnostic();
            exit(0);

        default:
            Syntax("cf-execd - cfengine's execution agent", OPTIONS, HINTS, ID);
            exit(1);

        }
    }

    if (argv[optind] != NULL)
    {
        CfOut(cf_error, "", "Unexpected argument with no preceding option: %s\n", argv[optind]);
    }

    return config;
}

/*****************************************************************************/

static void ThisAgentInit(void)
{
    umask(077);

    if (SCHEDULE == NULL)
    {
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
}

/*****************************************************************************/

static double GetSplay(void)
{
    char splay[CF_BUFSIZE];

    snprintf(splay, CF_BUFSIZE, "%s+%s+%ju", VFQNAME, VIPADDRESS, (uintmax_t)getuid());

    return ((double) GetHash(splay)) / CF_HASHTABLESIZE;
}

/*****************************************************************************/
/* Might be called back from NovaWin_StartExecService */

void KeepPromises(Policy *policy, ExecConfig *config)
{
    for (Constraint *cp = ControlBodyConstraints(policy, AGENT_TYPE_EXECUTOR); cp != NULL; cp = cp->next)
    {
    if (IsExcluded(cp->classes, NULL))
        {
            continue;
        }

        Rval retval;
        if (GetVariable("control_executor", cp->lval, &retval) == cf_notype)
        {
            CfOut(cf_error, "", "Unknown lval %s in exec control body", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFEX_CONTROLBODY[cfex_mailfrom].lval) == 0)
        {
            free(config->mail_from_address);
            config->mail_from_address = SafeStringDuplicate(retval.item);
            CfDebug("mailfrom = %s\n", config->mail_from_address);
        }

        if (strcmp(cp->lval, CFEX_CONTROLBODY[cfex_mailto].lval) == 0)
        {
            free(config->mail_to_address);
            config->mail_to_address = SafeStringDuplicate(retval.item);
            CfDebug("mailto = %s\n", config->mail_to_address);
        }

        if (strcmp(cp->lval, CFEX_CONTROLBODY[cfex_smtpserver].lval) == 0)
        {
            free(config->mail_server);
            config->mail_server = SafeStringDuplicate(retval.item);
            CfDebug("smtpserver = %s\n", config->mail_server);
        }

        if (strcmp(cp->lval, CFEX_CONTROLBODY[cfex_execcommand].lval) == 0)
        {
            free(config->exec_command);
            config->exec_command = SafeStringDuplicate(retval.item);
            CfDebug("exec_command = %s\n", config->exec_command);
        }

        if (strcmp(cp->lval, CFEX_CONTROLBODY[cfex_agent_expireafter].lval) == 0)
        {
            config->agent_expireafter = Str2Int(retval.item);
            CfDebug("agent_expireafter = %d\n", config->agent_expireafter);
        }

        if (strcmp(cp->lval, CFEX_CONTROLBODY[cfex_executorfacility].lval) == 0)
        {
            SetFacility(retval.item);
            continue;
        }

        if (strcmp(cp->lval, CFEX_CONTROLBODY[cfex_mailmaxlines].lval) == 0)
        {
            config->mail_max_lines = Str2Int(retval.item);
            CfDebug("maxlines = %d\n", config->mail_max_lines);
        }

        if (strcmp(cp->lval, CFEX_CONTROLBODY[cfex_splaytime].lval) == 0)
        {
            int time = Str2Int(ScalarRvalValue(retval));

            SPLAYTIME = (int) (time * SECONDS_PER_MINUTE * GetSplay());
        }

        if (strcmp(cp->lval, CFEX_CONTROLBODY[cfex_schedule].lval) == 0)
        {
            Rlist *rp;

            CfDebug("schedule ...\n");
            DeleteItemList(SCHEDULE);
            SCHEDULE = NULL;

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                if (!IsItemIn(SCHEDULE, rp->item))
                {
                    AppendItem(&SCHEDULE, rp->item, NULL);
                }
            }
        }
    }
}

/*****************************************************************************/

/* Might be called back from NovaWin_StartExecService */
void StartServer(Policy *policy, ExecConfig *config, const ReportContext *report_context)
{
#if !defined(__MINGW32__)
    time_t now = time(NULL);
#endif
    Promise *pp = NewPromise("exec_cfengine", "the executor agent");
    Attributes dummyattr;
    CfLock thislock;

#if defined(HAVE_PTHREAD)
    pthread_attr_init(&threads_attrs);
    pthread_attr_setdetachstate(&threads_attrs, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&threads_attrs, (size_t)2048*1024);
#endif

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
            DeletePromise(pp);
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

#ifdef MINGW

    if (!NO_FORK)
    {
        CfOut(cf_verbose, "", "Windows does not support starting processes in the background - starting in foreground");
    }

#else /* NOT MINGW */

    if ((!NO_FORK) && (fork() != 0))
    {
        CfOut(cf_inform, "", "cf-execd starting %.24s\n", cf_ctime(&now));
        _exit(0);
    }

    if (!NO_FORK)
    {
        ActAsDaemon(0);
    }

#endif /* NOT MINGW */

    WritePID("cf-execd.pid");
    signal(SIGINT, HandleSignals);
    signal(SIGTERM, HandleSignals);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignals);
    signal(SIGUSR2, HandleSignals);

    umask(077);

    if (ONCE)
    {
        CfOut(cf_verbose, "", "Sleeping for splaytime %d seconds\n\n", SPLAYTIME);
        sleep(SPLAYTIME);
        LocalExec(config);
        CloseLog();
    }
    else
    {
        while (true)
        {
            if (ScheduleRun(&policy, config, report_context))
            {
                CfOut(cf_verbose, "", "Sleeping for splaytime %d seconds\n\n", SPLAYTIME);
                sleep(SPLAYTIME);

#if defined(HAVE_PTHREAD)
                if (!LocalExecInThread(config))
                {
                    CfOut(cf_inform, "", "Unable to run agent in thread, falling back to blocking execution");
#endif
                    LocalExec(config);
#if defined(HAVE_PTHREAD)
                }
#endif
            }
        }
    }

    if (!ONCE)
    {
        YieldCurrentLock(thislock);
    }
}

/*****************************************************************************/

#if defined(HAVE_PTHREAD)
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
        CfOut(cf_inform, "pthread_create", "Can't create thread!");
        return false;
    }
}
#endif

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static void Apoptosis()
{
    Promise pp = { 0 };
    Rlist *signals = NULL, *owners = NULL;
    char mypid[32];
    static char promiser_buf[CF_SMALLBUF];

#if defined(__CYGWIN__) || defined(__MINGW32__)
    return;
#endif

    CfOut(cf_verbose, "", " !! Programmed pruning of the scheduler cluster");

#ifdef MINGW
    snprintf(promiser_buf, sizeof(promiser_buf), "cf-execd");     // using '\' causes regexp problems
#else
    snprintf(promiser_buf, sizeof(promiser_buf), "%s/bin/cf-execd", CFWORKDIR);
#endif

    pp.promiser = promiser_buf;
    pp.promisee = (Rval) {"cfengine", CF_SCALAR};
    pp.classes = "any";
    pp.offset.line = 0;
    pp.audit = NULL;
    pp.conlist = NULL;

    pp.bundletype = "agent";
    pp.bundle = "exec_apoptosis";
    pp.ref = "Programmed death";
    pp.agentsubtype = "processes";
    pp.done = false;
    pp.next = NULL;
    pp.cache = NULL;
    pp.inode_cache = NULL;
    pp.this_server = NULL;
    pp.donep = &(pp.done);
    pp.conn = NULL;

    GetCurrentUserName(mypid, 31);

    PrependRlist(&signals, "term", CF_SCALAR);
    PrependRlist(&owners, mypid, CF_SCALAR);

    ConstraintAppendToPromise(&pp, "signals", (Rval) {signals, CF_LIST}, "any", false);
    ConstraintAppendToPromise(&pp, "process_select", (Rval) {xstrdup("true"), CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&pp, "process_owner", (Rval) {owners, CF_LIST}, "any", false);
    ConstraintAppendToPromise(&pp, "ifelapsed", (Rval) {xstrdup("0"), CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&pp, "process_count", (Rval) {xstrdup("true"), CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&pp, "match_range", (Rval) {xstrdup("0,2"), CF_SCALAR}, "any", false);
    ConstraintAppendToPromise(&pp, "process_result", (Rval) {xstrdup("process_owner.process_count"), CF_SCALAR}, "any", false);

    CfOut(cf_verbose, "", " -> Looking for cf-execd processes owned by %s", mypid);

    if (LoadProcessTable(&PROCESSTABLE))
    {
        VerifyProcessesPromise(&pp);
    }

    DeleteItemList(PROCESSTABLE);

    if (pp.conlist)
    {
        DeleteConstraintList(pp.conlist);
    }

    CfOut(cf_verbose, "", " !! Pruning complete");
}

/*****************************************************************************/

typedef enum
{
    RELOAD_ENVIRONMENT,
    RELOAD_FULL
} Reload;

static Reload CheckNewPromises(const ReportContext *report_context)
{
    if (NewPromiseProposals())
    {
        CfOut(cf_verbose, "", " -> New promises detected...\n");

        if (CheckPromises(AGENT_TYPE_EXECUTOR, report_context))
        {
            return RELOAD_FULL;
        }
        else
        {
            CfOut(cf_inform, "", " !! New promises file contains syntax errors -- ignoring");
            PROMISETIME = time(NULL);
        }
    }
    else
    {
        CfDebug(" -> No new promises found\n");
    }

    return RELOAD_ENVIRONMENT;
}

static bool ScheduleRun(Policy **policy, ExecConfig *exec_config, const ReportContext *report_context)
{
    Item *ip;

    CfOut(cf_verbose, "", "Sleeping...\n");
    sleep(CFPULSETIME);         /* 1 Minute resolution is enough */

// recheck license (in case of license updates or expiry)

    if (EnterpriseExpiry())
    {
        CfOut(cf_error, "", "Cfengine - autonomous configuration engine. This enterprise license is invalid.\n");
        exit(1);
    }

    /*
     * FIXME: this logic duplicates the one from cf-serverd.c. Unify ASAP.
     */

    if (CheckNewPromises(report_context) == RELOAD_FULL)
    {
        /* Full reload */

        CfOut(cf_inform, "", "Re-reading promise file %s..\n", VINPUTFILE);

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
        VINPUTLIST = NULL;

        PolicyDestroy(*policy);
        *policy = NULL;

        ERRORCOUNT = 0;

        NewScope("sys");

        SetPolicyServer(POLICY_SERVER);
        NewScalar("sys", "policy_hub", POLICY_SERVER, cf_str);

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

        GenericAgentConfig config = {
            .bundlesequence = NULL
        };

        *policy = ReadPromises(AGENT_TYPE_EXECUTOR, CF_EXECC, config, report_context);
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
        CfOut(cf_verbose, "", "Checking schedule %s...\n", ip->name);

        if (IsDefinedClass(ip->name, NULL))
        {
            CfOut(cf_verbose, "", "Waking up the agent at %s ~ %s \n", cf_ctime(&CFSTARTTIME), ip->name);
            return true;
        }
    }

    CfOut(cf_verbose, "", "Nothing to do at %s\n", cf_ctime(&CFSTARTTIME));
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
