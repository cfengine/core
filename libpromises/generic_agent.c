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
#include "locks.h"
#include "scope.h"
#include "atexit.h"
#include "unix.h"
#include "logging_old.h"
#include "logging.h"
#include "client_code.h"
#include "string_lib.h"
#include "exec_tools.h"
#include "list.h"
#include "misc_lib.h"
#include "fncall.h"
#include "rlist.h"
#include "syslog_client.h"
#include "audit.h"
#include "verify_classes.h"
#include "verify_vars.h"

#ifdef HAVE_NOVA
# include "cf.nova.h"
#endif

#include <assert.h>

static pthread_once_t pid_cleanup_once = PTHREAD_ONCE_INIT;

static char PIDFILE[CF_BUFSIZE];

static void VerifyPromises(EvalContext *ctx, Policy *policy, GenericAgentConfig *config);
static void CheckWorkingDirectories(EvalContext *ctx);
static Policy *Cf3ParseFile(const GenericAgentConfig *config, const char *input_path);
static Policy *Cf3ParseFiles(EvalContext *ctx, GenericAgentConfig *config, const Rlist *inputs);
static bool MissingInputFile(const char *input_file);
static void CheckControlPromises(EvalContext *ctx, GenericAgentConfig *config, const Body *control_body);
static void CheckVariablePromises(EvalContext *ctx, Seq *var_promises);
static void CheckCommonClassPromises(EvalContext *ctx, Seq *class_promises);

#if !defined(__MINGW32__)
static void OpenLog(int facility);
#endif
static bool VerifyBundleSequence(EvalContext *ctx, const Policy *policy, const GenericAgentConfig *config);

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

void CheckForPolicyHub(EvalContext *ctx)
{
    struct stat sb;
    char name[CF_BUFSIZE];

    snprintf(name, sizeof(name), "%s/state/am_policy_hub", CFWORKDIR);
    MapName(name);

    if (stat(name, &sb) != -1)
    {
        EvalContextHeapAddHard(ctx, "am_policy_hub");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Additional class defined: am_policy_hub");
    }
}

/*****************************************************************************/

void GenericAgentDiscoverContext(EvalContext *ctx, GenericAgentConfig *config)
{
#ifdef HAVE_NOVA
    CF_DEFAULT_DIGEST = HASH_METHOD_SHA256;
    CF_DEFAULT_DIGEST_LEN = CF_SHA256_LEN;
#else
    CF_DEFAULT_DIGEST = HASH_METHOD_MD5;
    CF_DEFAULT_DIGEST_LEN = CF_MD5_LEN;
#endif

    InitializeGA(ctx, config);

    SetReferenceTime(ctx, true);
    SetStartTime();
    SanitizeEnvironment();

    THIS_AGENT_TYPE = config->agent_type;
    EvalContextHeapAddHard(ctx, CF_AGENTTYPES[config->agent_type]);

#ifdef HAVE_NOVA
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> This is CFEngine Enterprise\n");
#endif

    GetNameInfo3(ctx, config->agent_type);
    GetInterfacesInfo(ctx, config->agent_type);

    Get3Environment(ctx, config->agent_type);
    BuiltinClasses(ctx);
    OSClasses(ctx);

    EvalContextHeapPersistentLoadAll(ctx);
    LoadSystemConstants(ctx);

    if (BOOTSTRAP)
    {
        CheckAutoBootstrap(ctx);
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

    SetPolicyServer(ctx, POLICY_SERVER);
}

static bool IsPolicyPrecheckNeeded(EvalContext *ctx, GenericAgentConfig *config, bool force_validation)
{
    bool check_policy = false;

    if (IsFileOutsideDefaultRepository(config->input_file))
    {
        check_policy = true;
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Input file is outside default repository, validating it");
    }
    if (NewPromiseProposals(ctx, config, NULL))
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

bool GenericAgentCheckPolicy(EvalContext *ctx, GenericAgentConfig *config, bool force_validation)
{
    if (!MissingInputFile(config->input_file))
    {
        if (IsPolicyPrecheckNeeded(ctx, config, force_validation))
        {
            bool policy_check_ok = CheckPromises(config);

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

int CheckPromises(const GenericAgentConfig *config)
{
    char cmd[CF_BUFSIZE];

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Verifying the syntax of the inputs...\n");
    {
        char cfpromises[CF_MAXVARSIZE];
        snprintf(cfpromises, sizeof(cfpromises), "%s%cbin%ccf-promises%s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR,
                 EXEC_SUFFIX);

        struct stat sb;
        if (stat(cfpromises, &sb) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "cf-promises%s needs to be installed in %s%cbin for pre-validation of full configuration",
                  EXEC_SUFFIX, CFWORKDIR, FILE_SEPARATOR);
            return false;
        }

        if (config->bundlesequence)
        {
            snprintf(cmd, sizeof(cmd), "\"%s\" \"", cfpromises);
        }
        else
        {
            snprintf(cmd, sizeof(cmd), "\"%s\" -c \"", cfpromises);
        }
    }

    strlcat(cmd, config->input_file, CF_BUFSIZE);

    strlcat(cmd, "\"", CF_BUFSIZE);

    if (config->bundlesequence)
    {
        strlcat(cmd, " -b \"", CF_BUFSIZE);
        for (const Rlist *rp = config->bundlesequence; rp; rp = rp->next)
        {
            const char *bundle_ref = rp->item;
            strlcat(cmd, bundle_ref, CF_BUFSIZE);

            if (rp->next)
            {
                strlcat(cmd, ",", CF_BUFSIZE);
            }
        }
        strlcat(cmd, "\"", CF_BUFSIZE);
    }

    if (BOOTSTRAP)
    {
        // avoids license complains from commercial cf-promises during bootstrap - see Nova_CheckLicensePromise
        strlcat(cmd, " -D bootstrap_mode", CF_BUFSIZE);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking policy with command \"%s\"", cmd);

    if (ShellCommandReturnsZero(cmd, true))
    {
        if (!IsFileOutsideDefaultRepository(config->input_file))
        {
            char filename[CF_MAXVARSIZE];

            if (MINUSF)
            {
                snprintf(filename, CF_MAXVARSIZE, "%s/state/validated_%s", CFWORKDIR, CanonifyName(config->input_file));
                MapName(filename);
            }
            else
            {
                snprintf(filename, CF_MAXVARSIZE, "%s/masterfiles/cf_promises_validated", CFWORKDIR);
                MapName(filename);
            }

            MakeParentDirectory(filename, true);

            int fd = creat(filename, 0600);
            if (fd != -1)
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


static void ShowContext(EvalContext *ctx)
{
    {
        printf("%s>  -> Hard classes = { ", VPREFIX);

        Seq *hard_contexts = SeqNew(1000, NULL);
        SetIterator it = EvalContextHeapIteratorHard(ctx);
        char *context = NULL;
        while ((context = SetIteratorNext(&it)))
        {
            if (!EvalContextHeapContainsNegated(ctx, context))
            {
                SeqAppend(hard_contexts, context);
            }
        }

        SeqSort(hard_contexts, (SeqItemComparator)strcmp, NULL);

        for (size_t i = 0; i < SeqLength(hard_contexts); i++)
        {
            const char *context = SeqAt(hard_contexts, i);
            printf("%s ", context);
        }

        printf("}\n");
        SeqDestroy(hard_contexts);
    }

    {
        printf("%s>  -> Additional classes = { ", VPREFIX);

        Seq *soft_contexts = SeqNew(1000, NULL);
        SetIterator it = EvalContextHeapIteratorSoft(ctx);
        char *context = NULL;
        while ((context = SetIteratorNext(&it)))
        {
            if (!EvalContextHeapContainsNegated(ctx, context))
            {
                SeqAppend(soft_contexts, context);
            }
        }

        SeqSort(soft_contexts, (SeqItemComparator)strcmp, NULL);

        for (size_t i = 0; i < SeqLength(soft_contexts); i++)
        {
            const char *context = SeqAt(soft_contexts, i);
            printf("%s ", context);
        }

        printf("}\n");
        SeqDestroy(soft_contexts);
    }

    {
        printf("%s>  -> Negated Classes = { ", VPREFIX);

        StringSetIterator it = EvalContextHeapIteratorNegated(ctx);
        const char *context = NULL;
        while ((context = StringSetIteratorNext(&it)))
        {
            printf("%s ", context);
        }

        printf("}\n");
    }
}

Policy *GenericAgentLoadPolicy(EvalContext *ctx, GenericAgentConfig *config)
{
    PROMISETIME = time(NULL);

    Policy *main_policy = Cf3ParseFile(config, config->input_file);

    if (main_policy)
    {
        PolicyHashVariables(ctx, main_policy);
        HashControls(ctx, main_policy, config);

        if (PolicyIsRunnable(main_policy))
        {
            Policy *aux_policy = Cf3ParseFiles(ctx, config, InputFiles(ctx, main_policy));
            if (aux_policy)
            {
                main_policy = PolicyMerge(main_policy, aux_policy);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Syntax errors were found in policy files included from the main policy");
                exit(EXIT_FAILURE); // TODO: do not exit
            }
        }
    }
    else
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Syntax errors were found in the main policy file");
        exit(EXIT_FAILURE); // TODO: do not exit
    }

    {
        Seq *errors = SeqNew(100, PolicyErrorDestroy);

        if (PolicyCheckPartial(main_policy, errors))
        {
            if (!config->bundlesequence && (PolicyIsRunnable(main_policy) || config->check_runnable))
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", "Running full policy integrity checks");
                PolicyCheckRunnable(ctx, main_policy, errors, config->ignore_missing_bundles);
            }
        }

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
    }

    if (VERBOSE || DEBUG)
    {
        ShowContext(ctx);
    }

    if (main_policy)
    {
        VerifyPromises(ctx, main_policy, config);
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

void InitializeGA(EvalContext *ctx, GenericAgentConfig *config)
{
    int force = false;
    struct stat statbuf, sb;
    char vbuff[CF_BUFSIZE];
    char ebuff[CF_EXPANDSIZE];

    SHORT_CFENGINEPORT = htons((unsigned short) 5308);
    snprintf(STR_CFENGINEPORT, 15, "5308");

    EvalContextHeapAddHard(ctx, "any");

#if defined HAVE_NOVA
    EvalContextHeapAddHard(ctx, "nova_edition");
    EvalContextHeapAddHard(ctx, "enterprise_edition");
#else
    EvalContextHeapAddHard(ctx, "community_edition");
#endif

    strcpy(VPREFIX, GetConsolePrefix());

    if (VERBOSE)
    {
        EvalContextHeapAddHard(ctx, "verbose_mode");
    }

    if (INFORM)
    {
        EvalContextHeapAddHard(ctx, "inform_mode");
    }

    if (DEBUG)
    {
        EvalContextHeapAddHard(ctx, "debug_mode");
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "CFEngine - autonomous configuration engine - commence self-diagnostic prelude\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "------------------------------------------------------------------------\n");

/* Define trusted directories */

    {
        const char *workdir = GetWorkDir();
        if (!workdir)
        {
            FatalError(ctx, "Error determining working directory");
        }

        strcpy(CFWORKDIR, workdir);
        MapName(CFWORKDIR);
    }

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

        if (stat(vbuff, &sb) == -1)
        {
            FatalError(ctx, " !!! No access to WORKSPACE/inputs dir");
        }
        else
        {
            chmod(vbuff, sb.st_mode | 0700);
        }

        snprintf(vbuff, CF_BUFSIZE, "%s%coutputs", CFWORKDIR, FILE_SEPARATOR);

        if (stat(vbuff, &sb) == -1)
        {
            FatalError(ctx, " !!! No access to WORKSPACE/outputs dir");
        }
        else
        {
            chmod(vbuff, sb.st_mode | 0700);
        }

        sprintf(ebuff, "%s%cstate%ccf_procs", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(ebuff, force);

        if (stat(ebuff, &statbuf) == -1)
        {
            CreateEmptyFile(ebuff);
        }

        sprintf(ebuff, "%s%cstate%ccf_rootprocs", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);

        if (stat(ebuff, &statbuf) == -1)
        {
            CreateEmptyFile(ebuff);
        }

        sprintf(ebuff, "%s%cstate%ccf_otherprocs", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);

        if (stat(ebuff, &statbuf) == -1)
        {
            CreateEmptyFile(ebuff);
        }
    }

    OpenNetwork();

    CryptoInitialize();

    if (!LOOKUP)
    {
        CheckWorkingDirectories(ctx);
    }

    if (!LoadSecretKeys())
    {
        FatalError(ctx, "Could not load secret keys");
    }

    if (!MINUSF)
    {
        GenericAgentConfigSetInputFile(config, GetWorkDir(), "promises.cf");
    }

    DetermineCfenginePort();

    VIFELAPSED = 1;
    VEXPIREAFTER = 1;

    setlinebuf(stdout);

    if (BOOTSTRAP)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cinputs%cfailsafe.cf", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);

#ifndef HAVE_NOVA
        if (stat(vbuff, &statbuf) == -1)
        {
            GenericAgentConfigSetInputFile(config, GetWorkDir(), "failsafe.cf");
        }
        else
#endif
        {
            GenericAgentConfigSetInputFile(config, GetWorkDir(), vbuff);
        }
    }
}

/*******************************************************************/

static Policy *Cf3ParseFiles(EvalContext *ctx, GenericAgentConfig *config, const Rlist *inputs)
{
    Policy *policy = PolicyNew();
    bool contains_parse_errors = false;

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

            returnval = EvaluateFinalRval(ctx, "sys", (Rval) {rp->item, rp->type}, true, NULL);

            Policy *aux_policy = NULL;
            switch (returnval.type)
            {
            case RVAL_TYPE_SCALAR:
                aux_policy = Cf3ParseFile(config, GenericAgentResolveInputPath(config, returnval.item));
                break;

            case RVAL_TYPE_LIST:
                aux_policy = Cf3ParseFiles(ctx, config, returnval.item);
                break;

            default:
                ProgrammingError("Unknown type in input list for parsing: %d", returnval.type);
                break;
            }

            if (aux_policy)
            {
                policy = PolicyMerge(policy, aux_policy);
            }
            else
            {
                contains_parse_errors = true;
            }

            RvalDestroy(returnval);
        }

        PolicyHashVariables(ctx, policy);
        HashControls(ctx, policy, config);
    }

    PolicyHashVariables(ctx, policy);

    if (contains_parse_errors)
    {
        PolicyDestroy(policy);
        return NULL;
    }
    else
    {
        return policy;
    }
}

/*******************************************************************/

static bool MissingInputFile(const char *input_file)
{
    struct stat sb;

    if (stat(input_file, &sb) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "stat", "There is no readable input file at %s", input_file);
        return true;
    }

    return false;
}

/*******************************************************************/

int NewPromiseProposals(EvalContext *ctx, const GenericAgentConfig *config, const Rlist *input_files)
{
    Rlist *sl;
    struct stat sb;
    int result = false;
    char filename[CF_MAXVARSIZE];
    time_t validated_at;

    if (MINUSF)
    {
        snprintf(filename, CF_MAXVARSIZE, "%s/state/validated_%s", CFWORKDIR, CanonifyName(config->original_input_file));
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

    if (stat(config->input_file, &sb) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "stat", "There is no readable input file at %s", config->input_file);
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
            Rval returnval = EvaluateFinalRval(ctx, "sys", (Rval) { rp->item, rp->type }, true, NULL);

            switch (returnval.type)
            {
            case RVAL_TYPE_SCALAR:

                if (stat(GenericAgentResolveInputPath(config, (char *) returnval.item), &sb) == -1)
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
                    if (stat(GenericAgentResolveInputPath(config, (char *) sl->item), &sb) == -1)
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

    if ((stat(filename, &sb) != -1) && (sb.st_mtime > PROMISETIME))
    {
        result = true;
    }

    return result;
}

/*
 * The difference between filename and input_input file is that the latter is the file specified by -f or
 * equivalently the file containing body common control. This will hopefully be squashed in later refactoring.
 */
static Policy *Cf3ParseFile(const GenericAgentConfig *config, const char *input_path)
{
    struct stat statbuf;

    if (stat(input_path, &statbuf) == -1)
    {
        if (config->ignore_missing_inputs)
        {
            return PolicyNew();
        }

        CfOut(OUTPUT_LEVEL_ERROR, "stat", "Can't stat file \"%s\" for parsing\n", input_path);
        exit(1);
    }

#ifndef _WIN32
    if (config->check_not_writable_by_others && (statbuf.st_mode & (S_IWGRP | S_IWOTH)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "File %s (owner %ju) is writable by others (security exception)", input_path, (uintmax_t)statbuf.st_uid);
        exit(1);
    }
#endif

    CfDebug("+++++++++++++++++++++++++++++++++++++++++++++++\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "  > Parsing file %s\n", input_path);
    CfDebug("+++++++++++++++++++++++++++++++++++++++++++++++\n");

    if (!FileCanOpen(input_path, "r"))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Can't open file for parsing: %s\n", input_path);
        exit(1);
    }

    Policy *policy = NULL;
    if (StringEndsWith(input_path, ".json"))
    {
        char *contents = NULL;
        if (FileReadMax(&contents, input_path, SIZE_MAX) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Error reading JSON input file: %s", input_path);
            return NULL;
        }
        JsonElement *json_policy = NULL;
        const char *data = contents; // TODO: need to fix JSON parser signature, just silly
        if (JsonParse(&data, &json_policy) != JSON_PARSE_OK)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Error parsing JSON input file: %s", input_path);
            free(contents);
            return NULL;
        }

        policy = PolicyFromJson(json_policy);

        JsonElementDestroy(json_policy);
        free(contents);
    }
    else
    {
        if (config->agent_type == AGENT_TYPE_COMMON)
        {
            policy = ParserParseFile(input_path, config->agent_specific.common.parser_warnings, config->agent_specific.common.parser_warnings_error);
        }
        else
        {
            policy = ParserParseFile(input_path, 0, 0);
        }
    }

    return policy;
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

const Rlist *InputFiles(EvalContext *ctx, Policy *policy)
{
    Body *body_common_control = PolicyGetBody(policy, NULL, "common", "control");
    if (!body_common_control)
    {
        ProgrammingError("Attempted to get input files from policy without body common control");
        return NULL;
    }

    Seq *potential_inputs = BodyGetConstraint(body_common_control, "inputs");
    Constraint *cp = EffectiveConstraint(ctx, potential_inputs);
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

static void CheckWorkingDirectories(EvalContext *ctx)
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

    if (stat(CFWORKDIR, &statbuf) != -1)
    {
        /* change permissions go-w */
        chmod(CFWORKDIR, (mode_t) (statbuf.st_mode & ~022));
    }

    snprintf(vbuff, CF_BUFSIZE, "%s%cstate%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(vbuff, false);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking integrity of the state database\n");
    snprintf(vbuff, CF_BUFSIZE, "%s%cstate", CFWORKDIR, FILE_SEPARATOR);

    if (stat(vbuff, &statbuf) == -1)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cstate%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, false);

        if (chown(vbuff, getuid(), getgid()) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "chown", "Unable to set owner on %s to %jd.%jd", vbuff, (uintmax_t)getuid(), (uintmax_t)getgid());
        }

        chmod(vbuff, (mode_t) 0755);
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

    if (stat(vbuff, &statbuf) == -1)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cmodules%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, false);

        if (chown(vbuff, getuid(), getgid()) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "chown", "Unable to set owner on %s to %ju.%ju", vbuff, (uintmax_t)getuid(), (uintmax_t)getgid());
        }

        chmod(vbuff, (mode_t) 0700);
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

    if (stat(vbuff, &statbuf) == -1)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cppkeys%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, false);

        chmod(vbuff, (mode_t) 0700); /* Keys must be immutable to others */
    }
    else
    {
#ifndef __MINGW32__
        if (statbuf.st_mode & 077)
        {
            FatalError(ctx, "UNTRUSTED: Private key directory %s%cppkeys (mode %jo) was not private!\n", CFWORKDIR,
                       FILE_SEPARATOR, (uintmax_t)(statbuf.st_mode & 0777));
        }
#endif /* !__MINGW32__ */
    }
}


const char *GenericAgentResolveInputPath(const GenericAgentConfig *config, const char *input_file)
{
    static char input_path[CF_BUFSIZE];

    switch (FilePathGetType(input_file))
    {
    case FILE_PATH_TYPE_ABSOLUTE:
        strlcpy(input_path, input_file, CF_BUFSIZE);
        break;

    case FILE_PATH_TYPE_NON_ANCHORED:
    case FILE_PATH_TYPE_RELATIVE:
        snprintf(input_path, CF_BUFSIZE, "%s%c%s", config->input_dir, FILE_SEPARATOR, input_file);
        break;
    }

    return MapName(input_path);
}

static void VerifyPromises(EvalContext *ctx, Policy *policy, GenericAgentConfig *config)
{

/* Now look once through ALL the bundles themselves */

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);
        EvalContextStackPushBundleFrame(ctx, bp, false);

        for (size_t j = 0; j < SeqLength(bp->promise_types); j++)
        {
            PromiseType *sp = SeqAt(bp->promise_types, j);

            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                Promise *pp = SeqAt(sp->promises, ppi);
                ExpandPromise(ctx, pp, CommonEvalPromise, NULL);
            }
        }

        EvalContextStackPopFrame(ctx);
    }

    PolicyHashVariables(ctx, policy);
    HashControls(ctx, policy, config);

    // TODO: need to move this inside PolicyCheckRunnable eventually.
    if (!config->bundlesequence && config->check_runnable)
    {
        // only verify policy-defined bundlesequence for cf-agent, cf-promises, cf-gendoc
        if ((config->agent_type == AGENT_TYPE_AGENT) ||
            (config->agent_type == AGENT_TYPE_COMMON) ||
            (config->agent_type == AGENT_TYPE_GENDOC))
        {
            if (!VerifyBundleSequence(ctx, policy, config))
            {
                FatalError(ctx, "Errors in promise bundles");
            }
        }
    }
}

/*******************************************************************/
/* Level 3                                                         */
/*******************************************************************/

static void CheckVariablePromises(EvalContext *ctx, Seq *var_promises)
{
    int allow_redefine = false;

    CfDebug("CheckVariablePromises()\n");

    for (size_t i = 0; i < SeqLength(var_promises); i++)
    {
        Promise *pp = SeqAt(var_promises, i);
        VerifyVarPromise(ctx, pp, allow_redefine);
    }
}

/*******************************************************************/

static void CheckCommonClassPromises(EvalContext *ctx, Seq *class_promises)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Checking common class promises...\n");

    for (size_t i = 0; i < SeqLength(class_promises); i++)
    {
        Promise *pp = SeqAt(class_promises, i);

        char *sp = NULL;
        if (VarClassExcluded(ctx, pp, &sp))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
            CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping whole next promise (%s), as var-context %s is not relevant\n", pp->promiser, sp);
            CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
            continue;
        }

        ExpandPromise(ctx, pp, VerifyClassPromise, NULL);
    }
}

/*******************************************************************/

static void CheckControlPromises(EvalContext *ctx, GenericAgentConfig *config, const Body *control_body)
{
    const ConstraintSyntax *body_syntax = NULL;
    Rval returnval;

    CfDebug("CheckControlPromises(%s)\n", control_body->type);
    assert(strcmp(control_body->name, "control") == 0);

    for (int i = 0; CONTROL_BODIES[i].constraints != NULL; i++)
    {
        body_syntax = CONTROL_BODIES[i].constraints;

        if (strcmp(control_body->type, CONTROL_BODIES[i].body_type) == 0)
        {
            break;
        }
    }

    if (body_syntax == NULL)
    {
        FatalError(ctx, "Unknown agent");
    }

    char scope[CF_BUFSIZE];
    snprintf(scope, CF_BUFSIZE, "%s_%s", control_body->name, control_body->type);
    CfDebug("Initiate control variable convergence...%s\n", scope);
    ScopeClear(scope);
    ScopeSetCurrent(scope);

    for (size_t i = 0; i < SeqLength(control_body->conlist); i++)
    {
        Constraint *cp = SeqAt(control_body->conlist, i);

        if (!IsDefinedClass(ctx, cp->classes, NULL))
        {
            continue;
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_BUNDLESEQUENCE].lval) == 0)
        {
            returnval = ExpandPrivateRval(ctx, scope, cp->rval);
        }
        else
        {
            returnval = EvaluateFinalRval(ctx, scope, cp->rval, true, NULL);
        }

        ScopeDeleteVariable(scope, cp->lval);

        if (!EvalContextVariablePut(ctx, (VarRef) { NULL, scope, cp->lval }, returnval, ConstraintSyntaxGetDataType(body_syntax, cp->lval)))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Rule from %s at/before line %zu\n", control_body->source_path, cp->offset.line);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_OUTPUT_PREFIX].lval) == 0)
        {
            strncpy(VPREFIX, returnval.item, CF_MAXVARSIZE);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_DOMAIN].lval) == 0)
        {
            strcpy(VDOMAIN, cp->rval.item);
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET domain = %s\n", VDOMAIN);
            ScopeDeleteSpecialScalar("sys", "domain");
            ScopeDeleteSpecialScalar("sys", "fqhost");
            snprintf(VFQNAME, CF_MAXVARSIZE, "%s.%s", VUQNAME, VDOMAIN);
            ScopeNewSpecialScalar(ctx, "sys", "fqhost", VFQNAME, DATA_TYPE_STRING);
            ScopeNewSpecialScalar(ctx, "sys", "domain", VDOMAIN, DATA_TYPE_STRING);
            EvalContextHeapAddHard(ctx, VDOMAIN);
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

void Syntax(const char *component, const struct option options[], const char *hints[], const char *description, bool accepts_file_argument)
{
    printf("Usage: %s [OPTION]...%s\n", component, accepts_file_argument ? " [FILE]" : "");

    printf("\n%s\n", description);

    printf("\nOptions:\n");

    for (int i = 0; options[i].name != NULL; i++)
    {
        if (options[i].has_arg)
        {
            printf("  --%-12s, -%c value - %s\n", options[i].name, (char) options[i].val, hints[i]);
        }
        else
        {
            printf("  --%-12s, -%-7c - %s\n", options[i].name, (char) options[i].val, hints[i]);
        }
    }

    printf("\nWebsite: http://www.cfengine.com\n");
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

void PrintVersion(void)
{
    printf("%s\n", NameVersion());
#ifdef HAVE_NOVA
    printf("%s\n", Nova_NameVersion());
#endif
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

void BundleHashVariables(EvalContext *ctx, Bundle *bundle)
{
    for (size_t j = 0; j < SeqLength(bundle->promise_types); j++)
    {
        PromiseType *sp = SeqAt(bundle->promise_types, j);

        if (strcmp(sp->name, "vars") == 0)
        {
            CheckVariablePromises(ctx, sp->promises);
        }

        // We must also set global classes here?

        if (strcmp(bundle->type, "common") == 0 && strcmp(sp->name, "classes") == 0)
        {
            CheckCommonClassPromises(ctx, sp->promises);
        }
    }
}

void PolicyHashVariables(EvalContext *ctx, Policy *policy)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Initiate variable convergence...\n");

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bundle = SeqAt(policy->bundles, i);
        EvalContextStackPushBundleFrame(ctx, bundle, false);

        BundleHashVariables(ctx, bundle);

        EvalContextStackPopFrame(ctx);
    }
}

/*******************************************************************/

void HashControls(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
{
/* Only control bodies need to be hashed like variables */

    for (size_t i = 0; i < SeqLength(policy->bodies); i++)
    {
        Body *bdp = SeqAt(policy->bodies, i);

        if (strcmp(bdp->name, "control") == 0)
        {
            CheckControlPromises(ctx, config, bdp);
        }
    }
}

/********************************************************************/

static bool VerifyBundleSequence(EvalContext *ctx, const Policy *policy, const GenericAgentConfig *config)
{
    Rlist *rp;
    char *name;
    Rval retval;
    int ok = true;
    FnCall *fp;

    if (!EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_BUNDLESEQUENCE, &retval))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !!! No bundlesequence in the common control body");
        return false;
    }

    if (retval.type != RVAL_TYPE_LIST)
    {
        FatalError(ctx, "Promised bundlesequence was not a list");
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


bool GenericAgentConfigParseArguments(GenericAgentConfig *config, int argc, char **argv)
{
    if (argc == 0)
    {
        return true;
    }

    if (argc > 1)
    {
        return false;
    }

    GenericAgentConfigSetInputFile(config, NULL, argv[0]);
    MINUSF = true;
    return true;
}

bool GenericAgentConfigParseWarningOptions(GenericAgentConfig *config, const char *warning_options)
{
    if (strlen(warning_options) == 0)
    {
        return false;
    }

    if (strcmp("error", warning_options) == 0)
    {
        config->agent_specific.common.parser_warnings_error |= PARSER_WARNING_ALL;
        return true;
    }

    const char *options_start = warning_options;
    bool warnings_as_errors = false;

    if (StringStartsWith(warning_options, "error="))
    {
        options_start = warning_options + strlen("error=");
        warnings_as_errors = true;
    }

    StringSet *warnings_set = StringSetFromString(options_start, ',');
    StringSetIterator it = StringSetIteratorInit(warnings_set);
    const char *warning_str = NULL;
    while ((warning_str = StringSetIteratorNext(&it)))
    {
        int warning = ParserWarningFromString(warning_str);
        if (warning == -1)
        {
            Log(LOG_LEVEL_ERR, "Unrecognized warning '%s'", warning_str);
            StringSetDestroy(warnings_set);
            return false;
        }

        if (warnings_as_errors)
        {
            config->agent_specific.common.parser_warnings_error |= warning;
        }
        else
        {
            config->agent_specific.common.parser_warnings |= warning;
        }
    }

    StringSetDestroy(warnings_set);
    return true;
}

GenericAgentConfig *GenericAgentConfigNewDefault(AgentType agent_type)
{
    GenericAgentConfig *config = xmalloc(sizeof(GenericAgentConfig));

    config->agent_type = agent_type;

    // TODO: system state, perhaps pull out as param
    config->tty_interactive = isatty(0) && isatty(1);

    config->bundlesequence = NULL;

    config->original_input_file = NULL;
    config->input_file = NULL;
    config->input_dir = NULL;

    config->check_not_writable_by_others = agent_type != AGENT_TYPE_COMMON && !config->tty_interactive;
    config->check_runnable = agent_type != AGENT_TYPE_COMMON;
    config->ignore_missing_bundles = false;
    config->ignore_missing_inputs = false;
    config->debug_mode = false;

    config->heap_soft = NULL;
    config->heap_negated = NULL;

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
        StringSetDestroy(config->heap_soft);
        StringSetDestroy(config->heap_negated);
        free(config->input_file);
    }
}

void GenericAgentConfigApply(EvalContext *ctx, const GenericAgentConfig *config)
{
    if (config->heap_soft)
    {
        StringSetIterator it = StringSetIteratorInit(config->heap_soft);
        const char *context = NULL;
        while ((context = StringSetIteratorNext(&it)))
        {
            if (EvalContextHeapContainsHard(ctx, context))
            {
                FatalError(ctx, "cfengine: You cannot use -D to define a reserved class!");
            }

            EvalContextHeapAddSoft(ctx, context, NULL);
        }
    }

    if (config->heap_negated)
    {
        StringSetIterator it = StringSetIteratorInit(config->heap_negated);
        const char *context = NULL;
        while ((context = StringSetIteratorNext(&it)))
        {
            if (EvalContextHeapContainsHard(ctx, context))
            {
                FatalError(ctx, "Cannot negate the reserved class [%s]\n", context);
            }

            EvalContextHeapAddNegated(ctx, context);
        }
    }

    if (config->debug_mode)
    {
        EvalContextHeapAddHard(ctx, "opt_debug");
        DEBUG = true;
    }
}

void GenericAgentConfigSetInputFile(GenericAgentConfig *config, const char *workdir, const char *input_file)
{
    free(config->original_input_file);
    free(config->input_file);
    free(config->input_dir);

    config->original_input_file = xstrdup(input_file);

    if (workdir && FilePathGetType(input_file) == FILE_PATH_TYPE_NON_ANCHORED)
    {
        config->input_file = StringFormat("%s%cinputs%c%s", workdir, FILE_SEPARATOR, FILE_SEPARATOR, input_file);
    }
    else
    {
        config->input_file = xstrdup(input_file);
    }

    config->input_dir = xstrdup(config->input_file);
    if (!ChopLastNode(config->input_dir))
    {
        free(config->input_dir);
        config->input_dir = xstrdup(".");
    }
}

void GenericAgentConfigSetBundleSequence(GenericAgentConfig *config, const Rlist *bundlesequence)
{
    RlistDestroy(config->bundlesequence);
    config->bundlesequence = RlistCopy(bundlesequence);
}
