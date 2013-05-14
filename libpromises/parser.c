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

#include "parser.h"
#include "parser_state.h"

#include "misc_lib.h"

#include <errno.h>

int yyparse(void);

ParserState P = { 0 };

extern FILE *yyin;

static void ParserStateReset(ParserState *p)
{
    p->policy = NULL;

    p->warnings = PARSER_WARNING_ALL;

    free(p->current_line);
    p->current_line = NULL;
    p->line_no = 1;
    p->line_pos = 1;
    p->error_count = 0;
    p->warning_count = 0;
    p->list_nesting = 0;
    p->arg_nesting = 0;

    free(p->current_namespace);
    p->current_namespace = xstrdup("default");

    p->currentid[0] = '\0';
    p->currentstring = NULL;
    p->currenttype[0] = '\0';
    p->currentclasses = NULL;
    p->currentRlist = NULL;
    p->currentpromise = NULL;
    p->currentbody = NULL;
    p->promiser = NULL;
    p->blockid[0] = '\0';
    p->blocktype[0] = '\0';
}

Policy *ParserParseFile(const char *path, unsigned int warnings, unsigned int warnings_error)
{
    ParserStateReset(&P);
    P.policy = PolicyNew();

    P.warnings = warnings;
    P.warnings_error = warnings_error;

    strncpy(P.filename, path, CF_MAXVARSIZE);

    yyin = fopen(path, "r");
    if (yyin == NULL)
    {
        fprintf(stderr, "Error opening file %s: %s\n",
                path, strerror(errno));
        exit(1);
    }

    while (!feof(yyin))
    {
        yyparse();

        if (ferror(yyin))
        {
            perror("cfengine");
            exit(1);
        }
    }

    fclose(yyin);

    if (P.error_count > 0)
    {
        PolicyDestroy(P.policy);
        return NULL;
    }

    return P.policy;
}

int ParserWarningFromString(const char *warning_str)
{
    if (strcmp("deprecated", warning_str) == 0)
    {
        return PARSER_WARNING_DEPRECATED;
    }
    else if (strcmp("removed", warning_str) == 0)
    {
        return PARSER_WARNING_REMOVED;
    }
    else if (strcmp("all", warning_str) == 0)
    {
        return PARSER_WARNING_ALL;
    }
    else
    {
        return -1;
    }
}

const char *ParserWarningToString(unsigned int warning)
{
    switch (warning)
    {
    case PARSER_WARNING_DEPRECATED:
        return "deprecated";
    case PARSER_WARNING_REMOVED:
        return "removed";

    default:
        ProgrammingError("Invalid parser warning: %u", warning);
    }
}
