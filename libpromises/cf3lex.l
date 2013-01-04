%{
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

/*******************************************************************/
/*                                                                 */
/*  LEXER for cfengine 3                                           */
/*                                                                 */
/*******************************************************************/

#include "cf3.defs.h"
#include "cf3parse.h"

#include "env_context.h"
#include "parser_state.h"
#include "files_names.h"
#include "unix.h"

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

int yywrap(void)
{
return 1;
}

static void yyuseraction()
{
P.offsets.current += yyleng;
}

#define YY_USER_ACTION yyuseraction();

// Do not use lex - flex only

%}

space      [ \t]+

newline    ([\n]|[\xd][\xa])

comment    #[^\n]*

promises   bundle

body       body

nakedvar   [$@][(][a-zA-Z0-9_\[\]\200-\377.:]+[)]|[$@][{][a-zA-Z0-9_\[\]\200-\377.:]+[}]

id         [a-zA-Z0-9_\200-\377]+

blockid    [a-zA-Z0-9_\200-\377]+[:][a-zA-Z0-9_\200-\377]+

assign     =>

arrow      ->

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

qstring    \"((\\(.|\n))|[^"\\])*\"|\'((\\(.|\n))|[^'\\])*\'|`[^`]*`

class      [.|&!()a-zA-Z0-9_\200-\377:]+::

category   [a-zA-Z_]+:

%%

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
                          P.line_pos += strlen(yytext);
                          return BUNDLE;
                      }

{body}                {
                          /* Note this has to come before "id" since it is a subset of id */

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }
                          P.line_pos += strlen(yytext);
                          return BODY;
                      }

{id}                  {
                          size_t token_length = strlen(yytext);
                          P.offsets.last_id = P.offsets.current - token_length;
                          P.line_pos += token_length;
                          if (strlen(yytext) > CF_MAXVARSIZE-1)
                          {
                              yyerror("identifier too long");
                          }
                          strncpy(P.currentid, yytext, CF_MAXVARSIZE);
                          return IDSYNTAX;
                      }


{blockid}             {
                          size_t token_length = strlen(yytext);
                          P.offsets.last_id = P.offsets.current - token_length;
                          P.line_pos += token_length;
                          if (strlen(yytext) > CF_MAXVARSIZE-1)
                          {
                              yyerror("qualified identifier too long");
                          }
                          strncpy(P.currentid, yytext, CF_MAXVARSIZE);
                          return IDSYNTAX;
                      }


{assign}              {
                          P.line_pos += strlen(yytext);
                          return ASSIGN;
                      }

{arrow}               {
                          P.line_pos += strlen(yytext);
                          return ARROW;
                      }

{class}               {
                          P.line_pos += strlen(yytext);

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                          }

                          yytext[strlen(yytext)-2] = '\0';
                          ValidateClassSyntax(yytext);
                          P.currentclasses = xstrdup(yytext);
                          return CLASS;
                      }

{category}            {
                          size_t token_size = strlen(yytext);
                          P.line_pos += token_size;
                          P.offsets.last_subtype_id = P.offsets.current - token_size;
                          yytext[token_size - 1] = '\0';
                          strncpy(P.currenttype, yytext, CF_MAXVARSIZE);

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }

                          return CATEGORY;
                      }

{qstring}             {
                          char *tmp = NULL;
                          int less = 0;
                          size_t token_length = strlen(yytext);

                          P.offsets.last_string = P.offsets.current - token_length;
                          P.line_pos += token_length;

                          for (char *c = yytext; *c; ++c)
                          {
                              if (*c == '\n')
                              {
                                  P.line_no++;
                              }
                          }

                          tmp = xmalloc(token_length + 1);

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

                          if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
                          {
                              IsCf3VarString(tmp);
                          }

                          free(tmp);
                          return QSTRING;
                      }


{nakedvar}            {
                          P.line_pos += strlen(yytext);
                          P.currentstring = xstrdup(yytext);
                          return NAKEDVAR;
                      }


{space}+              {
                          P.line_pos += strlen(yytext);
                      }

{comment}             {
                      }


.                     {
                          P.line_pos++;
                          return yytext[0];
                      }


%%


/* EOF */
