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

#include <bootstrap.h>
#include <sysinfo.h>
#include <known_dirs.h>
#include <eval_context.h>
#include <policy.h>
#include <promises.h>
#include <files_lib.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <files_hashes.h>
#include <parser.h>
#include <dbm_api.h>
#include <crypto.h>
#include <vars.h>
#include <syntax.h>
#include <conversion.h>
#include <expand.h>
#include <locks.h>
#include <scope.h>
#include <atexit.h>
#include <unix.h>
#include <client_code.h>
#include <string_lib.h>
#include <exec_tools.h>
#include <list.h>
#include <misc_lib.h>
#include <fncall.h>
#include <rlist.h>
#include <syslog_client.h>
#include <audit.h>
#include <verify_classes.h>
#include <verify_vars.h>
#include <timeout.h>
#include <time_classes.h>
#include <unix_iface.h>
#include <constants.h>

#include <cf-windows-functions.h>

static pthread_once_t pid_cleanup_once = PTHREAD_ONCE_INIT; /* GLOBAL_T */

static char PIDFILE[CF_BUFSIZE] = ""; /* GLOBAL_C */

static void CheckWorkingDirectories(EvalContext *ctx);

static void GetAutotagDir(char *dirname, size_t max_size, const char *maybe_dirname);
static void GetPromisesValidatedFile(char *filename, size_t max_size, const GenericAgentConfig *config, const char *maybe_dirname);
static bool WriteReleaseIdFile(const char *filename, const char *dirname);
static bool GeneratePolicyReleaseIDFromGit(char *release_id_out, size_t out_size,
                                           const char *policy_dir);
static bool GeneratePolicyReleaseID(char *release_id_out, size_t out_size,
                                    const char *policy_dir);
static char* ReadReleaseIdFromReleaseIdFileMasterfiles(const char *maybe_dirname);

static bool MissingInputFile(const char *input_file);

#if !defined(__MINGW32__)
static void OpenLog(int facility);
#endif

/*****************************************************************************/

static void SanitizeEnvironment()
{
    /* ps(1) and other utilities invoked by CFEngine may be affected */
    unsetenv("COLUMNS");

    /* Make sure subprocesses output is not localized */
    unsetenv("LANG");
    unsetenv("LANGUAGE");
    unsetenv("LC_MESSAGES");
}

/*****************************************************************************/

ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(void, GenericAgentSetDefaultDigest, HashMethod *, digest, int *, digest_len)
{
    *digest = HASH_METHOD_MD5;
    *digest_len = CF_MD5_LEN;
}

void GenericAgentDiscoverContext(EvalContext *ctx, GenericAgentConfig *config)
{
    GenericAgentSetDefaultDigest(&CF_DEFAULT_DIGEST, &CF_DEFAULT_DIGEST_LEN);

    GenericAgentInitialize(ctx, config);

    time_t t = SetReferenceTime();
    UpdateTimeClasses(ctx, t);
    SanitizeEnvironment();

    THIS_AGENT_TYPE = config->agent_type;
    EvalContextClassPutHard(ctx, CF_AGENTTYPES[config->agent_type], "cfe_internal,source=agent");

    DetectEnvironment(ctx);

    EvalContextHeapPersistentLoadAll(ctx);
    LoadSystemConstants(ctx);

    if (config->agent_type == AGENT_TYPE_AGENT && config->agent_specific.agent.bootstrap_policy_server)
    {
        if (!RemoveAllExistingPolicyInInputs(GetInputDir()))
        {
            Log(LOG_LEVEL_ERR, "Error removing existing input files prior to bootstrap");
            exit(EXIT_FAILURE);
        }

        if (!WriteBuiltinFailsafePolicy(GetInputDir()))
        {
            Log(LOG_LEVEL_ERR, "Error writing builtin failsafe to inputs prior to bootstrap");
            exit(EXIT_FAILURE);
        }

        bool am_policy_server = false;
        {
            const char *canonified_bootstrap_policy_server = CanonifyName(config->agent_specific.agent.bootstrap_policy_server);
            am_policy_server = NULL != EvalContextClassGet(ctx, NULL, canonified_bootstrap_policy_server);
            {
                char policy_server_ipv4_class[CF_BUFSIZE];
                snprintf(policy_server_ipv4_class, CF_MAXVARSIZE, "ipv4_%s", canonified_bootstrap_policy_server);
                am_policy_server |= NULL != EvalContextClassGet(ctx, NULL, policy_server_ipv4_class);
            }

            if (am_policy_server)
            {
                Log(LOG_LEVEL_INFO, "Assuming role as policy server, with policy distribution point at %s", GetMasterDir());
                EvalContextClassPutHard(ctx, "am_policy_hub", "source=bootstrap");

                if (!MasterfileExists(GetMasterDir()))
                {
                    Log(LOG_LEVEL_ERR, "In order to bootstrap as a policy server, the file '%s/promises.cf' must exist.", GetMasterDir());
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                Log(LOG_LEVEL_INFO, "Not assuming role as policy server");
            }

            WriteAmPolicyHubFile(CFWORKDIR, am_policy_server);
        }

        WritePolicyServerFile(GetWorkDir(), config->agent_specific.agent.bootstrap_policy_server);
        SetPolicyServer(ctx, config->agent_specific.agent.bootstrap_policy_server);
        /* FIXME: Why it is called here? Can't we move both invocations to before if? */
        UpdateLastPolicyUpdateTime(ctx);
        Log(LOG_LEVEL_INFO, "Bootstrapping to '%s'", POLICY_SERVER);
    }
    else
    {
        char *existing_policy_server = ReadPolicyServerFile(GetWorkDir());
        if (existing_policy_server)
        {
            Log(LOG_LEVEL_VERBOSE, "This agent is bootstrapped to '%s'", existing_policy_server);
            SetPolicyServer(ctx, existing_policy_server);
            free(existing_policy_server);
            UpdateLastPolicyUpdateTime(ctx);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "This agent is not bootstrapped");
            return;
        }

        if (GetAmPolicyHub(GetWorkDir()))
        {
            EvalContextClassPutHard(ctx, "am_policy_hub", "source=bootstrap,deprecated,alias=policy_server");
            Log(LOG_LEVEL_VERBOSE, "Additional class defined: am_policy_hub");
            EvalContextClassPutHard(ctx, "policy_server", "inventory,attribute_name=CFEngine roles,source=bootstrap");
            Log(LOG_LEVEL_VERBOSE, "Additional class defined: policy_server");
        }
    }
}

static bool IsPolicyPrecheckNeeded(GenericAgentConfig *config, bool force_validation)
{
    bool check_policy = false;

    if (IsFileOutsideDefaultRepository(config->input_file))
    {
        check_policy = true;
        Log(LOG_LEVEL_VERBOSE, "Input file is outside default repository, validating it");
    }
    if (GenericAgentIsPolicyReloadNeeded(config))
    {
        check_policy = true;
        Log(LOG_LEVEL_VERBOSE, "Input file is changed since last validation, validating it");
    }
    if (force_validation)
    {
        check_policy = true;
        Log(LOG_LEVEL_VERBOSE, "always_validate is set, forcing policy validation");
    }

    return check_policy;
}

bool GenericAgentCheckPolicy(GenericAgentConfig *config, bool force_validation, bool write_validated_file)
{
    if (!MissingInputFile(config->input_file))
    {
        {
            if (config->agent_type == AGENT_TYPE_SERVER ||
                config->agent_type == AGENT_TYPE_MONITOR ||
                config->agent_type == AGENT_TYPE_EXECUTOR)
            {
                time_t validated_at = ReadTimestampFromPolicyValidatedFile(config, NULL);
                config->agent_specific.daemon.last_validated_at = validated_at;
            }
        }

        if (IsPolicyPrecheckNeeded(config, force_validation))
        {
            bool policy_check_ok = GenericAgentArePromisesValid(config);
            if (policy_check_ok && write_validated_file)
            {
                GenericAgentTagReleaseDirectory(config,
                                                NULL, // use GetAutotagDir
                                                write_validated_file, // true
                                                GetAmPolicyHub(GetWorkDir())); // write release ID?
            }

            if (config->agent_specific.agent.bootstrap_policy_server && !policy_check_ok)
            {
                Log(LOG_LEVEL_VERBOSE, "Policy is not valid, but proceeding with bootstrap");
                return true;
            }

            return policy_check_ok;
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Policy is already validated");
            return true;
        }
    }
    return false;
}

static JsonElement *ReadJsonFile(const char *filename)
{
    struct stat sb;
    if (stat(filename, &sb) == -1)
    {
        Log(LOG_LEVEL_DEBUG, "Could not open JSON file %s", filename);
        return NULL;
    }

    JsonElement *doc = NULL;
    JsonParseError err = JsonParseFile(filename, 4096, &doc);

    if (err != JSON_PARSE_OK
        || NULL == doc)
    {
        Log(LOG_LEVEL_DEBUG, "Could not parse JSON file %s", filename);
    }

    return doc;
}

static JsonElement *ReadPolicyValidatedFile(const char *filename)
{
    bool missing = true;
    struct stat sb;
    if (stat(filename, &sb) != -1)
    {
        missing = false;
    }

    JsonElement *validated_doc = ReadJsonFile(filename);
    if (NULL == validated_doc)
    {
        Log(missing ? LOG_LEVEL_DEBUG : LOG_LEVEL_VERBOSE, "Could not parse policy_validated JSON file '%s', using dummy data", filename);
        validated_doc = JsonObjectCreate(2);
        if (missing)
        {
            JsonObjectAppendInteger(validated_doc, "timestamp", 0);
        }
        else
        {
            JsonObjectAppendInteger(validated_doc, "timestamp", sb.st_mtime);
        }
    }

    return validated_doc;
}

static JsonElement *ReadPolicyValidatedFileFromMasterfiles(const GenericAgentConfig *config, const char *maybe_dirname)
{
    char filename[CF_MAXVARSIZE];

    GetPromisesValidatedFile(filename, sizeof(filename), config, maybe_dirname);

    return ReadPolicyValidatedFile(filename);
}

/**
 * @brief Writes a file with a contained timestamp to mark a policy file as validated
 * @param filename the filename
 * @return True if successful.
 */
static bool WritePolicyValidatedFile(ARG_UNUSED const GenericAgentConfig *config, const char *filename)
{
    if (!MakeParentDirectory(filename, true))
    {
        Log(LOG_LEVEL_ERR, "While writing policy validated marker file '%s', could not create directory (MakeParentDirectory: %s)", filename, GetErrorStr());
        return false;
    }

    int fd = creat(filename, 0600);
    if (fd == -1)
    {
        Log(LOG_LEVEL_ERR, "While writing policy validated marker file '%s', could not create file (creat: %s)", filename, GetErrorStr());
        return false;
    }

    JsonElement *info = JsonObjectCreate(3);
    JsonObjectAppendInteger(info, "timestamp", time(NULL));

    Writer *w = FileWriter(fdopen(fd, "w"));
    JsonWrite(w, info, 0);

    WriterClose(w);
    JsonDestroy(info);

    Log(LOG_LEVEL_VERBOSE, "Saved policy validated marker file '%s'", filename);
    return true;
}

/**
 * @brief Writes the policy validation file and release ID to a directory
 * @return True if successful.
 */
bool GenericAgentTagReleaseDirectory(const GenericAgentConfig *config, const char *dirname, bool write_validated, bool write_release)
{
    char local_dirname[PATH_MAX + 1];
    if (NULL == dirname)
    {
        GetAutotagDir(local_dirname, PATH_MAX, NULL);
        dirname = local_dirname;
    }

    char filename[CF_MAXVARSIZE];
    char git_checksum[GENERIC_AGENT_CHECKSUM_SIZE];
    bool have_git_checksum = GeneratePolicyReleaseIDFromGit(git_checksum, sizeof(git_checksum), dirname);

    Log(LOG_LEVEL_DEBUG, "Tagging directory %s for release (write_validated: %s, write_release: %s)",
        dirname,
        write_validated ? "yes" : "no",
        write_release ? "yes" : "no");

    if (write_release)
    {
        // first, tag the release ID
        GetReleaseIdFile(dirname, filename, sizeof(filename));
        char *id = ReadReleaseIdFromReleaseIdFileMasterfiles(dirname);
        if (NULL == id
            || (have_git_checksum && 0 != strcmp(id, git_checksum)))
        {
            if (NULL == id)
            {
                Log(LOG_LEVEL_DEBUG, "The release_id of %s was missing", dirname);
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "The release_id of %s needs to be updated", dirname);
            }

            bool wrote_release = WriteReleaseIdFile(filename, dirname);
            if (!wrote_release)
            {
                Log(LOG_LEVEL_VERBOSE, "The release_id file %s was NOT updated", filename);
                free(id);
                return false;
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "The release_id file %s was updated", filename);
            }
        }

        free(id);
    }

    // now, tag the promises_validated
    if (write_validated)
    {
        Log(LOG_LEVEL_DEBUG, "Tagging directory %s for validation", dirname);

        GetPromisesValidatedFile(filename, sizeof(filename), config, dirname);

        bool wrote_validated = WritePolicyValidatedFile(config, filename);

        if (!wrote_validated)
        {
            Log(LOG_LEVEL_VERBOSE, "The promises_validated file %s was NOT updated", filename);
            return false;
        }

        Log(LOG_LEVEL_DEBUG, "The promises_validated file %s was updated", filename);
        return true;
    }

    return true;
}

/**
 * @brief Writes a file with a contained release ID based on git SHA,
 *        or file checksum if git SHA is not available.
 * @param filename the release_id file
 * @param dirname the directory to checksum or get the Git hash
 * @return True if successful
 */
static bool WriteReleaseIdFile(const char *filename, const char *dirname)
{
    char release_id[GENERIC_AGENT_CHECKSUM_SIZE];

    bool have_release_id =
        GeneratePolicyReleaseID(release_id, sizeof(release_id), dirname);

    if (!have_release_id)
    {
        return false;
    }

    int fd = creat(filename, 0600);
    if (fd == -1)
    {
        Log(LOG_LEVEL_ERR, "While writing policy release ID file '%s', could not create file (creat: %s)", filename, GetErrorStr());
        return false;
    }

    JsonElement *info = JsonObjectCreate(3);
    JsonObjectAppendString(info, "releaseId", release_id);

    Writer *w = FileWriter(fdopen(fd, "w"));
    JsonWrite(w, info, 0);

    WriterClose(w);
    JsonDestroy(info);

    Log(LOG_LEVEL_VERBOSE, "Saved policy release ID file '%s'", filename);
    return true;
}

bool GenericAgentArePromisesValid(const GenericAgentConfig *config)
{
    char cmd[CF_BUFSIZE];

    Log(LOG_LEVEL_VERBOSE, "Verifying the syntax of the inputs...");
    {
        char cfpromises[CF_MAXVARSIZE];
        snprintf(cfpromises, sizeof(cfpromises), "%s%cbin%ccf-promises%s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR,
                 EXEC_SUFFIX);

        struct stat sb;
        if (stat(cfpromises, &sb) == -1)
        {
            Log(LOG_LEVEL_ERR, "cf-promises%s needs to be installed in %s%cbin for pre-validation of full configuration",
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
            const char *bundle_ref = RlistScalarValue(rp);
            strlcat(cmd, bundle_ref, CF_BUFSIZE);

            if (rp->next)
            {
                strlcat(cmd, ",", CF_BUFSIZE);
            }
        }
        strlcat(cmd, "\"", CF_BUFSIZE);
    }

    Log(LOG_LEVEL_VERBOSE, "Checking policy with command '%s'", cmd);

    if (!ShellCommandReturnsZero(cmd, true))
    {
        Log(LOG_LEVEL_ERR, "Policy failed validation with command '%s'", cmd);
        return false;
    }

    return true;
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

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, GenericAgentAddEditionClasses, EvalContext *, ctx)
{
    EvalContextClassPutHard(ctx, "community_edition", "inventory,attribute_name=none,source=agent");
}

void GenericAgentInitialize(EvalContext *ctx, GenericAgentConfig *config)
{
    int force = false;
    struct stat statbuf, sb;
    char vbuff[CF_BUFSIZE];
    char ebuff[CF_EXPANDSIZE];

#ifdef __MINGW32__
    InitializeWindows();
#endif

    DetermineCfenginePort();

    EvalContextClassPutHard(ctx, "any", "source=agent");

    GenericAgentAddEditionClasses(ctx);

    strcpy(VPREFIX, GetConsolePrefix());

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

    OpenLog(LOG_USER);
    SetSyslogFacility(LOG_USER);

    Log(LOG_LEVEL_VERBOSE, "Work directory is %s", CFWORKDIR);

    snprintf(vbuff, CF_BUFSIZE, "%s%cupdate.conf", GetInputDir(), FILE_SEPARATOR);
    MakeParentDirectory(vbuff, force);
    snprintf(vbuff, CF_BUFSIZE, "%s%cbin%ccf-agent -D from_cfexecd", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(vbuff, force);
    snprintf(vbuff, CF_BUFSIZE, "%s%coutputs%cspooled_reports", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(vbuff, force);
    snprintf(vbuff, CF_BUFSIZE, "%s%clastseen%cintermittencies", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(vbuff, force);
    snprintf(vbuff, CF_BUFSIZE, "%s%creports%cvarious", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(vbuff, force);

    snprintf(vbuff, CF_BUFSIZE, "%s", GetInputDir());

    if (stat(vbuff, &sb) == -1)
    {
        FatalError(ctx, " No access to WORKSPACE/inputs dir");
    }
    else
    {
        chmod(vbuff, sb.st_mode | 0700);
    }

    snprintf(vbuff, CF_BUFSIZE, "%s%coutputs", CFWORKDIR, FILE_SEPARATOR);

    if (stat(vbuff, &sb) == -1)
    {
        FatalError(ctx, " No access to WORKSPACE/outputs dir");
    }
    else
    {
        chmod(vbuff, sb.st_mode | 0700);
    }

    snprintf(ebuff, sizeof(ebuff), "%s%cstate%ccf_procs",
             CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(ebuff, force);

    if (stat(ebuff, &statbuf) == -1)
    {
        CreateEmptyFile(ebuff);
    }

    snprintf(ebuff, sizeof(ebuff), "%s%cstate%ccf_rootprocs",
             CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);

    if (stat(ebuff, &statbuf) == -1)
    {
        CreateEmptyFile(ebuff);
    }

    snprintf(ebuff, sizeof(ebuff), "%s%cstate%ccf_otherprocs",
             CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);

    if (stat(ebuff, &statbuf) == -1)
    {
        CreateEmptyFile(ebuff);
    }

    snprintf(ebuff, sizeof(ebuff), "%s%cstate%cprevious_state%c",
             CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(ebuff, force);

    snprintf(ebuff, sizeof(ebuff), "%s%cstate%cdiff%c",
             CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(ebuff, force);

    snprintf(ebuff, sizeof(ebuff), "%s%cstate%cuntracked%c",
            CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(ebuff, force);

    OpenNetwork();
    CryptoInitialize();

    CheckWorkingDirectories(ctx);

    /* Initialize keys and networking. cf-key, doesn't need keys. In fact it
       must function properly even without them, so that it generates them! */
    if (config->agent_type != AGENT_TYPE_KEYGEN)
    {
        LoadSecretKeys();
        char *bootstrapped_policy_server = ReadPolicyServerFile(CFWORKDIR);
        PolicyHubUpdateKeys(bootstrapped_policy_server);
        free(bootstrapped_policy_server);
        cfnet_init();
    }

    size_t cwd_size = PATH_MAX;
    while (true)
    {
        char cwd[cwd_size];
        if (!getcwd(cwd, cwd_size))
        {
            if (errno == ERANGE)
            {
                cwd_size *= 2;
                continue;
            }
            Log(LOG_LEVEL_WARNING, "Could not determine current directory. (getcwd: '%s')", GetErrorStr());
            break;
        }
        EvalContextSetLaunchDirectory(ctx, cwd);
        break;
    }

    if (!MINUSF)
    {
        GenericAgentConfigSetInputFile(config, GetInputDir(), "promises.cf");
    }

    VIFELAPSED = 1;
    VEXPIREAFTER = 1;

    setlinebuf(stdout);

    if (config->agent_specific.agent.bootstrap_policy_server)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cfailsafe.cf", GetInputDir(), FILE_SEPARATOR);

        if (stat(vbuff, &statbuf) == -1)
        {
            GenericAgentConfigSetInputFile(config, GetInputDir(), "failsafe.cf");
        }
        else
        {
            GenericAgentConfigSetInputFile(config, GetInputDir(), vbuff);
        }
    }
}

void GenericAgentFinalize(EvalContext *ctx, GenericAgentConfig *config)
{
    /* TODO, FIXME: what else from the above do we need to undo here ? */
    if (config->agent_type != AGENT_TYPE_KEYGEN)
    {
        cfnet_shut();
    }
    CryptoDeInitialize();
    GenericAgentConfigDestroy(config);
    EvalContextDestroy(ctx);
}

static bool MissingInputFile(const char *input_file)
{
    struct stat sb;

    if (stat(input_file, &sb) == -1)
    {
        Log(LOG_LEVEL_ERR, "There is no readable input file at '%s'. (stat: %s)", input_file, GetErrorStr());
        return true;
    }

    return false;
}

// Git only.
static bool GeneratePolicyReleaseIDFromGit(char *release_id_out, size_t out_size,
                                           const char *policy_dir)
{
    char git_filename[PATH_MAX + 1];
    snprintf(git_filename, PATH_MAX, "%s/.git/HEAD", policy_dir);
    MapName(git_filename);

    FILE *git_file = fopen(git_filename, "r");
    if (git_file)
    {
        char git_head[128];
        fscanf(git_file, "ref: %127s", git_head);
        fclose(git_file);

        snprintf(git_filename, PATH_MAX, "%s/.git/%s", policy_dir, git_head);
        git_file = fopen(git_filename, "r");
        if (git_file)
        {
            assert(out_size > 40);
            fscanf(git_file, "%40s", release_id_out);
            fclose(git_file);
            return true;
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "While generating policy release ID, found git head ref '%s', but unable to open (errno: %s)",
                policy_dir, GetErrorStr());
        }
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "While generating policy release ID, directory is '%s' not a git repository",
            policy_dir);
    }

    return false;
}

static bool GeneratePolicyReleaseIDFromTree(char *release_id_out, size_t out_size,
                                            const char *policy_dir)
{
    if (access(policy_dir, R_OK) != 0)
    {
        Log(LOG_LEVEL_ERR, "Cannot access policy directory '%s' to generate release ID", policy_dir);
        return false;
    }

    // fallback, produce some pseudo sha1 hash
    EVP_MD_CTX crypto_ctx;
    EVP_DigestInit(&crypto_ctx, EVP_get_digestbyname(HashNameFromId(GENERIC_AGENT_CHECKSUM_METHOD)));

    bool success = HashDirectoryTree(policy_dir,
                                     (const char *[]) { ".cf", ".dat", ".txt", ".conf", NULL},
                                     &crypto_ctx);

    int md_len;
    unsigned char digest[EVP_MAX_MD_SIZE + 1] = { 0 };
    EVP_DigestFinal(&crypto_ctx, digest, &md_len);

    HashPrintSafe(release_id_out, out_size, digest,
                  GENERIC_AGENT_CHECKSUM_METHOD, false);
    return success;
}

static bool GeneratePolicyReleaseID(char *release_id_out, size_t out_size,
                                    const char *policy_dir)
{
    if (GeneratePolicyReleaseIDFromGit(release_id_out, out_size, policy_dir))
    {
        return true;
    }

    return GeneratePolicyReleaseIDFromTree(release_id_out, out_size,
                                           policy_dir);
}

/**
 * @brief Gets the promises_validated file name depending on context and options
 */
static void GetPromisesValidatedFile(char *filename, size_t max_size, const GenericAgentConfig *config, const char *maybe_dirname)
{
    char dirname[PATH_MAX + 1];
    GetAutotagDir(dirname, max_size, maybe_dirname);

    if (NULL == maybe_dirname && MINUSF)
    {
        snprintf(filename, max_size, "%s/validated_%s", dirname, CanonifyName(config->original_input_file));
    }
    else
    {
        snprintf(filename, max_size, "%s/cf_promises_validated", dirname);
    }

    MapName(filename);
}

 /**
 * @brief Gets the promises_validated file name depending on context and options
 */
static void GetAutotagDir(char *dirname, size_t max_size, const char *maybe_dirname)
{
    if (NULL != maybe_dirname)
    {
        strlcpy(dirname, maybe_dirname, max_size);
    }
    else if (MINUSF)
    {
        snprintf(dirname, max_size, "%s/state", CFWORKDIR);
    }
    else
    {
        strlcpy(dirname, GetMasterDir(), max_size);
    }

    MapName(dirname);
}

/**
 * @brief Gets the release_id file name in the given base_path.
 */
void GetReleaseIdFile(const char *base_path, char *filename, size_t max_size)
{
    snprintf(filename, max_size, "%s/cf_promises_release_id", base_path);
    MapName(filename);
}

static JsonElement *ReadReleaseIdFileFromMasterfiles(const char *maybe_dirname)
{
    char filename[CF_MAXVARSIZE];

    GetReleaseIdFile(NULL == maybe_dirname ? GetMasterDir() : maybe_dirname,
                     filename, sizeof(filename));

    JsonElement *doc = ReadJsonFile(filename);
    if (NULL == doc)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not parse release_id JSON file %s", filename);
    }

    return doc;
}

static char* ReadReleaseIdFromReleaseIdFileMasterfiles(const char *maybe_dirname)
{
    JsonElement *doc = ReadReleaseIdFileFromMasterfiles(maybe_dirname);
    char *id = NULL;
    if (doc)
    {
        JsonElement *jid = JsonObjectGet(doc, "releaseId");
        if (jid)
        {
            id = xstrdup(JsonPrimitiveGetAsString(jid));
        }
        JsonDestroy(doc);
    }

    return id;
}

// TODO: refactor Read*FromPolicyValidatedMasterfiles
time_t ReadTimestampFromPolicyValidatedFile(const GenericAgentConfig *config, const char *maybe_dirname)
{
    time_t validated_at = 0;
    {
        JsonElement *validated_doc = ReadPolicyValidatedFileFromMasterfiles(config, maybe_dirname);
        if (validated_doc)
        {
            JsonElement *timestamp = JsonObjectGet(validated_doc, "timestamp");
            if (timestamp)
            {
                validated_at = JsonPrimitiveGetAsInteger(timestamp);
            }
            JsonDestroy(validated_doc);
        }
    }

    return validated_at;
}

// TODO: refactor Read*FromPolicyValidatedMasterfiles
char* ReadChecksumFromPolicyValidatedMasterfiles(const GenericAgentConfig *config, const char *maybe_dirname)
{
    char *checksum_str = NULL;

    {
        JsonElement *validated_doc = ReadPolicyValidatedFileFromMasterfiles(config, maybe_dirname);
        if (validated_doc)
        {
            JsonElement *checksum = JsonObjectGet(validated_doc, "checksum");
            if (checksum )
            {
                checksum_str = xstrdup(JsonPrimitiveGetAsString(checksum));
            }
            JsonDestroy(validated_doc);
        }
    }

    return checksum_str;
}

/**
 * @NOTE Updates the config->agent_specific.daemon.last_validated_at timestamp
 *       used by serverd, execd etc daemons when checking for new policies.
 */
bool GenericAgentIsPolicyReloadNeeded(const GenericAgentConfig *config)
{
    time_t validated_at = ReadTimestampFromPolicyValidatedFile(config, NULL);
    time_t now = time(NULL);

    if (validated_at > now)
    {
        Log(LOG_LEVEL_INFO,
            "Clock seems to have jumped back in time, mtime of %jd is newer than current time %jd, touching it",
            (intmax_t) validated_at, (intmax_t) now);

        GenericAgentTagReleaseDirectory(config,
                                        NULL, // use GetAutotagDir
                                        true, // write validated
                                        false); // write release ID
        return true;
    }

    {
        struct stat sb;
        if (stat(config->input_file, &sb) == -1)
        {
            Log(LOG_LEVEL_VERBOSE, "There is no readable input file at '%s'. (stat: %s)", config->input_file, GetErrorStr());
            return true;
        }
        else if (sb.st_mtime > validated_at)
        {
            Log(LOG_LEVEL_VERBOSE, "Input file '%s' has changed since the last policy read attempt", config->input_file);
            return true;
        }
    }

    // Check the directories first for speed and because non-input/data files should trigger an update
    {
        if (IsNewerFileTree( (char *)GetInputDir(), validated_at))
        {
            Log(LOG_LEVEL_VERBOSE, "Quick search detected file changes");
            return true;
        }
    }

    {
        char filename[MAX_FILENAME];
        snprintf(filename, MAX_FILENAME, "%s/policy_server.dat", CFWORKDIR);
        MapName(filename);

        struct stat sb;
        if ((stat(filename, &sb) != -1) && (sb.st_mtime > validated_at))
        {
            return true;
        }
    }

    return false;
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
    Log(LOG_LEVEL_VERBOSE, "SET Syslog FACILITY = %s", retval);

    CloseLog();
    OpenLog(ParseFacility(retval));
    SetSyslogFacility(ParseFacility(retval));
}

static void CheckWorkingDirectories(EvalContext *ctx)
/* NOTE: We do not care about permissions (ACLs) in windows */
{
    struct stat statbuf;
    char vbuff[CF_BUFSIZE];

    if (uname(&VSYSNAME) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't get kernel name info. (uname: %s)", GetErrorStr());
        memset(&VSYSNAME, 0, sizeof(VSYSNAME));
    }

    snprintf(vbuff, CF_BUFSIZE, "%s%c.", CFWORKDIR, FILE_SEPARATOR);
    MakeParentDirectory(vbuff, false);

    Log(LOG_LEVEL_VERBOSE, "Making sure that locks are private...");

    if (chown(CFWORKDIR, getuid(), getgid()) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to set owner on '%s'' to '%ju.%ju'. (chown: %s)", CFWORKDIR, (uintmax_t)getuid(),
            (uintmax_t)getgid(), GetErrorStr());
    }

    if (stat(CFWORKDIR, &statbuf) != -1)
    {
        /* change permissions go-w */
        chmod(CFWORKDIR, (mode_t) (statbuf.st_mode & ~022));
    }

    snprintf(vbuff, CF_BUFSIZE, "%s%cstate%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    MakeParentDirectory(vbuff, false);

    Log(LOG_LEVEL_VERBOSE, "Checking integrity of the state database");
    snprintf(vbuff, CF_BUFSIZE, "%s%cstate", CFWORKDIR, FILE_SEPARATOR);

    if (stat(vbuff, &statbuf) == -1)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cstate%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, false);

        if (chown(vbuff, getuid(), getgid()) == -1)
        {
            Log(LOG_LEVEL_ERR, "Unable to set owner on '%s' to '%jd.%jd'. (chown: %s)", vbuff,
                (uintmax_t)getuid(), (uintmax_t)getgid(), GetErrorStr());
        }

        chmod(vbuff, (mode_t) 0755);
    }
    else
    {
#ifndef __MINGW32__
        if (statbuf.st_mode & 022)
        {
            Log(LOG_LEVEL_ERR, "UNTRUSTED: State directory %s (mode %jo) was not private!", CFWORKDIR,
                  (uintmax_t)(statbuf.st_mode & 0777));
        }
#endif /* !__MINGW32__ */
    }

    Log(LOG_LEVEL_VERBOSE, "Checking integrity of the module directory");

    snprintf(vbuff, CF_BUFSIZE, "%s%cmodules", CFWORKDIR, FILE_SEPARATOR);

    if (stat(vbuff, &statbuf) == -1)
    {
        snprintf(vbuff, CF_BUFSIZE, "%s%cmodules%c.", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
        MakeParentDirectory(vbuff, false);

        if (chown(vbuff, getuid(), getgid()) == -1)
        {
            Log(LOG_LEVEL_ERR, "Unable to set owner on '%s' to '%ju.%ju'. (chown: %s)", vbuff,
                (uintmax_t)getuid(), (uintmax_t)getgid(), GetErrorStr());
        }

        chmod(vbuff, (mode_t) 0700);
    }
    else
    {
#ifndef __MINGW32__
        if (statbuf.st_mode & 022)
        {
            Log(LOG_LEVEL_ERR, "UNTRUSTED: Module directory %s (mode %jo) was not private!", vbuff,
                  (uintmax_t)(statbuf.st_mode & 0777));
        }
#endif /* !__MINGW32__ */
    }

    Log(LOG_LEVEL_VERBOSE, "Checking integrity of the PKI directory");

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
    static char input_path[CF_BUFSIZE]; /* GLOBAL_R, no initialization needed */

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

void GenericAgentWriteHelp(Writer *w, const char *component, const struct option options[], const char *const hints[], bool accepts_file_argument)
{
    WriterWriteF(w, "Usage: %s [OPTION]...%s\n", component, accepts_file_argument ? " [FILE]" : "");

    WriterWriteF(w, "\nOptions:\n");

    for (int i = 0; options[i].name != NULL; i++)
    {
        if (options[i].has_arg)
        {
            WriterWriteF(w, "  --%-12s, -%c value - %s\n", options[i].name, (char) options[i].val, hints[i]);
        }
        else
        {
            WriterWriteF(w, "  --%-12s, -%-7c - %s\n", options[i].name, (char) options[i].val, hints[i]);
        }
    }

    WriterWriteF(w, "\nWebsite: http://www.cfengine.com\n");
    WriterWriteF(w, "This software is Copyright (C) 2008,2010-present CFEngine AS.\n");
}

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, GenericAgentWriteVersion, Writer *, w)
{
    WriterWriteF(w, "%s\n", NameVersion());
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
            Log(LOG_LEVEL_ERR, "Unable to remove pid file '%s'. (unlink: %s)", PIDFILE, GetErrorStr());
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

    snprintf(PIDFILE, CF_BUFSIZE - 1, "%s%c%s", GetPidDir(), FILE_SEPARATOR, filename);

    if ((fp = fopen(PIDFILE, "w")) == NULL)
    {
        Log(LOG_LEVEL_INFO, "Could not write to PID file '%s'. (fopen: %s)", filename, GetErrorStr());
        return;
    }

    fprintf(fp, "%" PRIuMAX "\n", (uintmax_t)getpid());

    fclose(fp);
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

bool GenericAgentConfigParseColor(GenericAgentConfig *config, const char *mode)
{
    if (!mode || strcmp("auto", mode) == 0)
    {
        config->color = config->tty_interactive;
        return true;
    }
    else if (strcmp("always", mode) == 0)
    {
        config->color = true;
        return true;
    }
    else if (strcmp("never", mode) == 0)
    {
        config->color = false;
        return true;
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Unrecognized color mode '%s'", mode);
        return false;
    }
}

GenericAgentConfig *GenericAgentConfigNewDefault(AgentType agent_type)
{
    GenericAgentConfig *config = xmalloc(sizeof(GenericAgentConfig));

    config->agent_type = agent_type;

    // TODO: system state, perhaps pull out as param
    config->tty_interactive = isatty(0) && isatty(1);

    const char *color_env = getenv("CFENGINE_COLOR");
    config->color = (color_env && 0 == strcmp(color_env, "1"));

    config->bundlesequence = NULL;

    config->original_input_file = NULL;
    config->input_file = NULL;
    config->input_dir = NULL;

    config->check_not_writable_by_others = agent_type != AGENT_TYPE_COMMON && !config->tty_interactive;
    config->check_runnable = agent_type != AGENT_TYPE_COMMON;
    config->ignore_missing_bundles = false;
    config->ignore_missing_inputs = false;

    config->heap_soft = NULL;
    config->heap_negated = NULL;
    config->ignore_locks = false;

    config->agent_specific.agent.bootstrap_policy_server = NULL;

    switch (agent_type)
    {
    case AGENT_TYPE_COMMON:
        config->agent_specific.common.eval_functions = true;
        config->agent_specific.common.show_classes = false;
        config->agent_specific.common.show_variables = false;
        config->agent_specific.common.policy_output_format = GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_NONE;
        /* Bitfields of warnings to be recorded, or treated as errors. */
        config->agent_specific.common.parser_warnings = PARSER_WARNING_ALL;
        config->agent_specific.common.parser_warnings_error = 0;
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
        free(config->original_input_file);
        free(config->input_file);
        free(config->input_dir);
        free(config);
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
            Class *cls = EvalContextClassGet(ctx, NULL, context);
            if (cls && !cls->is_soft)
            {
                FatalError(ctx, "You cannot use -D to define a reserved class");
            }

            EvalContextClassPutSoft(ctx, context, CONTEXT_SCOPE_NAMESPACE, "source=environment");
        }
    }

    switch (LogGetGlobalLevel())
    {
    case LOG_LEVEL_DEBUG:
        EvalContextClassPutHard(ctx, "debug_mode", "cfe_internal,source=agent");
        EvalContextClassPutHard(ctx, "opt_debug", "cfe_internal,source=agent");
        // intentional fall
    case LOG_LEVEL_VERBOSE:
        EvalContextClassPutHard(ctx, "verbose_mode", "cfe_internal,source=agent");
        // intentional fall
    case LOG_LEVEL_INFO:
        EvalContextClassPutHard(ctx, "inform_mode", "cfe_internal,source=agent");
        break;
    default:
        break;
    }

    if (config->agent_specific.agent.bootstrap_policy_server)
    {
        EvalContextClassPutHard(ctx, "bootstrap_mode", "source=environment");
    }

    if (config->color)
    {
        LoggingSetColor(config->color);
    }

    switch (config->agent_type)
    {
    case AGENT_TYPE_COMMON:
        EvalContextSetEvalOption(ctx, EVAL_OPTION_FULL, false);
        if (config->agent_specific.common.eval_functions)
            EvalContextSetEvalOption(ctx, EVAL_OPTION_EVAL_FUNCTIONS, true);
        break;

    default:
        break;
    }

    EvalContextSetIgnoreLocks(ctx, config->ignore_locks);

    if (DONTDO)
    {
        EvalContextClassPutHard(ctx, "opt_dry_run", "cfe_internal,source=environment");
    }
}

void GenericAgentConfigSetInputFile(GenericAgentConfig *config, const char *inputdir, const char *input_file)
{
    free(config->original_input_file);
    free(config->input_file);
    free(config->input_dir);

    config->original_input_file = xstrdup(input_file);

    if (inputdir && FilePathGetType(input_file) == FILE_PATH_TYPE_NON_ANCHORED)
    {
        config->input_file = StringFormat("%s%c%s", inputdir, FILE_SEPARATOR, input_file);
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
