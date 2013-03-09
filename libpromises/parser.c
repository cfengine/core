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

#include "parser.h"
#include "parser_state.h"

#include <errno.h>

int yyparse(void);

ParserState P = { 0 };

extern FILE *yyin;

static void ParserStateReset(ParserState *p)
{
    p->policy = NULL;

    p->line_no = 1;
    p->line_pos = 1;
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
    p->promiser = NULL;
    p->blockid[0] = '\0';
    p->blocktype[0] = '\0';
}

Policy *ParserParseFile(const char *path)
{
    ParserStateReset(&P);
    P.policy = PolicyNew();

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

    return P.policy;
}
