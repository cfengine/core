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

char cf_linebuf[8192];
int  cf_tokenpos;
int  cf_lineno;
int  cf_block_open=0;

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

#define HvBDebug printf


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

block_open  [{]
block_close [}]

nakedvar   [$@][(][a-zA-Z0-9_\[\]\200-\377.:]+[)]|[$@][{][a-zA-Z0-9_\[\]\200-\377.:]+[}]

id         [a-zA-Z0-9_\200-\377]+

blockid    [a-zA-Z0-9_\200-\377]+[:][a-zA-Z0-9_\200-\377]+

assign     =>

arrow      ->

string     string


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

/*
 * declare a token for each catogory
*/
vars_category       vars:
classes_category    classes:
interfaces_category interfaces:
processes_category  processes:
storage_category    storage:
packages_storage    packages:
commands_category   commands:
methods_category    methods:
files_category      files:
databases_category   databases:
services_category   services:
reports_category    reports:
field_edits_category field_edits:
insert_lines_category insert_lines:
replace_patterns_category  replace_patterns:
delete_lines_category  delete_lines:

%%

{newline}.*                   {
                          /*
                          * Read Entire line and push
                          * it back to analyse it
                          */
                          cf_tokenpos = 0;
                          cf_lineno++;
                          strcpy(cf_linebuf, yytext+1);
                          HvBDebug("L:- %s\n", cf_linebuf);

                          P.line_no++;
                          P.line_pos = 0;

                          yyless(1);
                      }

{promises}            {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:bundle %d\n", cf_tokenpos);

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }
                          P.line_pos += strlen(yytext);

                          return BUNDLE;
                      }

common                {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:common %d\n", cf_tokenpos);
                          return COMMON;
                      }

agent                {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:agent %d\n", cf_tokenpos);
                          return AGENT;
                     }

edit_line            {
                          cf_tokenpos += yyleng;
                          HvBDebug("L\t:edit_line %d\n", cf_tokenpos);
                          return EDITLINE;
                     }


and                  {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:and %d\n", cf_tokenpos);
                          return AND;
                     }

dist           {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:dist %d\n", cf_tokenpos);
                          return DIST;
                     }

expression           {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:expression %d\n", cf_tokenpos);
                          return EXPRESSION;
                     }

or                   {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:or %d\n", cf_tokenpos);
                          return OR;
                     }

xor                  {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:xor %d\n", cf_tokenpos);
                          return XOR;
                     }

not                  {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:not %d\n", cf_tokenpos);
                          return NOT;
                     }


string               {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:string %d\n", cf_tokenpos);
                          return STRING;
                     }

slist               {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:slist %d\n", cf_tokenpos);
                          return SLIST;
                     }

usebundle               {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:usebundle %d\n", cf_tokenpos);
                          return USEBUNDLE;
                     }


{block_open}         {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:{ %d\n", cf_tokenpos);
                          cf_block_open++;
                          return BLOCK_OPEN;
                     }

{block_close}        {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:} %d\n", cf_tokenpos);
                          --cf_block_open;
                          return BLOCK_CLOSE;
                     }


{body}                {
                          cf_tokenpos += yyleng;

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }
                          P.line_pos += strlen(yytext);
                          HvBDebug("\tL:body %d\n", cf_tokenpos);
                          return BODY;
                      }

{id}                  {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:idsyntax: %s\n", yytext);
                          size_t token_length = strlen(yytext);
                          P.offsets.last_id = P.offsets.current - token_length;
                          P.line_pos += token_length;
                          if (strlen(yytext) > CF_MAXVARSIZE-1)
                          {
                              yyerror("identifier too long");
                          }
                          strncpy(P.currentid, yytext, CF_MAXVARSIZE);
                          /*
                          strcpy(yylval.text, yytext);
                          */
                          return IDSYNTAX;
                      }


{blockid}             {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:block_idsyntax: %s\n", yytext);
                          size_t token_length = strlen(yytext);
                          P.offsets.last_id = P.offsets.current - token_length;
                          P.line_pos += token_length;
                          if (strlen(yytext) > CF_MAXVARSIZE-1)
                          {
                              yyerror("qualified identifier too long");
                          }
                          strncpy(P.currentid, yytext, CF_MAXVARSIZE);
                          /*
                          strcpy(yylval.text, yytext);
                          */
                          return BLOCK_IDSYNTAX;
                      }


{assign}              {
                          cf_tokenpos += yyleng;
                          P.line_pos += strlen(yytext);
                          HvBDebug("\tL:ASSIGN %d\n", cf_tokenpos);
                          return ASSIGN;
                      }

{arrow}               {
                          cf_tokenpos += yyleng;
                          P.line_pos += strlen(yytext);
                          HvBDebug("\tL:ARROW %d\n", cf_tokenpos);
                          return ARROW;
                      }

{class}               {
                          cf_tokenpos += yyleng;
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

{vars_category}       { 
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          strncpy(P.currenttype, "vars", CF_MAXVARSIZE);
                          return VARS_CATEGORY;
                      }
{classes_category}    {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return CLASSES_CATEGORY;
                      }
{interfaces_category} {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return INTERFACES_CATEGORY;
                      }
{processes_category}  {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return PROCESSES_CATEGORY;
                      }
{storage_category}    { 
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return STORAGE_CATEGORY;
                      }
{packages_storage}    {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return PACKAGES_CATEGORY;
                      }
{commands_category}   {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          strncpy(P.currenttype, "commands", CF_MAXVARSIZE);
                          return COMMANDS_CATEGORY;
                      }
{methods_category}    {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return METHODS_CATEGORY;
                      }
{files_category}      {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return FILES_CATEGORY;
                      }
{databases_category}  {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return DATABASES_CATEGORY;
                      }
{services_category}   {
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return SERVICES_CATEGORY;
                      }
{reports_category}    { 
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return REPORTS_CATEGORY;
                      }

{field_edits_category}    { 
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return EDIT_FIELD_EDITS_CATEGORY;
                 }

{insert_lines_category}   { 
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return EDIT_INSERT_LINES_CATEGORY;
                 }

{replace_patterns_category}   { 
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return EDIT_REPLACE_PATTERNS_CATEGORY;
                     }

{delete_lines_category}   { 
                          cf_tokenpos += yyleng;
                          HvBDebug("\tL:category:%s\n", yytext);
                          return EDIT_DELETE_LINES_CATEGORY;
                 }


{qstring}             {
                          cf_tokenpos += yyleng;
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

                          if (THIS_AGENT_TYPE == cf_common)
                          {
                              IsCf3VarString(tmp);
                          }

                          free(tmp);
                          HvBDebug("\tL:QuoteString:%s\n", P.currentstring);
                          return QSTRING;
                      }


{nakedvar}            {
                          cf_tokenpos += yyleng;
                          P.line_pos += strlen(yytext);
                          P.currentstring = xstrdup(yytext);
                          return NAKEDVAR;
                      }


{space}+              {
                          cf_tokenpos += yyleng;
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
