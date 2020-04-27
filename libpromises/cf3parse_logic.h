/*
  Copyright 2020 Northern.tech AS

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

/*
  This file is an attempt to clean up cf3parse.y, moving as much
  C code (logic) as possible out of it, so the actual grammar
  is more readable. It should only be included from cf3parse.y (!).

  The advantages of moving the C code out of the grammar are:
  * Separate overall grammar from noisy details
  * Less crazy indentation
  * Better editor support for auto complete / syntax highlighting
*/

#ifndef CF3_PARSE_LOGIC_H
#define CF3_PARSE_LOGIC_H

#include "cf3.defs.h"
#include "parser.h"
#include "parser_helpers.h"
#include "parser_state.h"

#include "logging.h"
#include "fncall.h"
#include "rlist.h"
#include "item_lib.h"
#include "policy.h"
#include "mod_files.h"
#include "string_lib.h"
#include "logic_expressions.h"
#include "json-yaml.h"
#include "cleanup.h"

// FIX: remove
#include "syntax.h"

#include <assert.h>

int yylex(void);
extern char *yytext;

static int RelevantBundle(const char *agent, const char *blocktype);
static bool LvalWantsBody(char *stype, char *lval);
static SyntaxTypeMatch CheckSelection(
    const char *type, const char *name, const char *lval, Rval rval);
static SyntaxTypeMatch CheckConstraint(
    const char *type,
    const char *lval,
    Rval rval,
    const PromiseTypeSyntax *ss);
static void fatal_yyerror(const char *s);

static void ParseErrorColumnOffset(int column_offset, const char *s, ...)
    FUNC_ATTR_PRINTF(2, 3);
static void ParseError(const char *s, ...) FUNC_ATTR_PRINTF(1, 2);
static void ParseWarning(unsigned int warning, const char *s, ...)
    FUNC_ATTR_PRINTF(2, 3);

static void ValidateClassLiteral(const char *class_literal);

static bool INSTALL_SKIP = false;
static size_t CURRENT_BLOCKID_LINE = 0;
static size_t CURRENT_PROMISER_LINE = 0;

#define YYMALLOC xmalloc
#define P PARSER_STATE

#define ParserDebug(...) LogDebug(LOG_MOD_PARSER, __VA_ARGS__)


/*****************************************************************/

static void ParseErrorVColumnOffset(
    int column_offset, const char *s, va_list ap)
{
    char *errmsg = StringVFormat(s, ap);
    fprintf(
        stderr,
        "%s:%d:%d: error: %s\n",
        P.filename,
        P.line_no,
        P.line_pos + column_offset,
        errmsg);
    free(errmsg);

    P.error_count++;

    /* Current line is not set when syntax error in first line */
    if (P.current_line)
    {
        fprintf(stderr, "%s\n", P.current_line);
        fprintf(stderr, "%*s\n", P.line_pos + column_offset, "^");
    }

    if (P.error_count > 12)
    {
        fprintf(stderr, "Too many errors\n");
        DoCleanupAndExit(EXIT_FAILURE);
    }
}

static void ParseErrorColumnOffset(int column_offset, const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    ParseErrorVColumnOffset(column_offset, s, ap);
    va_end(ap);
}

static void ParseErrorV(const char *s, va_list ap)
{
    ParseErrorVColumnOffset(0, s, ap);
}

static void ParseError(const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    ParseErrorV(s, ap);
    va_end(ap);
}

static void ParseWarningV(unsigned int warning, const char *s, va_list ap)
{
    if (((P.warnings | P.warnings_error) & warning) == 0)
    {
        return;
    }

    char *errmsg = StringVFormat(s, ap);
    const char *warning_str = ParserWarningToString(warning);

    fprintf(
        stderr,
        "%s:%d:%d: warning: %s [-W%s]\n",
        P.filename,
        P.line_no,
        P.line_pos,
        errmsg,
        warning_str);
    fprintf(stderr, "%s\n", P.current_line);
    fprintf(stderr, "%*s\n", P.line_pos, "^");

    free(errmsg);

    P.warning_count++;

    if ((P.warnings_error & warning) != 0)
    {
        P.error_count++;
    }

    if (P.error_count > 12)
    {
        fprintf(stderr, "Too many errors\n");
        DoCleanupAndExit(EXIT_FAILURE);
    }
}

static void ParseWarning(unsigned int warning, const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    ParseWarningV(warning, s, ap);
    va_end(ap);
}

void yyerror(const char *str)
{
    ParseError("%s", str);
}

static void fatal_yyerror(const char *s)
{
    char *sp = yytext;
    /* Skip quotation mark */
    if (sp && *sp == '\"' && sp[1])
    {
        sp++;
    }

    fprintf(
        stderr,
        "%s: %d,%d: Fatal error during parsing: %s, near token \'%.20s\'\n",
        P.filename,
        P.line_no,
        P.line_pos,
        s,
        sp ? sp : "NULL");
    DoCleanupAndExit(EXIT_FAILURE);
}

static int RelevantBundle(const char *agent, const char *blocktype)
{
    if ((strcmp(agent, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0)
        || (strcmp(CF_COMMONC, blocktype) == 0))
    {
        return true;
    }

    /* Here are some additional bundle types handled by cfAgent */

    Item *ip = SplitString("edit_line,edit_xml", ',');

    if (strcmp(agent, CF_AGENTTYPES[AGENT_TYPE_AGENT]) == 0)
    {
        if (IsItemIn(ip, blocktype))
        {
            DeleteItemList(ip);
            return true;
        }
    }

    DeleteItemList(ip);
    return false;
}

static bool LvalWantsBody(char *stype, char *lval)
{
    for (int i = 0; i < CF3_MODULES; i++)
    {
        const PromiseTypeSyntax *promise_type_syntax = CF_ALL_PROMISE_TYPES[i];
        if (!promise_type_syntax)
        {
            continue;
        }

        for (int j = 0; promise_type_syntax[j].promise_type != NULL; j++)
        {
            const ConstraintSyntax *bs = promise_type_syntax[j].constraints;
            if (!bs)
            {
                continue;
            }

            if (strcmp(promise_type_syntax[j].promise_type, stype) != 0)
            {
                continue;
            }

            for (int l = 0; bs[l].lval != NULL; l++)
            {
                if (strcmp(bs[l].lval, lval) == 0)
                {
                    if (bs[l].dtype == CF_DATA_TYPE_BODY)
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }
            }
        }
    }

    return false;
}

static SyntaxTypeMatch CheckSelection(
    const char *type, const char *name, const char *lval, Rval rval)
{
    // Check internal control bodies etc
    if (strcmp("control", name) == 0)
    {
        for (int i = 0; CONTROL_BODIES[i].body_type != NULL; i++)
        {
            if (strcmp(type, CONTROL_BODIES[i].body_type) == 0)
            {
                const ConstraintSyntax *bs = CONTROL_BODIES[i].constraints;

                for (int l = 0; bs[l].lval != NULL; l++)
                {
                    if (strcmp(lval, bs[l].lval) == 0)
                    {
                        if (bs[l].dtype == CF_DATA_TYPE_BODY)
                        {
                            return SYNTAX_TYPE_MATCH_OK;
                        }
                        else if (bs[l].dtype == CF_DATA_TYPE_BUNDLE)
                        {
                            return SYNTAX_TYPE_MATCH_OK;
                        }
                        else
                        {
                            return CheckConstraintTypeMatch(
                                lval,
                                rval,
                                bs[l].dtype,
                                bs[l].range.validation_string,
                                0);
                        }
                    }
                }
            }
        }
    }

    // Now check the functional modules - extra level of indirection
    for (int i = 0; i < CF3_MODULES; i++)
    {
        const PromiseTypeSyntax *promise_type_syntax = CF_ALL_PROMISE_TYPES[i];
        if (!promise_type_syntax)
        {
            continue;
        }

        for (int j = 0; promise_type_syntax[j].promise_type != NULL; j++)
        {
            const ConstraintSyntax *bs = bs =
                promise_type_syntax[j].constraints;

            if (!bs)
            {
                continue;
            }

            for (int l = 0; bs[l].lval != NULL; l++)
            {
                if (bs[l].dtype == CF_DATA_TYPE_BODY)
                {
                    const ConstraintSyntax *bs2 =
                        bs[l].range.body_type_syntax->constraints;

                    if (bs2 == NULL || bs2 == (void *) CF_BUNDLE)
                    {
                        continue;
                    }

                    for (int k = 0; bs2[k].dtype != CF_DATA_TYPE_NONE; k++)
                    {
                        /* Either module defined or common */

                        if (strcmp(promise_type_syntax[j].promise_type, type)
                                == 0
                            && strcmp(promise_type_syntax[j].promise_type, "*")
                                   != 0)
                        {
                            char output[CF_BUFSIZE];
                            snprintf(
                                output,
                                CF_BUFSIZE,
                                "lval %s belongs to promise type '%s': but this is '%s'\n",
                                lval,
                                promise_type_syntax[j].promise_type,
                                type);
                            yyerror(output);
                            return SYNTAX_TYPE_MATCH_OK;
                        }

                        if (strcmp(lval, bs2[k].lval) == 0)
                        {
                            /* Body definitions will be checked later. */
                            if (bs2[k].dtype != CF_DATA_TYPE_BODY)
                            {
                                return CheckConstraintTypeMatch(
                                    lval,
                                    rval,
                                    bs2[k].dtype,
                                    bs2[k].range.validation_string,
                                    0);
                            }
                            else
                            {
                                return SYNTAX_TYPE_MATCH_OK;
                            }
                        }
                    }
                }
            }
        }
    }

    char output[CF_BUFSIZE];
    snprintf(
        output,
        CF_BUFSIZE,
        "Constraint lvalue \"%s\" is not allowed in \'%s\' constraint body",
        lval,
        type);
    yyerror(output);

    return SYNTAX_TYPE_MATCH_OK; // TODO: OK?
}

static SyntaxTypeMatch CheckConstraint(
    const char *type,
    const char *lval,
    Rval rval,
    const PromiseTypeSyntax *promise_type_syntax)
{
    assert(promise_type_syntax);

    if (promise_type_syntax->promise_type != NULL) /* In a bundle */
    {
        if (strcmp(promise_type_syntax->promise_type, type) == 0)
        {
            const ConstraintSyntax *bs = promise_type_syntax->constraints;

            for (int l = 0; bs[l].lval != NULL; l++)
            {
                if (strcmp(lval, bs[l].lval) == 0)
                {
                    /* If we get here we have found the lval and it is valid
                       for this promise_type */

                    /* For bodies and bundles definitions can be elsewhere, so
                       they are checked in PolicyCheckRunnable(). */
                    if (bs[l].dtype != CF_DATA_TYPE_BODY
                        && bs[l].dtype != CF_DATA_TYPE_BUNDLE)
                    {
                        return CheckConstraintTypeMatch(
                            lval,
                            rval,
                            bs[l].dtype,
                            bs[l].range.validation_string,
                            0);
                    }
                }
            }
        }
    }

    return SYNTAX_TYPE_MATCH_OK;
}

static void ValidateClassLiteral(const char *class_literal)
{
    ParseResult res = ParseExpression(class_literal, 0, strlen(class_literal));

    if (!res.result)
    {
        ParseErrorColumnOffset(
            res.position - strlen(class_literal),
            "Syntax error in context string");
    }

    FreeExpression(res.result);
}

#endif // CF3_PARSE_LOGIC_H
