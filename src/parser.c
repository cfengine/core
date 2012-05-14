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

struct ParserState P = { 0 };

extern FILE *yyin;

static void ParserStateReset()
{
    P.policy = NULL;

    P.line_no = 1;
    P.line_pos = 1;
    P.list_nesting = 0;
    P.arg_nesting = 0;

    P.currentid[0] = '\0';
    P.currentstring = NULL;
    P.currenttype[0] = '\0';
    P.currentclasses = NULL;
    P.currentRlist = NULL;
    P.currentpromise = NULL;
    P.promiser = NULL;
    P.blockid[0] = '\0';
    P.blocktype[0] = '\0';
}

Policy *ParserParseFile(Policy *policy, const char *path)
{
    ParserStateReset();
    P.policy = policy;

    strncpy(P.filename, path, CF_MAXVARSIZE);

    yyin = fopen(path, "r");

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
