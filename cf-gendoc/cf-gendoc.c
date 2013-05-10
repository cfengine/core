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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "generic_agent.h"

#include "env_context.h"
#include "files_names.h"
#include "export_xml.h"
#include "item_lib.h"
#include "sort.h"
#include "conversion.h"
#include "expand.h"
#include "misc_lib.h"

static void GenerateManual(EvalContext *ctx);
static void GenerateXml(void);

static GenericAgentConfig *CheckOpts(int argc, char **argv);

char SOURCE_DIR[CF_BUFSIZE];
char OUTPUT_FILE[CF_BUFSIZE];

int GENERATE_XML = false;

static const struct option OPTIONS[] =
{
    {"help", no_argument, 0, 'h'},
    {"xml", no_argument, 0, 'x'},
    {"input-dir", required_argument, 0, 'i'},
    {"output-file", required_argument, 0, 'o'},
    {NULL, 0, 0, '\0'}
};

static const char *HINTS[] =
{
    "Print the help message",
    "Generate documentation in XML format",
    "Input directory for Texinfo fragments (defaults to current directory)",
    "Output file for XML documentation",
    NULL
};

/*****************************************************************************/

int main(int argc, char *argv[])
{
    EvalContext *ctx = EvalContextNew();

    GenericAgentConfig *config = CheckOpts(argc, argv);
    GenericAgentConfigApply(ctx, config);

    GenericAgentDiscoverContext(ctx, config);

    if (GENERATE_XML)
    {
        GenerateXml();
    }
    else
    {
        GenerateManual(ctx);
    }

    GenericAgentConfigDestroy(config);
    EvalContextDestroy(ctx);
    return 0;
}

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_GENDOC);

    if (getcwd(SOURCE_DIR, CF_BUFSIZE) == NULL)
    {
        UnexpectedError("Failed to get the pathname to the current directory");
    }
    snprintf(OUTPUT_FILE, CF_BUFSIZE, "%scf3-Reference.texinfo", SOURCE_DIR);

    while ((c = getopt_long(argc, argv, "hxi:o:", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'h':
            PrintHelp("cf-gendoc", OPTIONS, HINTS, false);
            exit(0);

        case 'x':
            GENERATE_XML = true;
            break;

        case 'i':
            strlcpy(SOURCE_DIR, optarg, CF_BUFSIZE);

        case 'o':
            strlcpy(OUTPUT_FILE, optarg, CF_BUFSIZE);
            break;

        default:
            PrintHelp("cf-gendoc", OPTIONS, HINTS, false);
            exit(1);
        }
    }

    if (argv[optind] != NULL)
    {
        Log(LOG_LEVEL_ERR, "Unexpected argument: %s", argv[optind]);
    }

    return config;
}

static void GenerateManual(EvalContext *ctx)
{
    TexinfoManual(ctx, SOURCE_DIR, OUTPUT_FILE);
}

static void GenerateXml(void)
{
    if (OUTPUT_FILE == NULL)
    {
        /* Reconsider this once agents do not output any error messages to stdout */
        Log(LOG_LEVEL_ERR, "Please specify output file");
        exit(EXIT_FAILURE);
    }
    else
    {
        FILE *out = fopen(OUTPUT_FILE, "w");

        if (out == NULL)
        {
            Log(LOG_LEVEL_ERR, "Unable to open %s for writing", OUTPUT_FILE);
            exit(EXIT_FAILURE);
        }
        XmlManual(SOURCE_DIR, out);
    }
}
