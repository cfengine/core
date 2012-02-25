#include "parser.h"
#include "parser_state.h"

struct ParserState P = { 0 };

extern FILE *yyin;

static void ParserStateReset()
{
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

void ParserParseFile(const char *path)
{
    ParserStateReset();

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
}
