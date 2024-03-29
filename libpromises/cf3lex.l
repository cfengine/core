%top{
/*
   Copyright 2021 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
#include <regex.h>
#include <feature.h>
#include <version_comparison.h>
}

%{
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

//#define ParserDebug if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG) printf
#define ParserDebug(...) ((void) 0)
#define P PARSER_STATE

static Regex *context_expression_whitespace_rx = NULL;

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

// Do not use lex - flex only

%}

%x if_ignore_state

space      [ \t]+

newline    ([\n]|[\xd][\xa])

eat_line  ([\n]|[\xd][\xa]).*

comment    #[^\n]*

macro_if_minimum_version  @if\ minimum_version\([0-9]{1,5}(\.[0-9]{1,5}){0,2}\)
macro_if_maximum_version  @if\ maximum_version\([0-9]{1,5}(\.[0-9]{1,5}){0,2}\)
macro_if_before_version   @if\ before_version\([0-9]{1,5}(\.[0-9]{1,5}){0,2}\)
macro_if_at_version       @if\ at_version\([0-9]{1,5}(\.[0-9]{1,5}){0,2}\)
macro_if_after_version    @if\ after_version\([0-9]{1,5}(\.[0-9]{1,5}){0,2}\)
macro_if_between_versions @if\ between_versions\([0-9]{1,5}(\.[0-9]{1,5}){0,2}\ *,\ *[0-9]{1,5}(\.[0-9]{1,5}){0,2}\)
macro_if_feature          @if\ feature\([a-zA-Z0-9_]+\)
macro_else  @else
macro_endif @endif
macro_line [^@\n\xd\xa].*

bundle     bundle

body       body

promise    promise

nakedvar   [$@][(][a-zA-Z0-9_\[\]\200-\377.:]+[)]|[$@][{][a-zA-Z0-9_\[\]\200-\377.:]+[}]|[$@][(][a-zA-Z0-9_\200-\377.:]+[\[][a-zA-Z0-9_$(){}\200-\377.:]+[\]]+[)]|[$@][{][a-zA-Z0-9_\200-\377.:]+[\[][a-zA-Z0-9_$(){}\200-\377.:]+[\]]+[}]

identifier [a-zA-Z0-9_\200-\377]+

symbol     [a-zA-Z0-9_\200-\377]+[:][a-zA-Z0-9_\200-\377]+

fat_arrow  =>

thin_arrow ->

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

class          [.|&!()a-zA-Z0-9_\200-\377:][\t .|&!()a-zA-Z0-9_\200-\377:]*::
varclass       (\"[^"\0]*\"|\'[^'\0]*\')::

promise_guard  [a-zA-Z_]+:

%%
{eat_line}             {
                          free(P.current_line);
                          P.current_line = xstrdup(yytext+1);
                          ParserDebug("L:line %s\n", P.current_line);
                          P.line_no++;
                          P.line_pos = 1;
                          // push back on stack and skip first char
                          yyless(1);
                      }

{macro_if_minimum_version} {
                        if (  P.line_pos != 1 )
                        {
                             yyerror("fatal: macro @if must be at beginning of the line.");
                             return 0;
                        }
                        if (P.if_depth > 0)
                        {
                          yyerror("fatal: nested @if macros are not allowed");
                          return 0;
                        }

                        P.if_depth++;

                        const char* minimum = yytext+20;
                        ParserDebug("\tL:macro @if %d:version=%s\n", P.line_pos, minimum);

                        VersionComparison result = CompareVersion(Version(), minimum);
                        if (result == VERSION_GREATER || result == VERSION_EQUAL)
                        {
                            ParserDebug("\tL:macro @if %d:accepted to next @endif\n", P.line_pos);
                        }
                        else if (result == VERSION_SMALLER)
                        {
                            ParserDebug("\tL:macro @if %d:ignoring to next @endif or EOF\n", P.line_pos);
                            BEGIN(if_ignore_state);
                        }
                        else
                        {
                            assert(result == VERSION_ERROR);
                            yyerror("fatal: macro @if requested an unparseable version");
                        }
                      }

{macro_if_maximum_version} {
                        if (  P.line_pos != 1 )
                        {
                             yyerror("fatal: macro @if must be at beginning of the line.");
                             return 0;
                        }
                        if (P.if_depth > 0)
                        {
                          yyerror("fatal: nested @if macros are not allowed");
                          return 0;
                        }

                        P.if_depth++;

                        const char* maximum = yytext+20;
                        ParserDebug("\tL:macro @if %d:version=%s\n", P.line_pos, maximum);

                        VersionComparison result = CompareVersion(Version(), maximum);
                        if (result == VERSION_SMALLER || result == VERSION_EQUAL)
                        {
                            ParserDebug("\tL:macro @if %d:accepted to next @endif\n", P.line_pos);
                        }
                        else if (result == VERSION_GREATER)
                        {
                            ParserDebug("\tL:macro @if %d:ignoring to next @endif or EOF\n", P.line_pos);
                            BEGIN(if_ignore_state);
                        }
                        else
                        {
                            assert(result == VERSION_ERROR);
                            yyerror("fatal: macro @if requested an unparseable version");
                        }
                      }

{macro_if_before_version} {
                        if (  P.line_pos != 1 )
                        {
                             yyerror("fatal: macro @if must be at beginning of the line.");
                             return 0;
                        }
                        if (P.if_depth > 0)
                        {
                          yyerror("fatal: nested @if macros are not allowed");
                          return 0;
                        }

                        P.if_depth++;

                        const char* target = yytext + strlen("@if before_version(");
                        ParserDebug("\tL:macro @if %d:version=%s\n", P.line_pos, target);

                        VersionComparison result = CompareVersion(Version(), target);
                        if (result == VERSION_SMALLER)
                        {
                            ParserDebug("\tL:macro @if %d:accepted to next @endif\n", P.line_pos);
                        }
                        else if (result == VERSION_GREATER || result == VERSION_EQUAL)
                        {
                            ParserDebug("\tL:macro @if %d:ignoring to next @endif or EOF\n", P.line_pos);
                            BEGIN(if_ignore_state);
                        }
                        else
                        {
                            assert(result == VERSION_ERROR);
                            yyerror("fatal: macro @if requested an unparseable version");
                        }
                      }

{macro_if_at_version} {
                        if (  P.line_pos != 1 )
                        {
                             yyerror("fatal: macro @if must be at beginning of the line.");
                             return 0;
                        }
                        if (P.if_depth > 0)
                        {
                          yyerror("fatal: nested @if macros are not allowed");
                          return 0;
                        }

                        P.if_depth++;

                        const char* target = yytext + strlen("@if at_version(");
                        ParserDebug("\tL:macro @if %d:version=%s\n", P.line_pos, target);

                        VersionComparison result = CompareVersion(Version(), target);
                        if (result == VERSION_EQUAL)
                        {
                            ParserDebug("\tL:macro @if %d:accepted to next @endif\n", P.line_pos);
                        }
                        else if (result == VERSION_GREATER || result == VERSION_SMALLER)
                        {
                            ParserDebug("\tL:macro @if %d:ignoring to next @endif or EOF\n", P.line_pos);
                            BEGIN(if_ignore_state);
                        }
                        else
                        {
                            assert(result == VERSION_ERROR);
                            yyerror("fatal: macro @if requested an unparseable version");
                        }
                      }

{macro_if_after_version} {
                        if (  P.line_pos != 1 )
                        {
                             yyerror("fatal: macro @if must be at beginning of the line.");
                             return 0;
                        }
                        if (P.if_depth > 0)
                        {
                          yyerror("fatal: nested @if macros are not allowed");
                          return 0;
                        }

                        P.if_depth++;

                        const char* target = yytext + strlen("@if after_version(");
                        ParserDebug("\tL:macro @if %d:version=%s\n", P.line_pos, target);

                        VersionComparison result = CompareVersion(Version(), target);
                        if (result == VERSION_GREATER)
                        {
                            ParserDebug("\tL:macro @if %d:accepted to next @endif\n", P.line_pos);
                        }
                        else if (result == VERSION_EQUAL || result == VERSION_SMALLER)
                        {
                            ParserDebug("\tL:macro @if %d:ignoring to next @endif or EOF\n", P.line_pos);
                            BEGIN(if_ignore_state);
                        }
                        else
                        {
                            assert(result == VERSION_ERROR);
                            yyerror("fatal: macro @if requested an unparseable version");
                        }
                      }

{macro_if_between_versions} {
                        if ( P.line_pos != 1 )
                        {
                             yyerror("fatal: macro @if must be at beginning of the line.");
                             return 0;
                        }
                        if (P.if_depth > 0)
                        {
                          yyerror("fatal: nested @if macros are not allowed");
                          return 0;
                        }

                        P.if_depth++;

                        const char* from = yytext + strlen("@if between_versions(");

                        const char *to = strchr(from, ',');
                        to += 1;
                        while (*to == ' ')
                        {
                            to += 1;
                        }

                        ParserDebug("\tL:macro @if %d:between_versions(%s\n", P.line_pos, from);

                        VersionComparison a = CompareVersion(Version(), from);
                        VersionComparison b = CompareVersion(Version(), to);
                        if ((a == VERSION_EQUAL || a == VERSION_GREATER)
                            && (b == VERSION_EQUAL || b == VERSION_SMALLER))
                        {
                            ParserDebug("\tL:macro @if %d:accepted to next @endif\n", P.line_pos);
                        }
                        else
                        {
                            ParserDebug("\tL:macro @if %d:ignoring to next @endif or EOF\n", P.line_pos);
                            BEGIN(if_ignore_state);
                        }
                      }

{macro_if_feature}    {
                        if (  P.line_pos != 1 )
                        {
                          yyerror("fatal: macro @if must be at beginning of the line.");
                          return 0;
                        }

                        char* feature_text = yytext+12;
                        // remove trailing ')'
                        feature_text[strlen(feature_text)-1] = 0;
                        ParserDebug("\tL:macro @if %d:feature=%s\n", P.line_pos, feature_text);
                        {
                          if (P.if_depth > 0)
                          {
                            yyerror("fatal: nested @if macros are not allowed");
                            return 0;
                          }

                          P.if_depth++;

                          if (KnownFeature(feature_text))
                          {
                            ParserDebug("\tL:macro @if %d:accepted to next @endif\n", P.line_pos);
                          }
                          else
                          {
                            /* ignore to the next @endif */
                            ParserDebug("\tL:macro @if %d:ignoring to next @endif or EOF\n", P.line_pos);
                            BEGIN(if_ignore_state);
                          }
                        }
                      }

{macro_endif}         {
                        if (  P.line_pos != 1 )
                        {
                          yyerror("fatal: macro @endif must be at beginning of the line.");
                          return 0;
                        }

                        ParserDebug("\tL:macro @endif %d\n", P.line_pos);
                        BEGIN(INITIAL);
                        if (P.if_depth <= 0)
                        {
                          yyerror("fatal: @endif macros without a matching @if are not allowed");
                          return 0;
                        }
                        P.if_depth--;
                      }


<if_ignore_state>{macro_line}  {
                                  ParserDebug("\tL:inside macro @if, ignoring line text:\"%s\"\n", yytext);
                               }

<if_ignore_state>{macro_else} {
                                ParserDebug("\tL:macro @else, will no longer ignore lines\n", P.line_pos);
                                BEGIN(INITIAL);
                              }
{macro_else}                  {
                                if (P.if_depth <= 0)
                                {
                                  yyerror("fatal: @else macro without a matching @if are not allowed");
                                  return 0;
                                }
                                ParserDebug("\tL:macro @else, will now ignore lines\n", P.line_pos);
                                BEGIN(if_ignore_state);
                              }

<if_ignore_state>{macro_endif} {
                                 ParserDebug("\tL:macro @endif %d\n", P.line_pos);
                                 BEGIN(INITIAL);
                                 if (P.if_depth <= 0)
                                 {
                                   yyerror("fatal: @endif macros without a matching @if are not allowed");
                                   return 0;
                                 }
                                 P.if_depth--;
                               }

<if_ignore_state>{newline} {
                           }

<if_ignore_state>.    {
                          /* eat up al unknown chars when line starts with @*/
                          ParserDebug("\tL:inside macro @if, ignoring char text:\"%s\"\n", yytext);
                      }

{bundle}              {
                          /* Note this has to come before "id" since it is a subset of id */

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }

                          if (P.currentvarclasses != NULL)
                          {
                              free(P.currentvarclasses);
                              P.currentvarclasses = NULL;
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

                          if (P.currentvarclasses != NULL)
                          {
                              free(P.currentvarclasses);
                              P.currentvarclasses = NULL;
                          }

                          P.line_pos += yyleng;
                          ParserDebug("\tL:body %d\n", P.line_pos);
                          return BODY;
                      }

{promise}             {
                          /* Note this has to come before "id" since it is a subset of id */

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }

                          if (P.currentvarclasses != NULL)
                          {
                              free(P.currentvarclasses);
                              P.currentvarclasses = NULL;
                          }

                          P.line_pos += yyleng;
                          ParserDebug("\tL:promise %d\n", P.line_pos);
                          return PROMISE;
                      }

{identifier}          {
                          P.offsets.last_id = P.offsets.current - yyleng;
                          P.line_pos += yyleng;
                          ParserDebug("\tL:id %s %d\n", yytext, P.line_pos);
                          if (yyleng  > CF_MAXVARSIZE-1)
                          {
                              yyerror("identifier too long");
                          }
                          strncpy(P.currentid, yytext, CF_MAXVARSIZE - 1);
                          return IDENTIFIER;
                      }


{symbol}              {
                          P.offsets.last_id = P.offsets.current - yyleng;
                          P.line_pos += yyleng;
                          ParserDebug("\tL:symbol %s %d\n", yytext, P.line_pos);
                          if (yyleng > CF_MAXVARSIZE-1)
                          {
                              yyerror("qualified identifier too long");
                          }
                          strncpy(P.currentid, yytext, CF_MAXVARSIZE - 1);
                          return IDENTIFIER;
                      }


{fat_arrow}           {
                          P.line_pos += yyleng;
                          ParserDebug("\tL:assign %d\n", P.line_pos);
                          return FAT_ARROW;
                      }

{thin_arrow}          {
                          P.line_pos += yyleng;
                          ParserDebug("\tL:arrow %d\n", P.line_pos);
                          return THIN_ARROW;
                      }

{varclass}            {
                          char *tmp = NULL;

                          P.line_pos += yyleng;
                          ParserDebug("\tL:varclass %s %d\n", yytext, P.line_pos);

                          tmp = xstrdup(yytext+1); // remove leading quote
                          tmp[yyleng-4] = '\0'; // remove tail quote plus ::

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }

                          if (P.currentvarclasses != NULL)
                          {
                              free(P.currentvarclasses);
                              P.currentvarclasses = NULL;
                          }

                          P.currentvarclasses = xstrdup(tmp);

                          free(tmp);
                          return CLASS_GUARD;
                      }

{class}               {
                          char *tmp = NULL;

                          P.line_pos += yyleng;
                          ParserDebug("\tL:class %s %d\n", yytext, P.line_pos);
                          if (context_expression_whitespace_rx == NULL)
                          {
                              context_expression_whitespace_rx = CompileRegex(CFENGINE_REGEX_WHITESPACE_IN_CONTEXTS);
                          }

                          if (context_expression_whitespace_rx == NULL)
                          {
                              yyerror("The context expression whitespace regular expression could not be compiled, aborting.");
                          }

                          if (StringMatchFullWithPrecompiledRegex(context_expression_whitespace_rx, yytext))
                          {
                              yyerror("class names can't be separated by whitespace without an intervening operator");
                          }

                          tmp = xstrdup(yytext);
                          tmp[yyleng-2] = '\0';

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }

                          if (P.currentvarclasses != NULL)
                          {
                              free(P.currentvarclasses);
                              P.currentvarclasses = NULL;
                          }

                          P.currentclasses = xstrdup(tmp);

                          free(tmp);
                          return CLASS_GUARD;
                      }

{promise_guard}       {
                          char *tmp = NULL;

                          P.line_pos += yyleng;
                          ParserDebug("\tL:promise_guard %s %d\n", yytext, P.line_pos);
                          P.offsets.last_promise_guard_id = P.offsets.current - yyleng;

                          tmp = xstrdup(yytext);
                          assert(tmp[yyleng - 1] == ':');
                          tmp[yyleng - 1] = '\0'; // Exclude trailing colon in promise guard
                          assert(strlen(tmp) > 0);
                          assert(tmp[strlen(tmp) - 1] != ':');

                          strncpy(P.currenttype, tmp, CF_MAXVARSIZE - 1);

                          if (P.currentclasses != NULL)
                          {
                              free(P.currentclasses);
                              P.currentclasses = NULL;
                          }

                          if (P.currentvarclasses != NULL)
                          {
                              free(P.currentvarclasses);
                              P.currentvarclasses = NULL;
                          }

                          free(tmp);
                          return PROMISE_GUARD;
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
                          return QUOTED_STRING;
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

{space}+              {
                          P.line_pos += yyleng;
                      }

{comment}             {
                      }


.                     {
                          P.line_pos++;
                          return yytext[0];
                      }

<if_ignore_state><<EOF>>     {
                               if (P.if_depth > 0)
                               {
                                 yyerror("EOF seen while @if was waiting for @endif in ignore_state");
                                 return 0;
                               }
                             }

<<EOF>>     {
              if (P.if_depth > 0)
              {
                yyerror("EOF seen while @if was waiting for @endif without ignore_state");
              }
              return 0; // loops forever without this
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
