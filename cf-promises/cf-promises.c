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

#include "env_context.h"
#include "conversion.h"
#include "reporting.h"
#include "cfstream.h"
#include "logging.h"
#include "syntax.h"
#include "rlist.h"
#include "parser.h"

/*******************************************************************/

static void ThisAgentInit(void);
static GenericAgentConfig *CheckOpts(int argc, char **argv);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *ID = "The promise agent is a validator and analysis tool for\n"
    "configuration files belonging to any of the components\n"
    "of Cfengine. Configurations that make changes must be\n" "approved by this validator before being executed.";

static const struct option OPTIONS[] =
{
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
    {"full-check", no_argument, 0, 'c'},
    {NULL, 0, 0, '\0'}
};

static const char *HINTS[] =
{
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
    "Ensure full policy integrity checks",
    NULL
};

/*******************************************************************/
/* Level 0 : Main                                                  */
/*******************************************************************/

int main(int argc, char *argv[])
{
    GenericAgentConfig *config = CheckOpts(argc, argv);
    ReportContext *report_context = OpenReports(config->agent_type);
    
    GenericAgentDiscoverContext(config, report_context);
    Policy *policy = GenericAgentLoadPolicy(config->agent_type, config, report_context);

    if (SHOWREPORTS)
    {
        CompilationReport(policy, config->input_file);
    }

    CheckLicenses();

    switch (config->agent_specific.common.policy_output_format)
    {
    case GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_CF:
        {
            Policy *output_policy = ParserParseFile(GenericAgentResolveInputPath(config->input_file, config->input_file));
            Writer *writer = FileWriter(stdout);
            PolicyToString(policy, writer);
            WriterClose(writer);
            PolicyDestroy(output_policy);
        }
        break;

    case GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_JSON:
        {
            Policy *output_policy = ParserParseFile(GenericAgentResolveInputPath(config->input_file, config->input_file));
            JsonElement *json_policy = PolicyToJson(output_policy);
            Writer *writer = FileWriter(stdout);
            JsonElementPrint(writer, json_policy, 2);
            WriterClose(writer);
            JsonElementDestroy(json_policy);
            PolicyDestroy(output_policy);
        }
        break;

    case GENERIC_AGENT_CONFIG_COMMON_POLICY_OUTPUT_FORMAT_NONE:
        break;
    }

    ThisAgentInit();
    AnalyzePromiseConflicts();

    GenericAgentConfigDestroy(config);
    CloseReports("commmon", report_context);

    if (ERRORCOUNT > 0)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Inputs are invalid\n");
        exit(1);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Inputs are valid\n");
        exit(0);
    }
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON);

    while ((c = getopt_long(argc, argv, "dvnIf:D:N:VSrxMb:i:p:cg:h", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'c':
            config->check_runnable = true;
            break;

        case 'f':

            if (optarg && (strlen(optarg) < 5))
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

        case 'b':
            if (optarg)
            {
                Rlist *bundlesequence = RlistFromSplitString(optarg, ',');
                GenericAgentConfigSetBundleSequence(config, bundlesequence);
                RlistDestroy(bundlesequence);
                CBUNDLESEQUENCE_STR = optarg; // TODO: wtf is this
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
                CfOut(OUTPUT_LEVEL_ERROR, "", "Invalid policy output format: '%s'. Possible values are 'none', 'cf', 'json'", optarg);
                exit(EXIT_FAILURE);
            }
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
            break;

        case 'n':
            DONTDO = true;
            IGNORELOCK = true;
            LOOKUP = true;
            HardClass("opt_dry_run");
            break;

        case 'V':
            PrintVersionBanner("cf-promises");
            exit(0);

        case 'h':
            Syntax("cf-promises - cfengine's promise analyzer", OPTIONS, HINTS, ID);
            exit(0);

        case 'M':
            ManPage("cf-promises - cfengine's promise analyzer", OPTIONS, HINTS, ID);
            exit(0);

        case 'r':
            SHOWREPORTS = true;
            break;

        case 'x':
            CfOut(OUTPUT_LEVEL_ERROR, "", "Self-diagnostic functionality is retired.");
            exit(0);

        default:
            Syntax("cf-promises - cfengine's promise analyzer", OPTIONS, HINTS, ID);
            exit(1);

        }
    }

    if (argv[optind] != NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unexpected argument with no preceding option: %s\n", argv[optind]);
    }

    CfDebug("Set debugging\n");

    return config;
}

/*******************************************************************/

static void ThisAgentInit(void)
{
    SHOWREPORTS = false;
}

/* EOF */
