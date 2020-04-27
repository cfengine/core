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

static void inline ParserEndCurrentBlock()
{
    P.offsets.last_id = -1;
    P.offsets.last_string = -1;
    P.offsets.last_class_id = -1;
    if (P.block != PARSER_BLOCK_BUNDLE && P.currentbody != NULL)
    {
        P.currentbody->offset.end = P.offsets.current;
    }

    if (P.block == PARSER_BLOCK_BUNDLE && P.currentbundle != NULL)
    {
        P.currentbundle->offset.end = P.offsets.current;
    }
}

// This function is called for every rval (right hand side value) of every
// promise while parsing. The reason why it is so big is because it does a lot
// of transformation, for example transforming strings into function calls like
// parsejson, and then attempts to resolve those function calls.
static inline void ParserHandleBundlePromiseRval()
{
    if (!INSTALL_SKIP)
    {
        const ConstraintSyntax *constraint_syntax = NULL;
        const PromiseTypeSyntax *promise_type_syntax =
            PromiseTypeSyntaxGet(P.blocktype, P.currenttype);
        if (promise_type_syntax != NULL)
        {
            constraint_syntax = PromiseTypeSyntaxGetConstraintSyntax(
                promise_type_syntax, P.lval);
        }

        if (promise_type_syntax == NULL)
        {
            ParseError(
                "Invalid promise type '%s' in bundle '%s' of type '%s'",
                P.currenttype,
                P.blockid,
                P.blocktype);
            INSTALL_SKIP = true;
        }
        else if (constraint_syntax != NULL)
        {
            switch (constraint_syntax->status)
            {
            case SYNTAX_STATUS_DEPRECATED:
                ParseWarning(
                    PARSER_WARNING_DEPRECATED,
                    "Deprecated constraint '%s' in promise type '%s'",
                    constraint_syntax->lval,
                    promise_type_syntax->promise_type);
                // Intentional fall
            case SYNTAX_STATUS_NORMAL:
            {
                const char *item = P.rval.item;
                // convert @(x) to mergedata(x)
                if (P.rval.type == RVAL_TYPE_SCALAR
                    && (strcmp(P.lval, "data") == 0
                        || strcmp(P.lval, "template_data") == 0)
                    && strlen(item) > 3 && item[0] == '@'
                    && (item[1] == '(' || item[1] == '{'))
                {
                    Rlist *synthetic_args = NULL;
                    char *tmp =
                        xstrndup(P.rval.item + 2, strlen(P.rval.item) - 3);
                    RlistAppendScalar(&synthetic_args, tmp);
                    free(tmp);
                    RvalDestroy(P.rval);

                    P.rval = (Rval){FnCallNew("mergedata", synthetic_args),
                                    RVAL_TYPE_FNCALL};
                }
                // convert 'json or yaml' to direct container or parsejson(x)
                // or parseyaml(x)
                else if (
                    P.rval.type == RVAL_TYPE_SCALAR
                    && (strcmp(P.lval, "data") == 0
                        || strcmp(P.lval, "template_data") == 0))
                {
                    JsonElement *json = NULL;
                    JsonParseError res;
                    bool json_parse_attempted = false;
                    Buffer *copy =
                        BufferNewFrom(P.rval.item, strlen(P.rval.item));

                    const char *fname = NULL;
                    if (strlen(P.rval.item) > 3
                        && strncmp("---", P.rval.item, 3) == 0)
                    {
                        fname = "parseyaml";

                        // look for unexpanded variables
                        if (strstr(P.rval.item, "$(") == NULL
                            && strstr(P.rval.item, "${") == NULL)
                        {
                            const char *copy_data = BufferData(copy);
                            res = JsonParseYamlString(&copy_data, &json);
                            json_parse_attempted = true;
                        }
                    }
                    else
                    {
                        fname = "parsejson";
                        // look for unexpanded variables
                        if (strstr(P.rval.item, "$(") == NULL
                            && strstr(P.rval.item, "${") == NULL)
                        {
                            const char *copy_data = BufferData(copy);
                            res = JsonParse(&copy_data, &json);
                            json_parse_attempted = true;
                        }
                    }

                    BufferDestroy(copy);

                    if (json_parse_attempted && res != JSON_PARSE_OK)
                    {
                        // Parsing failed, insert fncall so it can be retried
                        // during evaluation
                    }
                    else if (
                        json != NULL
                        && JsonGetElementType(json)
                               == JSON_ELEMENT_TYPE_PRIMITIVE)
                    {
                        // Parsing failed, insert fncall so it can be retried
                        // during evaluation
                        JsonDestroy(json);
                        json = NULL;
                    }

                    if (fname != NULL)
                    {
                        if (json == NULL)
                        {
                            Rlist *synthetic_args = NULL;
                            RlistAppendScalar(&synthetic_args, P.rval.item);
                            RvalDestroy(P.rval);

                            P.rval = (Rval){FnCallNew(fname, synthetic_args),
                                            RVAL_TYPE_FNCALL};
                        }
                        else
                        {
                            RvalDestroy(P.rval);
                            P.rval = (Rval){json, RVAL_TYPE_CONTAINER};
                        }
                    }
                }

                {
                    SyntaxTypeMatch err = CheckConstraint(
                        P.currenttype, P.lval, P.rval, promise_type_syntax);
                    if (err != SYNTAX_TYPE_MATCH_OK
                        && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
                    {
                        yyerror(SyntaxTypeMatchToString(err));
                    }
                }

                if (P.rval.type == RVAL_TYPE_SCALAR
                    && (strcmp(P.lval, "ifvarclass") == 0
                        || strcmp(P.lval, "if") == 0))
                {
                    ValidateClassLiteral(P.rval.item);
                }

                Constraint *cp = PromiseAppendConstraint(
                    P.currentpromise,
                    P.lval,
                    RvalCopy(P.rval),
                    P.references_body);
                cp->offset.line = P.line_no;
                cp->offset.start = P.offsets.last_id;
                cp->offset.end = P.offsets.current;
                cp->offset.context = P.offsets.last_class_id;
                P.currentstype->offset.end = P.offsets.current;
            }
            break;
            case SYNTAX_STATUS_REMOVED:
                ParseWarning(
                    PARSER_WARNING_REMOVED,
                    "Removed constraint '%s' in promise type '%s'",
                    constraint_syntax->lval,
                    promise_type_syntax->promise_type);
                break;
            }
        }
        else
        {
            ParseError(
                "Unknown constraint '%s' in promise type '%s'",
                P.lval,
                promise_type_syntax->promise_type);
        }

        RvalDestroy(P.rval);
        P.rval = RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
        strcpy(P.lval, "no lval");
        RlistDestroy(P.currentRlist);
        P.currentRlist = NULL;
    }
    else
    {
        RvalDestroy(P.rval);
        P.rval = RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
    }
}

static inline void ParserBeginBundle()
{
    ParserDebug("P:bundle:%s\n", P.blocktype);
    P.block = PARSER_BLOCK_BUNDLE;
    RvalDestroy(P.rval);
    P.rval = RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
    RlistDestroy(P.currentRlist);
    P.currentRlist = NULL;
    if (P.currentstring)
    {
        free(P.currentstring);
    }
    P.currentstring = NULL;
    strcpy(P.blockid, "");
}

static inline void ParserBeginBody()
{
    ParserDebug("P:body:%s\n", P.blocktype);
    P.block = PARSER_BLOCK_BODY;
    strcpy(P.blockid, "");
    RlistDestroy(P.currentRlist);
    P.currentRlist = NULL;
    if (P.currentstring)
    {
        free(P.currentstring);
    }
    P.currentstring = NULL;
}

#endif // CF3_PARSE_LOGIC_H
