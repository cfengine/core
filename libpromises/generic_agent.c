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

#include "bootstrap.h"
#include "sysinfo.h"
#include "env_context.h"
#include "policy.h"
#include "promises.h"
#include "files_lib.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "parser.h"
#include "dbm_api.h"
#include "crypto.h"
#include "vars.h"
#include "syntax.h"
#include "conversion.h"
#include "expand.h"
#include "transaction.h"
#include "scope.h"
#include "atexit.h"
#include "unix.h"
#include "cfstream.h"
#include "client_code.h"
#include "logging.h"
#include "string_lib.h"
#include "exec_tools.h"
#include "list.h"
#include "misc_lib.h"
#include "fncall.h"
#include "rlist.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#include "nova_reporting.h"
#else
#include "reporting.h"
#endif

#include <assert.h>

static pthread_once_t pid_cleanup_once = PTHREAD_ONCE_INIT;

static char PIDFILE[CF_BUFSIZE];

static void VerifyPromises(Policy *policy, GenericAgentConfig *config, const ReportContext *report_context);
static void CheckWorkingDirectories(void);
static Policy *Cf3ParseFile(const GenericAgentConfig *config, const char *filename, Seq *errors);
static Policy *Cf3ParseFiles(GenericAgentConfig *config, const Rlist *inputs, Seq *errors, const ReportContext *report_context);
static bool MissingInputFile(const char *input_file);
static void CheckControlPromises(GenericAgentConfig *config, char *scope, char *agent, Seq *controllist);
static void CheckVariablePromises(char *scope, Seq *var_promises);
static void CheckCommonClassPromises(Seq *class_promises, const ReportContext *report_context);
static void PrependAuditFile(char *file);

#if !defined(__MINGW32__)
static void OpenLog(int facility);
#endif
static bool VerifyBundleSequence(const Policy *policy, const GenericAgentConfig *config);

/*****************************************************************************/

static void SanitizeEnvironment()
{
    /* ps(1) and other utilities invoked by Cfengine may be affected */
    unsetenv("COLUMNS");

    /* Make sure subprocesses output is not localized */
    unsetenv("LANG");
    unsetenv("LANGUAGE");
    unsetenv("LC_MESSAGES");
}

/*****************************************************************************/

#if !defined(HAVE_NOVA)

void CheckLicenses(void)
{
    struct stat sb;
    char name[CF_BUFSIZE];

    snprintf(name, sizeof(name), "%s/state/am_policy_hub", CFWORKDIR);
    MapName(name);

    if (stat(name, &sb) != -1)
    {
        HardClass("am_policy_hub");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Additional class defined: am_policy_hub");
    }
}

#endif

/*****************************************************************************/

void GenericAgentDiscoverContext(GenericAgentConfig *config, ReportContext *report_context)
{
    char vbuff[CF_BUFSIZE];

#ifdef HAVE_NOVA
    CF_DEFAULT_DIGEST = HASH_METHOD_SHA256;
    CF_DEFAULT_DIGEST_LEN = CF_SHA256_LEN;
#else
    CF_DEFAULT_DIGEST = HASH_METHOD_MD5;
    CF_DEFAULT_DIGEST_LEN = CF_MD5_LEN;
#endif

    InitializeGA(config);

    SetReferenceTime(true);
    SetStartTime();
    SanitizeEnvironment();

    THIS_AGENT_TYPE = config->agent_type;
    HardClass(CF_AGENTTYPES[THIS_AGENT_TYPE]);

// need scope sys to set vars in expiry function
    SetNewScope("sys");

    if (EnterpriseExpiry())
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Cfengine - autonomous configuration engine. This enterprise license is invalid.\n");
        exit(1);
    }

    if (AM_NOVA)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> This is CFE Nova\n");
    }

    if (report_context->report_writers[REPORT_OUTPUT_TYPE_KNOWLEDGE])
    {
        WriterWriteF(report_context->report_writers[REPORT_OUTPUT_TYPE_KNOWLEDGE], "bundle knowledge CFEngine_nomenclature\n{\n");
        WriterWriteF(report_context->report_writers[REPORT_OUTPUT_TYPE_KNOWLEDGE], "}\n\nbundle knowledge policy_analysis\n{\n");
    }

    NewScope("const");
    NewScope("match");
    NewScope("mon");
    GetNameInfo3();
    GetInterfacesInfo(config->agent_type);

    Get3Environment();
    BuiltinClasses();
    OSClasses();

    LoadPersistentContext();
    LoadSystemConstants();

    snprintf(vbuff, CF_BUFSIZE, "control_%s", CF_AGENTTYPES[THIS_AGENT_TYPE]);
    SetNewScope(vbuff);
    NewScope("this");
    NewScope("match");

    if (BOOTSTRAP)
    {
        CheckAutoBootstrap();
    }
    else
    {
        if (strlen(POLICY_SERVER) > 0)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Found a policy server (hub) on %s", POLICY_SERVER);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> No policy server (hub) watch yet registered");
        }
    }

    SetPolicyServer(POLICY_SERVER);
}

static bool IsPolicyPrecheckNeeded(GenericAgentConfig *config, bool force_validation)
{
    bool check_policy = false;

    if (SHOWREPORTS)
    {
        check_policy = true;
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Reports mode is enabled, force-validating policy");
    }
    if (IsFileOutsideDefaultRepository(config->input_file))
    {
        check_policy = true;
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Input file is outside default repository, validating it");
    }
    if (NewPromiseProposals(config->input_file, NULL))
    {
        check_policy = true;
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Input file is changed since last validation, validating it");
    }
    if (force_validation)
    {
        check_policy = true;
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> always_validate is set, forcing policy validation");
    }

    return check_policy;
}

bool GenericAgentCheckPolicy(GenericAgentConfig *config, bool force_validation)
{
    if (!MissingInputFile(config->input_file))
    {
        if (IsPolicyPrecheckNeeded(config, force_validation))
        {
            bool policy_check_ok = CheckPromises(config->input_file);

            if (BOOTSTRAP && !policy_check_ok)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Policy is not valid, but proceeding with bootstrap");
                return true;
            }

            return policy_check_ok;
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Policy is already validated");
            return true;
        }
    }
    return false;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int CheckPromises(const char *input_file)
{
    char cmd[CF_BUFSIZE], cfpromises[CF_MAXVARSIZE];
    char filename[CF_MAXVARSIZE];
    struct stat sb;
    int fd;
    bool outsideRepo = false;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Verifying the syntax of the inputs...\n");

    snprintf(cfpromises, sizeof(cfpromises), "%s%cbin%ccf-promises%s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR,
             EXEC_SUFFIX);

    if (cfstat(cfpromises, &sb) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "cf-promises%s needs to be installed in %s%cbin for pre-validation of full configuration",
              EXEC_SUFFIX, CFWORKDIR, FILE_SEPARATOR);
        return false;
    }

/* If we are cf-agent, check syntax before attempting to run */

    snprintf(cmd, sizeof(cmd), "\"%s\" -cf \"", cfpromises);

    outsideRepo = IsFileOutsideDefaultRepository(input_file);

    if (outsideRepo)
    {
        strlcat(cmd, input_file, CF_BUFSIZE);
    }
    else
    {
        strlcat(cmd, CFWORKDIR, CF_BUFSIZE);
        strlcat(cmd, FILE_SEPARATOR_STR "inputs" FILE_SEPARATOR_STR, CF_BUFSIZE);
        strlcat(cmd, input_file, CF_BUFSIZE);
    }

    strlcat(cmd, "\"", CF_BUFSIZE);

    if (CBUNDLESEQUENCE_STR)
    {
        strlcat(cmd, " -b \"", CF_BUFSIZE);
        strlcat(cmd, CBUNDLESEQUENCE_STR, CF_BUFSIZE);
        strlcat(cmd, "\"", CF_BUFSIZE);
    }

    if (BOOTSTRAP)
    {
        // avoids license complains from commercial cf-promises during bootstrap - see Nova_CheckLicensePromise
        strlcat(cmd, " -D bootstrap_mode", CF_BUFSIZE);
    }

/* Check if reloading policy will succeed */

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking policy with command \"%s\"", cmd);

    if (ShellCommandReturnsZero(cmd, true))
    {

        if (!outsideRepo)
        {
            if (MINUSF)
            {
                snprintf(filename, CF_MAXVARSIZE, "%s/state/validated_%s", CFWORKDIR, CanonifyName(input_file));
                MapName(filename);
            }
            else
            {
                snprintf(filename, CF_MAXVARSIZE, "%s/masterfiles/cf_promises_validated", CFWORKDIR);
                MapName(filename);
            }

            MakeParentDirectory(filename, true);

            if ((fd = creat(filename, 0600)) != -1)
            {
                FILE *fp = fdopen(fd, "w");
                time_t now = time(NULL);

                char timebuf[26];

                fprintf(fp, "%s", cf_strtimestamp_local(now, timebuf));
                fclose(fp);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Caching the state of validation\n");
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "creat", " -> Failed to cache the state of validation\n");
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

/*****************************************************************************/

Policy *GenericAgentLoadPolicy(AgentType agent_type, GenericAgentConfig *config, const ReportContext *report_context)
{
    // TODO: remove PARSING
    PARSING = true;

    PROMISETIME = time(NULL);

    Seq *errors = SeqNew(100, PolicyErrorDestroy);
    Policy *main_policy = Cf3ParseFile(config, config->input_file, errors);

    HashVariables(main_policy, NULL, report_context);
    HashControls(main_policy, config);

    if (PolicyIsRunnable(main_policy))
    {
        Policy *aux_policy = Cf3ParseFiles(config, InputFiles(main_policy), errors, report_context);
        if (aux_policy)
        {
            main_policy = PolicyMerge(main_policy, aux_policy);
        }

        if (config->check_runnable)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Running full policy integrity checks");
            PolicyCheckRunnable(main_policy, errors, config->ignore_missing_bundles);
        }
    }

    PARSING = false;

    if (SeqLength(errors) > 0)
    {
        Writer *writer = FileWriter(stderr);
        for (size_t i = 0; i < errors->length; i++)
        {
            PolicyErrorWrite(writer, errors->data[i]);
        }
        WriterClose(writer);
        exit(EXIT_FAILURE); // TODO: do not exit
    }

    SeqDestroy(errors);

/* Now import some web variables that are set in cf-know/control for the report options */

    {
        Rval rval = { 0 };

        switch (GetVariable("control_common", "cfinputs_version", &rval))
        {
        case DATA_TYPE_STRING:
            AUDITPTR->version = xstrdup((char *) rval.item);
            break;

        default:
            AUDITPTR->version = xstrdup("no specified version");
            break;
        }
    }

    WriterWriteF(report_context->report_writers[REPORT_OUTPUT_TYPE_TEXT], "Expanded promise list for %s component\n\n",
                 AgentTypeToString(agent_type));

    ShowContext(report_context);

    VerifyPromises(main_policy, config, report_context);

    if (agent_type != AGENT_TYPE_COMMON)
    {
        ShowScopedVariables(report_context, REPORT_OUTPUT_TYPE_TEXT);
    }

    return main_policy;
}

/*****************************************************************************/

#if !defined(__MINGW32__)
static void OpenLog(int facility)
{
    openlog(VPREFIX, LOG_PID | LOG_NOWAIT | LOG_ODELAY, facility);
}
#endif

/*****************************************************************************/

#if !defined(__MINGW32__)
void CloseLog(void)
{
    closelog();
}
#endif

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

void InitializeGA(GenericAgentConfig *config)
{
    int force = false;
    struct stat statbuf, sb;
    char vbuff[CF_BUFSIZE];
    char ebuff[CF_EXPANDSIZE];

    SHORT_CFENGINEPORT = htons((unsigned short) 5308);
    snprintf(STR_CFENGINEPORT, 15, "5308");

    HardClass("any");

#if defined HAVE_NOVA
    HardClass("nova_edition");
    HardClass("enterprise_edition");
#else
    HardClass("community_edition");
#endif

    strcpy(VPREFIX, GetConsolePrefix());

    if (VERBOSE)
    {
        HardClass("verbose_mode");
    }

    if (INFORM)
    {
        HardClass("inform_mode");
    }

    if (DEBUG)
    {
        HardClass("debug_mode");
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "CFEngine - autonomous configuration engine - commence self-diagnostic prelude\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "------------------------------------------------------------------------\n");

/* Define trusted directories */

    strcpy(CFWORKDIR, GetWorkDir());
    MapName(CFWORKDIR);

    CfDebug("Setting CFWORKDIR=%s\n", CFWORKDIR);

/* On windows, use 'binary mode' as default for files */

#ifdef __MINGW32__
    _fmode = _O_BINARY;
#endif

    OpenLog(LOG_USER);
    SetSyslogFacility(LOG_USER);

    if (!LOOKUP)                /* cf-know should not do this in lookup mode */
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Work directory is %s\n", CFWORKDIR);

        snprintf(vbuff, CF_BUFSIZE, "%s%cinputs%cupdate.conf", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, force);
        snprintf(vbuff, CF_BUFSIZE, "%s%cbin%ccf-agent -D from_cfexecd", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, force);
        snprintf(vbuff, CF_BUFSIZE, "%s%coutputs%cspooled_reports", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, force);
        snprintf(vbuff, CF_BUFSIZE, "%s%clastseen%cintermittencies", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, force);
        snprintf(vbuff, CF_BUFSIZE, "%s%creports%cvarious", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, force);

        snprintf(vbuff, CF_BUFSIZE, "%s%cinputs", CFWORKDIR, FILE_SEPARATOR);

        if (cfstat(vbuff, &sb) == -1)
        {
            FatalError(" !!! No access to WORKSPACE/inputs dir");
        }
        else
        {
            cf_chmod(vbuff, sb.st_mode | 0700);
        }

        snprintf(vbuff, CF_BUFSIZE, "%s%coutputs", CFWORKDIR, FILE_SEPARATOR);

        if (cfstat(vbuff, &sb) == -1)
        {
            FatalError(" !!! No access to WORKSPACE/outputs dir");
        }
        else
        {
            cf_chmod(vbuff, sb.st_mode | 0700);
        }

        sprintf(ebuff, "%s%cstate%ccf_procs", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(ebuff, force);

        if (cfstat(ebuff, &statbuf) == -1)
        {
            CreateEmptyFile(ebuff);
        }

        sprintf(ebuff, "%s%cstate%ccf_rootprocs", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);

        if (cfstat(ebuff, &statbuf) == -1)
        {
            CreateEmptyFile(ebuff);
        }

        sprintf(ebuff, "%s%cstate%ccf_otherprocs", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);

        if (cfstat(ebuff, &statbuf) == -1)
        {
            CreateEmptyFile(ebuff);
        }
    }

    OpenNetwork();

    CryptoInitialize();

    if (!LOOKUP)
    {
        CheckWorkingDirectories();
    }

    LoadSecretKeys();

    if (!MINUSF)
    {
        GenericAgentConfigSetInputFile(config, "promises.cf");
    }

    DetermineCfenginePort();

    VIFELAPSED = 1;
    VEXPIREAFTER = 1;

    setlinebuf(stdout);

    if (BOOTSTRAP)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cinputs%cfailsafe.cf", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);

        if (!IsEnterprise() && cfstat(vbuff, &statbuf) == -1)
        {
            GenericAgentConfigSetInputFile(config, "failsafe.cf");
        }
        else
        {
            GenericAgentConfigSetInputFile(config, vbuff);
        }
    }
}

/*******************************************************************/

static Policy *Cf3ParseFiles(GenericAgentConfig *config, const Rlist *inputs, Seq *errors, const ReportContext *report_context)
{
    Policy *policy = PolicyNew();

    for (const Rlist *rp = inputs; rp; rp = rp->next)
    {
        // TODO: ad-hoc validation, necessary?
        if (rp->type != RVAL_TYPE_SCALAR)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Non-file object in inputs list\n");
            continue;
        }
        else
        {
            Rval returnval;

            if (strcmp(rp->item, CF_NULL_VALUE) == 0)
            {
                continue;
            }

            returnval = EvaluateFinalRval("sys", (Rval) {rp->item, rp->type}, true, NULL);

            Policy *aux_policy = NULL;
            switch (returnval.type)
            {
            case RVAL_TYPE_SCALAR:
                aux_policy = Cf3ParseFile(config, returnval.item, errors);
                break;

            case RVAL_TYPE_LIST:
                aux_policy = Cf3ParseFiles(config, returnval.item, errors, report_context);
                break;

            default:
                ProgrammingError("Unknown type in input list for parsing: %d", returnval.type);
                break;
            }

            if (aux_policy)
            {
                policy = PolicyMerge(policy, aux_policy);
            }

            RvalDestroy(returnval);
        }

        HashVariables(policy, NULL, report_context);
        HashControls(policy, config);
    }

    HashVariables(policy, NULL, report_context);

    return policy;
}

/*******************************************************************/

static bool MissingInputFile(const char *input_file)
{
    struct stat sb;
    char wfilename[CF_BUFSIZE];

    strncpy(wfilename, GenericAgentResolveInputPath(input_file, input_file), CF_BUFSIZE);

    if (cfstat(wfilename, &sb) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "stat", "There is no readable input file at %s", wfilename);
        return true;
    }

    return false;
}

/*******************************************************************/

int NewPromiseProposals(const char *input_file, const Rlist *input_files)
{
    Rlist *sl;
    struct stat sb;
    int result = false;
    char filename[CF_MAXVARSIZE];
    char wfilename[CF_BUFSIZE];
    time_t validated_at;

    if (MINUSF)
    {
        snprintf(filename, CF_MAXVARSIZE, "%s/state/validated_%s", CFWORKDIR, CanonifyName(input_file));
        MapName(filename);
    }
    else
    {
        snprintf(filename, CF_MAXVARSIZE, "%s/masterfiles/cf_promises_validated", CFWORKDIR);
        MapName(filename);
    }

    if (stat(filename, &sb) != -1)
    {
        validated_at = sb.st_mtime;
    }
    else
    {
        validated_at = 0;
    }

// sanity check

    if (validated_at > time(NULL))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "",
              "!! Clock seems to have jumped back in time - mtime of %s is newer than current time - touching it",
              filename);

        if (utime(filename, NULL) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "utime", "!! Could not touch %s", filename);
        }

        validated_at = 0;
        return true;
    }

    strncpy(wfilename, GenericAgentResolveInputPath(input_file, input_file), CF_BUFSIZE);

    if (cfstat(wfilename, &sb) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "stat", "There is no readable input file at %s", input_file);
        return true;
    }

    if (sb.st_mtime > validated_at || sb.st_mtime > PROMISETIME)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Promises seem to change");
        return true;
    }

// Check the directories first for speed and because non-input/data files should trigger an update

    snprintf(filename, CF_MAXVARSIZE, "%s/inputs", CFWORKDIR);
    MapName(filename);

    if (IsNewerFileTree(filename, PROMISETIME))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Quick search detected file changes");
        return true;
    }

// Check files in case there are any abs paths

    for (const Rlist *rp = input_files; rp != NULL; rp = rp->next)
    {
        if (rp->type != RVAL_TYPE_SCALAR)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Non file object %s in list\n", (char *) rp->item);
        }
        else
        {
            Rval returnval = EvaluateFinalRval("sys", (Rval) { rp->item, rp->type }, true, NULL);

            switch (returnval.type)
            {
            case RVAL_TYPE_SCALAR:

                if (cfstat(GenericAgentResolveInputPath((char *) returnval.item, input_file), &sb) == -1)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "stat", "Unreadable promise proposals at %s", (char *) returnval.item);
                    result = true;
                    break;
                }

                if (sb.st_mtime > PROMISETIME)
                {
                    result = true;
                }
                break;

            case RVAL_TYPE_LIST:

                for (sl = (Rlist *) returnval.item; sl != NULL; sl = sl->next)
                {
                    if (cfstat(GenericAgentResolveInputPath((char *) sl->item, input_file), &sb) == -1)
                    {
                        CfOut(OUTPUT_LEVEL_ERROR, "stat", "Unreadable promise proposals at %s", (char *) sl->item);
                        result = true;
                        break;
                    }

                    if (sb.st_mtime > PROMISETIME)
                    {
                        result = true;
                        break;
                    }
                }
                break;

            default:
                break;
            }

            RvalDestroy(returnval);

            if (result)
            {
                break;
            }
        }
    }

// did policy server change (used in $(sys.policy_hub))?
    snprintf(filename, CF_MAXVARSIZE, "%s/policy_server.dat", CFWORKDIR);
    MapName(filename);

    if ((cfstat(filename, &sb) != -1) && (sb.st_mtime > PROMISETIME))
    {
        result = true;
    }

    return result;
}

/*******************************************************************/

ReportContext *OpenReports(AgentType agent_type)
{
    const char *workdir = GetWorkDir();
    char name[CF_BUFSIZE];

    FILE *freport_text = NULL;
    FILE *freport_knowledge = NULL;

    if (SHOWREPORTS)
    {
        snprintf(name, CF_BUFSIZE, "%s%creports%cpromise_output_%s.txt", workdir, FILE_SEPARATOR, FILE_SEPARATOR,
                 AgentTypeToString(agent_type));

        if ((freport_text = fopen(name, "w")) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Cannot open output file %s", name);
            freport_text = fopen(NULLFILE, "w");
        }

        snprintf(name, CF_BUFSIZE, "%s%cpromise_knowledge.cf", workdir, FILE_SEPARATOR);

        if ((freport_knowledge = fopen(name, "w")) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Cannot open output file %s", name);
        }

        CfOut(OUTPUT_LEVEL_INFORM, "", " -> Writing knowledge output to %s", workdir);
    }
    else
    {
        snprintf(name, CF_BUFSIZE, NULLFILE);
        if ((freport_text = fopen(name, "w")) == NULL)
        {
            FatalError("Cannot open output file %s", name);
        }
    }

    if (!freport_text)
    {
        FatalError("Unable to continue as the null-file is unwritable");
    }


    ReportContext *context = ReportContextNew();
    ReportContextAddWriter(context, REPORT_OUTPUT_TYPE_TEXT, FileWriter(freport_text));

    if (freport_knowledge)
    {
        ReportContextAddWriter(context, REPORT_OUTPUT_TYPE_KNOWLEDGE, FileWriter(freport_knowledge));
    }

    return context;
}

/*******************************************************************/

void CloseReports(const char *agents, ReportContext *report_context)
{
    char name[CF_BUFSIZE];

#ifndef HAVE_NOVA
    if (SHOWREPORTS)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Wrote compilation report %s%creports%cpromise_output_%s.txt", CFWORKDIR, FILE_SEPARATOR,
              FILE_SEPARATOR, agents);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Wrote compilation report %s%creports%cpromise_output_%s.html", CFWORKDIR, FILE_SEPARATOR,
              FILE_SEPARATOR, agents);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Wrote knowledge map %s%cpromise_knowledge.cf", CFWORKDIR, FILE_SEPARATOR);
    }
#endif
    
    ReportContextDestroy(report_context);

// Make the knowledge readable in situ

    snprintf(name, CF_BUFSIZE, "%s%cpromise_knowledge.cf", CFWORKDIR, FILE_SEPARATOR);
    chmod(name, 0644);
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

/*
 * The difference between filename and input_input file is that the latter is the file specified by -f or
 * equivalently the file containing body common control. This will hopefully be squashed in later refactoring.
 */
static Policy *Cf3ParseFile(const GenericAgentConfig *config, const char *filename, Seq *errors)
{
    struct stat statbuf;
    char wfilename[CF_BUFSIZE];

    strncpy(wfilename, GenericAgentResolveInputPath(filename, config->input_file), CF_BUFSIZE);

    if (cfstat(wfilename, &statbuf) == -1)
    {
        if (config->ignore_missing_inputs)
        {
            return PolicyNew();
        }

        CfOut(OUTPUT_LEVEL_ERROR, "stat", "Can't stat file \"%s\" for parsing\n", wfilename);
        exit(1);
    }

#ifndef _WIN32
    if (config->check_not_writable_by_others && (statbuf.st_mode & (S_IWGRP | S_IWOTH)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "File %s (owner %ju) is writable by others (security exception)", wfilename, (uintmax_t)statbuf.st_uid);
        exit(1);
    }
#endif

    CfDebug("+++++++++++++++++++++++++++++++++++++++++++++++\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "  > Parsing file %s\n", wfilename);
    CfDebug("+++++++++++++++++++++++++++++++++++++++++++++++\n");

    PrependAuditFile(wfilename);

    if (!FileCanOpen(wfilename, "r"))
    {
        printf("Can't open file %s for parsing\n", wfilename);
        exit(1);
    }

    Policy *policy = NULL;
    if (StringEndsWith(wfilename, ".json"))
    {
        char *contents = NULL;
        if (FileReadMax(&contents, wfilename, SIZE_MAX) == -1)
        {
            FatalError("Error reading JSON input file");
        }
        JsonElement *json_policy = NULL;
        const char *data = contents; // TODO: need to fix JSON parser signature, just silly
        if (JsonParse(&data, &json_policy) != JSON_PARSE_OK)
        {
            FatalError("Error parsing JSON input file");
        }

        policy = PolicyFromJson(json_policy);

        JsonElementDestroy(json_policy);
        free(contents);
    }
    else
    {
        policy = ParserParseFile(wfilename);
    }

    assert(policy);

    return PolicyCheckPartial(policy, errors) ? policy : NULL;
}

/*******************************************************************/

Seq *ControlBodyConstraints(const Policy *policy, AgentType agent)
{
    for (size_t i = 0; i < SeqLength(policy->bodies); i++)
    {
        const Body *body = SeqAt(policy->bodies, i);

        if (strcmp(body->type, CF_AGENTTYPES[agent]) == 0)
        {
            if (strcmp(body->name, "control") == 0)
            {
                CfDebug("%s body for type %s\n", body->name, body->type);
                return body->conlist;
            }
        }
    }

    return NULL;
}

const Rlist *InputFiles(Policy *policy)
{
    Body *body_common_control = PolicyGetBody(policy, NULL, "common", "control");
    if (!body_common_control)
    {
        ProgrammingError("Attempted to get input files from policy without body common control");
        return NULL;
    }

    Seq *potential_inputs = BodyGetConstraint(body_common_control, "inputs");
    Constraint *cp = EffectiveConstraint(potential_inputs);
    SeqDestroy(potential_inputs);

    return cp ? cp->rval.item : NULL;
}

/*******************************************************************/

static int ParseFacility(const char *name)
{
    if (strcmp(name, "LOG_USER") == 0)
    {
        return LOG_USER;
    }
    if (strcmp(name, "LOG_DAEMON") == 0)
    {
        return LOG_DAEMON;
    }
    if (strcmp(name, "LOG_LOCAL0") == 0)
    {
        return LOG_LOCAL0;
    }
    if (strcmp(name, "LOG_LOCAL1") == 0)
    {
        return LOG_LOCAL1;
    }
    if (strcmp(name, "LOG_LOCAL2") == 0)
    {
        return LOG_LOCAL2;
    }
    if (strcmp(name, "LOG_LOCAL3") == 0)
    {
        return LOG_LOCAL3;
    }
    if (strcmp(name, "LOG_LOCAL4") == 0)
    {
        return LOG_LOCAL4;
    }
    if (strcmp(name, "LOG_LOCAL5") == 0)
    {
        return LOG_LOCAL5;
    }
    if (strcmp(name, "LOG_LOCAL6") == 0)
    {
        return LOG_LOCAL6;
    }
    if (strcmp(name, "LOG_LOCAL7") == 0)
    {
        return LOG_LOCAL7;
    }
    return -1;
}

void SetFacility(const char *retval)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET Syslog FACILITY = %s\n", retval);

    CloseLog();
    OpenLog(ParseFacility(retval));
    SetSyslogFacility(ParseFacility(retval));
}

/**************************************************************/

void BannerBundle(Bundle *bp, Rlist *params)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "*****************************************************************\n");

    if (VERBOSE || DEBUG)
    {
        printf("%s> BUNDLE %s", VPREFIX, bp->name);
    }

    if (params && (VERBOSE || DEBUG))
    {
        printf("(");
        RlistShow(stdout, params);
        printf(" )\n");
    }
    else
    {
        if (VERBOSE || DEBUG)
            printf("\n");
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "*****************************************************************\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");

}

/*********************************************************************/

static void CheckWorkingDirectories(void)
/* NOTE: We do not care about permissions (ACLs) in windows */
{
    struct stat statbuf;
    char vbuff[CF_BUFSIZE];

    CfDebug("CheckWorkingDirectories()\n");

    if (uname(&VSYSNAME) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "uname", "!!! Couldn't get kernel name info!");
        memset(&VSYSNAME, 0, sizeof(VSYSNAME));
    }

    snprintf(vbuff, CF_BUFSIZE, "%s%c.", CFWORKDIR, FILE_SEPARATOR);
    MakeParentDirectory(vbuff, false);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Making sure that locks are private...\n");

    if (chown(CFWORKDIR, getuid(), getgid()) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "chown", "Unable to set owner on %s to %ju.%ju", CFWORKDIR, (uintmax_t)getuid(), (uintmax_t)getgid());
    }

    if (cfstat(CFWORKDIR, &statbuf) != -1)
    {
        /* change permissions go-w */
        cf_chmod(CFWORKDIR, (mode_t) (statbuf.st_mode & ~022));
    }

    snprintf(vbuff, CF_BUFSIZE, "%s%cstate%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(vbuff, false);

    if (strlen(CFPRIVKEYFILE) == 0)
    {
        snprintf(CFPRIVKEYFILE, CF_BUFSIZE, "%s%cppkeys%clocalhost.priv", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        snprintf(CFPUBKEYFILE, CF_BUFSIZE, "%s%cppkeys%clocalhost.pub", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking integrity of the state database\n");
    snprintf(vbuff, CF_BUFSIZE, "%s%cstate", CFWORKDIR, FILE_SEPARATOR);

    if (cfstat(vbuff, &statbuf) == -1)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cstate%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, false);

        if (chown(vbuff, getuid(), getgid()) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "chown", "Unable to set owner on %s to %jd.%jd", vbuff, (uintmax_t)getuid(), (uintmax_t)getgid());
        }

        cf_chmod(vbuff, (mode_t) 0755);
    }
    else
    {
#ifndef __MINGW32__
        if (statbuf.st_mode & 022)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "UNTRUSTED: State directory %s (mode %jo) was not private!\n", CFWORKDIR,
                  (uintmax_t)(statbuf.st_mode & 0777));
        }
#endif /* !__MINGW32__ */
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking integrity of the module directory\n");

    snprintf(vbuff, CF_BUFSIZE, "%s%cmodules", CFWORKDIR, FILE_SEPARATOR);

    if (cfstat(vbuff, &statbuf) == -1)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cmodules%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, false);

        if (chown(vbuff, getuid(), getgid()) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "chown", "Unable to set owner on %s to %ju.%ju", vbuff, (uintmax_t)getuid(), (uintmax_t)getgid());
        }

        cf_chmod(vbuff, (mode_t) 0700);
    }
    else
    {
#ifndef __MINGW32__
        if (statbuf.st_mode & 022)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "UNTRUSTED: Module directory %s (mode %jo) was not private!\n", vbuff,
                  (uintmax_t)(statbuf.st_mode & 0777));
        }
#endif /* !__MINGW32__ */
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking integrity of the PKI directory\n");

    snprintf(vbuff, CF_BUFSIZE, "%s%cppkeys", CFWORKDIR, FILE_SEPARATOR);

    if (cfstat(vbuff, &statbuf) == -1)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cppkeys%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, false);

        cf_chmod(vbuff, (mode_t) 0700); /* Keys must be immutable to others */
    }
    else
    {
#ifndef __MINGW32__
        if (statbuf.st_mode & 077)
        {
            FatalError("UNTRUSTED: Private key directory %s%cppkeys (mode %jo) was not private!\n", CFWORKDIR,
                       FILE_SEPARATOR, (uintmax_t)(statbuf.st_mode & 0777));
        }
#endif /* !__MINGW32__ */
    }
}


const char *GenericAgentResolveInputPath(const char *filename, const char *base_input_file)
{
    static char wfilename[CF_BUFSIZE], path[CF_BUFSIZE];

    if (MINUSF && (filename != base_input_file) && IsFileOutsideDefaultRepository(base_input_file)
        && !IsAbsoluteFileName(filename))
    {
        /* If -f assume included relative files are in same directory */
        strncpy(path, base_input_file, CF_BUFSIZE - 1);
        ChopLastNode(path);
        snprintf(wfilename, CF_BUFSIZE - 1, "%s%c%s", path, FILE_SEPARATOR, filename);
    }
    else if (IsFileOutsideDefaultRepository(filename))
    {
        strncpy(wfilename, filename, CF_BUFSIZE - 1);
    }
    else
    {
        snprintf(wfilename, CF_BUFSIZE - 1, "%s%cinputs%c%s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, filename);
    }

    return MapName(wfilename);
}

/*******************************************************************/

void CompilationReport(Policy *policy, char *fname)
{
#if defined(HAVE_NOVA)
    ReportContext *compilation_report_context = Nova_OpenCompilationReportFiles(fname);
#else
    ReportContext *compilation_report_context = OpenCompilationReportFiles(fname);
#endif

    ShowPromises(compilation_report_context, policy->bundles, policy->bodies);

    ReportContextDestroy(compilation_report_context);
}

/****************************************************************************/

ReportContext *OpenCompilationReportFiles(const char *fname)
{
    char filename[CF_BUFSIZE];
    FILE *freport_text = NULL;

    snprintf(filename, CF_BUFSIZE - 1, "%s.txt", fname);
    CfOut(OUTPUT_LEVEL_INFORM, "", "Summarizing promises as text to %s\n", filename);

    if ((freport_text = fopen(filename, "w")) == NULL)
    {
        FatalError("Could not write output log to %s", filename);
    }

    ReportContext *context = ReportContextNew();
    ReportContextAddWriter(context, REPORT_OUTPUT_TYPE_TEXT, FileWriter(freport_text));

    return context;
}

/*******************************************************************/

static void VerifyPromises(Policy *policy, GenericAgentConfig *config, const ReportContext *report_context)
{

/* Now look once through ALL the bundles themselves */

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);

        const char *scope = bp->name;
        THIS_BUNDLE = bp->name;

        for (size_t j = 0; j < SeqLength(bp->subtypes); j++)
        {
            SubType *sp = SeqAt(bp->subtypes, j);

            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                Promise *pp = SeqAt(sp->promises, ppi);
                ExpandPromise(AGENT_TYPE_COMMON, scope, pp, NULL, report_context);
            }
        }
    }

    HashVariables(policy, NULL, report_context);
    HashControls(policy, config);

    // TODO: need to move this inside PolicyCheckRunnable eventually.
    if (!config->bundlesequence && config->check_runnable)
    {
        // only verify policy-defined bundlesequence for cf-agent, cf-promises, cf-gendoc
        if ((THIS_AGENT_TYPE == AGENT_TYPE_AGENT) ||
            (THIS_AGENT_TYPE == AGENT_TYPE_COMMON) ||
            (THIS_AGENT_TYPE == AGENT_TYPE_GENDOC))
        {
            if (!VerifyBundleSequence(policy, config))
            {
                FatalError("Errors in promise bundles");
            }
        }
    }
}

/********************************************************************/

static void PrependAuditFile(char *file)
{
    struct stat statbuf;

    AUDITPTR = xmalloc(sizeof(Audit));

    if (cfstat(file, &statbuf) == -1)
    {
        /* shouldn't happen */
        return;
    }

    HashFile(file, AUDITPTR->digest, CF_DEFAULT_DIGEST);

    AUDITPTR->next = VAUDIT;
    AUDITPTR->filename = xstrdup(file);
    AUDITPTR->date = xstrdup(cf_ctime(&statbuf.st_mtime));
    if (Chop(AUDITPTR->date, CF_EXPANDSIZE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
    }
    AUDITPTR->version = NULL;
    VAUDIT = AUDITPTR;
}

/*******************************************************************/
/* Level 3                                                         */
/*******************************************************************/

static void CheckVariablePromises(char *scope, Seq *var_promises)
{
    int allow_redefine = false;

    CfDebug("CheckVariablePromises()\n");

    for (size_t i = 0; i < SeqLength(var_promises); i++)
    {
        Promise *pp = SeqAt(var_promises, i);
        ConvergeVarHashPromise(scope, pp, allow_redefine);
    }
}

/*******************************************************************/

static void CheckCommonClassPromises(Seq *class_promises, const ReportContext *report_context)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Checking common class promises...\n");

    for (size_t i = 0; i < SeqLength(class_promises); i++)
    {
        Promise *pp = SeqAt(class_promises, i);
        ExpandPromise(AGENT_TYPE_AGENT, THIS_BUNDLE, pp, KeepClassContextPromise, report_context);
    }
}

/*******************************************************************/

static void CheckControlPromises(GenericAgentConfig *config, char *scope, char *agent, Seq *controllist)
{
    const BodySyntax *bp = NULL;
    Rval returnval;

    CfDebug("CheckControlPromises(%s)\n", agent);

    for (int i = 0; CF_ALL_BODIES[i].bs != NULL; i++)
    {
        bp = CF_ALL_BODIES[i].bs;

        if (strcmp(agent, CF_ALL_BODIES[i].bundle_type) == 0)
        {
            break;
        }
    }

    if (bp == NULL)
    {
        FatalError("Unknown agent");
    }

    for (size_t i = 0; i < SeqLength(controllist); i++)
    {
        Constraint *cp = SeqAt(controllist, i);

        if (IsExcluded(cp->classes, NULL))
        {
            continue;
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_BUNDLESEQUENCE].lval) == 0)
        {
            returnval = ExpandPrivateRval(CONTEXTID, cp->rval);
        }
        else
        {
            returnval = EvaluateFinalRval(CONTEXTID, cp->rval, true, NULL);
        }

        DeleteVariable(scope, cp->lval);

        if (!AddVariableHash(scope, cp->lval, returnval,
                             BodySyntaxGetDataType(bp, cp->lval), cp->audit->filename, cp->offset.line))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Rule from %s at/before line %zu\n", cp->audit->filename, cp->offset.line);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_OUTPUT_PREFIX].lval) == 0)
        {
            strncpy(VPREFIX, returnval.item, CF_MAXVARSIZE);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_DOMAIN].lval) == 0)
        {
            strcpy(VDOMAIN, cp->rval.item);
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET domain = %s\n", VDOMAIN);
            DeleteScalar("sys", "domain");
            DeleteScalar("sys", "fqhost");
            snprintf(VFQNAME, CF_MAXVARSIZE, "%s.%s", VUQNAME, VDOMAIN);
            NewScalar("sys", "fqhost", VFQNAME, DATA_TYPE_STRING);
            NewScalar("sys", "domain", VDOMAIN, DATA_TYPE_STRING);
            DeleteClass("undefined_domain", NULL);
            HardClass(VDOMAIN);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_IGNORE_MISSING_INPUTS].lval) == 0)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET ignore_missing_inputs %s\n", RvalScalarValue(cp->rval));
            config->ignore_missing_inputs = BooleanFromString(cp->rval.item);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_IGNORE_MISSING_BUNDLES].lval) == 0)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET ignore_missing_bundles %s\n", RvalScalarValue(cp->rval));
            config->ignore_missing_bundles = BooleanFromString(cp->rval.item);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_GOALPATTERNS].lval) == 0)
        {
            /* Ignored */
            continue;
        }
        
        RvalDestroy(returnval);
    }
}

/*******************************************************************/

void Syntax(const char *component, const struct option options[], const char *hints[], const char *id)
{
    int i;

    printf("\n\n%s\n\n", component);

    printf("SYNOPSIS:\n\n   program [options]\n\nDESCRIPTION:\n\n%s\n", id);
    printf("Command line options:\n\n");

    for (i = 0; options[i].name != NULL; i++)
    {
        if (options[i].has_arg)
        {
            printf("--%-12s, -%c value - %s\n", options[i].name, (char) options[i].val, hints[i]);
        }
        else
        {
            printf("--%-12s, -%-7c - %s\n", options[i].name, (char) options[i].val, hints[i]);
        }
    }

    printf("\nBug reports: http://bug.cfengine.com, ");
    printf("Community help: http://forum.cfengine.com\n");
    printf("Community info: http://www.cfengine.com/pages/community, ");
    printf("Support services: http://www.cfengine.com\n\n");
    printf("This software is Copyright (C) 2008,2010-present CFEngine AS.\n");
}

/*******************************************************************/

void ManPage(const char *component, const struct option options[], const char *hints[], const char *id)
{
    int i;

    printf(".TH %s 8 \"Maintenance Commands\"\n", CommandArg0(component));
    printf(".SH NAME\n%s\n\n", component);

    printf(".SH SYNOPSIS:\n\n %s [options]\n\n.SH DESCRIPTION:\n\n%s\n", CommandArg0(component), id);

    printf(".B cfengine\n"
           "is a self-healing configuration and change management based system. You can think of"
           ".B cfengine\n"
           "as a very high level language, much higher level than Perl or shell. A"
           "single statement is called a promise, and compliance can result in many hundreds of files"
           "being created, or the permissions of many hundreds of"
           "files being set. The idea of "
           ".B cfengine\n"
           "is to create a one or more sets of configuration files which will"
           "classify and describe the setup of every host in a network.\n");

    printf(".SH COMMAND LINE OPTIONS:\n");

    for (i = 0; options[i].name != NULL; i++)
    {
        if (options[i].has_arg)
        {
            printf(".IP \"--%s, -%c\" value\n%s\n", options[i].name, (char) options[i].val, hints[i]);
        }
        else
        {
            printf(".IP \"--%s, -%c\"\n%s\n", options[i].name, (char) options[i].val, hints[i]);
        }
    }

    printf(".SH AUTHOR\n" "Mark Burgess and CFEngine AS\n" ".SH INFORMATION\n");

    printf("\nBug reports: http://bug.cfengine.com, ");
    printf(".pp\nCommunity help: http://forum.cfengine.com\n");
    printf(".pp\nCommunity info: http://www.cfengine.com/pages/community\n");
    printf(".pp\nSupport services: http://www.cfengine.com\n");
    printf(".pp\nThis software is Copyright (C) 2008-%d CFEngine AS.\n", BUILD_YEAR);
}

/*******************************************************************/

static const char *banner_lines[] =
{
    "   @@@      ",
    "   @@@      ",
    "            ",
    " @ @@@ @    ",
    " @ @@@ @    ",
    " @ @@@ @    ",
    " @     @    ",
    "   @@@      ",
    "   @ @      ",
    "   @ @      ",
    "   @ @      ",
    NULL
};

static void AgentBanner(const char **text)
{
    const char **b = banner_lines;

    while (*b)
    {
        printf("%s%s\n", *b, *text ? *text : "");
        b++;
        if (*text)
        {
            text++;
        }
    }
}

/*******************************************************************/

void PrintVersionBanner(const char *component)
{
    const char *text[] =
{
        "",
        component,
        "",
        NameVersion(),
#ifdef HAVE_NOVA
        Nova_NameVersion(),
#endif
        NULL
    };

    printf("\n");
    AgentBanner(text);
    printf("\n");
    printf("Copyright (C) CFEngine AS 2008-%d\n", BUILD_YEAR);
    printf("See Licensing at http://cfengine.com/3rdpartylicenses\n");
}

/*******************************************************************/

const char *Version(void)
{
    return VERSION;
}

/*******************************************************************/

const char *NameVersion(void)
{
    return "CFEngine Core " VERSION;
}

/********************************************************************/

static void CleanPidFile(void)
{
    if (unlink(PIDFILE) != 0)
    {
        if (errno != ENOENT)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "unlink", "Unable to remove pid file");
        }
    }
}

/********************************************************************/

static void RegisterPidCleanup(void)
{
    RegisterAtExitFunction(&CleanPidFile);
}

/********************************************************************/

void WritePID(char *filename)
{
    FILE *fp;

    pthread_once(&pid_cleanup_once, RegisterPidCleanup);

    snprintf(PIDFILE, CF_BUFSIZE - 1, "%s%c%s", CFWORKDIR, FILE_SEPARATOR, filename);

    if ((fp = fopen(PIDFILE, "w")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "fopen", "Could not write to PID file %s\n", filename);
        return;
    }

    fprintf(fp, "%" PRIuMAX "\n", (uintmax_t)getpid());

    fclose(fp);
}

/*******************************************************************/

void HashVariables(Policy *policy, const char *name, const ReportContext *report_context)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Initiate variable convergence...\n");

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);

        if (name && strcmp(name, bp->name) != 0)
        {
            continue;
        }

        SetNewScope(bp->name);
        char scope[CF_BUFSIZE];
        snprintf(scope,CF_BUFSIZE,"%s_meta", bp->name);
        NewScope(scope);

        // TODO: seems sketchy, investigate purpose.
        THIS_BUNDLE = bp->name;

        for (size_t j = 0; j < SeqLength(bp->subtypes); j++)
        {
            SubType *sp = SeqAt(bp->subtypes, j);

            if (strcmp(sp->name, "vars") == 0)
            {
                CheckVariablePromises(bp->name, sp->promises);
            }

            // We must also set global classes here?

            if (strcmp(bp->type, "common") == 0 && strcmp(sp->name, "classes") == 0)
            {
                CheckCommonClassPromises(sp->promises, report_context);
            }

        }
    }
}

/*******************************************************************/

void HashControls(const Policy *policy, GenericAgentConfig *config)
{
    char buf[CF_BUFSIZE];

/* Only control bodies need to be hashed like variables */

    for (size_t i = 0; i < SeqLength(policy->bodies); i++)
    {
        Body *bdp = SeqAt(policy->bodies, i);

        if (strcmp(bdp->name, "control") == 0)
        {
            snprintf(buf, CF_BUFSIZE, "%s_%s", bdp->name, bdp->type);
            CfDebug("Initiate control variable convergence...%s\n", buf);
            DeleteScope(buf);
            SetNewScope(buf);
            CheckControlPromises(config, buf, bdp->type, bdp->conlist);
        }
    }
}

/********************************************************************/

static bool VerifyBundleSequence(const Policy *policy, const GenericAgentConfig *config)
{
    Rlist *rp;
    char *name;
    Rval retval;
    int ok = true;
    FnCall *fp;

    if (GetVariable("control_common", "bundlesequence", &retval) == DATA_TYPE_NONE)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !!! No bundlesequence in the common control body");
        return false;
    }

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
            break;

        case RVAL_TYPE_FNCALL:
            fp = (FnCall *) rp->item;
            name = (char *) fp->name;
            break;

        default:
            name = NULL;
            CfOut(OUTPUT_LEVEL_ERROR, "", "Illegal item found in bundlesequence: ");
            RvalShow(stdout, (Rval) {rp->item, rp->type});
            printf(" = %c\n", rp->type);
            ok = false;
            break;
        }

        if (strcmp(name, CF_NULL_VALUE) == 0)
        {
            continue;
        }

        if (!config->ignore_missing_bundles && !PolicyGetBundle(policy, NULL, NULL, name))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Bundle \"%s\" listed in the bundlesequence is not a defined bundle\n", name);
            ok = false;
        }
    }

    return ok;
}

/*******************************************************************/

GenericAgentConfig *GenericAgentConfigNewDefault(AgentType agent_type)
{
    GenericAgentConfig *config = xmalloc(sizeof(GenericAgentConfig));

    config->agent_type = agent_type;

    // TODO: system state, perhaps pull out as param
    config->tty_interactive = isatty(0) && isatty(1);

    config->bundlesequence = NULL;
    config->input_file = NULL;
    config->check_not_writable_by_others = agent_type != AGENT_TYPE_COMMON && !config->tty_interactive;
    config->check_runnable = agent_type != AGENT_TYPE_COMMON;
    config->ignore_missing_bundles = false;
    config->ignore_missing_inputs = false;

    switch (agent_type)
    {
    case AGENT_TYPE_COMMON:
        config->agent_specific.common.policy_output_format = GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_NONE;
        break;

    default:
        break;
    }

    return config;
}

void GenericAgentConfigDestroy(GenericAgentConfig *config)
{
    if (config)
    {
        RlistDestroy(config->bundlesequence);
        free(config->input_file);
    }
}

void GenericAgentConfigSetInputFile(GenericAgentConfig *config, const char *input_file)
{
    free(config->input_file);
    config->input_file = SafeStringDuplicate(input_file);
}

void GenericAgentConfigSetBundleSequence(GenericAgentConfig *config, const Rlist *bundlesequence)
{
    RlistDestroy(config->bundlesequence);
    config->bundlesequence = RlistCopy(bundlesequence);
}

const char *AgentTypeToString(AgentType agent_type)
{
    return CF_AGENTTYPES[agent_type];
}
