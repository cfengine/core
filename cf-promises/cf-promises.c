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

#include <eval_context.h>
#include <conversion.h>
#include <syntax.h>
#include <rlist.h>
#include <parser.h>
#include <known_dirs.h>
#include <man.h>
#include <bootstrap.h>
#include <string_lib.h>
#include <loading.h>

#include <time.h>

static GenericAgentConfig *CheckOpts(int argc, char **argv);
static void ShowContextsFormatted(EvalContext *ctx);
static void ShowVariablesFormatted(EvalContext *ctx);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *const CF_PROMISES_SHORT_DESCRIPTION =
    "validate and analyze CFEngine policy code";

static const char *const CF_PROMISES_MANPAGE_LONG_DESCRIPTION = "cf-promises is a tool for checking CFEngine policy code. "
    "It operates by first parsing policy code checing for syntax errors. Second, it validates the integrity of "
    "policy consisting of multiple files. Third, it checks for semantic errors, e.g. specific attribute set rules. "
    "Finally, cf-promises attempts to expose errors by partially evaluating the policy, resolving as many variable and "
    "classes promise statements as possible. At no point does cf-promises make any changes to the system.";

/* Long-style only options, values must start above max ASCII value. */
enum
{
    OPT_EVAL_FUNCTIONS = 256,
    OPT_SHOW_CLASSES,
    OPT_SHOW_VARS
};

static const struct option OPTIONS[] =
{
    {"eval-functions", optional_argument, 0, OPT_EVAL_FUNCTIONS },
    {"show-classes", no_argument, 0, OPT_SHOW_CLASSES },
    {"show-vars", no_argument, 0, OPT_SHOW_VARS },
    {"help", no_argument, 0, 'h'},
    {"bundlesequence", required_argument, 0, 'b'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"dry-run", no_argument, 0, 'n'},
    {"version", no_argument, 0, 'V'},
    {"file", required_argument, 0, 'f'},
    {"define", required_argument, 0, 'D'},
    {"negate", required_argument, 0, 'N'},
    {"inform", no_argument, 0, 'I'},
    {"diagnostic", no_argument, 0, 'x'},
    {"reports", no_argument, 0, 'r'},
    {"policy-output-format", required_argument, 0, 'p'},
    {"syntax-description", required_argument, 0, 's'},
    {"full-check", no_argument, 0, 'c'},
    {"warn", required_argument, 0, 'W'},
    {"color", optional_argument, 0, 'C'},
    {"tag-release", required_argument, 0, 'T'},
    {"timestamp", no_argument, 0, 'l'},
    {NULL, 0, 0, '\0'}
};

static const char *const HINTS[] =
{
    "Evaluate functions during syntax checking (may catch more run-time errors). Possible values: 'yes', 'no'. Default is 'yes'",
    "Show discovered classes, including those defined in common bundles in policy",
    "Show discovered variables, including those defined without dependency to user-defined classes in policy",
    "Print the help message",
    "Use the specified bundlesequence for verification",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "All talk and no action mode - make no changes, only inform of promises not kept",
    "Output the version of the software",
    "Specify an alternative input file than the default",
    "Define a list of comma separated classes to be defined at the start of execution",
    "Define a list of comma separated classes to be undefined at the start of execution",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Activate internal diagnostics (developers only)",
    "Generate reports about configuration and insert into CFDB",
    "Output the parsed policy. Possible values: 'none', 'cf', 'json'. Default is 'none'. (experimental)",
    "Output a document describing the available syntax elements of CFEngine. Possible values: 'none', 'json'. Default is 'none'.",
    "Ensure full policy integrity checks",
    "Pass comma-separated <warnings>|all to enable non-default warnings, or error=<warnings>|all",
    "Enable colorized output. Possible values: 'always', 'auto', 'never'. If option is used, the default value is 'auto'",
    "Tag a directory with promises.cf with cf_promises_validated and cf_promises_release_id",
    "Log timestamps on each line of log output",
    NULL
};

/*******************************************************************/
/* Level 0 : Main                                                  */
/*******************************************************************/

int main(int argc, char *argv[])
{
    GenericAgentConfig *config = CheckOpts(argc, argv);
    EvalContext *ctx = EvalContextNew();
    GenericAgentConfigApply(ctx, config);

    GenericAgentDiscoverContext(ctx, config);
    Policy *policy = LoadPolicy(ctx, config);
    if (!policy)
    {
        Log(LOG_LEVEL_ERR, "Input files contain errors.");
        exit(EXIT_FAILURE);
    }

    GenericAgentPostLoadInit(ctx);

    if (NULL != config->tag_release_dir)
    {
        // write the validated file and the release ID
        bool tagged = GenericAgentTagReleaseDirectory(config, config->tag_release_dir, true, true);
        if (tagged)
        {
            Log(LOG_LEVEL_VERBOSE, "Release tagging done!");
        }
        else
        {
            Log(LOG_LEVEL_ERR, "The given directory could not be tagged, sorry.");
            exit(EXIT_FAILURE);
        }
    }

    if (SHOWREPORTS)
    {
        ShowPromises(policy->bundles, policy->bodies);
    }

    switch (config->agent_specific.common.policy_output_format)
    {
    case GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_CF:
    {
        Policy *output_policy = ParserParseFile(AGENT_TYPE_COMMON, config->input_file,
                                                config->agent_specific.common.parser_warnings,
                                                config->agent_specific.common.parser_warnings_error);
        Writer *writer = FileWriter(stdout);
        PolicyToString(policy, writer);
        WriterClose(writer);
        PolicyDestroy(output_policy);
    }
    break;

    case GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_JSON:
    {
        Policy *output_policy = ParserParseFile(AGENT_TYPE_COMMON, config->input_file,
                                                config->agent_specific.common.parser_warnings,
                                                config->agent_specific.common.parser_warnings_error);
        JsonElement *json_policy = PolicyToJson(output_policy);
        Writer *writer = FileWriter(stdout);
        JsonWrite(writer, json_policy, 2);
        WriterClose(writer);
        JsonDestroy(json_policy);
        PolicyDestroy(output_policy);
    }
    break;

    case GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_NONE:
        break;
    }

    if(config->agent_specific.common.show_classes)
    {
        ShowContextsFormatted(ctx);
    }

    if(config->agent_specific.common.show_variables)
    {
        ShowVariablesFormatted(ctx);
    }

    PolicyDestroy(policy);
    GenericAgentFinalize(ctx, config);
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON);
    config->tag_release_dir = NULL;

    while ((c = getopt_long(argc, argv, "dvnIf:D:N:VSrxMb:i:p:s:cg:hW:C::T:l",
                            OPTIONS, NULL))
           != -1)
    {
        switch (c)
        {
        case OPT_EVAL_FUNCTIONS:
            if (!optarg)
            {
                optarg = "yes";
            }
            config->agent_specific.common.eval_functions = strcmp("yes", optarg) == 0;
            break;

        case OPT_SHOW_CLASSES:
            config->agent_specific.common.show_classes = true;
            break;

        case OPT_SHOW_VARS:
            config->agent_specific.common.show_variables = true;
            break;

        case 'c':
            config->check_runnable = true;
            break;

        case 'f':
            GenericAgentConfigSetInputFile(config, GetInputDir(), optarg);
            MINUSF = true;
            break;

        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            break;

        case 'b':
            if (optarg)
            {
                Rlist *bundlesequence = RlistFromSplitString(optarg, ',');
                GenericAgentConfigSetBundleSequence(config, bundlesequence);
                RlistDestroy(bundlesequence);
            }
            break;

        case 'p':
            if (strcmp("none", optarg) == 0)
            {
                config->agent_specific.common.policy_output_format = GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_NONE;
            }
            else if (strcmp("cf", optarg) == 0)
            {
                config->agent_specific.common.policy_output_format = GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_CF;
            }
            else if (strcmp("json", optarg) == 0)
            {
                config->agent_specific.common.policy_output_format = GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_JSON;
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Invalid policy output format: '%s'. Possible values are 'none', 'cf', 'json'", optarg);
                exit(EXIT_FAILURE);
            }
            break;

        case 's':
            if (strcmp("none", optarg) == 0)
            {
                break;
            }
            else if (strcmp("json", optarg) == 0)
            {
                JsonElement *json_syntax = SyntaxToJson();
                Writer *out = FileWriter(stdout);
                JsonWrite(out, json_syntax, 0);
                FileWriterDetach(out);
                JsonDestroy(json_syntax);
                exit(EXIT_SUCCESS);
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Invalid syntax description output format: '%s'. Possible values are 'none', 'json'", optarg);
                exit(EXIT_FAILURE);
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
            GenericAgentWriteHelp(w, "cf-promises", OPTIONS, HINTS, true);
            FileWriterDetach(w);
        }
        exit(EXIT_SUCCESS);

        case 'M':
        {
            Writer *out = FileWriter(stdout);
            ManPageWrite(out, "cf-promises", time(NULL),
                         CF_PROMISES_SHORT_DESCRIPTION,
                         CF_PROMISES_MANPAGE_LONG_DESCRIPTION,
                         OPTIONS, HINTS,
                         true);
            FileWriterDetach(out);
            exit(EXIT_SUCCESS);
        }

        case 'r':
            SHOWREPORTS = true;
            break;

        case 'W':
            if (!GenericAgentConfigParseWarningOptions(config, optarg))
            {
                Log(LOG_LEVEL_ERR, "Error parsing warning option");
                exit(EXIT_FAILURE);
            }
            break;

        case 'x':
            Log(LOG_LEVEL_ERR, "Self-diagnostic functionality is retired.");
            exit(EXIT_SUCCESS);

        case 'C':
            if (!GenericAgentConfigParseColor(config, optarg))
            {
                exit(EXIT_FAILURE);
            }
            break;

        case 'T':
            GenericAgentConfigSetInputFile(config, optarg, "promises.cf");
            MINUSF = true;
            config->tag_release_dir = xstrdup(optarg);
            break;

        case 'l':
            LoggingEnableTimestamps(true);
            break;

        default:
        {
            Writer *w = FileWriter(stdout);
            GenericAgentWriteHelp(w, "cf-promises", OPTIONS, HINTS, true);
            FileWriterDetach(w);
        }
        exit(EXIT_FAILURE);

        }
    }

    if (!GenericAgentConfigParseArguments(config, argc - optind, argv + optind))
    {
        Log(LOG_LEVEL_ERR, "Too many arguments");
        exit(EXIT_FAILURE);
    }

    return config;
}

static void ShowContextsFormatted(EvalContext *ctx)
{
    ClassTableIterator *iter = EvalContextClassTableIteratorNewGlobal(ctx, NULL, true, true);
    Class *cls = NULL;

    Seq *seq = SeqNew(1000, free);

    while ((cls = ClassTableIteratorNext(iter)))
    {
        char *class_name = ClassRefToString(cls->ns, cls->name);
        StringSet *tagset = EvalContextClassTags(ctx, cls->ns, cls->name);
        Buffer *tagbuf = StringSetToBuffer(tagset, ',');

        char *line;
        xasprintf(&line, "%-60s %-40s", class_name, BufferData(tagbuf));
        SeqAppend(seq, line);

        BufferDestroy(tagbuf);
        free(class_name);
    }

    SeqSort(seq, (SeqItemComparator)strcmp, NULL);

    printf("%-60s %-40s\n", "Class name", "Meta tags");

    for (size_t i = 0; i < SeqLength(seq); i++)
    {
        const char *context = SeqAt(seq, i);
        printf("%s\n", context);
    }

    SeqDestroy(seq);

    ClassTableIteratorDestroy(iter);
}

static void ShowVariablesFormatted(EvalContext *ctx)
{
    VariableTableIterator *iter = EvalContextVariableTableIteratorNew(ctx, NULL, NULL, NULL);
    Variable *v = NULL;

    Seq *seq = SeqNew(2000, free);

    while ((v = VariableTableIteratorNext(iter)))
    {
        char *varname = VarRefToString(v->ref, true);

        Writer *w = StringWriter();
        RvalWrite(w, v->rval);

        StringSet *tagset = EvalContextVariableTags(ctx, v->ref);
        Buffer *tagbuf = StringSetToBuffer(tagset, ',');

        char *line;
        const char *var_value;

        if(StringIsPrintable(StringWriterData(w)))
        {
            var_value = StringWriterData(w);
        }
        else
        {
            var_value = "<non-printable>";
        }

        xasprintf(&line, "%-40s %-60s %-40s", varname, var_value, BufferData(tagbuf));

        SeqAppend(seq, line);

        BufferDestroy(tagbuf);
        WriterClose(w);
        free(varname);
    }

    SeqSort(seq, (SeqItemComparator)strcmp, NULL);

    printf("%-40s %-60s %-40s\n", "Variable name", "Variable value", "Meta tags");

    for (size_t i = 0; i < SeqLength(seq); i++)
    {
        const char *variable = SeqAt(seq, i);
        printf("%s\n", variable);
    }

    SeqDestroy(seq);
    VariableTableIteratorDestroy(iter);
}
