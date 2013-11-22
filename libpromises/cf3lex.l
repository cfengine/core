%{
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

/*******************************************************************/
/*                                                                 */
/*  LEXER for cfengine 3                                           */
/*                                                                 */
/*******************************************************************/

#include <platform.h>

#include <cf3parse.h>
#include <parser_state.h>
#include <file_lib.h>

/* yyinput/input are generated and unused */

#if defined(__GNUC__)
# define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
# if GCC_VERSION >= 40200
#  pragma GCC diagnostic ignored "-Wunused-function"
# endif
#endif

#undef malloc
#undef realloc
#define malloc xmalloc
#define realloc xrealloc

#define ParserDebug if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG) printf

static int DeEscapeQuotedString(const char *from, char *to);

int yywrap(void)
{
return 1;
}

static void yyuseraction()
{
P.offsets.current += yyleng;
}

#define YY_USER_ACTION yyuseraction();

#define MAX_INCLUDE_DEPTH 1
YY_BUFFER_STATE include_stack[MAX_INCLUDE_DEPTH];
int include_stack_ptr = 0;

%}

%x incl_state

space      [ \t]+

newline    ([\n]|[\xd][\xa])

line       ^.*$

comment    #[^\n]*

include    ^@include

promises   bundle

body       body

nakedvar   [$@][(][a-zA-Z0-9_\[\]\200-\377.:]+[)]|[$@][{][a-zA-Z0-9_\[\]\200-\377.:]+[}]

id         [a-zA-Z0-9_\200-\377]+

symbol     [a-zA-Z0-9_\200-\377]+[:][a-zA-Z0-9_\200-\377]+

assign     =>

arrow      ->

op         [(]

cp         [)]

ob         [{]

cb         [}]

/*
 * Three types of quoted strings:
 *
 * - string in double quotes, starts with double quote, runs until another
 *   double quote, \" masks the double quote.
 * - string in single quotes, starts with single quote, runs until another
 *   single quote, \' masks the single quote.
 * - string in backquotes, starts with backquote, runs until another backquote.
 *
 * The same rule formatted for the better readability:
 *
 * <qstring> := \" <dq> \" | \' <sq> \' | ` <bq> `
 * <dq> = <dqs>*
 * <dqs> = \\ <any> | [^"\\]
 * <sq> = <sqs>*
 * <sqs> = \\ <any> | [^'\\]
 * <bq> = <bqs>*
 * <bqs> = [^`]
 * <any> = . | \n
 *
 */

qstring        \"((\\(.|\n))|[^"\\])*\"|\'((\\(.|\n))|[^'\\])*\'|`[^`]*`

class          [.|&!()a-zA-Z0-9_\200-\377:]+::

promise_type   [a-zA-Z_]+:

%%

{line}                {
                          free(P.current_line);
                          P.current_line = xstrdup(yytext);
                          ParserDebug("L:line %s\n", P.current_line);
                          REJECT;
                      }

{newline}             {
                          P.line_no++;
                          P.line_pos = 0;
                      }

{promises}            {
                          /* Note this has to come before "id" since it is a subset of id */

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }
                          P.line_pos += yyleng;
                          ParserDebug("\tL:bundle %d\n", P.line_pos);
                          return BUNDLE;
                      }

{body}                {
                          /* Note this has to come before "id" since it is a subset of id */

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }
                          P.line_pos += yyleng;
                          ParserDebug("\tL:body %d\n", P.line_pos);
                          return BODY;
                      }

{include}             {
                      ParserDebug("\tL:include %d\n", P.line_pos);
                      if ( include_stack_ptr >= MAX_INCLUDE_DEPTH )
                      {
                          ParserDebug("\tI: Includes nested too deeply\n" );
                          exit( 1 );
                      }
                      BEGIN(incl_state);
                      }

<incl_state>[ \t]*         
<incl_state>{qstring}  {
                          ParserDebug("\tL:include state:qstring:%s\n", yytext);
                          include_stack[include_stack_ptr++] = YY_CURRENT_BUFFER;

                          char *fname = NULL;
                          fname = xstrdup(yytext+1); // skip up to the opening quote
                          fname[strlen(fname)-1] = '\0'; // clear the ending quote

                          // check for leading path components, attempting to catch Windows paths
                          if (fname == strchr(fname, '/') || (strlen(fname) > 3 && fname[2] == ':'))
                          {
                               ParserDebug("\tL:include_state:%s (absolute path, included in %s)\n", fname, P.filename);
                          }
                          else
                          {
                               ParserDebug("\tL:include_state:%s (relative path, included in %s)\n", fname, P.filename);
                               char current[CFE_PARSER_PATH_MAX];
                               char dirname[CFE_PARSER_PATH_MAX];

                               if (NULL == realpath(P.filename, dirname))
                               {
                                   char error_str[CF_MAXVARSIZE];

                                   snprintf(error_str, CF_MAXVARSIZE, "\tI: Could not resolve realpath of file '%s': %s\n", P.filename, strerror(errno));
                                   yyerror(error_str);
                                   exit(1);
                               }

                               char *chop = strrchr(dirname, '/');

                               if (NULL == chop)
                               {
                                    chop = strrchr(dirname, '\\');
                               }

                               if (NULL != chop)
                               {
                                    *chop = '\0';
                               }

                               // this should work on Windows too
                               snprintf(current, CFE_PARSER_PATH_MAX-1, "%s/%s", dirname, fname);
                               free(fname);
                               fname = xstrdup(current);
                          }

                          yyin = safe_fopen(fname, "r" );

                          if ( ! yyin )
                          {
                              char error_str[CF_MAXVARSIZE];

                              snprintf(error_str, CF_MAXVARSIZE, "\tI: Could not open file '%s' for reading: %s\n", fname, strerror(errno));
                              yyerror(error_str);
                              exit(1);
                          }
                          else
                          {
                              ParserDebug("\tI: @Including file '%s'\n", fname);
                          }

                          strncpy(P.include_filename, fname, CFE_PARSER_PATH_MAX);
                          free(fname);

                          yy_switch_to_buffer( yy_create_buffer( yyin, YY_BUF_SIZE ) );

                          BEGIN(INITIAL);
                      }

<incl_state>.[^'"]*   {
                          yyerror("Not a valid include statement");
                          exit(1);
                      }


<<EOF>>               {
                          P.include_filename[0] = '\0';

                          if ( --include_stack_ptr < 0 || !YY_CURRENT_BUFFER)
                          {
                              yyterminate();
                          }
                          else
                          {
                              yy_delete_buffer( YY_CURRENT_BUFFER );
                              yy_switch_to_buffer(include_stack[include_stack_ptr] );
                          }
                      }

{id}                  {
                          P.offsets.last_id = P.offsets.current - yyleng;
                          P.line_pos += yyleng;
                          ParserDebug("\tL:id %s %d\n", yytext, P.line_pos);
                          if (yyleng  > CF_MAXVARSIZE-1)
                          {
                              yyerror("identifier too long");
                          }
                          strncpy(P.currentid, yytext, CF_MAXVARSIZE);
                          return IDSYNTAX;
                      }


{symbol}              {
                          P.offsets.last_id = P.offsets.current - yyleng;
                          P.line_pos += yyleng;
                          ParserDebug("\tL:symbol %s %d\n", yytext, P.line_pos);
                          if (yyleng > CF_MAXVARSIZE-1)
                          {
                              yyerror("qualified identifier too long");
                          }
                          strncpy(P.currentid, yytext, CF_MAXVARSIZE);
                          return IDSYNTAX;
                      }


{assign}              {
                          P.line_pos += yyleng;
                          ParserDebug("\tL:assign %d\n", P.line_pos);
                          return ASSIGN;
                      }

{arrow}               {
                          P.line_pos += yyleng;
                          ParserDebug("\tL:arrow %d\n", P.line_pos);
                          return ARROW;
                      }

{class}               {
                          char *tmp = NULL;
                        
                          P.line_pos += yyleng;
                          ParserDebug("\tL:class %s %d\n", yytext, P.line_pos);

                          tmp = xstrdup(yytext);
                          tmp[yyleng-2] = '\0';

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                          }

                          P.currentclasses = xstrdup(tmp);

                          free(tmp);
                          return CLASS;
                      }

{promise_type}        {
                          char *tmp = NULL;

                          P.line_pos += yyleng;
                          ParserDebug("\tL:promise_type %s %d\n", yytext, P.line_pos);
                          P.offsets.last_promise_type_id = P.offsets.current - yyleng;

                          tmp = xstrdup(yytext);
                          tmp[yyleng - 1] = '\0';
                          strncpy(P.currenttype, tmp, CF_MAXVARSIZE);

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }

                          free(tmp);
                          return PROMISE_TYPE;
                      }

{qstring}             {
                          char *tmp = NULL;
                          int less = 0;

                          P.offsets.last_string = P.offsets.current - yyleng;
                          P.line_pos += yyleng;
                          ParserDebug("\tL:qstring %s %d\n", yytext, P.line_pos);

                          for (char *c = yytext; *c; ++c)
                          {
                              if (*c == '\n')
                              {
                                  P.line_no++;
                              }
                          }

                          tmp = xmalloc(yyleng + 1);

                          if ((less = DeEscapeQuotedString(yytext,tmp)) > 0)
                          {
                              yyless(less);
                              P.offsets.current -= less;
                          }

                          if (P.currentstring)
                          {
                              free(P.currentstring);
                          }

                          P.currentstring = xstrdup(tmp);

                          free(tmp);
                          return QSTRING;
                      }


{nakedvar}            {
                          P.line_pos += yyleng;
                          ParserDebug("\tL: %s %d\n", yytext, P.line_pos);
                          if (P.currentstring)
                          {
                              free(P.currentstring);
                          }
                          P.currentstring = xstrdup(yytext);
                          return NAKEDVAR;
                      }

{op}                  {
                          P.line_pos += yyleng;
                          ParserDebug("\tL: %s %d\n", yytext, P.line_pos);
                          return OP;
                      }

{cp}                  {
                          P.line_pos += yyleng;
                          ParserDebug("\tL: %s %d\n", yytext, P.line_pos);
                          return CP;
                      }

{ob}                  {
                          P.line_pos += yyleng;
                          ParserDebug("\tL: %s %d\n", yytext, P.line_pos);
                          return OB;
                      }

{cb}                  {
                          P.line_pos += yyleng;
                          ParserDebug("\tL: %s %d\n", yytext, P.line_pos);
                          return CB;
                      }


{space}+              {
                          P.line_pos += yyleng;
                      }

{comment}             {
                      }


.                     {
                          P.line_pos++;
                          return yytext[0];
                      }


%%

static int DeEscapeQuotedString(const char *from, char *to)
{
    char *cp;
    const char *sp;
    char start = *from;
    int len = strlen(from);

    if (len == 0)
    {
        return 0;
    }

    for (sp = from + 1, cp = to; (sp - from) < len; sp++, cp++)
    {
        if ((*sp == start))
        {
            *(cp) = '\0';

            if (*(sp + 1) != '\0')
            {
                return (2 + (sp - from));
            }

            return 0;
        }

      if (*sp == '\\')
      {
            switch (*(sp + 1))
            {
            case '\n':
                sp += 2;
                break;

            case ' ':
                break;

            case '\\':
            case '\"':
            case '\'':
                sp++;
                break;
            }
        }

        *cp = *sp;
    }

    yyerror("Runaway string");
    *(cp) = '\0';
    return 0;
}


/* EOF */
