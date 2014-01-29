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

#include <generic_agent.h>

#include <actuator.h>
#include <audit.h>
#include <eval_context.h>
#include <verify_classes.h>
#include <verify_databases.h>
#include <verify_environments.h>
#include <verify_exec.h>
#include <verify_methods.h>
#include <verify_processes.h>
#include <verify_packages.h>
#include <verify_users.h>
#include <verify_services.h>
#include <verify_storage.h>
#include <verify_files.h>
#include <verify_files_utils.h>
#include <verify_vars.h>
#include <addr_lib.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <files_repository.h>
#include <files_edit.h>
#include <files_properties.h>
#include <item_lib.h>
#include <vars.h>
#include <conversion.h>
#include <expand.h>
#include <locks.h>
#include <scope.h>
#include <matching.h>
#include <match_scope.h>
#include <instrumentation.h>
#include <promises.h>
#include <unix.h>
#include <attributes.h>
#include <communication.h>
#include <signals.h>
#include <nfs.h>
#include <processes_select.h>
#include <list.h>
#include <fncall.h>
#include <rlist.h>
#include <agent-diagnostics.h>
#include <known_dirs.h>
#include <cf-agent-enterprise-stubs.h>
#include <syslog_client.h>
#include <man.h>
#include <bootstrap.h>
#include <misc_lib.h>
#include <buffer.h>

#include <mod_common.h>

typedef enum
{
    TYPE_SEQUENCE_META,
    TYPE_SEQUENCE_VARS,
    TYPE_SEQUENCE_DEFAULTS,
    TYPE_SEQUENCE_CONTEXTS,
    TYPE_SEQUENCE_INTERFACES,
    TYPE_SEQUENCE_USERS,
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
#include <findhub.h>
#endif
#endif

#include <ornaments.h>


extern int PR_KEPT;
extern int PR_REPAIRED;
extern int PR_NOTKEPT;

static bool ALLCLASSESREPORT = false; /* GLOBAL_P */
static bool ALWAYS_VALIDATE = false; /* GLOBAL_P */
static bool CFPARANOID = false; /* GLOBAL_P */

static const Rlist *ACCESSLIST = NULL; /* GLOBAL_P */

static int CFA_BACKGROUND = 0; /* GLOBAL_X */
static int CFA_BACKGROUND_LIMIT = 1; /* GLOBAL_P */

static Item *PROCESSREFRESH = NULL; /* GLOBAL_P */

static const char *const AGENT_TYPESEQUENCE[] =
{
    "meta",
    "vars",
    "defaults",
    "classes",                  /* Maelstrom order 2 */
    "interfaces",
    "users",
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
static char **TranslateOldBootstrapOptionsSeparate(int *argc_new, char **argv);
static char **TranslateOldBootstrapOptionsConcatenated(int argc, char **argv);
static void FreeStringArray(int size, char **array);
static void CheckAgentAccess(const Rlist *list, const Policy *policy);
static void KeepControlPromises(EvalContext *ctx, const Policy *policy);
static PromiseResult KeepAgentPromise(EvalContext *ctx, const Promise *pp, void *param);
static int NewTypeContext(TypeSequence type);
static void DeleteTypeContext(EvalContext *ctx, const Bundle *bp, TypeSequence type);
static void ClassBanner(EvalContext *ctx, TypeSequence type);
static PromiseResult ParallelFindAndVerifyFilesPromises(EvalContext *ctx, const Promise *pp);
static bool VerifyBootstrap(void);
static void KeepPromiseBundles(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config);
static void KeepPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config);
static int NoteBundleCompliance(const Bundle *bundle, int save_pr_kept, int save_pr_repaired, int save_pr_notkept);
static void AllClassesReport(const EvalContext *ctx);
static bool HasAvahiSupport(void);
static int AutomaticBootstrap(GenericAgentConfig *config);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *const CF_AGENT_SHORT_DESCRIPTION =
    "evaluate CFEngine policy code and actuate change to the system.";

static const char *const CF_AGENT_MANPAGE_LONG_DESCRIPTION =
        "cf-agent evaluates policy code and makes changes to the system. Policy bundles are evaluated in the order of the "
        "provided bundlesequence (this is normally specified in the common control body). "
        "For each bundle, cf-agent groups promise statements according to their type. Promise types are then evaluated in a preset "
        "order to ensure fast system convergence to policy.\n";

static const struct option OPTIONS[] =
{
    {"bootstrap", required_argument, 0, 'B'},
    {"bundlesequence", required_argument, 0, 'b'},
    {"debug", no_argument, 0, 'd'},
    {"define", required_argument, 0, 'D'},
    {"self-diagnostics", optional_argument, 0, 'x'},
    {"dry-run", no_argument, 0, 'n'},
    {"file", required_argument, 0, 'f'},
    {"help", no_argument, 0, 'h'},
    {"inform", no_argument, 0, 'I'},
    {"negate", required_argument, 0, 'N'},
    {"no-lock", no_argument, 0, 'K'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"legacy-output", no_argument, 0, 'l'},
    {"color", optional_argument, 0, 'C'},
    {"no-extensions", no_argument, 0, 'E'},
    {NULL, 0, 0, '\0'}
};

static const char *const HINTS[] =
{
    "Bootstrap CFEngine to the given policy server IP, hostname or :avahi (automatic detection)",
    "Set or override bundlesequence from command line",
    "Enable debugging output",
    "Define a list of comma separated classes to be defined at the start of execution",
    "Run checks to diagnose a CFEngine agent installation",
    "All talk and no action mode - make no changes, only inform of promises not kept",
    "Specify an alternative input file than the default",
    "Print the help message",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Define a list of comma separated classes to be undefined at the start of execution",
    "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    "Use legacy output format",
    "Enable colorized output. Possible values: 'always', 'auto', 'never'. If option is used, the default value is 'auto'",
    "Disable extension loading (used while upgrading)",
    NULL
};

/*******************************************************************/

int main(int argc, char *argv[])
{
    int ret = 0;

    EvalContext *ctx = EvalContextNew();

    GenericAgentConfig *config = CheckOpts(argc, argv);
    GenericAgentConfigApply(ctx, config);

    GenericAgentDiscoverContext(ctx, config);

    Policy *policy = NULL;
    if (GenericAgentCheckPolicy(config, ALWAYS_VALIDATE, true))
    {
        policy = GenericAgentLoadPolicy(ctx, config);
    }
    else if (config->tty_interactive)
    {
        exit(EXIT_FAILURE);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "CFEngine was not able to get confirmation of promises from cf-promises, so going to failsafe");
        EvalContextClassPutHard(ctx, "failsafe_fallback", "group=Errors,source=agent");
        GenericAgentConfigSetInputFile(config, GetInputDir(), "failsafe.cf");
        policy = GenericAgentLoadPolicy(ctx, config);
    }

    ThisAgentInit();
    BeginAudit();
    KeepPromises(ctx, policy, config);

    if (ALLCLASSESREPORT)
    {
        AllClassesReport(ctx);
    }

    // only note class usage when default policy is run
    Nova_NoteClassUsage(ctx);
    Nova_NoteVarUsageDB(ctx);
    Nova_TrackExecution(config->input_file);
    PurgeLocks();

    if (config->agent_specific.agent.bootstrap_policy_server && !VerifyBootstrap())
    {
        RemovePolicyServerFile(GetWorkDir());
        WriteAmPolicyHubFile(GetWorkDir(), false);
        ret = 1;
    }

    EndAudit(ctx, CFA_BACKGROUND);
    EvalContextDestroy(ctx);

    GenerateDiffReports(config->input_file);
    GenericAgentConfigDestroy(config);

    return ret;
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_AGENT);

/* DEPRECATED:
   --policy-server (-s) is deprecated in community version 3.5.0.
   Support rewrite from some common old bootstrap options (until community version 3.6.0?).
 */

    int argc_new = argc;
    char **argv_tmp = TranslateOldBootstrapOptionsSeparate(&argc_new, argv);
    char **argv_new = TranslateOldBootstrapOptionsConcatenated(argc_new, argv_tmp);
    FreeStringArray(argc_new, argv_tmp);

    while ((c = getopt_long(argc_new, argv_new, "dvnKIf:D:N:VxMB:b:hlC::E", OPTIONS, NULL)) != EOF)
    {
        switch ((char) c)
        {
        case 'l':
            LEGACY_OUTPUT = true;
            break;

        case 'f':
            if (optarg && strlen(optarg) < 5)
            {
                Log(LOG_LEVEL_ERR, "-f used but argument '%s' incorrect", optarg);
                exit(EXIT_FAILURE);
            }

            GenericAgentConfigSetInputFile(config, GetInputDir(), optarg);
            MINUSF = true;
            break;

        case 'b':
            if (optarg)
            {
                Rlist *bundlesequence = RlistFromSplitString(optarg, ',');
                GenericAgentConfigSetBundleSequence(config, bundlesequence);
                RlistDestroy(bundlesequence);
            }
            break;

        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            break;

        case 'B':
            {
                if (!BootstrapAllowed())
                {
                    Log(LOG_LEVEL_ERR, "Not enough privileges to bootstrap CFEngine");
                    exit(EXIT_FAILURE);
                }

                if(strcmp(optarg, ":avahi") == 0)
                {
                    if(!HasAvahiSupport())
                    {
                        Log(LOG_LEVEL_ERR, "Avahi support is not built in, please see options to the configure script and rebuild CFEngine");
                        exit(EXIT_FAILURE);
                    }

                    int err = AutomaticBootstrap(config);
                    if (err < 0)
                    {
                        Log(LOG_LEVEL_ERR, "Automatic bootstrap failed, error code '%d'", err);
                        exit(EXIT_FAILURE);
                    }
                    break;
                }

                if(IsLoopbackAddress(optarg))
                {
                    Log(LOG_LEVEL_ERR, "Cannot bootstrap to a loopback address");
                    exit(EXIT_FAILURE);
                }

                // temporary assure that network functions are working
                OpenNetwork();

                char mapped_policy_server[CF_MAX_IP_LEN] = "";
                if (Hostname2IPString(mapped_policy_server, optarg,
                                      sizeof(mapped_policy_server)) == -1)
                {
                    Log(LOG_LEVEL_ERR,
                        "Could not resolve hostname '%s', unable to bootstrap",
                        optarg);
                    exit(EXIT_FAILURE);
                }

                CloseNetwork();

                MINUSF = true;
                config->ignore_locks = true;
                GenericAgentConfigSetInputFile(config, GetInputDir(),
                                               "promises.cf");
                config->agent_specific.agent.bootstrap_policy_server =
                    xstrdup(mapped_policy_server);
            }
            break;

        case 'K':
            config->ignore_locks = true;
            break;

        case 'D':
            config->heap_soft = StringSetFromString(optarg, ',');
            break;

        case 'N':
            config->heap_negated = StringSetFromString(optarg, ',');
            break;

        case 'I':
            LogSetGlobalLevel(LOG_LEVEL_INFO);
            break;

        case 'v':
            LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
            break;

        case 'n':
            DONTDO = true;
            config->ignore_locks = true;
            break;

        case 'V':
            {
                Writer *w = FileWriter(stdout);
                GenericAgentWriteVersion(w);
                FileWriterDetach(w);
            }
            exit(EXIT_SUCCESS);

        case 'h':
            {
                Writer *w = FileWriter(stdout);
                GenericAgentWriteHelp(w, "cf-agent", OPTIONS, HINTS, true);
                FileWriterDetach(w);
            }
            exit(EXIT_SUCCESS);

        case 'M':
            {
                Writer *out = FileWriter(stdout);
                ManPageWrite(out, "cf-agent", time(NULL),
                             CF_AGENT_SHORT_DESCRIPTION,
                             CF_AGENT_MANPAGE_LONG_DESCRIPTION,
                             OPTIONS, HINTS,
                             true);
                FileWriterDetach(out);
                exit(EXIT_SUCCESS);
            }

        case 'x':
            {
                const char *workdir = GetWorkDir();
                const char *inputdir = GetInputDir();
                const char *logdir = GetLogDir();
                strcpy(CFWORKDIR, workdir);
                Writer *out = FileWriter(stdout);
                WriterWriteF(out, "self-diagnostics for agent using workdir '%s'\n", workdir);
                WriterWriteF(out, "self-diagnostics for agent using inputdir '%s'\n", inputdir);
                WriterWriteF(out, "self-diagnostics for agent using logdir '%s'\n", logdir);

                AgentDiagnosticsRun(workdir, AgentDiagnosticsAllChecks(), out);
                AgentDiagnosticsRunAllChecksNova(workdir, out, &AgentDiagnosticsRun, &AgentDiagnosticsResultNew);
                FileWriterDetach(out);
            }
            exit(EXIT_SUCCESS);

        case 'C':
            if (!GenericAgentConfigParseColor(config, optarg))
            {
                exit(EXIT_FAILURE);
            }
            break;

        case 'E':
            extension_libraries_disable();
            break;

        default:
            {
                Writer *w = FileWriter(stdout);
                GenericAgentWriteHelp(w, "cf-agent", OPTIONS, HINTS, true);
                FileWriterDetach(w);
            }
            exit(EXIT_FAILURE);
        }
    }

    if (!GenericAgentConfigParseArguments(config, argc_new - optind,
                                          argv_new + optind))
    {
        Log(LOG_LEVEL_ERR, "Too many arguments");
        exit(EXIT_FAILURE);
    }

    FreeStringArray(argc_new, argv_new);

    return config;
}


static char **TranslateOldBootstrapOptionsSeparate(int *argc_new, char **argv)
{
    int i;
    int policy_server_argnum = 0;
    int server_address_argnum = 0;
    int bootstrap_argnum = 0;
    int argc = *argc_new;

    for(i = 0; i < argc; i++)
    {
        if(strcmp(argv[i], "--policy-server") == 0 || strcmp(argv[i], "-s") == 0)
        {
            policy_server_argnum = i;
        }

        if(strcmp(argv[i], "--bootstrap") == 0 || strcmp(argv[i], "-B") == 0)
        {
            bootstrap_argnum = i;
        }
    }

    if(policy_server_argnum > 0)
    {
        if(policy_server_argnum + 1 < argc)
        {
            server_address_argnum = policy_server_argnum + 1;
        }
    }

    char **argv_new;

    if(bootstrap_argnum > 0 && server_address_argnum > 0)
    {
        Log(LOG_LEVEL_WARNING, "Deprecated bootstrap options detected. The --policy-server (-s) option is deprecated from CFEngine community version 3.5.0."
            "Please provide the address argument to --bootstrap (-B) instead. Rewriting your arguments now, but you need to adjust them as this support will be removed soon.");

        *argc_new = argc - 1;  // --policy-server deprecated
        argv_new = xcalloc(1, sizeof(char *) * (*argc_new + 1));

        int new_i = 0;

        for(i = 0; i < argc; i++)
        {
            if(i == bootstrap_argnum)
            {
                argv_new[new_i++] = xstrdup(argv[bootstrap_argnum]);
                argv_new[new_i++] = xstrdup(argv[server_address_argnum]);
            }
            else if(i == server_address_argnum)
            {
                // skip: handled above
            }
            else if(i == policy_server_argnum)
            {
                // skip: deprecated
            }
            else
            {
                argv_new[new_i++] = xstrdup(argv[i]);
            }
        }
    }
    else
    {
        argv_new = xcalloc(1, sizeof(char *) * (*argc_new + 1));

        for(i = 0; i < argc; i++)
        {
            argv_new[i] = xstrdup(argv[i]);
        }
    }

    return argv_new;
}


static char **TranslateOldBootstrapOptionsConcatenated(int argc, char **argv)
{
    char **argv_new = xcalloc(1, sizeof(char *) * (argc + 1));

    for(int i = 0; i < argc; i++)
    {
        if(strcmp(argv[i], "-Bs") == 0)
        {
            Log(LOG_LEVEL_WARNING, "Deprecated bootstrap options detected. The --policy-server (-s) option is deprecated from CFEngine community version 3.5.0."
                "Please provide the address argument to --bootstrap (-B) instead. Rewriting your arguments now, but you need to adjust them as this support will be removed soon.");
            argv_new[i] = xstrdup("-B");
        }
        else
        {
            argv_new[i] = xstrdup(argv[i]);
        }
    }

    return argv_new;
}


static void FreeStringArray(int size, char **array)
{
    for(int i = 0; i < size; i++)
    {
        free(array[i]);
    }

    free(array);
}

/*******************************************************************/

static void ThisAgentInit(void)
{
    FILE *fp;
    char filename[CF_BUFSIZE];

#ifdef HAVE_SETSID
    Log(LOG_LEVEL_VERBOSE, "Setting session ID, becoming process group leader");
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

    snprintf(filename, CF_BUFSIZE, "%s/cfagent.%s.log", GetLogDir(), VSYSNAME.nodename);
    MapName(filename);

    if ((fp = fopen(filename, "a")) != NULL)
    {
        fclose(fp);
    }
}

/*******************************************************************/

static void KeepPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
{
    KeepControlPromises(ctx, policy);
    KeepPromiseBundles(ctx, policy, config);
}

/*******************************************************************/
/* Level 2                                                         */
/*******************************************************************/

static void KeepControlPromises(EvalContext *ctx, const Policy *policy)
{
    Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_AGENT);
    if (constraints)
    {
        for (size_t i = 0; i < SeqLength(constraints); i++)
        {
            Constraint *cp = SeqAt(constraints, i);

            if (!IsDefinedClass(ctx, cp->classes))
            {
                continue;
            }

            if (EvalContextVariableControlCommonGet(ctx, CommonControlFromString(cp->lval)))
            {
                /* Already handled in generic_agent */
                continue;
            }

            VarRef *ref = VarRefParseFromScope(cp->lval, "control_agent");
            const void *value = EvalContextVariableGet(ctx, ref, NULL);
            VarRefDestroy(ref);
            if (!value)
            {
                Log(LOG_LEVEL_ERR, "Unknown lval '%s' in agent control body", cp->lval);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_MAXCONNECTIONS].lval) == 0)
            {
                CFA_MAXTHREADS = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting maxconnections to %d", CFA_MAXTHREADS);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_CHECKSUM_ALERT_TIME].lval) == 0)
            {
                CF_PERSISTENCE = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting checksum_alert_time to %d", CF_PERSISTENCE);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_AGENTFACILITY].lval) == 0)
            {
                SetFacility(value);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_AGENTACCESS].lval) == 0)
            {
                ACCESSLIST = value;
                CheckAgentAccess(ACCESSLIST, policy);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_REFRESH_PROCESSES].lval) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Setting refresh_processes when starting to...");
                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    Log(LOG_LEVEL_VERBOSE, "%s", RlistScalarValue(rp));
                    // TODO: why is this only done in verbose mode?
                    // original commit says 'optimization'.
                    if (LogGetGlobalLevel() >= LOG_LEVEL_VERBOSE)
                    {
                        PrependItem(&PROCESSREFRESH, RlistScalarValue(rp), NULL);
                    }
                }
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ABORTCLASSES].lval) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Setting abort classes from ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    char name[CF_MAXVARSIZE] = "";

                    strncpy(name, RlistScalarValue(rp), CF_MAXVARSIZE - 1);

                    EvalContextHeapAddAbort(ctx, name, cp->classes);
                }

                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ABORTBUNDLECLASSES].lval) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Setting abort bundle classes from ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    char name[CF_MAXVARSIZE] = "";
                    strncpy(name, RlistScalarValue(rp), CF_MAXVARSIZE - 1);

                    EvalContextHeapAddAbortCurrentBundle(ctx, name, cp->classes);
                }

                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ADDCLASSES].lval) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Add classes ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    Log(LOG_LEVEL_VERBOSE, "... %s", RlistScalarValue(rp));
                    EvalContextClassPutSoft(ctx, RlistScalarValue(rp), CONTEXT_SCOPE_NAMESPACE, "source=environment");
                }

                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ALWAYSVALIDATE].lval) == 0)
            {
                ALWAYS_VALIDATE = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting alwaysvalidate to '%s'", ALWAYS_VALIDATE ? "true" : "false");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ALLCLASSESREPORT].lval) == 0)
            {
                ALLCLASSESREPORT = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting allclassesreport to '%s'", ALLCLASSESREPORT ? "true" : "false");
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_SECUREINPUT].lval) == 0)
            {
                CFPARANOID = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting secure input to '%s'", CFPARANOID ? "true" : "false");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_BINDTOINTERFACE].lval) == 0)
            {
                strncpy(BINDINTERFACE, value, CF_BUFSIZE - 1);
                Log(LOG_LEVEL_VERBOSE, "Setting bindtointerface to '%s'", BINDINTERFACE);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_HASHUPDATES].lval) == 0)
            {
                bool enabled = BooleanFromString(value);

                SetChecksumUpdatesDefault(ctx, enabled);
                Log(LOG_LEVEL_VERBOSE, "Setting checksum updates to '%s'", enabled ? "true" : "false");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_CHILDLIBPATH].lval) == 0)
            {
                char output[CF_BUFSIZE];

                snprintf(output, CF_BUFSIZE, "Setting LD_LIBRARY_PATH to '%s'", (const char *)value);
                if (putenv(xstrdup(output)) == 0)
                {
                    Log(LOG_LEVEL_VERBOSE, "Setting '%s'", output);
                }
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_DEFAULTCOPYTYPE].lval) == 0)
            {
                DEFAULT_COPYTYPE = value;
                Log(LOG_LEVEL_VERBOSE, "Setting defaultcopytype to '%s'", DEFAULT_COPYTYPE);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_FSINGLECOPY].lval) == 0)
            {
                SINGLE_COPY_LIST = value;
                Log(LOG_LEVEL_VERBOSE, "Setting file single copy list");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_FAUTODEFINE].lval) == 0)
            {
                SetFileAutoDefineList(value);
                Log(LOG_LEVEL_VERBOSE, "Setting file auto define list");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_DRYRUN].lval) == 0)
            {
                DONTDO = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting dryrun to %c", DONTDO);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_INFORM].lval) == 0)
            {
                bool inform = BooleanFromString(value);
                if (inform)
                {
                    LogSetGlobalLevel(MAX(LOG_LEVEL_INFO, LogGetGlobalLevel()));
                }
                else
                {
                    if (LogGetGlobalLevel() >= LOG_LEVEL_INFO)
                    {
                        LogSetGlobalLevel(LOG_LEVEL_NOTICE);
                    }
                }
                Log(LOG_LEVEL_VERBOSE, "body agent control, inform => '%s', sets new log level to '%s'",
                    inform ? "true" : "false", LogLevelToString(LogGetGlobalLevel()));
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_VERBOSE].lval) == 0)
            {
                bool verbose = BooleanFromString(value);
                if (verbose)
                {
                    LogSetGlobalLevel(MAX(LOG_LEVEL_VERBOSE, LogGetGlobalLevel()));
                }
                else
                {
                    if (LogGetGlobalLevel() >= LOG_LEVEL_VERBOSE)
                    {
                        LogSetGlobalLevel(LOG_LEVEL_INFO);
                    }
                }
                Log(LOG_LEVEL_VERBOSE, "body agent control, verbose => '%s', sets new log level to '%s'",
                    verbose ? "true" : "false", LogLevelToString(LogGetGlobalLevel()));
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_REPOSITORY].lval) == 0)
            {
                SetRepositoryLocation(value);
                Log(LOG_LEVEL_VERBOSE, "SET repository = %s", (const char *)value);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_SKIPIDENTIFY].lval) == 0)
            {
                bool enabled = BooleanFromString(value);

                SetSkipIdentify(enabled);
                Log(LOG_LEVEL_VERBOSE, "Setting skipidentify to '%s'", enabled ? "true" : "false");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_SUSPICIOUSNAMES].lval) == 0)
            {
                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    AddFilenameToListOfSuspicious(RlistScalarValue(rp));
                    Log(LOG_LEVEL_VERBOSE, "Considering '%s' as suspicious file", RlistScalarValue(rp));
                }

                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_REPCHAR].lval) == 0)
            {
                char c = *(char *)value;

                SetRepositoryChar(c);
                Log(LOG_LEVEL_VERBOSE, "Setting repchar to '%c'", c);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_MOUNTFILESYSTEMS].lval) == 0)
            {
                CF_MOUNTALL = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting mountfilesystems to '%s'", CF_MOUNTALL ? "true" : "false");
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_EDITFILESIZE].lval) == 0)
            {
                EDITFILESIZE = IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting edit file size to %d", EDITFILESIZE);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_IFELAPSED].lval) == 0)
            {
                VIFELAPSED = IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting ifelapsed to %d", VIFELAPSED);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_EXPIREAFTER].lval) == 0)
            {
                VEXPIREAFTER = IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting expireafter to %d", VEXPIREAFTER);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_TIMEOUT].lval) == 0)
            {
                CONNTIMEOUT = IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting timeout = %jd", (intmax_t) CONNTIMEOUT);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_MAX_CHILDREN].lval) == 0)
            {
                CFA_BACKGROUND_LIMIT = IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting max_children to %d", CFA_BACKGROUND_LIMIT);
                if (CFA_BACKGROUND_LIMIT > 10)
                {
                    Log(LOG_LEVEL_ERR, "Silly value for max_children in agent control promise (%d > 10)",
                          CFA_BACKGROUND_LIMIT);
                    CFA_BACKGROUND_LIMIT = 1;
                }
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_ENVIRONMENT].lval) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Setting environment variables from ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (putenv(RlistScalarValue(rp)) != 0)
                    {
                        Log(LOG_LEVEL_ERR, "Failed to set environment variable '%s'. (putenv: %s)",
                            RlistScalarValue(rp), GetErrorStr());
                    }
                }

                continue;
            }
        }
    }

    const void *value = NULL;
    if ((value = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_LASTSEEN_EXPIRE_AFTER)))
    {
        LASTSEENEXPIREAFTER = IntFromString(value) * 60;
    }

    if ((value = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_FIPS_MODE)))
    {
        FIPS_MODE = BooleanFromString(value);
        Log(LOG_LEVEL_VERBOSE, "Setting FIPS mode to '%s'", FIPS_MODE ? "true" : "false");
    }

    if ((value = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_SYSLOG_PORT)))
    {
        SetSyslogPort(IntFromString(value));
        Log(LOG_LEVEL_VERBOSE, "Setting syslog_port to '%s'", (const char *)value);
    }

    if ((value = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_SYSLOG_HOST)))
    {
        /* Don't resolve syslog_host now, better do it per log request. */
        if (!SetSyslogHost(value))
        {
            Log(LOG_LEVEL_ERR,
                  "FAILed to set syslog_host, '%s' too long", (const char *)value);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Setting syslog_host to '%s'", (const char *)value);
        }
    }

    Nova_Initialize(ctx);
}

/*********************************************************************/

static void KeepPromiseBundles(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
{
    const Rlist *bundlesequence = NULL;
    if (config->bundlesequence)
    {
        Log(LOG_LEVEL_INFO, "Using command line specified bundlesequence");
        bundlesequence = config->bundlesequence;
    }
    else if (!(bundlesequence = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_BUNDLESEQUENCE)))
    {
        Log(LOG_LEVEL_ERR, "No bundlesequence in the common control body");
        exit(EXIT_FAILURE);
    }

    bool ok = true;
    for (const Rlist *rp = bundlesequence; rp; rp = rp->next)
    {
        const char *name = NULL;

        switch (rp->val.type)
        {
        case RVAL_TYPE_SCALAR:
            name = RlistScalarValue(rp);
            if (strcmp(name, CF_NULL_VALUE) == 0)
            {
                continue;
            }

            break;
        case RVAL_TYPE_FNCALL:
            name = RlistFnCallValue(rp)->name;
            break;

        default:
            name = NULL;
            {
                Writer *w = StringWriter();
                WriterWrite(w, "Illegal item found in bundlesequence: ");
                RvalWrite(w, rp->val);
                Log(LOG_LEVEL_ERR, "%s", StringWriterData(w));
                WriterClose(w);
            }
            ok = false;
            break;
        }

        if (!config->ignore_missing_bundles)
        {
            if (!(PolicyGetBundle(policy, NULL, "agent", name) || (PolicyGetBundle(policy, NULL, "common", name))))
            {
                Log(LOG_LEVEL_ERR, "Bundle '%s' listed in the bundlesequence was not found", name);
                ok = false;
            }
        }
    }

    if (!ok)
    {
        FatalError(ctx, "Errors in agent bundles");
    }

    if (LEGACY_OUTPUT)
    {
        Writer *w = StringWriter();
        RlistWrite(w, bundlesequence);
        Log(LOG_LEVEL_VERBOSE, " -> Bundlesequence => %s", StringWriterData(w));
        WriterClose(w);
    }
    else
    {
        Writer *w = StringWriter();
        WriterWrite(w, "Using bundlesequence => ");
        RlistWrite(w, bundlesequence);
        Log(LOG_LEVEL_VERBOSE, "%s", StringWriterData(w));
        WriterClose(w);
    }

/* If all is okay, go ahead and evaluate */

    for (const Rlist *rp = bundlesequence; rp; rp = rp->next)
    {
        const char *name = NULL;
        const Rlist *args = NULL;

        switch (rp->val.type)
        {
        case RVAL_TYPE_FNCALL:
            name = RlistFnCallValue(rp)->name;
            args = RlistFnCallValue(rp)->args;
            break;
        default:
            name = RlistScalarValue(rp);
            args = NULL;
            break;
        }

        const Bundle *bp = NULL;
        if ((bp = PolicyGetBundle(policy, NULL, "agent", name)) || (bp = PolicyGetBundle(policy, NULL, "common", name)))
        {
            BannerBundle(bp, args);

            EvalContextStackPushBundleFrame(ctx, bp, args, false);

            ScheduleAgentOperations(ctx, bp);

            EvalContextStackPopFrame(ctx);
        }
    }
}

static void AllClassesReport(const EvalContext *ctx)
{
    char context_report_file[CF_BUFSIZE];
    snprintf(context_report_file, CF_BUFSIZE, "%s/state/allclasses.txt", CFWORKDIR);

    FILE *fp = NULL;
    if ((fp = fopen(context_report_file, "w")) == NULL)
    {
        Log(LOG_LEVEL_INFO, "Could not open allclasses cache file");
    }
    else
    {
        Writer *writer = FileWriter(fp);
        ClassTableIterator *iter = EvalContextClassTableIteratorNewGlobal(ctx, NULL, true, true);
        Class *cls = NULL;
        while ((cls = ClassTableIteratorNext(iter)))
        {
            char *expr = ClassRefToString(cls->ns, cls->name);
            WriterWriteF(writer, "%s\n", expr);
            free(expr);
        }
        ClassTableIteratorDestroy(iter);
        WriterClose(writer);
    }
}

PromiseResult ScheduleAgentOperations(EvalContext *ctx, const Bundle *bp)
// NB - this function can be called recursively through "methods"
{
    int save_pr_kept = PR_KEPT;
    int save_pr_repaired = PR_REPAIRED;
    int save_pr_notkept = PR_NOTKEPT;

    if (PROCESSREFRESH == NULL || (PROCESSREFRESH && IsRegexItemIn(ctx, PROCESSREFRESH, bp->name)))
    {
        DeleteItemList(PROCESSTABLE);
        PROCESSTABLE = NULL;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    Log(LOG_LEVEL_VERBOSE, "/%s/%s: Evaluating bundle", bp->ns, bp->name);

    for (int pass = 1; pass < CF_DONEPASSES; pass++)
    {
        for (TypeSequence type = 0; AGENT_TYPESEQUENCE[type] != NULL; type++)
        {
            ClassBanner(ctx, type);

            const PromiseType *sp = BundleGetPromiseType((Bundle *)bp, AGENT_TYPESEQUENCE[type]);

            if (sp == NULL)
            {
                continue;
            }

            Log(LOG_LEVEL_VERBOSE, "/%s/%s/%s: Evaluating pass %d", bp->ns, bp->name, sp->name, pass);
            BannerPromiseType(bp->name, sp->name, pass);

            if (!NewTypeContext(type))
            {
                continue;
            }

            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                Promise *pp = SeqAt(sp->promises, ppi);

                PromiseResult promise_result = ExpandPromise(ctx, pass, pp, KeepAgentPromise, NULL);
                result = PromiseResultUpdate(result, promise_result);

                if (Abort(ctx))
                {
                    DeleteTypeContext(ctx, bp, type);
                    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept);
                    return result;
                }
            }

            DeleteTypeContext(ctx, bp, type);
        }
    }

    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept);
    return result;
}

/*********************************************************************/

#ifdef __MINGW32__

static void CheckAgentAccess(const Rlist *list, const Policy *policy)
{
}

#else

static void CheckAgentAccess(const Rlist *list, const Policy *policy)
{
    uid_t uid = getuid();

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (Str2Uid(RlistScalarValue(rp), NULL, NULL) == uid)
        {
            return;
        }
    }

    {
        StringSet *input_files = PolicySourceFiles(policy);
        StringSetIterator iter = StringSetIteratorInit(input_files);
        const char *input_file = NULL;
        while ((input_file = StringSetIteratorNext(&iter)))
        {
            struct stat sb;
            stat(input_file, &sb);

            if (ACCESSLIST)
            {
                bool access = false;
                for (const Rlist *rp2 = ACCESSLIST; rp2 != NULL; rp2 = rp2->next)
                {
                    if (Str2Uid(RlistScalarValue(rp2), NULL, NULL) == sb.st_uid)
                    {
                        access = true;
                        break;
                    }
                }

                if (!access)
                {
                    Log(LOG_LEVEL_ERR, "File '%s' is not owned by an authorized user (security exception)", input_file);
                    exit(EXIT_FAILURE);
                }
            }
            else if (CFPARANOID && IsPrivileged())
            {
                if (sb.st_uid != getuid())
                {
                    Log(LOG_LEVEL_ERR, "File '%s' is not owned by uid %ju (security exception)", input_file,
                          (uintmax_t)getuid());
                    exit(EXIT_FAILURE);
                }
            }
        }

        StringSetDestroy(input_files);
    }

    Log(LOG_LEVEL_ERR, "You are denied access to run this policy");
    exit(EXIT_FAILURE);
}
#endif /* !__MINGW32__ */

/*********************************************************************/

/**************************************************************/

static PromiseResult DefaultVarPromise(EvalContext *ctx, const Promise *pp)
{
    char *regex = PromiseGetConstraintAsRval(pp, "if_match_regex", RVAL_TYPE_SCALAR);
    bool okay = true;


    DataType value_type = DATA_TYPE_NONE;
    const void *value = NULL;
    {
        VarRef *ref = VarRefParseFromScope(pp->promiser, "this");
        value = EvalContextVariableGet(ctx, ref, &value_type);
        VarRefDestroy(ref);
    }

    switch (value_type)
    {
    case DATA_TYPE_STRING:
    case DATA_TYPE_INT:
    case DATA_TYPE_REAL:
        if (regex && !FullTextMatch(ctx, regex, value))
        {
            return PROMISE_RESULT_NOOP;
        }

        if (regex == NULL)
        {
            return PROMISE_RESULT_NOOP;
        }
        break;

    case DATA_TYPE_STRING_LIST:
    case DATA_TYPE_INT_LIST:
    case DATA_TYPE_REAL_LIST:
        if (regex)
        {
            for (const Rlist *rp = value; rp != NULL; rp = rp->next)
            {
                if (FullTextMatch(ctx, regex, RlistScalarValue(rp)))
                {
                    okay = false;
                    break;
                }
            }

            if (okay)
            {
                return PROMISE_RESULT_NOOP;
            }
        }
        break;

    default:
        break;
    }

    {
        VarRef *ref = VarRefParseFromBundle(pp->promiser, PromiseGetBundle(pp));
        EvalContextVariableRemove(ctx, ref);
        VarRefDestroy(ref);
    }

    return VerifyVarPromise(ctx, pp, true);
}

static PromiseResult KeepAgentPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(param == NULL);

    char *sp = NULL;
    struct timespec start = BeginMeasure();

    if (!IsDefinedClass(ctx, pp->classes))
    {
        if (LEGACY_OUTPUT)
        {
            Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
            Log(LOG_LEVEL_VERBOSE, "Skipping whole next promise (%s), as context %s is not relevant", pp->promiser,
                  pp->classes);
            Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping next promise '%s', as context '%s' is not relevant", pp->promiser, pp->classes);
        }
        return PROMISE_RESULT_SKIPPED;
    }

    if (EvalContextPromiseIsDone(ctx, pp))
    {
        return PROMISE_RESULT_SKIPPED;
    }

    if (VarClassExcluded(ctx, pp, &sp))
    {
        if (LEGACY_OUTPUT)
        {
            Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
            Log(LOG_LEVEL_VERBOSE, "Skipping whole next promise (%s), as var-context %s is not relevant", pp->promiser, sp ? sp : pp->classes);
            Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping next promise '%s', as var-context '%s' is not relevant", pp->promiser, sp ? sp : pp->classes);    
        }
        return PROMISE_RESULT_SKIPPED;
    }


    if (MissingDependencies(ctx, pp))
    {
        return PROMISE_RESULT_SKIPPED;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;

    if (strcmp("meta", pp->parent_promise_type->name) == 0 || strcmp("vars", pp->parent_promise_type->name) == 0)
    {
        result = VerifyVarPromise(ctx, pp, true);
    }
    else if (strcmp("defaults", pp->parent_promise_type->name) == 0)
    {
        result = DefaultVarPromise(ctx, pp);
    }
    else if (strcmp("classes", pp->parent_promise_type->name) == 0)
    {
        result = VerifyClassPromise(ctx, pp, NULL);
    }
    else if (strcmp("processes", pp->parent_promise_type->name) == 0)
    {
        result = VerifyProcessesPromise(ctx, pp);
    }
    else if (strcmp("storage", pp->parent_promise_type->name) == 0)
    {
        result = FindAndVerifyStoragePromises(ctx, pp);
        EndMeasurePromise(start, pp);
    }
    else if (strcmp("packages", pp->parent_promise_type->name) == 0)
    {
        result = VerifyPackagesPromise(ctx, pp);
        EndMeasurePromise(start, pp);
    }
    else if (strcmp("users", pp->parent_promise_type->name) == 0)
    {
        result = VerifyUsersPromise(ctx, pp);
        EndMeasurePromise(start, pp);
    }

    else if (strcmp("files", pp->parent_promise_type->name) == 0)
    {
        result = ParallelFindAndVerifyFilesPromises(ctx, pp);
        EndMeasurePromise(start, pp);
    }
    else if (strcmp("commands", pp->parent_promise_type->name) == 0)
    {
        result = VerifyExecPromise(ctx, pp);
        EndMeasurePromise(start, pp);
    }
    else if (strcmp("databases", pp->parent_promise_type->name) == 0)
    {
        result = VerifyDatabasePromises(ctx, pp);
        EndMeasurePromise(start, pp);
    }
    else if (strcmp("methods", pp->parent_promise_type->name) == 0)
    {
        result = VerifyMethodsPromise(ctx, pp);
        EndMeasurePromise(start, pp);
    }
    else if (strcmp("services", pp->parent_promise_type->name) == 0)
    {
        result = VerifyServicesPromise(ctx, pp);
        EndMeasurePromise(start, pp);
    }
    else if (strcmp("guest_environments", pp->parent_promise_type->name) == 0)
    {
        result = VerifyEnvironmentsPromise(ctx, pp);
        EndMeasurePromise(start, pp);
    }
    else if (strcmp("reports", pp->parent_promise_type->name) == 0)
    {
        result = VerifyReportPromise(ctx, pp);
    }
    else
    {
        result = PROMISE_RESULT_NOOP;
    }

    EvalContextLogPromiseIterationOutcome(ctx, pp, result);

    return result;
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
            Log(LOG_LEVEL_ERR, "Unable to read the process table - cannot keep process promises");
            return false;
        }
        break;

    case TYPE_SEQUENCE_STORAGE:

#ifndef __MINGW32__                   // TODO: Run if implemented on Windows
        if (SeqLength(GetGlobalMountedFSList()))
        {
            DeleteMountInfo(GetGlobalMountedFSList());
            SeqClear(GetGlobalMountedFSList());
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

static void DeleteTypeContext(EvalContext *ctx, const Bundle *bp, TypeSequence type)
{
    switch (type)
    {
    case TYPE_SEQUENCE_CONTEXTS:
        BundleResolve(ctx, bp);
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
        DeleteStorageContext();
        break;

    case TYPE_SEQUENCE_PACKAGES:

        ExecuteScheduledPackages(ctx);

        CleanScheduledPackages();
        break;

    default:

        /* Deinitialization is not required */
        ;

    }
}

/**************************************************************/

static void ClassBanner(EvalContext *ctx, TypeSequence type)
{
    if (type != TYPE_SEQUENCE_INTERFACES)  /* Just parsed all local classes */
    {
        return;
    }

    if (LEGACY_OUTPUT)
    {
        Log(LOG_LEVEL_VERBOSE, "     +  Private classes augmented:");

        ClassTableIterator *iter = EvalContextClassTableIteratorNewLocal(ctx);
        Class *cls = NULL;
        while ((cls = ClassTableIteratorNext(iter)))
        {
            Log(LOG_LEVEL_VERBOSE, "     +       %s", cls->name);
        }
        ClassTableIteratorDestroy(iter);
    }
    else
    {
        bool have_classes = false;
        Writer *w = StringWriter();

        WriterWrite(w, "Private classes augmented:");
        ClassTableIterator *iter = EvalContextClassTableIteratorNewLocal(ctx);
        Class *cls = NULL;
        while ((cls = ClassTableIteratorNext(iter)))
        {
            WriterWriteChar(w, ' ');
            WriterWrite(w, cls->name);
            have_classes = true;
        }
        ClassTableIteratorDestroy(iter);

        if (have_classes)
        {
            Log(LOG_LEVEL_VERBOSE, "%s", StringWriterData(w));
        }

        WriterClose(w);
    }
}

/**************************************************************/
/* Thread context                                             */
/**************************************************************/

#ifdef __MINGW32__

static PromiseResult ParallelFindAndVerifyFilesPromises(EvalContext *ctx, const Promise *pp)
{
    int background = PromiseGetConstraintAsBoolean(ctx, "background", pp);

    if (background)
    {
        Log(LOG_LEVEL_VERBOSE, "Background processing of files promises is not supported on Windows");
    }

    return FindAndVerifyFilesPromises(ctx, pp);
}

#else /* !__MINGW32__ */

static PromiseResult ParallelFindAndVerifyFilesPromises(EvalContext *ctx, const Promise *pp)
{
    int background = PromiseGetConstraintAsBoolean(ctx, "background", pp);
    pid_t child = 1;
    PromiseResult result = PROMISE_RESULT_NOOP;

    if (background)
    {
        if (CFA_BACKGROUND < CFA_BACKGROUND_LIMIT)
        {
            CFA_BACKGROUND++;
            Log(LOG_LEVEL_VERBOSE, "Spawning new process...");
            child = fork();

            if (child == 0)
            {
                ALARM_PID = -1;

                result = PromiseResultUpdate(result, FindAndVerifyFilesPromises(ctx, pp));

                Log(LOG_LEVEL_VERBOSE, "Exiting backgrounded promise");
                PromiseRef(LOG_LEVEL_VERBOSE, pp);
                _exit(EXIT_SUCCESS);
                // TODO: need to solve this
            }
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Promised parallel execution promised but exceeded the max number of promised background tasks, so serializing");
            background = 0;
        }
    }
    else
    {
        result = PromiseResultUpdate(result, FindAndVerifyFilesPromises(ctx, pp));
    }

    return result;
}

#endif /* !__MINGW32__ */

/**************************************************************/

static bool VerifyBootstrap(void)
{
    if (NULL_OR_EMPTY(POLICY_SERVER))
    {
        Log(LOG_LEVEL_ERR, "Bootstrapping failed, no policy server is specified");
        return false;
    }

    // we should at least have gotten promises.cf from the policy hub
    {
        char filename[CF_MAXVARSIZE];
        snprintf(filename, sizeof(filename), "%s/promises.cf", GetInputDir());
        MapName(filename);

        struct stat sb;
        if (stat(filename, &sb) == -1)
        {
            Log(LOG_LEVEL_ERR, "Bootstrapping failed, no input file at '%s' after bootstrap", filename);
            return false;
        }
    }

    // embedded failsafe.cf (bootstrap.c) contains a promise to start cf-execd (executed while running this cf-agent)
    DeleteItemList(PROCESSTABLE);
    PROCESSTABLE = NULL;
    LoadProcessTable(&PROCESSTABLE);

    if (!IsProcessNameRunning(".*cf-execd.*"))
    {
        Log(LOG_LEVEL_ERR, "Bootstrapping failed, cf-execd is not running");
        return false;
    }

    Log(LOG_LEVEL_NOTICE, "Bootstrap to '%s' completed successfully!", POLICY_SERVER);
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
        Log(LOG_LEVEL_VERBOSE, "Zero promises executed for bundle '%s'", bundle->name);
        return PROMISE_RESULT_NOOP;
    }

    Log(LOG_LEVEL_VERBOSE, "Bundle Accounting Summary for '%s'", bundle->name);
    Log(LOG_LEVEL_VERBOSE, "Promises kept in '%s' = %.0lf", bundle->name, delta_pr_kept);
    Log(LOG_LEVEL_VERBOSE, "Promises not kept in '%s' = %.0lf", bundle->name, delta_pr_notkept);
    Log(LOG_LEVEL_VERBOSE, "Promises repaired in '%s' = %.0lf", bundle->name, delta_pr_repaired);
    
    bundle_compliance = (delta_pr_kept + delta_pr_repaired) / (delta_pr_kept + delta_pr_notkept + delta_pr_repaired);

    Log(LOG_LEVEL_VERBOSE, "Aggregate compliance (promises kept/repaired) for bundle '%s' = %.1lf%%",
          bundle->name, bundle_compliance * 100.0);
    LastSawBundle(bundle, bundle_compliance);

    // return the worst case for the bundle status
    
    if (delta_pr_notkept > 0)
    {
        return PROMISE_RESULT_FAIL;
    }

    if (delta_pr_repaired > 0)
    {
        return PROMISE_RESULT_CHANGE;
    }

    return PROMISE_RESULT_NOOP;
}

#if defined(HAVE_AVAHI_CLIENT_CLIENT_H) && defined(HAVE_AVAHI_COMMON_ADDRESS_H)

static bool HasAvahiSupport(void)
{
    return true;
}


static int AutomaticBootstrap(GenericAgentConfig *config)
{
    List *foundhubs = NULL;
    int hubcount = ListHubs(&foundhubs);
    int ret;

    switch(hubcount)
    {
    case -1:
        Log(LOG_LEVEL_ERR, "Error while trying to find a Policy Server");
        ret = -1;
        break;
    case 0:
        Log(LOG_LEVEL_ERR, "No hubs were found. Exiting.");
        ret = -1;
        break;
    case 1:
    {
        char *hostname = ((HostProperties*)foundhubs)->Hostname;
        char *ipaddr = ((HostProperties*)foundhubs)->IPAddress;
        Log(LOG_LEVEL_NOTICE, "Autodiscovered hub installed on hostname '%s', IP address '%s'",
            hostname, ipaddr);

        if (strlen(ipaddr) < sizeof(POLICY_SERVER))
        {
            config->agent_specific.agent.bootstrap_policy_server = xstrdup(ipaddr);
            ret = 0;
        }
        else
        {
            Log(LOG_LEVEL_ERR,  "Invalid autodiscovered hub IP address '%s'", ipaddr);
            ret = -1;
        }
        break;
    }
    default:
        Log(LOG_LEVEL_ERR, "Found more than one hub registered in the network. Please bootstrap manually using IP from the list below.");
        PrintList(foundhubs);
        ret = -1;
    };

	if (avahi_handle)
	{
		/*
		 * This case happens when dlopen does not manage to open the library.
		 */
    	dlclose(avahi_handle);
	}
    ListDestroy(&foundhubs);

    return ret;
}
#else

static bool HasAvahiSupport(void)
{
    return false;
}

static int AutomaticBootstrap(ARG_UNUSED GenericAgentConfig *config)
{
    ProgrammingError("Attempted automated bootstrap on a non-avahi build of CFEngine");
}

#endif // Avahi
