/*
   Copyright 2017 Northern.tech AS

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

#include <cf3.defs.h>

#include <bool.h>
#include <string_expressions.h>
#include <misc_lib.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* <qname> */

static StringParseResult ParseQname(const char *expr, int start, int end)
{
    StringParseResult lhs, rhs;
    StringExpression *ret, *subret, *dot;

    lhs = ParseStringExpression(expr, start, end);

    if (!lhs.result)
    {
        return lhs;
    }

    if (lhs.position == end || expr[lhs.position] != '.')
    {
        return lhs;
    }

    rhs = ParseStringExpression(expr, lhs.position + 1, end);

    if (!rhs.result)
    {
        FreeStringExpression(lhs.result);
        return rhs;
    }

    dot = xcalloc(1, sizeof(StringExpression));
    dot->op = LITERAL;
    dot->val.literal.literal = xstrdup(".");

    subret = xcalloc(1, sizeof(StringExpression));
    subret->op = CONCAT;
    subret->val.concat.lhs = dot;
    subret->val.concat.rhs = rhs.result;

    ret = xcalloc(1, sizeof(StringExpression));
    ret->op = CONCAT;
    ret->val.concat.lhs = lhs.result;
    ret->val.concat.rhs = subret;

    return (StringParseResult) {ret, rhs.position};
}

/* <var-ref> */

static StringParseResult ParseVarRef(const char *expr, int start, int end)
{
    if (start + 1 < end && (expr[start] == '$' || expr[start] == '@'))
    {
        if (expr[start + 1] == '(' || expr[start + 1] == '{')
        {
            char closing_bracket = expr[start + 1] == '(' ? ')' : '}';
            StringParseResult res = ParseQname(expr, start + 2, end);

            if (res.result)
            {
                if (res.position < end && expr[res.position] == closing_bracket)
                {
                    StringExpression *ret = xcalloc(1, sizeof(StringExpression));

                    ret->op = VARREF;
                    ret->val.varref.name = res.result;

                    if (expr[start] == '$')
                    {
                        ret->val.varref.type = VAR_REF_TYPE_SCALAR;
                    }
                    else if (expr[start] == '@')
                    {
                        ret->val.varref.type = VAR_REF_TYPE_LIST;
                    }
                    else
                    {
                        ProgrammingError("Unrecognized var ref type");
                    }

                    return (StringParseResult) {ret, res.position + 1};
                }
                else
                {
                    FreeStringExpression(res.result);
                    return (StringParseResult) {NULL, res.position};
                }
            }
            else
            {
                return res;
            }
        }
        else
        {
            return (StringParseResult) {NULL, start + 1};
        }
    }
    else
    {
        return (StringParseResult) {NULL, start};
    }
}

/* <token> */

static bool ValidTokenCharacter(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return true;
    }

    if (c >= 'A' && c <= 'Z')
    {
        return true;
    }

    if (c >= '0' && c <= '9')
    {
        return true;
    }

    if (c == '_' || c == '[' || c == ']' || c == ':')
    {
        return true;
    }

    return false;
}

static StringParseResult ParseToken(const char *expr, int start, int end)
{
    int endlit = start;

    while (endlit < end && ValidTokenCharacter(expr[endlit]))
    {
        endlit++;
    }

    if (endlit > start)
    {
        StringExpression *ret = xcalloc(1, sizeof(StringExpression));

        ret->op = LITERAL;
        ret->val.literal.literal = xstrndup(expr + start, endlit - start);

        return (StringParseResult) {ret, endlit};
    }
    else
    {
        return (StringParseResult) {NULL, endlit};
    }
}

/* <term> */

static StringParseResult ParseTerm(const char *expr, int start, int end)
{
    StringParseResult res = ParseToken(expr, start, end);

    if (res.result)
    {
        return res;
    }
    else
    {
        return ParseVarRef(expr, start, end);
    }
}

/* <name> */

StringParseResult ParseStringExpression(const char *expr, int start, int end)
{
    StringParseResult lhs = ParseTerm(expr, start, end);

    if (lhs.result)
    {
        StringParseResult rhs = ParseStringExpression(expr, lhs.position, end);

        if (rhs.result)
        {
            StringExpression *ret = xcalloc(1, sizeof(StringExpression));

            ret->op = CONCAT;
            ret->val.concat.lhs = lhs.result;
            ret->val.concat.rhs = rhs.result;

            return (StringParseResult) {ret, rhs.position};
        }
        else
        {
            return lhs;
        }
    }
    else
    {
        return lhs;
    }
}

/* Evaluation */

static char *EvalConcat(const StringExpression *expr, VarRefEvaluator evalfn, void *param)
{
    char *lhs, *rhs, *res;

    lhs = EvalStringExpression(expr->val.concat.lhs, evalfn, param);
    if (!lhs)
    {
        return NULL;
    }

    rhs = EvalStringExpression(expr->val.concat.rhs, evalfn, param);
    if (!rhs)
    {
        free(lhs);
        return NULL;
    }

    xasprintf(&res, "%s%s", lhs, rhs);
    free(lhs);
    free(rhs);
    return res;
}

static char *EvalVarRef(const StringExpression *expr, VarRefEvaluator evalfn, void *param)
{
    char *name, *eval;

    name = EvalStringExpression(expr->val.varref.name, evalfn, param);
    if (!name)
    {
        return NULL;
    }

    eval = (*evalfn) (name, expr->val.varref.type, param);
    free(name);
    return eval;
}

char *EvalStringExpression(const StringExpression *expr, VarRefEvaluator evalfn, void *param)
{
    switch (expr->op)
    {
    case CONCAT:
        return EvalConcat(expr, evalfn, param);
    case LITERAL:
        return xstrdup(expr->val.literal.literal);
    case VARREF:
        return EvalVarRef(expr, evalfn, param);
    default:
        ProgrammingError("Unknown type of string expression" "encountered during evaluation: %d", expr->op);
    }
}

/* Freeing results */

void FreeStringExpression(StringExpression *expr)
{
    if (!expr)
    {
        return;
    }

    switch (expr->op)
    {
    case CONCAT:
        FreeStringExpression(expr->val.concat.lhs);
        FreeStringExpression(expr->val.concat.rhs);
        break;
    case LITERAL:
        free(expr->val.literal.literal);
        break;
    case VARREF:
        FreeStringExpression(expr->val.varref.name);
        break;
    default:
        ProgrammingError("Unknown type of string expression encountered: %d", expr->op);
    }

    free(expr);
}
