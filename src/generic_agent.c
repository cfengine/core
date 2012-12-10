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
#include "constraints.h"
#include "promises.h"
#include "files_lib.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_operators.h"
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

#ifdef HAVE_NOVA
#include "nova-reporting.h"
#else
#include "reporting.h"
#endif

static pthread_once_t pid_cleanup_once = PTHREAD_ONCE_INIT;

static char PIDFILE[CF_BUFSIZE];

extern char *CFH[][2];

static void VerifyPromises(Policy *policy, Rlist *bundlesequence, const ReportContext *report_context);
static void SetAuditVersion(void);
static void CheckWorkingDirectories(const ReportContext *report_context);
static void Cf3ParseFile(Policy *policy, char *filename, _Bool check_not_writable_by_others);
static void Cf3ParseFiles(Policy *policy, bool check_not_writable_by_others, const ReportContext *report_context);
static int MissingInputFile(void);
static void CheckControlPromises(char *scope, char *agent, Constraint *controllist);
static void CheckVariablePromises(char *scope, Promise *varlist);
static void CheckCommonClassPromises(Promise *classlist, const ReportContext *report_context);
static void PrependAuditFile(char *file);
static char *InputLocation(char *filename);

#if !defined(__MINGW32__)
static void OpenLog(int facility);
#endif
static bool VerifyBundleSequence(const Policy *policy, AgentType agent, Rlist *bundlesequence);

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
        CfOut(cf_verbose, "", " -> Additional class defined: am_policy_hub");
    }
}

#endif

/*****************************************************************************/

Policy *GenericInitialize(char *agents, GenericAgentConfig config, const ReportContext *report_context)
{
    AgentType ag = Agent2Type(agents);
    char vbuff[CF_BUFSIZE];
    int ok = false;

#ifdef HAVE_NOVA
    CF_DEFAULT_DIGEST = cf_sha256;
    CF_DEFAULT_DIGEST_LEN = CF_SHA256_LEN;
#else
    CF_DEFAULT_DIGEST = cf_md5;
    CF_DEFAULT_DIGEST_LEN = CF_MD5_LEN;
#endif

    InitializeGA(report_context);

    SetReferenceTime(true);
    SetStartTime();
    SanitizeEnvironment();

    THIS_AGENT_TYPE = ag;
    HardClass(CF_AGENTTYPES[THIS_AGENT_TYPE]);

// need scope sys to set vars in expiry function
    SetNewScope("sys");

    if (EnterpriseExpiry())
    {
        CfOut(cf_error, "", "Cfengine - autonomous configuration engine. This enterprise license is invalid.\n");
        exit(1);
    }

    if (AM_NOVA)
    {
        CfOut(cf_verbose, "", " -> This is CFE Nova\n");
    }

    if (report_context->report_writers[REPORT_OUTPUT_TYPE_KNOWLEDGE])
    {
        WriterWriteF(report_context->report_writers[REPORT_OUTPUT_TYPE_KNOWLEDGE], "bundle knowledge CFEngine_nomenclature\n{\n");
        ShowTopicRepresentation(report_context);
        WriterWriteF(report_context->report_writers[REPORT_OUTPUT_TYPE_KNOWLEDGE], "}\n\nbundle knowledge policy_analysis\n{\n");
    }       
    
    NewScope("const");
    NewScope("match");
    NewScope("mon");
    GetNameInfo3();
    GetInterfacesInfo(ag);

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
            CfOut(cf_verbose, "", " -> Found a policy server (hub) on %s", POLICY_SERVER);
        }
        else
        {
            CfOut(cf_verbose, "", " -> No policy server (hub) watch yet registered");
        }
    }

    SetPolicyServer(POLICY_SERVER);

    Policy *policy = NULL;

    if (ag != AGENT_TYPE_KEYGEN && ag != AGENT_TYPE_GENDOC)
    {
        if (!MissingInputFile())
        {
            bool check_promises = false;

            if (SHOWREPORTS)
            {
                check_promises = true;
                CfOut(cf_verbose, "", " -> Reports mode is enabled, force-validating policy");
            }
            if (IsFileOutsideDefaultRepository(VINPUTFILE))
            {
                check_promises = true;
                CfOut(cf_verbose, "", " -> Input file is outside default repository, validating it");
            }
            if (NewPromiseProposals())
            {
                check_promises = true;
                CfOut(cf_verbose, "", " -> Input file is changed since last validation, validating it");
            }

            if (check_promises)
            {
                ok = CheckPromises(ag, report_context);
                if (BOOTSTRAP && !ok)
                {
                    CfOut(cf_verbose, "", " -> Policy is not valid, but proceeding with bootstrap");
                    ok = true;
                }
            }
            else
            {
                CfOut(cf_verbose, "", " -> Policy is already validated");
                ok = true;
            }
        }

        if (ok)
        {
            policy = ReadPromises(ag, agents, config, report_context);
        }
        else
        {
            CfOut(cf_error, "",
                  "CFEngine was not able to get confirmation of promises from cf-promises, so going to failsafe\n");
            SetInputFile("failsafe.cf");
            policy = ReadPromises(ag, agents, config, report_context);
        }

        if (SHOWREPORTS)
        {
            CompilationReport(policy, VINPUTFILE);
        }

        if (SHOW_PARSE_TREE)
        {
            Writer *writer = FileWriter(stdout);

            PolicyPrintAsJson(writer, VINPUTFILE, policy->bundles, policy->bodies);
            WriterClose(writer);
        }

        CheckLicenses();
    }

    XML = 0;

    return policy;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int CheckPromises(AgentType ag, const ReportContext *report_context)
{
    char cmd[CF_BUFSIZE], cfpromises[CF_MAXVARSIZE];
    char filename[CF_MAXVARSIZE];
    struct stat sb;
    int fd;
    bool outsideRepo = false;

    if ((ag != AGENT_TYPE_AGENT) && (ag != AGENT_TYPE_EXECUTOR) && (ag != AGENT_TYPE_SERVER))
    {
        return true;
    }

    CfOut(cf_verbose, "", " -> Verifying the syntax of the inputs...\n");

    snprintf(cfpromises, sizeof(cfpromises), "%s%cbin%ccf-promises%s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR,
             EXEC_SUFFIX);

    if (cfstat(cfpromises, &sb) == -1)
    {
        CfOut(cf_error, "", "cf-promises%s needs to be installed in %s%cbin for pre-validation of full configuration",
              EXEC_SUFFIX, CFWORKDIR, FILE_SEPARATOR);
        return false;
    }

/* If we are cf-agent, check syntax before attempting to run */

    snprintf(cmd, sizeof(cmd), "\"%s\" -f \"", cfpromises);

    outsideRepo = IsFileOutsideDefaultRepository(VINPUTFILE);

    if (outsideRepo)
    {
        strlcat(cmd, VINPUTFILE, CF_BUFSIZE);
    }
    else
    {
        strlcat(cmd, CFWORKDIR, CF_BUFSIZE);
        strlcat(cmd, FILE_SEPARATOR_STR "inputs" FILE_SEPARATOR_STR, CF_BUFSIZE);
        strlcat(cmd, VINPUTFILE, CF_BUFSIZE);
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

    CfOut(cf_verbose, "", "Checking policy with command \"%s\"", cmd);

    if (ShellCommandReturnsZero(cmd, true))
    {

        if (!outsideRepo)
        {
            if (MINUSF)
            {
                snprintf(filename, CF_MAXVARSIZE, "%s/state/validated_%s", CFWORKDIR, CanonifyName(VINPUTFILE));
                MapName(filename);
            }
            else
            {
                snprintf(filename, CF_MAXVARSIZE, "%s/masterfiles/cf_promises_validated", CFWORKDIR);
                MapName(filename);
            }

            MakeParentDirectory(filename, true, report_context);

            if ((fd = creat(filename, 0600)) != -1)
            {
                FILE *fp = fdopen(fd, "w");
                time_t now = time(NULL);

                char timebuf[26];

                fprintf(fp, "%s", cf_strtimestamp_local(now, timebuf));
                fclose(fp);
                CfOut(cf_verbose, "", " -> Caching the state of validation\n");
            }
            else
            {
                CfOut(cf_verbose, "creat", " -> Failed to cache the state of validation\n");
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

Policy *ReadPromises(AgentType ag, char *agents, GenericAgentConfig config,
                     const ReportContext *report_context)
{
    Rval retval;
    char vbuff[CF_BUFSIZE];
    bool check_not_writable_by_others = true;

    switch (ag)
    {
    case AGENT_TYPE_COMMON:
        check_not_writable_by_others = false;
        break;

    case AGENT_TYPE_KEYGEN:
        return NULL;

    default:
        check_not_writable_by_others = true;
        break;
    }

    DeleteAllPromiseIds();      // in case we are re-reading, delete old handles

/* Parse the files*/

    Policy *policy = PolicyNew();
    Cf3ParseFiles(policy, check_not_writable_by_others, report_context);
    {
        Sequence *errors = SequenceCreate(100, PolicyErrorDestroy);
        if (!PolicyCheck(policy, errors))
        {
            Writer *writer = FileWriter(stderr);
            for (size_t i = 0; i < errors->length; i++)
            {
                PolicyErrorWrite(writer, errors->data[i]);
            }
            WriterClose(writer);
        }

        SequenceDestroy(errors);
    }

/* Now import some web variables that are set in cf-know/control for the report options */

    strncpy(STYLESHEET, "/cf_enterprise.css", CF_BUFSIZE - 1);
    strncpy(WEBDRIVER, "", CF_MAXVARSIZE - 1);

/* Make the compilation reports*/

    SetAuditVersion();

    GetVariable("control_common", "version", &retval);

    snprintf(vbuff, CF_BUFSIZE - 1, "Expanded promises for %s", agents);
    CfHtmlHeader(report_context->report_writers[REPORT_OUTPUT_TYPE_HTML], vbuff, STYLESHEET, WEBDRIVER, BANNER);

    WriterWriteF(report_context->report_writers[REPORT_OUTPUT_TYPE_TEXT], "Expanded promise list for %s component\n\n", agents);

    ShowContext(report_context);

    WriterWriteF(report_context->report_writers[REPORT_OUTPUT_TYPE_HTML], "<div id=\"reporttext\">\n");
    WriterWriteF(report_context->report_writers[REPORT_OUTPUT_TYPE_HTML], "%s", CFH[cfx_promise][cfb]);

    VerifyPromises(policy, config.bundlesequence, report_context);

    WriterWriteF(report_context->report_writers[REPORT_OUTPUT_TYPE_HTML], "%s", CFH[cfx_promise][cfe]);

    if (ag != AGENT_TYPE_COMMON)
    {
        ShowScopedVariables(report_context, REPORT_OUTPUT_TYPE_TEXT);
        ShowScopedVariables(report_context, REPORT_OUTPUT_TYPE_HTML);
    }

    WriterWriteF(report_context->report_writers[REPORT_OUTPUT_TYPE_HTML], "</div>\n");
    CfHtmlFooter(report_context->report_writers[REPORT_OUTPUT_TYPE_HTML], FOOTER);

    return policy;
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

void InitializeGA(const ReportContext *report_context)
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

    CfOut(cf_verbose, "", "CFEngine - autonomous configuration engine - commence self-diagnostic prelude\n");
    CfOut(cf_verbose, "", "------------------------------------------------------------------------\n");

/* Define trusted directories */

    strcpy(CFWORKDIR, GetWorkDir());
    MapName(CFWORKDIR);

    CfDebug("Setting CFWORKDIR=%s\n", CFWORKDIR);

/* On windows, use 'binary mode' as default for files */

#ifdef MINGW
    _fmode = _O_BINARY;
#endif

    OpenLog(LOG_USER);
    SetSyslogFacility(LOG_USER);

    if (!LOOKUP)                /* cf-know should not do this in lookup mode */
    {
        CfOut(cf_verbose, "", "Work directory is %s\n", CFWORKDIR);

        snprintf(vbuff, CF_BUFSIZE, "%s%cinputs%cupdate.conf", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, force, report_context);
        snprintf(vbuff, CF_BUFSIZE, "%s%cbin%ccf-agent -D from_cfexecd", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, force, report_context);
        snprintf(vbuff, CF_BUFSIZE, "%s%coutputs%cspooled_reports", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, force, report_context);
        snprintf(vbuff, CF_BUFSIZE, "%s%clastseen%cintermittencies", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, force, report_context);
        snprintf(vbuff, CF_BUFSIZE, "%s%creports%cvarious", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, force, report_context);

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
        MakeParentDirectory(ebuff, force, report_context);

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
        CheckWorkingDirectories(report_context);
    }

    LoadSecretKeys();

    if (!MINUSF)
    {
        SetInputFile("promises.cf");
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
            SetInputFile("failsafe.cf");
        }
        else
        {
            SetInputFile(vbuff);
        }
    }
}

/*******************************************************************/

static void Cf3ParseFiles(Policy *policy, bool check_not_writable_by_others, const ReportContext *report_context)
{
    Rlist *rp, *sl;

    PARSING = true;

    PROMISETIME = time(NULL);

    PolicySetNameSpace(policy, "default");    

    Cf3ParseFile(policy, VINPUTFILE, check_not_writable_by_others);

    // Expand any lists in this list now
    HashVariables(policy, NULL, report_context);
    HashControls(policy);

    if (VINPUTLIST != NULL)
    {
        for (rp = VINPUTLIST; rp != NULL; rp = rp->next)
        {
            if (rp->type != CF_SCALAR)
            {
                CfOut(cf_error, "", "Non-file object in inputs list\n");
            }
            else
            {
                Rval returnval;

                if (strcmp(rp->item, CF_NULL_VALUE) == 0)
                {
                    continue;
                }

                returnval = EvaluateFinalRval("sys", (Rval) {rp->item, rp->type}, true, NULL);

                switch (returnval.rtype)
                {
                case CF_SCALAR:
                    Cf3ParseFile(policy, (char *) returnval.item, check_not_writable_by_others);
                    break;

                case CF_LIST:
                    for (sl = (Rlist *) returnval.item; sl != NULL; sl = sl->next)
                    {
                        Cf3ParseFile(policy, (char *) sl->item, check_not_writable_by_others);
                    }
                    break;
                }

                DeleteRvalItem(returnval);
            }

            HashVariables(policy, NULL, report_context);
            HashControls(policy);
        }
    }

    HashVariables(policy, NULL, report_context);

    PARSING = false;
}

/*******************************************************************/

static int MissingInputFile()
{
    struct stat sb;

    if (cfstat(InputLocation(VINPUTFILE), &sb) == -1)
    {
        CfOut(cf_error, "stat", "There is no readable input file at %s", VINPUTFILE);
        return true;
    }

    return false;
}

/*******************************************************************/

int NewPromiseProposals()
{
    Rlist *rp, *sl;
    struct stat sb;
    int result = false;
    char filename[CF_MAXVARSIZE];

    if (MINUSF)
    {
        snprintf(filename, CF_MAXVARSIZE, "%s/state/validated_%s", CFWORKDIR, CanonifyName(VINPUTFILE));
        MapName(filename);
    }
    else
    {
        snprintf(filename, CF_MAXVARSIZE, "%s/masterfiles/cf_promises_validated", CFWORKDIR);
        MapName(filename);
    }

    if (stat(filename, &sb) != -1)
    {
        PROMISETIME = sb.st_mtime;
    }
    else
    {
        PROMISETIME = 0;
    }

// sanity check

    if (PROMISETIME > time(NULL))
    {
        CfOut(cf_inform, "",
              "!! Clock seems to have jumped back in time - mtime of %s is newer than current time - touching it",
              filename);

        if (utime(filename, NULL) == -1)
        {
            CfOut(cf_error, "utime", "!! Could not touch %s", filename);
        }

        PROMISETIME = 0;
        return true;
    }

    if (cfstat(InputLocation(VINPUTFILE), &sb) == -1)
    {
        CfOut(cf_verbose, "stat", "There is no readable input file at %s", VINPUTFILE);
        return true;
    }

    if (sb.st_mtime > PROMISETIME)
    {
        CfOut(cf_verbose, "", " -> Promises seem to change");
        return true;
    }

// Check the directories first for speed and because non-input/data files should trigger an update

    snprintf(filename, CF_MAXVARSIZE, "%s/inputs", CFWORKDIR);
    MapName(filename);

    if (IsNewerFileTree(filename, PROMISETIME))
    {
        CfOut(cf_verbose, "", " -> Quick search detected file changes");
        return true;
    }

// Check files in case there are any abs paths

    if (VINPUTLIST != NULL)
    {
        for (rp = VINPUTLIST; rp != NULL; rp = rp->next)
        {
            if (rp->type != CF_SCALAR)
            {
                CfOut(cf_error, "", "Non file object %s in list\n", (char *) rp->item);
            }
            else
            {
                Rval returnval = EvaluateFinalRval("sys", (Rval) { rp->item, rp->type }, true, NULL);

                switch (returnval.rtype)
                {
                case CF_SCALAR:

                    if (cfstat(InputLocation((char *) returnval.item), &sb) == -1)
                    {
                        CfOut(cf_error, "stat", "Unreadable promise proposals at %s", (char *) returnval.item);
                        result = true;
                        break;
                    }

                    if (sb.st_mtime > PROMISETIME)
                    {
                        result = true;
                    }

                    break;

                case CF_LIST:

                    for (sl = (Rlist *) returnval.item; sl != NULL; sl = sl->next)
                    {
                        if (cfstat(InputLocation((char *) sl->item), &sb) == -1)
                        {
                            CfOut(cf_error, "stat", "Unreadable promise proposals at %s", (char *) sl->item);
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
                }

                DeleteRvalItem(returnval);

                if (result)
                {
                    break;
                }
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

    return result | ALWAYS_VALIDATE;
}

/*******************************************************************/

ReportContext *OpenReports(const char *agents)
{
    const char *workdir = GetWorkDir();
    char name[CF_BUFSIZE];

    FILE *freport_text = NULL;
    FILE *freport_html = NULL;
    FILE *freport_knowledge = NULL;

    if (SHOWREPORTS)
    {
        snprintf(name, CF_BUFSIZE, "%s%creports%cpromise_output_%s.txt", workdir, FILE_SEPARATOR, FILE_SEPARATOR,
                 agents);

        if ((freport_text = fopen(name, "w")) == NULL)
        {
            CfOut(cf_error, "fopen", "Cannot open output file %s", name);
            freport_text = fopen(NULLFILE, "w");
        }

        snprintf(name, CF_BUFSIZE, "%s%creports%cpromise_output_%s.html", workdir, FILE_SEPARATOR, FILE_SEPARATOR,
                 agents);

        if ((freport_html = fopen(name, "w")) == NULL)
        {
            CfOut(cf_error, "fopen", "Cannot open output file %s", name);
            freport_html = fopen(NULLFILE, "w");
        }

        snprintf(name, CF_BUFSIZE, "%s%cpromise_knowledge.cf", workdir, FILE_SEPARATOR);

        if ((freport_knowledge = fopen(name, "w")) == NULL)
        {
            CfOut(cf_error, "fopen", "Cannot open output file %s", name);
        }

        CfOut(cf_inform, "", " -> Writing knowledge output to %s", workdir);
    }
    else
    {
        snprintf(name, CF_BUFSIZE, NULLFILE);
        if ((freport_text = fopen(name, "w")) == NULL)
        {
            FatalError("Cannot open output file %s", name);
        }

        if ((freport_html = fopen(name, "w")) == NULL)
        {
            FatalError("Cannot open output file %s", name);
        }
    }

    if (!(freport_html && freport_text))
    {
        FatalError("Unable to continue as the null-file is unwritable");
    }


    ReportContext *context = ReportContextNew();
    ReportContextAddWriter(context, REPORT_OUTPUT_TYPE_TEXT, FileWriter(freport_text));
    ReportContextAddWriter(context, REPORT_OUTPUT_TYPE_HTML, FileWriter(freport_html));

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
        CfOut(cf_verbose, "", "Wrote compilation report %s%creports%cpromise_output_%s.txt", CFWORKDIR, FILE_SEPARATOR,
              FILE_SEPARATOR, agents);
        CfOut(cf_verbose, "", "Wrote compilation report %s%creports%cpromise_output_%s.html", CFWORKDIR, FILE_SEPARATOR,
              FILE_SEPARATOR, agents);
        CfOut(cf_verbose, "", "Wrote knowledge map %s%cpromise_knowledge.cf", CFWORKDIR, FILE_SEPARATOR);
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

static void Cf3ParseFile(Policy *policy, char *filename, bool check_not_writable_by_others)
{
    struct stat statbuf;
    char wfilename[CF_BUFSIZE];

    PolicySetNameSpace(policy, "default");    

    strncpy(wfilename, InputLocation(filename), CF_BUFSIZE);

    if (cfstat(wfilename, &statbuf) == -1)
    {
        if (IGNORE_MISSING_INPUTS)
        {
            return;
        }

        CfOut(cf_error, "stat", "Can't stat file \"%s\" for parsing\n", wfilename);
        exit(1);
    }

#ifndef NT
    if (check_not_writable_by_others && (statbuf.st_mode & (S_IWGRP | S_IWOTH)))
    {
        CfOut(cf_error, "", "File %s (owner %ju) is writable by others (security exception)", wfilename, (uintmax_t)statbuf.st_uid);
        exit(1);
    }
#endif

    CfDebug("+++++++++++++++++++++++++++++++++++++++++++++++\n");
    CfOut(cf_verbose, "", "  > Parsing file %s\n", wfilename);
    CfDebug("+++++++++++++++++++++++++++++++++++++++++++++++\n");

    PrependAuditFile(wfilename);

    if (!FileCanOpen(wfilename, "r"))
    {
        printf("Can't open file %s for parsing\n", wfilename);
        exit(1);
    }

    ParserParseFile(policy, wfilename);
}

/*******************************************************************/

Constraint *ControlBodyConstraints(const Policy *policy, AgentType agent)
{
    for (const Body *body = policy->bodies; body != NULL; body = body->next)
    {
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
    CfOut(cf_verbose, "", "SET Syslog FACILITY = %s\n", retval);

    CloseLog();
    OpenLog(ParseFacility(retval));
    SetSyslogFacility(ParseFacility(retval));
}

/**************************************************************/

Bundle *GetBundle(const Policy *policy, const char *name, const char *agent)
{

    // We don't need to check for the namespace here, as it is prefixed to the name already
 
    for (Bundle *bp = policy->bundles; bp != NULL; bp = bp->next)       /* get schedule */
    {
        if (strcmp(bp->name, name) == 0)
        {
            if (agent)
            {
                if ((strcmp(bp->type, agent) == 0) || (strcmp(bp->type, "common") == 0))
                {
                    return bp;
                }
                else
                {
                    CfOut(cf_verbose, "", "The bundle called %s is not of type %s\n", name, agent);
                }
            }
            else
            {
                return bp;
            }
        }
    }

    return NULL;
}

/**************************************************************/

SubType *GetSubTypeForBundle(char *type, Bundle *bp)
{
    SubType *sp;

    if (bp == NULL)
    {
        return NULL;
    }

    for (sp = bp->subtypes; sp != NULL; sp = sp->next)
    {
        if (strcmp(type, sp->name) == 0)
        {
            return sp;
        }
    }

    return NULL;
}

/**************************************************************/

void BannerBundle(Bundle *bp, Rlist *params)
{
    CfOut(cf_verbose, "", "\n");
    CfOut(cf_verbose, "", "*****************************************************************\n");

    if (VERBOSE || DEBUG)
    {
        printf("%s> BUNDLE %s", VPREFIX, bp->name);
    }

    if (params && (VERBOSE || DEBUG))
    {
        printf("(");
        ShowRlist(stdout, params);
        printf(" )\n");
    }
    else
    {
        if (VERBOSE || DEBUG)
            printf("\n");
    }

    CfOut(cf_verbose, "", "*****************************************************************\n");
    CfOut(cf_verbose, "", "\n");

}

/**************************************************************/

void BannerSubBundle(Bundle *bp, Rlist *params)
{
    CfOut(cf_verbose, "", "\n");
    CfOut(cf_verbose, "", "      * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");

    if (VERBOSE || DEBUG)
    {
        printf("%s>       BUNDLE %s", VPREFIX, bp->name);
    }

    if (params && (VERBOSE || DEBUG))
    {
        printf("(");
        ShowRlist(stdout, params);
        printf(" )\n");
    }
    else
    {
        if (VERBOSE || DEBUG)
            printf("\n");
    }
    CfOut(cf_verbose, "", "      * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");
    CfOut(cf_verbose, "", "\n");

}

/**************************************************************/

void PromiseBanner(Promise *pp)
{
    char handle[CF_MAXVARSIZE];
    const char *sp;

    if ((sp = GetConstraintValue("handle", pp, CF_SCALAR)) || (sp = PromiseID(pp)))
    {
        strncpy(handle, sp, CF_MAXVARSIZE - 1);
    }
    else
    {
        strcpy(handle, "(enterprise only)");
    }

    CfOut(cf_verbose, "", "\n");
    CfOut(cf_verbose, "", "    .........................................................\n");

    if (VERBOSE || DEBUG)
    {
        printf("%s>     Promise's handle: %s\n", VPREFIX, handle);
        printf("%s>     Promise made by: \"%s\"", VPREFIX, pp->promiser);
    }

    if (pp->promisee.item)
    {
        if (VERBOSE)
        {
            printf("\n%s>     Promise made to (stakeholders): ", VPREFIX);
            ShowRval(stdout, pp->promisee);
        }
    }

    if (VERBOSE)
    {
        printf("\n");
    }

    if (pp->ref)
    {
        CfOut(cf_verbose, "", "\n");
        CfOut(cf_verbose, "", "    Comment:  %s\n", pp->ref);
    }

    CfOut(cf_verbose, "", "    .........................................................\n");
    CfOut(cf_verbose, "", "\n");
}

/*********************************************************************/

static void CheckWorkingDirectories(const ReportContext *report_context)
/* NOTE: We do not care about permissions (ACLs) in windows */
{
    struct stat statbuf;
    char vbuff[CF_BUFSIZE];

    CfDebug("CheckWorkingDirectories()\n");

    if (uname(&VSYSNAME) == -1)
    {
        CfOut(cf_error, "uname", "!!! Couldn't get kernel name info!");
        memset(&VSYSNAME, 0, sizeof(VSYSNAME));
    }

    snprintf(vbuff, CF_BUFSIZE, "%s%c.", CFWORKDIR, FILE_SEPARATOR);
    MakeParentDirectory(vbuff, false, report_context);

    CfOut(cf_verbose, "", "Making sure that locks are private...\n");

    if (chown(CFWORKDIR, getuid(), getgid()) == -1)
    {
        CfOut(cf_error, "chown", "Unable to set owner on %s to %ju.%ju", CFWORKDIR, (uintmax_t)getuid(), (uintmax_t)getgid());
    }

    if (cfstat(CFWORKDIR, &statbuf) != -1)
    {
        /* change permissions go-w */
        cf_chmod(CFWORKDIR, (mode_t) (statbuf.st_mode & ~022));
    }

    snprintf(vbuff, CF_BUFSIZE, "%s%cstate%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(vbuff, false, report_context);

    if (strlen(CFPRIVKEYFILE) == 0)
    {
        snprintf(CFPRIVKEYFILE, CF_BUFSIZE, "%s%cppkeys%clocalhost.priv", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        snprintf(CFPUBKEYFILE, CF_BUFSIZE, "%s%cppkeys%clocalhost.pub", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    }

    CfOut(cf_verbose, "", "Checking integrity of the state database\n");
    snprintf(vbuff, CF_BUFSIZE, "%s%cstate", CFWORKDIR, FILE_SEPARATOR);

    if (cfstat(vbuff, &statbuf) == -1)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cstate%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, false, report_context);

        if (chown(vbuff, getuid(), getgid()) == -1)
        {
            CfOut(cf_error, "chown", "Unable to set owner on %s to %jd.%jd", vbuff, (uintmax_t)getuid(), (uintmax_t)getgid());
        }

        cf_chmod(vbuff, (mode_t) 0755);
    }
    else
    {
#ifndef MINGW
        if (statbuf.st_mode & 022)
        {
            CfOut(cf_error, "", "UNTRUSTED: State directory %s (mode %jo) was not private!\n", CFWORKDIR,
                  (uintmax_t)(statbuf.st_mode & 0777));
        }
#endif /* NOT MINGW */
    }

    CfOut(cf_verbose, "", "Checking integrity of the module directory\n");

    snprintf(vbuff, CF_BUFSIZE, "%s%cmodules", CFWORKDIR, FILE_SEPARATOR);

    if (cfstat(vbuff, &statbuf) == -1)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cmodules%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, false, report_context);

        if (chown(vbuff, getuid(), getgid()) == -1)
        {
            CfOut(cf_error, "chown", "Unable to set owner on %s to %ju.%ju", vbuff, (uintmax_t)getuid(), (uintmax_t)getgid());
        }

        cf_chmod(vbuff, (mode_t) 0700);
    }
    else
    {
#ifndef MINGW
        if (statbuf.st_mode & 022)
        {
            CfOut(cf_error, "", "UNTRUSTED: Module directory %s (mode %jo) was not private!\n", vbuff,
                  (uintmax_t)(statbuf.st_mode & 0777));
        }
#endif /* NOT MINGW */
    }

    CfOut(cf_verbose, "", "Checking integrity of the PKI directory\n");

    snprintf(vbuff, CF_BUFSIZE, "%s%cppkeys", CFWORKDIR, FILE_SEPARATOR);

    if (cfstat(vbuff, &statbuf) == -1)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cppkeys%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, false, report_context);

        cf_chmod(vbuff, (mode_t) 0700); /* Keys must be immutable to others */
    }
    else
    {
#ifndef MINGW
        if (statbuf.st_mode & 077)
        {
            FatalError("UNTRUSTED: Private key directory %s%cppkeys (mode %jo) was not private!\n", CFWORKDIR,
                       FILE_SEPARATOR, (uintmax_t)(statbuf.st_mode & 0777));
        }
#endif /* NOT MINGW */
    }
}

/*******************************************************************/
/* Level 2                                                         */
/*******************************************************************/

static char *InputLocation(char *filename)
{
    static char wfilename[CF_BUFSIZE], path[CF_BUFSIZE];

    if (MINUSF && (filename != VINPUTFILE) && IsFileOutsideDefaultRepository(VINPUTFILE)
        && !IsAbsoluteFileName(filename))
    {
        /* If -f assume included relative files are in same directory */
        strncpy(path, VINPUTFILE, CF_BUFSIZE - 1);
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

void SetInputFile(const char *name)
{
    strlcpy(VINPUTFILE, name, CF_BUFSIZE);
}

/*******************************************************************/

void CompilationReport(Policy *policy, char *fname)
{
    if (THIS_AGENT_TYPE != AGENT_TYPE_COMMON)
    {
        return;
    }

#if defined(HAVE_NOVA)
    ReportContext *compilation_report_context = Nova_OpenCompilationReportFiles(fname);
#else
    ReportContext *compilation_report_context = OpenCompilationReportFiles(fname);
#endif


    ShowPromises(compilation_report_context, REPORT_OUTPUT_TYPE_TEXT, policy->bundles, policy->bodies);
    ShowPromises(compilation_report_context, REPORT_OUTPUT_TYPE_HTML, policy->bundles, policy->bodies);

    ReportContextDestroy(compilation_report_context);
}

/****************************************************************************/

ReportContext *OpenCompilationReportFiles(const char *fname)
{
    char filename[CF_BUFSIZE];
    FILE *freport_text = NULL, *freport_html = NULL;

    snprintf(filename, CF_BUFSIZE - 1, "%s.txt", fname);
    CfOut(cf_inform, "", "Summarizing promises as text to %s\n", filename);

    if ((freport_text = fopen(filename, "w")) == NULL)
    {
        FatalError("Could not write output log to %s", filename);
    }

    snprintf(filename, CF_BUFSIZE - 1, "%s.html", fname);
    CfOut(cf_inform, "", "Summarizing promises as html to %s\n", filename);

    if ((freport_text = fopen(filename, "w")) == NULL)
    {
        FatalError("Could not write output log to %s", filename);
    }

    ReportContext *context = ReportContextNew();
    ReportContextAddWriter(context, REPORT_OUTPUT_TYPE_TEXT, FileWriter(freport_text));
    ReportContextAddWriter(context, REPORT_OUTPUT_TYPE_HTML, FileWriter(freport_html));

    return context;
}

/*******************************************************************/

static void VerifyPromises(Policy *policy, Rlist *bundlesequence,
                           const ReportContext *report_context)
{
    Bundle *bp;
    SubType *sp;
    Promise *pp;
    Body *bdp;
    Rlist *rp;
    FnCall *fp;
    char *scope;

    if (REQUIRE_COMMENTS == CF_UNDEFINED)
    {
        for (bdp = policy->bodies; bdp != NULL; bdp = bdp->next)        /* get schedule */
        {
            if ((strcmp(bdp->name, "control") == 0) && (strcmp(bdp->type, "common") == 0))
            {
                REQUIRE_COMMENTS = GetRawBooleanConstraint("require_comments", bdp->conlist);
                break;
            }
        }
    }

    for (rp = BODYPARTS; rp != NULL; rp = rp->next)
    {
        char namespace[CF_BUFSIZE],name[CF_BUFSIZE];
        char fqname[CF_BUFSIZE];

        // This is a bit messy because tracking the namespace is not natural with the existing structures here

        sscanf((char *)rp->item,"%[^:]:%s",namespace,name); 

        if (strcmp(namespace,"default") == 0)
        {
            strcpy(fqname,name);
        }
        else
        {
            strcpy(fqname,(char *)rp->item);
        }
    
        if (!IsBody(policy->bodies, namespace, fqname))
        {
            CfOut(cf_error, "", "Undeclared unparameterized promise body \"%s()\" was referenced in a promise in namespace \"%s\"\n", fqname, namespace);
            ERRORCOUNT++;
        }
    }

/* Check for undefined subbundles */

    for (rp = SUBBUNDLES; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case CF_SCALAR:

            if (!IGNORE_MISSING_BUNDLES && !IsCf3VarString(rp->item) && !IsBundle(policy->bundles, (char *) rp->item))
            {
                CfOut(cf_error, "", "Undeclared promise bundle \"%s()\" was referenced in a promise\n", (char *) rp->item);
                ERRORCOUNT++;
            }
            break;

        case CF_FNCALL:

            fp = (FnCall *) rp->item;

            if (!IGNORE_MISSING_BUNDLES && !IsCf3VarString(fp->name) && !IsBundle(policy->bundles, fp->name))
            {
                CfOut(cf_error, "", "Undeclared promise bundle \"%s()\" was referenced in a promise\n", fp->name);
                ERRORCOUNT++;
            }
            break;
        }
    }

/* Now look once through ALL the bundles themselves */

    for (bp = policy->bundles; bp != NULL; bp = bp->next)       /* get schedule */
    {
        scope = bp->name;
        THIS_BUNDLE = bp->name;

        for (sp = bp->subtypes; sp != NULL; sp = sp->next)      /* get schedule */
        {
            for (pp = sp->promiselist; pp != NULL; pp = pp->next)
            {
                ExpandPromise(AGENT_TYPE_COMMON, scope, pp, NULL, report_context);
            }
        }
    }

    HashVariables(policy, NULL, report_context);
    HashControls(policy);

    /* Now look once through the sequences bundles themselves */
    if (VerifyBundleSequence(policy, AGENT_TYPE_COMMON, bundlesequence) == false)
    {
        FatalError("Errors in promise bundles");
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
    Chop(AUDITPTR->date);
    AUDITPTR->version = NULL;
    VAUDIT = AUDITPTR;
}

/*******************************************************************/
/* Level 3                                                         */
/*******************************************************************/

static void CheckVariablePromises(char *scope, Promise *varlist)
{
    Promise *pp;
    int allow_redefine = false;

    CfDebug("CheckVariablePromises()\n");

    for (pp = varlist; pp != NULL; pp = pp->next)
    {
        ConvergeVarHashPromise(scope, pp, allow_redefine);
    }
}

/*******************************************************************/

static void CheckCommonClassPromises(Promise *classlist, const ReportContext *report_context)
{
    Promise *pp;

    CfOut(cf_verbose, "", " -> Checking common class promises...\n");

    for (pp = classlist; pp != NULL; pp = pp->next)
    {
        ExpandPromise(AGENT_TYPE_AGENT, THIS_BUNDLE, pp, KeepClassContextPromise, report_context);
    }
}

/*******************************************************************/

static void CheckControlPromises(char *scope, char *agent, Constraint *controllist)
{
    Constraint *cp;
    const BodySyntax *bp = NULL;
    Rlist *rp;
    int i = 0;
    Rval returnval;

    CfDebug("CheckControlPromises(%s)\n", agent);

    for (i = 0; CF_ALL_BODIES[i].bs != NULL; i++)
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

    for (cp = controllist; cp != NULL; cp = cp->next)
    {
        if (IsExcluded(cp->classes, NULL))
        {
            continue;
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[cfg_bundlesequence].lval) == 0)
        {
            returnval = ExpandPrivateRval(CONTEXTID, cp->rval);
        }
        else
        {
            returnval = EvaluateFinalRval(CONTEXTID, cp->rval, true, NULL);
        }

        DeleteVariable(scope, cp->lval);

        if (!AddVariableHash(scope, cp->lval, returnval,
                             GetControlDatatype(cp->lval, bp), cp->audit->filename, cp->offset.line))
        {
            CfOut(cf_error, "", " !! Rule from %s at/before line %zu\n", cp->audit->filename, cp->offset.line);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[cfg_output_prefix].lval) == 0)
        {
            strncpy(VPREFIX, returnval.item, CF_MAXVARSIZE);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[cfg_domain].lval) == 0)
        {
            strcpy(VDOMAIN, cp->rval.item);
            CfOut(cf_verbose, "", "SET domain = %s\n", VDOMAIN);
            DeleteScalar("sys", "domain");
            DeleteScalar("sys", "fqhost");
            snprintf(VFQNAME, CF_MAXVARSIZE, "%s.%s", VUQNAME, VDOMAIN);
            NewScalar("sys", "fqhost", VFQNAME, cf_str);
            NewScalar("sys", "domain", VDOMAIN, cf_str);
            DeleteClass("undefined_domain", NULL);
            HardClass(VDOMAIN);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[cfg_ignore_missing_inputs].lval) == 0)
        {
            CfOut(cf_verbose, "", "SET ignore_missing_inputs %s\n", ScalarRvalValue(cp->rval));
            IGNORE_MISSING_INPUTS = GetBoolean(cp->rval.item);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[cfg_ignore_missing_bundles].lval) == 0)
        {
            CfOut(cf_verbose, "", "SET ignore_missing_bundles %s\n", ScalarRvalValue(cp->rval));
            IGNORE_MISSING_BUNDLES = GetBoolean(cp->rval.item);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[cfg_goalpatterns].lval) == 0)
        {
            GOALS = NULL;
            for (rp = (Rlist *) returnval.item; rp != NULL; rp = rp->next)
            {
                PrependRScalar(&GOALS, rp->item, CF_SCALAR);
            }
            CfOut(cf_verbose, "", "SET goal_patterns list\n");
            continue;
        }
        
        DeleteRvalItem(returnval);
    }
}

/*******************************************************************/

static void SetAuditVersion()
{
    Rval rval = { NULL, 'x' };  /* FIXME: why it is initialized? */

    /* In addition, each bundle can have its own version */

    switch (GetVariable("control_common", "cfinputs_version", &rval))
    {
    case cf_str:
        if (rval.rtype != CF_SCALAR)
        {
            yyerror("non-scalar version string");
        }
        AUDITPTR->version = xstrdup((char *) rval.item);
        break;

    default:
        AUDITPTR->version = xstrdup("no specified version");
        break;
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

    printf(".TH %s 8 \"Maintenance Commands\"\n", GetArg0(component));
    printf(".SH NAME\n%s\n\n", component);

    printf(".SH SYNOPSIS:\n\n %s [options]\n\n.SH DESCRIPTION:\n\n%s\n", GetArg0(component), id);

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
            CfOut(cf_error, "unlink", "Unable to remove pid file");
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
        CfOut(cf_inform, "fopen", "Could not write to PID file %s\n", filename);
        return;
    }

    fprintf(fp, "%" PRIuMAX "\n", (uintmax_t)getpid());

    fclose(fp);
}

/*******************************************************************/

void HashVariables(Policy *policy, const char *name, const ReportContext *report_context)
{
    Bundle *bp;
    SubType *sp;

    CfOut(cf_verbose, "", "Initiate variable convergence...\n");

    for (bp = policy->bundles; bp != NULL; bp = bp->next)       /* get schedule */
    {
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

        for (sp = bp->subtypes; sp != NULL; sp = sp->next)      /* get schedule */
        {
            if (strcmp(sp->name, "vars") == 0)
            {
                CheckVariablePromises(bp->name, sp->promiselist);
            }

            // We must also set global classes here?

            if (strcmp(bp->type, "common") == 0 && strcmp(sp->name, "classes") == 0)
            {
                CheckCommonClassPromises(sp->promiselist, report_context);
            }

        }
    }
}

/*******************************************************************/

void HashControls(const Policy *policy)
{
    Body *bdp;
    char buf[CF_BUFSIZE];

/* Only control bodies need to be hashed like variables */

    for (bdp = policy->bodies; bdp != NULL; bdp = bdp->next)    /* get schedule */
    {
        if (strcmp(bdp->name, "control") == 0)
        {
            snprintf(buf, CF_BUFSIZE, "%s_%s", bdp->name, bdp->type);
            CfDebug("Initiate control variable convergence...%s\n", buf);
            DeleteScope(buf);
            SetNewScope(buf);
            CheckControlPromises(buf, bdp->type, bdp->conlist);
        }
    }
}

/********************************************************************/

static bool VerifyBundleSequence(const Policy *policy, AgentType agent, Rlist *bundlesequence)
{
    Rlist *rp;
    char *name;
    Rval retval;
    int ok = true;
    FnCall *fp;

    if ((THIS_AGENT_TYPE != AGENT_TYPE_AGENT) && (THIS_AGENT_TYPE != AGENT_TYPE_KNOW) && (THIS_AGENT_TYPE != AGENT_TYPE_COMMON) && (THIS_AGENT_TYPE != AGENT_TYPE_GENDOC))
    {
        return true;
    }

    if (bundlesequence)
    {
        return true;
    }

    if (GetVariable("control_common", "bundlesequence", &retval) == cf_notype)
    {
        CfOut(cf_error, "", " !!! No bundlesequence in the common control body");
        return false;
    }

    if (retval.rtype != CF_LIST)
    {
        FatalError("Promised bundlesequence was not a list");
    }

    if ((agent != AGENT_TYPE_AGENT) && (agent != AGENT_TYPE_COMMON))
    {
        return true;
    }

    for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case CF_SCALAR:
            name = (char *) rp->item;
            break;

        case CF_FNCALL:
            fp = (FnCall *) rp->item;
            name = (char *) fp->name;
            break;

        default:
            name = NULL;
            CfOut(cf_error, "", "Illegal item found in bundlesequence: ");
            ShowRval(stdout, (Rval) {rp->item, rp->type});
            printf(" = %c\n", rp->type);
            ok = false;
            break;
        }

        if (strcmp(name, CF_NULL_VALUE) == 0)
        {
            continue;
        }

        if (!IGNORE_MISSING_BUNDLES && !GetBundle(policy, name, NULL))
        {
            CfOut(cf_error, "", "Bundle \"%s\" listed in the bundlesequence is not a defined bundle\n", name);
            ok = false;
        }
    }

    return ok;
}

/*******************************************************************/

GenericAgentConfig GenericAgentDefaultConfig(AgentType agent_type)
{
    GenericAgentConfig config = { 0 };

    config.bundlesequence = NULL;
    config.verify_promises = true;

    return config;
}

