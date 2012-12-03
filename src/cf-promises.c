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

#include "cf3.defs.h"

#include "env_context.h"
#include "conversion.h"
#include "reporting.h"
#include "cfstream.h"

/*******************************************************************/

static void ThisAgentInit(void);
static GenericAgentConfig CheckOpts(int argc, char **argv);

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
    {"analysis", no_argument, 0, 'a'},
    {"reports", no_argument, 0, 'r'},
    {"parse-tree", no_argument, 0, 'p'},
    {"gcc-brief-format", no_argument, 0, 'g'},
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
    "Perform additional analysis of configuration",
    "Generate reports about configuration and insert into CFDB",
    "Print a parse tree for the policy file in JSON format",
    "Use the GCC brief-format for output",
    NULL
};

/*******************************************************************/
/* Level 0 : Main                                                  */
/*******************************************************************/

int main(int argc, char *argv[])
{
    GenericAgentConfig config = CheckOpts(argc, argv);
    ReportContext *report_context = OpenReports("common");
    
    GenericInitialize("common", config, report_context);
    ThisAgentInit();
    AnalyzePromiseConflicts();
    CloseReports("commmon", report_context);

    if (ERRORCOUNT > 0)
    {
        CfOut(cf_verbose, "", " !! Inputs are invalid\n");
        exit(1);
    }
    else
    {
        CfOut(cf_verbose, "", " -> Inputs are valid\n");
        exit(0);
    }
}

/*******************************************************************/
/* Level 1                                                         */
/*******************************************************************/

GenericAgentConfig CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    int c;
    GenericAgentConfig config = GenericAgentDefaultConfig(AGENT_TYPE_COMMON);

    while ((c = getopt_long(argc, argv, "advnIf:D:N:VSrxMb:pg:h", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'f':

            if (optarg && (strlen(optarg) < 5))
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

        case 'b':
            if (optarg)
            {
                config.bundlesequence = SplitStringAsRList(optarg, ',');
                CBUNDLESEQUENCE_STR = optarg;
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
            PrependRScalar(&GOALS, "goal.*", CF_SCALAR);
            SHOWREPORTS = true;
            break;

        case 'x':
            SelfDiagnostic();
            exit(0);

        case 'a':
            printf("Self-analysis is not yet implemented.\n");
            exit(0);
            break;

        case 'p':
            SHOW_PARSE_TREE = true;
            break;

        case 'g':
            USE_GCC_BRIEF_FORMAT = true;
            break;

        default:
            Syntax("cf-promises - cfengine's promise analyzer", OPTIONS, HINTS, ID);
            exit(1);

        }
    }

    if (argv[optind] != NULL)
    {
        CfOut(cf_error, "", "Unexpected argument with no preceding option: %s\n", argv[optind]);
    }

    CfDebug("Set debugging\n");

    return config;
}

/*******************************************************************/

static void ThisAgentInit(void)
{
    AddGoalsToDB(Rlist2String(GOALS, ","));
    SHOWREPORTS = false;
}

/* EOF */
