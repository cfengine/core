/* Better error parsing

Some reading material
http://www.cs.nmsu.edu/~pfeiffer/classes/370/notes/yaccerrorhandling/index.php?currentsem=s10

Some interesting variables that are carried over:
P.currentstring <- For qstrings, etc.
P.promiser      <- The last promiser set
P.block         <- Are we in a body or a bundle? "body
P.blocktype     <- Type of the block we are in "common"
P.blockid       <- Name of the block "control"

P.filename      <- Filename we are in
P.line_no       <- The line number
P.line_pos      <- The column in the line

*/



void yyerror(const char *s)
{
    char *sp = yytext;
    
    
    if (sp && *sp == '\"' && sp[1])
    {
        sp++;
    }
    
    if(P.line_pos == 0) P.line_pos = 2; // For highlighting.
    
    int NEWLINE_CHAR = 10;
    

    if (ERRORCOUNT == 0) {
        fprintf(stderr, "\n## Errors ##\n");
        fprintf(stderr, "\n In file: %s \n Line #%d Col #%d \n %s \n", P.filename, P.line_no, P.line_pos, s);
        
        // Get length of error token
        int error_length = strlen(sp ? sp : "N");
        // Get Context
        FILE * errorfile =  fopen( P.filename, "r" );
        int current_line = 0;
        int current_col  = 0;
        int current_char = 0;
        while (current_line < P.line_no+1) {
            current_char = getc( errorfile );
            current_col++;
            
            if (current_char == NEWLINE_CHAR) {current_line++; current_col = 0;}
            
            // The printing
            if (current_line == P.line_no-1 && current_col == P.line_pos-1) fprintf(stderr, "%s", "\033[;41m");
            if (current_line == P.line_no-1 && current_col == P.line_pos+error_length+1) fprintf(stderr, "%s", "\033[0;m");
            if (current_line == P.line_no) fprintf(stderr, "%s", "\033[0;m"); // Reset end of line if the error was at the end
            if (current_line >= P.line_no-1 && current_line <= P.line_no+1)
                fprintf(stderr, "%c", current_char);
        }
        fclose(errorfile);
        fprintf(stderr, "\nOnly showing first error. See more with -g.\n");
    }

    ERRORCOUNT++;

    if (ERRORCOUNT > 10)
    {
        FatalError("Stopped parsing after 10 errors");
    }
}

static void fatal_yyerror(const char *s)
{
    char *sp = yytext;
    /* Skip quotation mark */
    if (sp && *sp == '\"' && sp[1])
    {
        sp++;
    }

    FatalError("%s: %d,%d: Fatal error during parsing: %s, near token \'%.20s\'\n", P.filename, P.line_no, P.line_pos, s, sp ? sp : "NULL");
}

static void DebugBanner(const char *s)
{
    CfDebug("----------------------------------------------------------------\n");
    CfDebug("  %s                                                            \n", s);
    CfDebug("----------------------------------------------------------------\n");
}
