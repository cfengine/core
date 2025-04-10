/*
  Copyright 2024 Northern.tech AS

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


#include <platform.h>
#include <getopt.h>
#include <generic_agent.h>

#include <actuator.h>
#include <audit.h>
#include <cleanup.h>
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
#include <policy_server.h>
#include <misc_lib.h>
#include <buffer.h>
#include <loading.h>
#include <conn_cache.h>                 /* ConnCache_Init,ConnCache_Destroy */
#include <net.h>
#include <package_module.h>
#include <string_lib.h>
#include <cfnet.h>
#include <repair.h>
#include <dbm_api.h>                    /* CheckDBRepairFlagFile() */
#include <sys/types.h>                  /* checking umask on writing setxid log */
#include <sys/stat.h>                   /* checking umask on writing setxid log */
#include <simulate_mode.h>              /* ManifestChangedFiles(), DiffChangedFiles() */
#include <ip_address.h>

#include <syntax.h>                     /* IsBuiltInPromiseType() */
#include <mod_common.h>
#include <mod_custom.h>                 /* EvaluateCustomPromise(), Intialize/FinalizeCustomPromises() */

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
static bool PERFORM_DB_CHECK = false;

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
static void FreeFixedStringArray(int size, char **array);
static void CheckAgentAccess(const Rlist *list, const Policy *policy);
static void KeepControlPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config);
static PromiseResult KeepAgentPromise(EvalContext *ctx, const Promise *pp, void *param);
static void NewTypeContext(TypeSequence type);
static void DeleteTypeContext(EvalContext *ctx, TypeSequence type);
static PromiseResult ParallelFindAndVerifyFilesPromises(EvalContext *ctx, const Promise *pp);
static bool VerifyBootstrap(bool skip_cf_execd_check);
static void KeepPromiseBundles(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config);
static void KeepPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config);
static int NoteBundleCompliance(const Bundle *bundle, int save_pr_kept, int save_pr_repaired, int save_pr_notkept, struct timespec start);
static void AllClassesReport(const EvalContext *ctx);
static bool HasAvahiSupport(void);
static int AutomaticBootstrap(GenericAgentConfig *config);
static void BannerStatusEnd(PromiseResult status, const char *type, char *name);
static void BannerStatusBegin(const char *type, char *name);
static PromiseResult DefaultVarPromise(EvalContext *ctx, const Promise *pp);
static void WaitForBackgroundProcesses();

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

static const Component COMPONENT =
{
    .name = "cf-agent",
    .website = CF_WEBSITE,
    .copyright = CF_COPYRIGHT
};

static const struct option OPTIONS[] =
{
    {"bootstrap", required_argument, 0, 'B'},
    {"bundlesequence", required_argument, 0, 'b'},
    {"workdir", required_argument, 0, 'w'},
    {"debug", no_argument, 0, 'd'},
    {"define", required_argument, 0, 'D'},
    {"self-diagnostics", optional_argument, 0, 'x'},
    {"dry-run", no_argument, 0, 'n'},
    {"file", required_argument, 0, 'f'},
    {"help", no_argument, 0, 'h'},
    {"inform", no_argument, 0, 'I'},
    {"log-level", required_argument, 0, 'g'},
    {"negate", required_argument, 0, 'N'},
    {"no-lock", no_argument, 0, 'K'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"timing-output", no_argument, 0, 't'},
    {"trust-server", optional_argument, 0, 'T'},
    {"color", optional_argument, 0, 'C'},
    {"no-extensions", no_argument, 0, 'E'},
    {"timestamp", no_argument, 0, 'l'},
    /* Only long option for the rest */
    {"ignore-preferred-augments", no_argument, 0, 0},
    {"log-modules", required_argument, 0, 0},
    {"no-augments", no_argument, 0, 0},
    {"no-host-specific-data", no_argument, 0, 0},
    {"show-evaluated-classes", optional_argument, 0, 0 },
    {"show-evaluated-vars", optional_argument, 0, 0 },
    {"skip-bootstrap-policy-run", no_argument, 0, 0 },
    {"skip-bootstrap-service-start", no_argument, 0, 0 },
    {"skip-db-check", optional_argument, 0, 0 },
    {"simulate", required_argument, 0, 0},
    {NULL, 0, 0, '\0'}
};

static const char *const HINTS[] =
{
    "Bootstrap CFEngine to the given policy server IP, hostname or :avahi (automatic detection)",
    "Set or override bundlesequence from command line",
    "Override the default /var/cfengine work directory for testing (same as setting CFENGINE_TEST_OVERRIDE_WORKDIR)",
    "Enable debugging output",
    "Define a list of comma separated classes to be defined at the start of execution",
    "Run checks to diagnose a CFEngine agent installation",
    "All talk and no action mode - make no changes, only inform of promises not kept",
    "Specify an alternative input file than the default. This option is overridden by FILE if supplied as argument.",
    "Print the help message",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Specify how detailed logs should be. Possible values: 'error', 'warning', 'notice', 'info', 'verbose', 'debug'",
    "Define a list of comma separated classes to be undefined at the start of execution",
    "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    "Output timing information on console when in verbose mode",
    "Possible values: 'yes' (default, trust the server when bootstrapping), 'no' (server key must already be trusted)",
    "Enable colorized output. Possible values: 'always', 'auto', 'never'. If option is used, the default value is 'auto'",
    "Disable extension loading (used while upgrading)",
    "Log timestamps on each line of log output",
    "Ignore def_preferred.json file in favor of def.json",
    "Enable even more detailed debug logging for specific areas of the implementation. Use together with '-d'. Use --log-modules=help for a list of available modules",
    "Do not load augments (def.json)",
    "Do not load host-specific data (host_specific.json)",
    "Show *final* evaluated classes, including those defined in common bundles in policy. Optionally can take a regular expression.",
    "Show *final* evaluated variables, including those defined without dependency to user-defined classes in policy. Optionally can take a regular expression.",
    "Do not run policy as the last step of the bootstrap process",
    "Do not start CFEngine services as part of the bootstrap process",
    "Do not run database integrity checks and repairs at startup",
    "Run in simulate mode, either 'manifest', 'manifest-full' or 'diff'",
    NULL
};

/**
 @brief
 Wrapper around DefaultVarPromise to silence cast-function-type compiler warning in ScheduleAgentOperations
 */
static PromiseResult DefaultVarPromiseWrapper(EvalContext *ctx, const Promise *pp, void *param) {
    UNUSED(param);
    return DefaultVarPromise(ctx, pp);
}

/*******************************************************************/

int main(int argc, char *argv[])
{
    SetupSignalsForAgent();
#ifdef HAVE_LIBXML2
        xmlInitParser();
#endif
    struct timespec start = BeginMeasure();

    GenericAgentConfig *config = CheckOpts(argc, argv);
    bool force_repair = CheckDBRepairFlagFile();
    if (force_repair || PERFORM_DB_CHECK)
    {
        repair_lmdb_default(force_repair);
    }
    EvalContext *ctx = EvalContextNew();

    // Enable only for cf-agent eval context.
    EvalContextAllClassesLoggingEnable(ctx, true);

    GenericAgentConfigApply(ctx, config);

    const char *program_invocation_name = argv[0];
    const char *last_dir_sep = strrchr(program_invocation_name, FILE_SEPARATOR);
    const char *program_name = (last_dir_sep != NULL ? last_dir_sep + 1 : program_invocation_name);
    GenericAgentDiscoverContext(ctx, config, program_name);

    /* FIXME: (CFE-2709) ALWAYS_VALIDATE will always be false here, since it can
     *        only change in KeepPromises(), five lines later on. */
    Policy *policy = SelectAndLoadPolicy(config, ctx, ALWAYS_VALIDATE, true);

    if (!policy)
    {
        Log(LOG_LEVEL_ERR, "Error reading CFEngine policy. Exiting...");
        DoCleanupAndExit(EXIT_FAILURE);
    }

    if ((config->agent_specific.agent.bootstrap_argument != NULL) &&
        config->agent_specific.agent.skip_bootstrap_service_start &&
        !EvalContextClassPutHard(ctx, "bootstrap_skip_services", "source=environment"))
    {
        Log(LOG_LEVEL_ERR, "Failed to define the 'bootstrap_skip_services' class");
        /* not a fatal issue, let's continue the bootstrap process */
    }

    int ret = 0;

    GenericAgentPostLoadInit(ctx);
    ThisAgentInit();
    ConnCache_Init();

    BeginAudit();

    KeepPromises(ctx, policy, config);

    if (EvalAborted(ctx))
    {
        ret = EC_EVAL_ABORTED;
    }

    ConnCache_Destroy();

    if (ALLCLASSESREPORT)
    {
        AllClassesReport(ctx);
    }

    Nova_TrackExecution(config->input_file);

    /* Update packages cache. */
    UpdatePackagesCache(ctx, false);

    /* Finalize custom promises before waiting for background processes because
     * they can be background processes and need special handling. */
    FinalizeCustomPromises();

    /* Wait for background processes before generating reports because
     * GenerateReports() does nothing if it detects multiple cf-agent processes
     * running. */
    WaitForBackgroundProcesses();

    GenerateReports(config, ctx);

    PurgeLocks();
    BackupLockDatabase();

    if (config->agent_specific.agent.show_evaluated_classes != NULL)
    {
        GenericAgentShowContextsFormatted(ctx, config->agent_specific.agent.show_evaluated_classes);
        free(config->agent_specific.agent.show_evaluated_classes);
    }

    if (config->agent_specific.agent.show_evaluated_variables != NULL)
    {
        GenericAgentShowVariablesFormatted(ctx, config->agent_specific.agent.show_evaluated_variables);
        free(config->agent_specific.agent.show_evaluated_variables);
    }

    PolicyDestroy(policy); /* Can we safely do this earlier ? */
    if (config->agent_specific.agent.bootstrap_argument &&
        !VerifyBootstrap(config->agent_specific.agent.skip_bootstrap_service_start))
    {
        PolicyServerRemoveFile(GetWorkDir());
        WriteAmPolicyHubFile(false);
        ret = 1;
    }

    EndAudit(ctx, CFA_BACKGROUND);

    Nova_NoteAgentExecutionPerformance(config->input_file, start);

    GenericAgentFinalize(ctx, config);

    StringSetDestroy(SINGLE_COPY_CACHE);

    StringSet *audited_files = NULL;
    if ((EVAL_MODE == EVAL_MODE_SIMULATE_MANIFEST) ||
        (EVAL_MODE == EVAL_MODE_SIMULATE_MANIFEST_FULL))
    {
        bool success = ManifestChangedFiles(&audited_files);
        if (!success)
        {
            Log(LOG_LEVEL_ERR, "Failed to manifest changed files");
        }
        if (EVAL_MODE == EVAL_MODE_SIMULATE_MANIFEST_FULL)
        {
            /* Skips the files already manifested above. */
            success = ManifestAllFiles(&audited_files);
            if (!success)
            {
                Log(LOG_LEVEL_ERR, "Failed to manifest unmodified files");
            }
        }
        success = ManifestPkgOperations();
        if (!success)
        {
            Log(LOG_LEVEL_ERR, "Failed to manifest present and absent packages");
        }
    }
    else if (EVAL_MODE == EVAL_MODE_SIMULATE_DIFF)
    {
        bool success = DiffChangedFiles(&audited_files);
        if (!success)
        {
            Log(LOG_LEVEL_ERR, "Failed to show differences for changed files");
        }
        success = DiffPkgOperations();
        if (!success)
        {
            Log(LOG_LEVEL_ERR, "Failed to show differences in installed packages");
        }
    }
    StringSetDestroy(audited_files);

#ifdef HAVE_LIBXML2
        xmlCleanupParser();
#endif

    CallCleanupFunctions();

    return ret;
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

static void ConfigureBootstrap(GenericAgentConfig *config, const char *argument)
{
    assert(config != NULL);
    if (!BootstrapAllowed())
    {
        Log(LOG_LEVEL_ERR, "Not enough privileges to bootstrap CFEngine");
        DoCleanupAndExit(EXIT_FAILURE);
    }

    if(strcmp(optarg, ":avahi") == 0)
    {
        if(!HasAvahiSupport())
        {
            Log(LOG_LEVEL_ERR, "Avahi support is not built in, please see options to the configure script and rebuild CFEngine");
            DoCleanupAndExit(EXIT_FAILURE);
        }

        int err = AutomaticBootstrap(config);
        if (err < 0)
        {
            Log(LOG_LEVEL_ERR, "Automatic bootstrap failed, error code '%d'", err);
            DoCleanupAndExit(EXIT_FAILURE);
        }
        return;
    }

    if(StringEqual(argument, "localhost") || StringIsLocalHostIP(argument))
    {
        Log(LOG_LEVEL_WARNING, "Bootstrapping to loopback interface (localhost), other hosts will not be able to bootstrap to this server");
    }

    // temporary assure that network functions are working
    OpenNetwork();

    config->agent_specific.agent.bootstrap_argument = xstrdup(argument);

    char *host, *port;
    ParseHostPort(optarg, &host, &port);

    char ipaddr[CF_MAX_IP_LEN] = "";
    if (Hostname2IPString(ipaddr, host,sizeof(ipaddr)) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Could not resolve hostname '%s', unable to bootstrap",
            host);
        DoCleanupAndExit(EXIT_FAILURE);
    }

    CloseNetwork();

    MINUSF = true;
    config->ignore_locks = true;
    GenericAgentConfigSetInputFile(config, GetInputDir(), "promises.cf");

    config->agent_specific.agent.bootstrap_ip = xstrdup(ipaddr);
    config->agent_specific.agent.bootstrap_host = xstrdup(host);

    if (port == NULL)
    {
        config->agent_specific.agent.bootstrap_port = NULL;
    }
    else
    {
        config->agent_specific.agent.bootstrap_port = xstrdup(port);
    }
}

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int c;

    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_AGENT, GetTTYInteractive());
    bool option_trust_server = false;
;
/* DEPRECATED:
   --policy-server (-s) is deprecated in community version 3.5.0.
   Support rewrite from some common old bootstrap options (until community version 3.6.0?).
 */

    int argc_new = argc;
    char **argv_tmp = TranslateOldBootstrapOptionsSeparate(&argc_new, argv);
    char **argv_new = TranslateOldBootstrapOptionsConcatenated(argc_new, argv_tmp);
    FreeFixedStringArray(argc_new, argv_tmp);

    int longopt_idx;
    while ((c = getopt_long(argc_new, argv_new, "tdvnKIf:g:w:D:N:VxMB:b:hC::ElT::",
                            OPTIONS, &longopt_idx))
           != -1)
    {
        switch (c)
        {
        case 't':
            TIMING = true;
            break;

        case 'w':
            Log(LOG_LEVEL_INFO, "Setting workdir to '%s'", optarg);
            setenv_wrapper("CFENGINE_TEST_OVERRIDE_WORKDIR", optarg, 1);
            break;

        case 'f':
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
                ConfigureBootstrap(config, optarg);
            }
            break;

        case 'K':
            config->ignore_locks = true;
            break;

        case 'D':
            {
                StringSet *defined_classes = StringSetFromString(optarg, ',');
                if (! config->heap_soft)
                {
                    config->heap_soft = defined_classes;
                }
                else
                {
                    StringSetJoin(config->heap_soft, defined_classes, xstrdup);
                    StringSetDestroy(defined_classes);
                }
            }
            break;

        case 'N':
            {
                StringSet *negated_classes = StringSetFromString(optarg, ',');
                if (! config->heap_negated)
                {
                    config->heap_negated = negated_classes;
                }
                else
                {
                    StringSetJoin(config->heap_negated, negated_classes, xstrdup);
                    StringSetDestroy(negated_classes);
                }
            }
            break;

        case 'I':
            LogSetGlobalLevel(LOG_LEVEL_INFO);
            break;

        case 'v':
            LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
            break;

        case 'g':
            LogSetGlobalLevelArgOrExit(optarg);
            break;

        case 'n':
            EVAL_MODE = EVAL_MODE_DRY_RUN;
            config->ignore_locks = true;
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
                WriterWriteHelp(w, &COMPONENT, OPTIONS, HINTS, NULL, false, true);
                FileWriterDetach(w);
            }
            DoCleanupAndExit(EXIT_SUCCESS);

        case 'M':
            {
                Writer *out = FileWriter(stdout);
                ManPageWrite(out, "cf-agent", time(NULL),
                             CF_AGENT_SHORT_DESCRIPTION,
                             CF_AGENT_MANPAGE_LONG_DESCRIPTION,
                             OPTIONS, HINTS,
                             NULL, false,
                             true);
                FileWriterDetach(out);
                DoCleanupAndExit(EXIT_SUCCESS);
            }

        case 'x':
            {
                const char *workdir = GetWorkDir();
                const char *inputdir = GetInputDir();
                const char *logdir = GetLogDir();
                const char *statedir = GetStateDir();
                Writer *out = FileWriter(stdout);
                WriterWriteF(out, "self-diagnostics for agent using workdir '%s'\n", workdir);
                WriterWriteF(out, "self-diagnostics for agent using inputdir '%s'\n", inputdir);
                WriterWriteF(out, "self-diagnostics for agent using logdir '%s'\n", logdir);
                WriterWriteF(out, "self-diagnostics for agent using statedir '%s'\n", statedir);

                AgentDiagnosticsRun(workdir, AgentDiagnosticsAllChecks(), out);
                AgentDiagnosticsRunAllChecksNova(workdir, out, &AgentDiagnosticsRun, &AgentDiagnosticsResultNew);
                FileWriterDetach(out);
            }
            DoCleanupAndExit(EXIT_SUCCESS);

        case 'C':
            if (!GenericAgentConfigParseColor(config, optarg))
            {
                DoCleanupAndExit(EXIT_FAILURE);
            }
            break;

        case 'E':
            extension_libraries_disable();
            break;

        case 'l':
            LoggingEnableTimestamps(true);
            break;

        case 'T':
            option_trust_server = true;

            /* If the argument is missing, we trust by default. */
            if (optarg == NULL || strcmp(optarg, "yes") == 0)
            {
                config->agent_specific.agent.bootstrap_trust_server = true;
            }
            else
            {
                config->agent_specific.agent.bootstrap_trust_server = false;
            }

            break;

        /* long options only */
        case 0:
        {
            const char *const option_name = OPTIONS[longopt_idx].name;
            if (StringEqual(option_name, "ignore-preferred-augments"))
            {
                config->ignore_preferred_augments = true;
            }
            else if (StringEqual(option_name, "log-modules"))
            {
                bool ret = LogEnableModulesFromString(optarg);
                if (!ret)
                {
                    DoCleanupAndExit(EXIT_FAILURE);
                }
            }
            else if (StringEqual(option_name, "no-augments"))
            {
                config->agent_specific.common.no_augments = true;
            }
            else if (StringEqual(option_name, "no-host-specific-data"))
            {
                config->agent_specific.common.no_host_specific = true;
            }
            else if (StringEqual(option_name, "show-evaluated-classes"))
            {
                if (optarg == NULL)
                {
                    optarg = ".*";
                }
                config->agent_specific.agent.show_evaluated_classes = xstrdup(optarg);
            }
            else if (StringEqual(option_name, "show-evaluated-vars"))
            {
                if (optarg == NULL)
                {
                    optarg = ".*";
                }
                config->agent_specific.agent.show_evaluated_variables = xstrdup(optarg);
            }
            else if (StringEqual(option_name, "skip-bootstrap-policy-run"))
            {
                config->agent_specific.agent.bootstrap_trigger_policy = false;
            }
            else if (StringEqual(option_name, "skip-bootstrap-service-start"))
            {
                config->agent_specific.agent.skip_bootstrap_service_start = true;
            }
            else if (StringEqual(option_name, "skip-db-check"))
            {
                if (optarg == NULL)
                {
                    PERFORM_DB_CHECK = false; // Skip (no arg), check = false
                }
                else if (StringEqual_IgnoreCase(optarg, "yes"))
                {
                    PERFORM_DB_CHECK = false; // Skip = yes, check = false
                }
                else if (StringEqual_IgnoreCase(optarg, "no"))
                {
                    PERFORM_DB_CHECK = true; // Skip = no, check = true
                }
                else
                {
                    Log(LOG_LEVEL_ERR,
                        "Invalid argument for --skip-db-check(yes/no): '%s'",
                        optarg);
                    DoCleanupAndExit(EXIT_FAILURE);
                }
            }
            else if (StringEqual(option_name, "simulate"))
            {
                if (optarg == NULL)
                {
                    Log(LOG_LEVEL_ERR,
                        "Missing argument for --simulate, 'manifest', 'manifest-full', or 'diff' required");
                    DoCleanupAndExit(EXIT_FAILURE);
                }
                else if (StringEqual_IgnoreCase(optarg, "manifest"))
                {
                    EVAL_MODE = EVAL_MODE_SIMULATE_MANIFEST;
                }
                else if (StringEqual_IgnoreCase(optarg, "manifest-full"))
                {
                    EVAL_MODE = EVAL_MODE_SIMULATE_MANIFEST_FULL;
                }
                else if (StringEqual_IgnoreCase(optarg, "diff"))
                {
                    EVAL_MODE = EVAL_MODE_SIMULATE_DIFF;
                }
                else
                {
                    Log(LOG_LEVEL_ERR,
                        "Invalid argument for --simulate, 'manifest' or 'diff' required, not '%s'",
                        optarg);
                    DoCleanupAndExit(EXIT_FAILURE);
                }
            }
            break;
        }
        default:
            {
                Writer *w = FileWriter(stdout);
                WriterWriteHelp(w, &COMPONENT, OPTIONS, HINTS, NULL, false, true);
                FileWriterDetach(w);
            }
            DoCleanupAndExit(EXIT_FAILURE);
        }
    }

    if (!GenericAgentConfigParseArguments(config, argc_new - optind,
                                          argv_new + optind))
    {
        Log(LOG_LEVEL_ERR, "Too many arguments");
        DoCleanupAndExit(EXIT_FAILURE);
    }

    if (option_trust_server &&
        config->agent_specific.agent.bootstrap_argument == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Option --trust-server can only be used when bootstrapping");
        DoCleanupAndExit(EXIT_FAILURE);
    }

    FreeFixedStringArray(argc_new, argv_new);

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


static void FreeFixedStringArray(int size, char **array)
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
    char filename[CF_BUFSIZE];

#ifdef HAVE_SETSID
    setsid();
#endif

    CFA_MAXTHREADS = 30;
    EDITFILESIZE = 100000;

/*
  do not set signal(SIGCHLD,SIG_IGN) in agent near
  popen() - or else pclose will fail to return
  status which we need for setting returns
*/

    snprintf(filename, CF_BUFSIZE, "%s/cfagent.%s.log", GetLogDir(), VSYSNAME.nodename);
    ToLowerStrInplace(filename);
    MapName(filename);

    const mode_t current_umask = umask(0777);  // Gets and changes umask
    umask(current_umask); // Restores umask
    Log(LOG_LEVEL_DEBUG, "Current umask is %o", current_umask);
    FILE *fp = safe_fopen(filename, "a");
    if (fp != NULL)
    {
        fclose(fp);
    }

    InitializeCustomPromises();
}

/*******************************************************************/

static void KeepPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
{
    KeepControlPromises(ctx, policy, config);
    /* Check if 'abortclasses' aborted evaluation or not. */
    if (EvalAborted(ctx))
    {
        return;
    }
    KeepPromiseBundles(ctx, policy, config);
}

/*******************************************************************/
/* Level 2                                                         */
/*******************************************************************/

static void KeepControlPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
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

            if (CommonControlFromString(cp->lval) != COMMON_CONTROL_MAX)
            {
                /* Already handled in generic_agent */
                continue;
            }

            VarRef *ref = VarRefParseFromScope(cp->lval, "control_agent");
            DataType value_type;
            const void *value = EvalContextVariableGet(ctx, ref, &value_type);
            VarRefDestroy(ref);

            /* If var not found */
            if (value_type == CF_DATA_TYPE_NONE)
            {
                Log(LOG_LEVEL_ERR, "Unknown lval '%s' in agent control body", cp->lval);
                continue;
            }

            /* 'files_single_copy => { }' is a perfectly valid case. */
            if (StringEqual(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_FSINGLECOPY].lval))
            {
                assert(value_type == CF_DATA_TYPE_STRING_LIST);
                SINGLE_COPY_LIST = value;
                SINGLE_COPY_CACHE = StringSetNew();
                if (WouldLog(LOG_LEVEL_VERBOSE))
                {
                    char *rlist_str = RlistToString(SINGLE_COPY_LIST);
                    Log(LOG_LEVEL_VERBOSE, "Setting file single copy list to: %s", rlist_str);
                    free(rlist_str);
                }
                continue;
            }

            /* Empty list is not supported for the other constraints/attributes. */
            if (value == NULL)
            {
                Log(LOG_LEVEL_ERR,
                    "Empty list is not a valid value for '%s' attribute in agent control body",
                    cp->lval);
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

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_COPYFROM_RESTRICT_KEYS].lval) == 0)
            {
                EvalContextSetRestrictKeys(ctx, value);
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

                    strlcpy(name, RlistScalarValue(rp), CF_MAXVARSIZE);

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
                    strlcpy(name, RlistScalarValue(rp), CF_MAXVARSIZE);

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
                SetBindInterface(value);
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
                Log(LOG_LEVEL_VERBOSE, "Setting 'LD_LIBRARY_PATH=%s'", (const char *)value);
                setenv_wrapper("LD_LIBRARY_PATH", value, 1);
                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_DEFAULTCOPYTYPE].lval) == 0)
            {
                DEFAULT_COPYTYPE = value;
                Log(LOG_LEVEL_VERBOSE, "Setting defaultcopytype to '%s'", DEFAULT_COPYTYPE);
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
                EVAL_MODE = BooleanFromString(value) ? EVAL_MODE_DRY_RUN : EVAL_MODE_NORMAL;
                Log(LOG_LEVEL_VERBOSE, "Setting dryrun to %d", DONTDO);
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
                Log(LOG_LEVEL_VERBOSE, "Setting repository to '%s'", (const char *)value);
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
                Log(LOG_LEVEL_VERBOSE, "Setting timeout to %jd", (intmax_t) CONNTIMEOUT);
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
                    assert(strchr(RlistScalarValue(rp), '=')); /* Valid for putenv() */
                    if (putenv_wrapper(RlistScalarValue(rp)) != 0)
                    {
                        Log(LOG_LEVEL_ERR, "Failed to set environment variable '%s'. (putenv: %s)",
                            RlistScalarValue(rp), GetErrorStr());
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_SELECT_END_MATCH_EOF].lval) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "SET select_end_match_eof %s", (char *) value);
                EvalContextSetSelectEndMatchEof(ctx, BooleanFromString(value));
            }

            if (strcmp(cp->lval, CFA_CONTROLBODY[AGENT_CONTROL_REPORTCLASSLOG].lval) == 0)
            {
                config->agent_specific.agent.report_class_log = BooleanFromString(value);

                Log(LOG_LEVEL_VERBOSE, "Setting report_class_log to %s",
                    config->agent_specific.agent.report_class_log? "true" : "false");
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
                  "Failed to set syslog_host to '%s', too long", (const char *)value);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Setting syslog_host to '%s'", (const char *)value);
        }
    }

    if ((value = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_BWLIMIT)))
    {
        double bval;
        if (DoubleFromString(value, &bval))
        {
            bwlimit_kbytes = (uint32_t) ( bval / 1000.0);
            Log(LOG_LEVEL_VERBOSE, "Setting rate limit to %d kBytes/sec", bwlimit_kbytes);
        }
    }
    Nova_Initialize(ctx);

    // If not have been enabled above then should be disabled.
    // By default it's enabled to catch all set classes on startup stage
    // before this part of the policy is processed.
    if (!config->agent_specific.agent.report_class_log)
    {
        EvalContextAllClassesLoggingEnable(ctx, false);
    }
}

/*********************************************************************/

static void KeepPromiseBundles(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
{
    Rlist *bundlesequence = NULL;

    Banner("Begin policy/promise evaluation");

    if (config->bundlesequence != NULL)
    {
        Log(LOG_LEVEL_INFO, "Using command line specified bundlesequence");
        bundlesequence = RlistCopy(config->bundlesequence);
    }
    else
    {
        bundlesequence = RlistCopy((Rlist *) EvalContextVariableControlCommonGet(
                                       ctx, COMMON_CONTROL_BUNDLESEQUENCE));

        if (bundlesequence == NULL)
        {
            RlistAppendScalar(&bundlesequence, "main");
        }
    }

    bool ok = true;
    for (const Rlist *rp = bundlesequence; rp; rp = rp->next)
    {
        const char *name = NULL;

        switch (rp->val.type)
        {
        case RVAL_TYPE_SCALAR:
            name = RlistScalarValue(rp);
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
            const Bundle *bp = EvalContextResolveBundleExpression(ctx, policy, name, "agent");
            if (!bp)
            {
                bp = EvalContextResolveBundleExpression(ctx, policy, name, "common");
            }

            if (!bp)
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

    Writer *w = StringWriter();
    WriterWrite(w, "Using bundlesequence => ");
    RlistWrite(w, bundlesequence);
    Log(LOG_LEVEL_VERBOSE, "%s", StringWriterData(w));
    WriterClose(w);

/* If all is okay, go ahead and evaluate */

    for (const Rlist *rp = bundlesequence; rp; rp = rp->next)
    {
        const char *name = NULL;
        const Rlist *args = NULL;

        if (rp->val.type == RVAL_TYPE_FNCALL)
        {
            name = RlistFnCallValue(rp)->name;
            args = RlistFnCallValue(rp)->args;
        }
        else
        {
            name = RlistScalarValue(rp);
            args = NULL;
        }

        EvalContextSetBundleArgs(ctx, args);

        const Bundle *bp = EvalContextResolveBundleExpression(ctx, policy, name, "agent");
        if (!bp)
        {
            bp = EvalContextResolveBundleExpression(ctx, policy, name, "common");
        }

        if (bp)
        {
            BundleBanner(bp,args);
            EvalContextStackPushBundleFrame(ctx, bp, args, false);
            ScheduleAgentOperations(ctx, bp);
            EvalContextStackPopFrame(ctx);
            EndBundleBanner(bp);
            if (EvalAborted(ctx))
            {
                break;
            }
        }
        else
        {
            if (config->ignore_missing_bundles)
            {
                Log(LOG_LEVEL_VERBOSE, "Ignoring missing bundle '%s'", name);
            }
            else
            {
                FatalError(ctx, "Bundlesequence contained unknown bundle reference '%s'", name);
            }
        }
    }

    RlistDestroy(bundlesequence);
}

static void AllClassesReport(const EvalContext *ctx)
{
    char context_report_file[CF_BUFSIZE];
    snprintf(context_report_file, CF_BUFSIZE, "%s%callclasses.txt", GetStateDir(), FILE_SEPARATOR);

    FILE *fp = safe_fopen(context_report_file, "w");
    if (fp == NULL)
    {
        Log(LOG_LEVEL_INFO, "Could not open allclasses cache file '%s' (fopen: %s)", context_report_file, GetErrorStr());
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
    assert(bp != NULL);

    int save_pr_kept = PR_KEPT;
    int save_pr_repaired = PR_REPAIRED;
    int save_pr_notkept = PR_NOTKEPT;
    struct timespec start = BeginMeasure();

    if (PROCESSREFRESH == NULL || (PROCESSREFRESH && IsRegexItemIn(ctx, PROCESSREFRESH, bp->name)))
    {
        ClearProcessTable();
    }

    PromiseResult result = PROMISE_RESULT_SKIPPED;

    for (int pass = 1; pass < CF_DONEPASSES; pass++)
    {
        // Evaluate built-in (non-custom) promise types, according to type sequence (normal order):
        for (TypeSequence type = 0; AGENT_TYPESEQUENCE[type] != NULL; type++)
        {
            const BundleSection *sp = BundleGetSection((Bundle *)bp, AGENT_TYPESEQUENCE[type]);

            if (!sp || SeqLength(sp->promises) == 0)
            {
                continue;
            }

            NewTypeContext(type);

            SpecialTypeBanner(type, pass);
            EvalContextStackPushBundleSectionFrame(ctx, sp);

            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                Promise *pp = SeqAt(sp->promises, ppi);

                EvalContextSetPass(ctx, pass);

                PromiseResult promise_result = ExpandPromise(ctx, pp, KeepAgentPromise, NULL);
                result = PromiseResultUpdate(result, promise_result);

                if (EvalAborted(ctx) || BundleAbort(ctx))
                {
                    DeleteTypeContext(ctx, type);
                    EvalContextStackPopFrame(ctx);
                    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
                    return result;
                }
            }

            DeleteTypeContext(ctx, type);
            EvalContextStackPopFrame(ctx);

            if (type == TYPE_SEQUENCE_CONTEXTS)
            {
                BundleResolve(ctx, bp);
                BundleResolvePromiseType(ctx, bp, "defaults", DefaultVarPromiseWrapper);
            }
        }

        // Custom promises are evaluated at the end of an evaluation pass:
        const size_t sections = SeqLength(bp->custom_sections);
        for (size_t i = 0; i < sections; ++i)
        {
            BundleSection *section = SeqAt(bp->custom_sections, i);

            EvalContextStackPushBundleSectionFrame(ctx, section);

            const size_t promises = SeqLength(section->promises);
            for (size_t ppi = 0; ppi < promises; ppi++)
            {
                Promise *pp = SeqAt(section->promises, ppi);

                EvalContextSetPass(ctx, pass);

                PromiseResult promise_result = ExpandPromise(ctx, pp, KeepAgentPromise, NULL);
                result = PromiseResultUpdate(result, promise_result);

                if (EvalAborted(ctx) || BundleAbort(ctx))
                {
                    EvalContextStackPopFrame(ctx);
                    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
                    return result;
                }
            }
            EvalContextStackPopFrame(ctx);
        }
    }

    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
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
                    DoCleanupAndExit(EXIT_FAILURE);
                }
            }
            else if (CFPARANOID && IsPrivileged())
            {
                if (sb.st_uid != getuid())
                {
                    Log(LOG_LEVEL_ERR, "File '%s' is not owned by uid %ju (security exception)", input_file,
                          (uintmax_t)getuid());
                    DoCleanupAndExit(EXIT_FAILURE);
                }
            }
        }

        StringSetDestroy(input_files);
    }

    Log(LOG_LEVEL_ERR, "You are denied access to run this policy");
    DoCleanupAndExit(EXIT_FAILURE);
}
#endif /* !__MINGW32__ */

/*********************************************************************/

static PromiseResult DefaultVarPromise(EvalContext *ctx, const Promise *pp)
{
    char *regex = PromiseGetConstraintAsRval(pp, "if_match_regex", RVAL_TYPE_SCALAR);
    bool okay = true;


    DataType value_type = CF_DATA_TYPE_NONE;
    const void *value = NULL;
    {
        VarRef *ref = VarRefParseFromScope(pp->promiser, "this");
        value = EvalContextVariableGet(ctx, ref, &value_type);
        VarRefDestroy(ref);
    }

    switch (value_type)
    {
    case CF_DATA_TYPE_STRING:
    case CF_DATA_TYPE_INT:
    case CF_DATA_TYPE_REAL:
        if (regex && !FullTextMatch(ctx, regex, value))
        {
            return PROMISE_RESULT_NOOP;
        }

        if (regex == NULL)
        {
            return PROMISE_RESULT_NOOP;
        }
        break;

    case CF_DATA_TYPE_STRING_LIST:
    case CF_DATA_TYPE_INT_LIST:
    case CF_DATA_TYPE_REAL_LIST:
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

    return VerifyVarPromise(ctx, pp, NULL);
}

static void LogVariableValue(const EvalContext *ctx, const Promise *pp)
{
    VarRef *ref = VarRefParseFromBundle(pp->promiser, PromiseGetBundle(pp));
    char *out = NULL;

    DataType type;
    const void *var = EvalContextVariableGet(ctx, ref, &type);
    switch (type)
    {
        case CF_DATA_TYPE_INT:
        case CF_DATA_TYPE_REAL:
        case CF_DATA_TYPE_STRING:
            out = xstrdup((char *) var);
            break;
        case CF_DATA_TYPE_INT_LIST:
        case CF_DATA_TYPE_REAL_LIST:
        case CF_DATA_TYPE_STRING_LIST:
        {
            size_t siz = CF_BUFSIZE;
            size_t len = 0;
            out = xcalloc(1, CF_BUFSIZE);

            for (Rlist *rp = (Rlist *) var; rp != NULL; rp = rp->next)
            {
                const char *s = (char *) rp->val.item;

                if (strlen(s) + len + 3  >= siz)                // ", " + NULL
                {
                    out = xrealloc(out, siz + CF_BUFSIZE);
                    siz += CF_BUFSIZE;
                }

                if (len > 0)
                {
                    len += strlcat(out, ", ", siz);
                }

                len += strlcat(out, s, siz);
            }
            break;
        }
        case CF_DATA_TYPE_CONTAINER:
        {
            Writer *w = StringWriter();
            JsonWriteCompact(w, (JsonElement *) var);
            out = StringWriterClose(w);
            break;
        }
        default:
            /* TODO is CF_DATA_TYPE_NONE acceptable? Today all meta variables
             * are of this type. */
            /* UnexpectedError("Variable '%s' is of unknown type %d", */
            /*                 pp->promiser, type); */
            out = xstrdup("NONE");
            break;
    }

    Log(LOG_LEVEL_DEBUG, "V: '%s' => '%s'", pp->promiser, out);
    free(out);
    VarRefDestroy(ref);
}

static PromiseResult KeepAgentPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(param == NULL);
    assert(pp != NULL);

    BannerStatusBegin(PromiseGetPromiseType(pp), pp->promiser);
    struct timespec start = BeginMeasure();
    PromiseResult result = PROMISE_RESULT_NOOP;

    if (strcmp("meta", PromiseGetPromiseType(pp)) == 0 ||
        strcmp("vars", PromiseGetPromiseType(pp)) == 0)
    {
        Log(LOG_LEVEL_VERBOSE, "V:     Computing value of '%s'", pp->promiser);

        result = VerifyVarPromise(ctx, pp, NULL);
        if (result != PROMISE_RESULT_FAIL)
        {
            if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
            {
                LogVariableValue(ctx, pp);
            }
        }
    }
    else if (strcmp("defaults", PromiseGetPromiseType(pp)) == 0)
    {
        result = DefaultVarPromise(ctx, pp);
    }
    else if (strcmp("classes", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyClassPromise(ctx, pp, NULL);
    }
    else if (strcmp("processes", PromiseGetPromiseType(pp)) == 0)
    {
        if (!LoadProcessTable())
        {
            Log(LOG_LEVEL_ERR, "Unable to read the process table - cannot keep processes: type promises");
            return PROMISE_RESULT_FAIL;
        }
        result = VerifyProcessesPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("storage", PromiseGetPromiseType(pp)) == 0)
    {
        result = FindAndVerifyStoragePromises(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("packages", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyPackagesPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("users", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyUsersPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }

    else if (strcmp("files", PromiseGetPromiseType(pp)) == 0)
    {
        result = ParallelFindAndVerifyFilesPromises(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("commands", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyExecPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("databases", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyDatabasePromises(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("methods", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyMethodsPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("services", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyServicesPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("guest_environments", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyEnvironmentsPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("reports", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyReportPromise(ctx, pp);
    }
    else if (!IsBuiltInPromiseType(PromiseGetPromiseType(pp)))
    {
        result = EvaluateCustomPromise(ctx, pp);
    }
    else
    {
        result = PROMISE_RESULT_NOOP;
    }

    BannerStatusEnd(result, PromiseGetPromiseType(pp), pp->promiser);
    EvalContextLogPromiseIterationOutcome(ctx, pp, result);
    return result;
}

static void BannerStatusBegin(const char *type, char *name)
{
    if (StringEqual(type, "vars") || StringEqual(type, "classes"))
    {
        return;
    }
    Log(LOG_LEVEL_VERBOSE, "P: BEGIN %s promise (%.30s%s)",
        type, name,
        (strlen(name) > 30) ? "..." : "");
}

static void BannerStatusEnd(PromiseResult status, const char *type, char *name)
{
    if ((strcmp(type, "vars") == 0) || (strcmp(type, "classes") == 0))
    {
        return;
    }

    switch (status)
    {
    case PROMISE_RESULT_CHANGE:
        Log(LOG_LEVEL_VERBOSE, "A: Promise REPAIRED");
        break;

    case PROMISE_RESULT_TIMEOUT:
        Log(LOG_LEVEL_VERBOSE, "A: Promise TIMED-OUT");
        break;

    case PROMISE_RESULT_WARN:
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_INTERRUPTED:
        Log(LOG_LEVEL_VERBOSE, "A: Promise NOT KEPT!");
        break;

    case PROMISE_RESULT_DENIED:
        Log(LOG_LEVEL_VERBOSE, "A: Promise NOT KEPT - denied");
        break;

    case PROMISE_RESULT_NOOP:
        Log(LOG_LEVEL_VERBOSE, "A: Promise was KEPT");
        break;
    default:
        return;
        break;
    }

    Log(LOG_LEVEL_VERBOSE, "P: END %s promise (%.30s%s)",
        type, name,
        (strlen(name) > 30) ? "..." : "");
}

/*********************************************************************/
/* Type context                                                      */
/*********************************************************************/

static void NewTypeContext(TypeSequence type)
{
// get maxconnections

    switch (type)
    {
    case TYPE_SEQUENCE_ENVIRONMENTS:
        NewEnvironmentsContext();
        break;

    case TYPE_SEQUENCE_FILES:
        break;

    case TYPE_SEQUENCE_PROCESSES:
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
        break;
    }

    return;
}

/*********************************************************************/

static void DeleteTypeContext(EvalContext *ctx, TypeSequence type)
{
    switch (type)
    {
    case TYPE_SEQUENCE_ENVIRONMENTS:
        DeleteEnvironmentsContext();
        break;

    case TYPE_SEQUENCE_FILES:
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
        break;
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
    PromiseResult result = PROMISE_RESULT_SKIPPED;

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

static bool VerifyBootstrap(bool skip_cf_execd_check)
{
    const char *policy_server = PolicyServerGet();
    if (NULL_OR_EMPTY(policy_server))
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
    ClearProcessTable();
    LoadProcessTable();

    if (!skip_cf_execd_check && !IsProcessNameRunning(".*cf-execd.*"))
    {
        Log(LOG_LEVEL_ERR, "Bootstrapping failed, cf-execd is not running");
        return false;
    }


    Log(LOG_LEVEL_NOTICE, "Bootstrap to '%s' completed successfully!", policy_server);
    return true;
}

/**************************************************************/
/* Compliance comp                                            */
/**************************************************************/

static int NoteBundleCompliance(const Bundle *bundle, int save_pr_kept, int save_pr_repaired, int save_pr_notkept, struct timespec start)
{
    double delta_pr_kept, delta_pr_repaired, delta_pr_notkept;
    double bundle_compliance = 0.0;

    delta_pr_kept = (double) (PR_KEPT - save_pr_kept);
    delta_pr_notkept = (double) (PR_NOTKEPT - save_pr_notkept);
    delta_pr_repaired = (double) (PR_REPAIRED - save_pr_repaired);

    Log(LOG_LEVEL_VERBOSE, "A: ...................................................");
    Log(LOG_LEVEL_VERBOSE, "A: Bundle Accounting Summary for '%s' in namespace %s", bundle->name, bundle->ns);

    if (delta_pr_kept + delta_pr_notkept + delta_pr_repaired <= 0)
    {
        Log(LOG_LEVEL_VERBOSE, "A: Zero promises executed for bundle '%s'", bundle->name);
        Log(LOG_LEVEL_VERBOSE, "A: ...................................................");
        return PROMISE_RESULT_NOOP;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "A: Promises kept in '%s' = %.0lf", bundle->name, delta_pr_kept);
        Log(LOG_LEVEL_VERBOSE, "A: Promises not kept in '%s' = %.0lf", bundle->name, delta_pr_notkept);
        Log(LOG_LEVEL_VERBOSE, "A: Promises repaired in '%s' = %.0lf", bundle->name, delta_pr_repaired);

        bundle_compliance = (delta_pr_kept + delta_pr_repaired) / (delta_pr_kept + delta_pr_notkept + delta_pr_repaired);

        Log(LOG_LEVEL_VERBOSE, "A: Aggregate compliance (promises kept/repaired) for bundle '%s' = %.1lf%%",
          bundle->name, bundle_compliance * 100.0);

        if (LogGetGlobalLevel() >= LOG_LEVEL_INFO)
        {
            char name[CF_MAXVARSIZE];
            snprintf(name, CF_MAXVARSIZE, "%s:%s", bundle->ns, bundle->name);
            EndMeasure(name, start);
        }
        else
        {
            EndMeasure(NULL, start);
        }
        Log(LOG_LEVEL_VERBOSE, "A: ...................................................");
    }

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

        // TODO: This is a very bad way to check for valid IP(?)
        if (strlen(ipaddr) < CF_MAX_IP_LEN)
        {
            config->agent_specific.agent.bootstrap_argument = xstrdup(ipaddr);
            config->agent_specific.agent.bootstrap_ip       = xstrdup(ipaddr);
            config->agent_specific.agent.bootstrap_host     = xstrdup(ipaddr);
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

static void WaitForBackgroundProcesses()
{
#ifdef __MINGW32__
    /* no fork() on Windows */
    return;
#else
    Log(LOG_LEVEL_VERBOSE, "Waiting for background processes");
    bool have_children = true;
    while (have_children)
    {
        pid_t child = wait(NULL);
        if (child >= 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Background process %ju terminated", (uintmax_t) child);
        }
        have_children = !((child == -1) && (errno == ECHILD));
    }
    Log(LOG_LEVEL_VERBOSE, "No more background processes to wait for");
    return;
#endif  /* __MINGW32__ */
}
