/*
   Copyright 2018 Northern.tech AS

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
#include <loading.h>

#include <eval_context.h>
#include <files_hashes.h>
#include <file_lib.h>
#include <files_names.h>
#include <string_lib.h>
#include <parser.h>
#include <expand.h>
#include <rlist.h>
#include <misc_lib.h>
#include <class.h>
#include <fncall.h>
#include <known_dirs.h>
#include <ornaments.h>
#include <policy.h>
#include <cleanup.h>

// TODO: remove
#include <vars.h>                                         /* IsCf3VarString */
#include <audit.h>                                        /* FatalError */


static Policy *LoadPolicyFile(EvalContext *ctx, GenericAgentConfig *config, const char *policy_file,
                              StringSet *parsed_files_and_checksums, StringSet *failed_files);




/*
 * The difference between filename and input_input file is that the latter is the file specified by -f or
 * equivalently the file containing body common control. This will hopefully be squashed in later refactoring.
 */
Policy *Cf3ParseFile(const GenericAgentConfig *config, const char *input_path)
{
    struct stat statbuf;

    if (stat(input_path, &statbuf) == -1)
    {
        if (config->ignore_missing_inputs)
        {
            return PolicyNew();
        }

        Log(LOG_LEVEL_ERR, "Can't stat file '%s' for parsing. (stat: %s)", input_path, GetErrorStr());
        DoCleanupAndExit(EXIT_FAILURE);
    }
    else if (S_ISDIR(statbuf.st_mode))
    {
        if (config->ignore_missing_inputs)
        {
            return PolicyNew();
        }

        Log(LOG_LEVEL_ERR, "Can't parse directory '%s'.", input_path);
        DoCleanupAndExit(EXIT_FAILURE);
    }

#ifndef _WIN32
    if (config->check_not_writable_by_others && (statbuf.st_mode & (S_IWGRP | S_IWOTH)))
    {
        Log(LOG_LEVEL_ERR, "File %s (owner %ju) is writable by others (security exception)", input_path, (uintmax_t)statbuf.st_uid);
        DoCleanupAndExit(EXIT_FAILURE);
    }
#endif

    Log(LOG_LEVEL_VERBOSE, "BEGIN parsing file: %s", input_path);

    if (!FileCanOpen(input_path, "r"))
    {
        Log(LOG_LEVEL_ERR, "Can't open file '%s' for parsing", input_path);
        DoCleanupAndExit(EXIT_FAILURE);
    }

    Policy *policy = NULL;
    if (StringEndsWith(input_path, ".json"))
    {
        Writer *contents = FileRead(input_path, SIZE_MAX, NULL);
        if (!contents)
        {
            Log(LOG_LEVEL_ERR, "Error reading JSON input file '%s'", input_path);
            return NULL;
        }
        JsonElement *json_policy = NULL;
        const char *data = StringWriterData(contents);
        if (JsonParse(&data, &json_policy) != JSON_PARSE_OK)
        {
            Log(LOG_LEVEL_ERR, "Error parsing JSON input file '%s'", input_path);
            WriterClose(contents);
            return NULL;
        }

        policy = PolicyFromJson(json_policy);
        if (policy == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to deserialize a policy from the JSON input file '%s'",
                input_path);
            JsonDestroy(json_policy);
            WriterClose(contents);
            return NULL;
        }

        JsonDestroy(json_policy);
        WriterClose(contents);
    }
    else
    {
        if (config->agent_type == AGENT_TYPE_COMMON)
        {
            policy = ParserParseFile(config->agent_type, input_path, config->agent_specific.common.parser_warnings, config->agent_specific.common.parser_warnings_error);
        }
        else
        {
            policy = ParserParseFile(config->agent_type, input_path, 0, 0);
        }
    }

    Log(LOG_LEVEL_VERBOSE, "END   parsing file: %s", input_path);
    return policy;
}

static Policy *LoadPolicyInputFiles(EvalContext *ctx, GenericAgentConfig *config, const Rlist *inputs, StringSet *parsed_files_and_checksums, StringSet *failed_files)
{
    Policy *policy = PolicyNew();

    for (const Rlist *rp = inputs; rp; rp = rp->next)
    {
        if (rp->val.type != RVAL_TYPE_SCALAR)
        {
            Log(LOG_LEVEL_ERR, "Non-file object in inputs list");
            continue;
        }

        const char *unresolved_input = RlistScalarValue(rp);
        if (IsExpandable(unresolved_input))
        {
            PolicyResolve(ctx, policy, config);
        }

        Rval resolved_input = EvaluateFinalRval(ctx, policy, NULL, "sys", rp->val, true, NULL);

        Policy *aux_policy = NULL;
        switch (resolved_input.type)
        {
        case RVAL_TYPE_SCALAR:
            if (IsCf3VarString(RvalScalarValue(resolved_input)))
            {
                Log(LOG_LEVEL_ERR, "Unresolved variable '%s' in input list, cannot parse", RvalScalarValue(resolved_input));
                break;
            }

            aux_policy = LoadPolicyFile(ctx, config, GenericAgentResolveInputPath(config, RvalScalarValue(resolved_input)), parsed_files_and_checksums, failed_files);
            break;

        case RVAL_TYPE_LIST:
            aux_policy = LoadPolicyInputFiles(ctx, config, RvalRlistValue(resolved_input), parsed_files_and_checksums, failed_files);
            break;

        default:
            ProgrammingError("Unknown type in input list for parsing: %d", resolved_input.type);
            break;
        }

        if (aux_policy)
        {
            policy = PolicyMerge(policy, aux_policy);
        }

        RvalDestroy(resolved_input);
    }

    return policy;
}

// TODO: should be replaced by something not complected with loading
static void ShowContext(EvalContext *ctx)
{
    Seq *hard_contexts = SeqNew(1000, NULL);
    Seq *soft_contexts = SeqNew(1000, NULL);

    {
        ClassTableIterator *iter = EvalContextClassTableIteratorNewGlobal(ctx, NULL, true, true);
        Class *cls = NULL;
        while ((cls = ClassTableIteratorNext(iter)))
        {
            if (cls->is_soft)
            {
                SeqAppend(soft_contexts, cls->name);
            }
            else
            {
                SeqAppend(hard_contexts, cls->name);
            }
        }

        ClassTableIteratorDestroy(iter);
    }

    SeqSort(soft_contexts, (SeqItemComparator)strcmp, NULL);
    SeqSort(hard_contexts, (SeqItemComparator)strcmp, NULL);

    Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");

    {
        Log(LOG_LEVEL_VERBOSE, "BEGIN Discovered hard classes:");

        for (size_t i = 0; i < SeqLength(hard_contexts); i++)
        {
            const char *context = SeqAt(hard_contexts, i);
            Log(LOG_LEVEL_VERBOSE, "C: discovered hard class %s", context);
        }

        Log(LOG_LEVEL_VERBOSE, "END Discovered hard classes");
    }

    Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");

    if (SeqLength(soft_contexts))
    {
        Log(LOG_LEVEL_VERBOSE, "BEGIN initial soft classes:");

        for (size_t i = 0; i < SeqLength(soft_contexts); i++)
        {
            const char *context = SeqAt(soft_contexts, i);
            Log(LOG_LEVEL_VERBOSE, "C: added soft class %s", context);
        }

        Log(LOG_LEVEL_VERBOSE, "END initial soft classes");
    }

    SeqDestroy(hard_contexts);
    SeqDestroy(soft_contexts);
}

static void RenameMainBundle(EvalContext *ctx, Policy *policy)
{
    assert(policy != NULL);
    assert(ctx != NULL);
    assert(policy->bundles != NULL);
    char *const entry_point = GetRealPath(EvalContextGetEntryPoint(ctx));
    if (NULL_OR_EMPTY(entry_point))
    {
        free(entry_point);
        return;
    }
    Seq *bundles = policy->bundles;
    int length = SeqLength(bundles);
    bool removed = false;
    for (int i = 0; i < length; ++i)
    {
        Bundle *const bundle = SeqAt(bundles, i);
        if (StringSafeEqual(bundle->name, "__main__"))
        {
            char *abspath = GetRealPath(bundle->source_path);
            if (StringSafeEqual(abspath, entry_point))
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Redefining __main__ bundle from file %s to be main",
                    abspath);
                strncpy(bundle->name, "main", 4+1);
                // "__main__" is always big enough for "main"
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Dropping __main__ bundle from file %s (entry point: %s)",
                    abspath,
                    entry_point);
                removed = true;
                SeqSet(bundles, i, NULL); // SeqSet calls destroy function
            }
            free(abspath);
        }
    }
    if (removed)
    {
        SeqRemoveNulls(bundles);
    }
    free(entry_point);
}

static Policy *LoadPolicyFile(EvalContext *ctx, GenericAgentConfig *config, const char *policy_file, StringSet *parsed_files_and_checksums, StringSet *failed_files)
{
    unsigned char digest[EVP_MAX_MD_SIZE + 1] = { 0 };
    char hashbuffer[CF_HOSTKEY_STRING_SIZE] = { 0 };
    char hashprintbuffer[CF_BUFSIZE] = { 0 };

    HashFile(policy_file, digest, CF_DEFAULT_DIGEST, false);
    snprintf(hashprintbuffer, CF_BUFSIZE - 1, "{checksum}%s",
             HashPrintSafe(hashbuffer, sizeof(hashbuffer), digest,
                           CF_DEFAULT_DIGEST, true));

    Log(LOG_LEVEL_DEBUG, "Hashed policy file %s to %s", policy_file, hashprintbuffer);

    if (StringSetContains(parsed_files_and_checksums, policy_file))
    {
        Log(LOG_LEVEL_VERBOSE, "Skipping loading of duplicate policy file %s", policy_file);
        return NULL;
    }
    else if (StringSetContains(parsed_files_and_checksums, hashprintbuffer))
    {
        Log(LOG_LEVEL_VERBOSE, "Skipping loading of duplicate (detected by hash) policy file %s", policy_file);
        return NULL;
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "Loading policy file %s", policy_file);
    }

    Policy *policy = Cf3ParseFile(config, policy_file);
    // we keep the checksum and the policy file name to help debugging
    StringSetAdd(parsed_files_and_checksums, xstrdup(policy_file));
    StringSetAdd(parsed_files_and_checksums, xstrdup(hashprintbuffer));

    if (policy)
    {
        RenameMainBundle(ctx, policy);
        Seq *errors = SeqNew(10, free);
        if (!PolicyCheckPartial(policy, errors))
        {
            Writer *writer = FileWriter(stderr);
            for (size_t i = 0; i < errors->length; i++)
            {
                PolicyErrorWrite(writer, errors->data[i]);
            }
            WriterClose(writer);
            SeqDestroy(errors);

            StringSetAdd(failed_files, xstrdup(policy_file));
            PolicyDestroy(policy);
            return NULL;
        }

        SeqDestroy(errors);
    }
    else
    {
        StringSetAdd(failed_files, xstrdup(policy_file));
        return NULL;
    }

    PolicyResolve(ctx, policy, config);

    Body *body_common_control = PolicyGetBody(policy, NULL, "common", "control");
    Body *body_file_control = PolicyGetBody(policy, NULL, "file", "control");

    if (body_common_control)
    {
        Seq *potential_inputs = BodyGetConstraint(body_common_control, "inputs");
        Constraint *cp = EffectiveConstraint(ctx, potential_inputs);
        SeqDestroy(potential_inputs);

        if (cp)
        {
            Policy *aux_policy = LoadPolicyInputFiles(ctx, config, RvalRlistValue(cp->rval), parsed_files_and_checksums, failed_files);
            if (aux_policy)
            {
                policy = PolicyMerge(policy, aux_policy);
            }
        }
    }

    if (body_file_control)
    {
        Seq *potential_inputs = BodyGetConstraint(body_file_control, "inputs");
        Constraint *cp = EffectiveConstraint(ctx, potential_inputs);
        SeqDestroy(potential_inputs);

        if (cp)
        {
            Policy *aux_policy = LoadPolicyInputFiles(ctx, config, RvalRlistValue(cp->rval), parsed_files_and_checksums, failed_files);
            if (aux_policy)
            {
                policy = PolicyMerge(policy, aux_policy);
            }
        }
    }

    return policy;
}

static bool VerifyBundleSequence(EvalContext *ctx, const Policy *policy, const GenericAgentConfig *config)
{
    Rlist *fallback = NULL;
    const Rlist *bundlesequence = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_BUNDLESEQUENCE);
    if (!bundlesequence)
    {
        RlistAppendScalar(&fallback, "main");
        bundlesequence = fallback;
    }

    const char *name;
    int ok = true;
    for (const Rlist *rp = bundlesequence; rp != NULL; rp = rp->next)
    {
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
            ok = false;
            {
                Writer *w = StringWriter();
                WriterWrite(w, "Illegal item found in bundlesequence '");
                RvalWrite(w, rp->val);
                WriterWrite(w, "'");
                Log(LOG_LEVEL_ERR, "%s", StringWriterData(w));
                WriterClose(w);
            }
            continue;
        }

        if (!config->ignore_missing_bundles && !PolicyGetBundle(policy, NULL, NULL, name))
        {
            Log(LOG_LEVEL_ERR, "Bundle '%s' listed in the bundlesequence is not a defined bundle", name);
            ok = false;
        }
    }

    RlistDestroy(fallback);
    return ok;
}

/**
 * @brief Reads the release_id file from inputs and return a JsonElement.
 */
static JsonElement *ReadReleaseIdFileFromInputs()
{
    char filename[CF_MAXVARSIZE];

    GetReleaseIdFile(GetInputDir(), filename, sizeof(filename));

    struct stat sb;
    if (stat(filename, &sb) == -1)
    {
        return NULL;
    }

    JsonElement *validated_doc = NULL;
    JsonParseError err = JsonParseFile(filename, 4096, &validated_doc);
    if (err != JSON_PARSE_OK)
    {
        Log(LOG_LEVEL_WARNING,
            "Could not read release ID: '%s' did not contain valid JSON data. "
            "(JsonParseFile: '%s')", filename, JsonParseErrorToString(err));
    }

    return validated_doc;
}

Policy *LoadPolicy(EvalContext *ctx, GenericAgentConfig *config)
{
    StringSet *parsed_files_and_checksums = StringSetNew();
    StringSet *failed_files = StringSetNew();

    Banner("Loading policy");

    Policy *policy = LoadPolicyFile(ctx, config, config->input_file,
                                    parsed_files_and_checksums, failed_files);

    if (StringSetSize(failed_files) > 0)
    {
        Log(LOG_LEVEL_ERR, "There are syntax errors in policy files");
        DoCleanupAndExit(EXIT_FAILURE);
    }

    StringSetDestroy(parsed_files_and_checksums);
    StringSetDestroy(failed_files);

    {
        Seq *errors = SeqNew(100, PolicyErrorDestroy);

        if (PolicyCheckPartial(policy, errors))
        {
            if (!config->bundlesequence &&
                (PolicyIsRunnable(policy) || config->check_runnable))
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Running full policy integrity checks");
                PolicyCheckRunnable(ctx, policy, errors,
                                    config->ignore_missing_bundles);
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
            DoCleanupAndExit(EXIT_FAILURE); // TODO: do not exit
        }

        SeqDestroy(errors);
    }

    if (LogGetGlobalLevel() >= LOG_LEVEL_VERBOSE)
    {
        Legend();
        ShowContext(ctx);
    }

    if (config->agent_type == AGENT_TYPE_AGENT)
    {
        Banner("Preliminary variable/class-context convergence");
    }

    if (policy)
    {
        /* store names of all bundles in the EvalContext */
        for (size_t i = 0; i < SeqLength(policy->bundles); i++)
        {
            Bundle *bp = SeqAt(policy->bundles, i);
            EvalContextPushBundleName(ctx, bp->name);
        }

        for (size_t i = 0; i < SeqLength(policy->bundles); i++)
        {
            Bundle *bp = SeqAt(policy->bundles, i);
            EvalContextStackPushBundleFrame(ctx, bp, NULL, false);

            for (size_t j = 0; j < SeqLength(bp->promise_types); j++)
            {
                PromiseType *sp = SeqAt(bp->promise_types, j);
                EvalContextStackPushPromiseTypeFrame(ctx, sp);

                for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
                {
                    Promise *pp = SeqAt(sp->promises, ppi);

                    /* Skip constraint syntax verification through all
                     * slist/container iterations for cf-promises! cf-agent
                     * still checks though. */
                    if (config->agent_type != AGENT_TYPE_COMMON)
                    {
                        ExpandPromise(ctx, pp, CommonEvalPromise, NULL);
                    }
                }

                EvalContextStackPopFrame(ctx);
            }

            EvalContextStackPopFrame(ctx);
        }

        PolicyResolve(ctx, policy, config);

        // TODO: need to move this inside PolicyCheckRunnable eventually.
        if (!config->bundlesequence && config->check_runnable)
        {
            // only verify policy-defined bundlesequence for cf-agent, cf-promises
            if ((config->agent_type == AGENT_TYPE_AGENT) ||
                (config->agent_type == AGENT_TYPE_COMMON))
            {
                if (!VerifyBundleSequence(ctx, policy, config))
                {
                    FatalError(ctx, "Errors in promise bundles: could not verify bundlesequence");
                }
            }
        }
    }

    JsonElement *validated_doc = ReadReleaseIdFileFromInputs();
    if (validated_doc)
    {
        const char *release_id = JsonObjectGetAsString(validated_doc, "releaseId");
        if (release_id)
        {
            policy->release_id = xstrdup(release_id);
        }
        JsonDestroy(validated_doc);
    }

    return policy;
}
