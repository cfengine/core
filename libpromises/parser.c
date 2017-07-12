/*
   Copyright 2017 Northern.tech AS

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

#include <parser.h>
#include <parser_state.h>

#include <misc_lib.h>
#include <file_lib.h>

#include <errno.h>

int yyparse(void);

ParserState P = { 0 }; /* GLOBAL_X */

extern FILE *yyin;

static void ParserStateReset(ParserState *p, bool discard)
{
    p->agent_type = AGENT_TYPE_COMMON;
    p->warnings = PARSER_WARNING_ALL;
    p->policy = NULL;

    int i = CF_MAX_NESTING;
    while (i-- > 0) /* Clear stacks from top down */
    {
        if (discard)
        {
            free(p->currentfnid[i]);
            RlistDestroy(p->giveargs[i]);
            FnCallDestroy(p->currentfncall[i]);
        }
        else
        {
            assert(!p->currentfnid[i]);
            assert(!p->giveargs[i]);
            assert(!p->currentfncall[i]);
        }
        p->currentfnid[i] = NULL;
        p->giveargs[i] = NULL;
        p->currentfncall[i] = NULL;
    }

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
    if (p->currentstring)
    {
        free(p->currentstring);
    }
    p->currentstring = NULL;
    p->currenttype[0] = '\0';
    if (p->currentclasses)
    {
        free(p->currentclasses);
    }
    p->currentclasses = NULL;
    p->currentRlist = NULL;
    p->currentpromise = NULL;
    p->currentbody = NULL;
    if (p->promiser)
    {
        free(p->promiser);
    }
    p->promiser = NULL;
    p->blockid[0] = '\0';
    p->blocktype[0] = '\0';
    p->rval = RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
}

static void ParserStateClean(ParserState *p)
{
    free(p->current_namespace);
    p->current_namespace = NULL;
}

Policy *ParserParseFile(AgentType agent_type, const char *path, unsigned int warnings, unsigned int warnings_error)
{
    ParserStateReset(&P, false);

    P.agent_type = agent_type;
    P.policy = PolicyNew();

    P.warnings = warnings;
    P.warnings_error = warnings_error;

    strlcpy(P.filename, path, CF_MAXVARSIZE);

    yyin = safe_fopen(path, "rt");
    if (yyin == NULL)
    {
        Log(LOG_LEVEL_ERR, "While opening file '%s' for parsing. (fopen: %s)", path, GetErrorStr());
        exit(EXIT_FAILURE);
    }

    while (!feof(yyin))
    {
        yyparse();

        if (ferror(yyin))
        {
            perror("cfengine");
            exit(EXIT_FAILURE);
        }
    }

    fclose(yyin);

    if (P.error_count > 0)
    {
        PolicyDestroy(P.policy);
        ParserStateReset(&P, true);
        ParserStateClean(&P);
        return NULL;
    }

    Policy *policy = P.policy;
    ParserStateReset(&P, false);
    ParserStateClean(&P);
    return policy;
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
