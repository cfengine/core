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

#include <platform.h>

#include <cf3.defs.h>
#include <bool.h>
#include <logic_expressions.h>
#include <misc_lib.h>

#include <stdlib.h>

/* <primary> */

static ParseResult ParsePrimary(const char *expr, int start, int end)
{
    if (start < end && expr[start] == '(')
    {
        ParseResult res = ParseExpression(expr, start + 1, end);

        if (res.result)
        {
            /* Check if there is a matching ')' at the end */
            if (res.position < end && expr[res.position] == ')')
            {
                return (ParseResult) {res.result, res.position + 1};
            }
            else
            {
                /* Haven't found a matching bracket. Give up */
                FreeExpression(res.result);
                return (ParseResult) {NULL, res.position};
            }
        }
        else
        {
            return res;
        }
    }
    else
    {
        StringParseResult strres = ParseStringExpression(expr, start, end);

        if (strres.result)
        {
            Expression *res = xcalloc(1, sizeof(Expression));

            res->op = LOGICAL_OP_EVAL;
            res->val.eval.name = strres.result;

            return (ParseResult) {res, strres.position};
        }
        else
        {
            return (ParseResult) {NULL, strres.position};
        }
    }
}

/* <not-expr> */

static ParseResult ParseNotExpression(const char *expr, int start, int end)
{
    if (start < end && expr[start] == '!')
    {
        ParseResult primres = ParsePrimary(expr, start + 1, end);

        if (primres.result)
        {
            Expression *res = xcalloc(1, sizeof(Expression));

            res->op = LOGICAL_OP_NOT;
            res->val.not.arg = primres.result;

            return (ParseResult) {res, primres.position};
        }
        else
        {
            return primres;
        }
    }
    else
    {
        return ParsePrimary(expr, start, end);
    }
}

/* <and-expr> */

static ParseResult ParseAndExpression(const char *expr, int start, int end)
{
    ParseResult lhs, rhs;
    Expression *res;

    lhs = ParseNotExpression(expr, start, end);

    if (!lhs.result)
    {
        return lhs;
    }

    if (lhs.position == end || (expr[lhs.position] != '.' && expr[lhs.position] != '&'))
    {
        return lhs;
    }

    rhs = ParseAndExpression(expr, lhs.position + 1, end);

    if (!rhs.result)
    {
        FreeExpression(lhs.result);
        return rhs;
    }

    res = xcalloc(1, sizeof(Expression));
    res->op = LOGICAL_OP_AND;
    res->val.andor.lhs = lhs.result;
    res->val.andor.rhs = rhs.result;

    return (ParseResult) {res, rhs.position};
}

/* <or-expr> */

ParseResult ParseExpression(const char *expr, int start, int end)
{
    ParseResult lhs, rhs;
    Expression *res;
    int position;

    lhs = ParseAndExpression(expr, start, end);

    if (!lhs.result)
    {
        return lhs;
    }

/* End of left-hand side expression */
    position = lhs.position;

    if (position == end || expr[position] != '|')
    {
        return lhs;
    }

/* Skip second '|' in 'lhs||rhs' */

    if (position + 1 < end && expr[position + 1] == '|')
    {
        position++;
    }

    rhs = ParseExpression(expr, position + 1, end);

    if (!rhs.result)
    {
        FreeExpression(lhs.result);
        return rhs;
    }

    res = xcalloc(1, sizeof(Expression));
    res->op = LOGICAL_OP_OR;
    res->val.andor.lhs = lhs.result;
    res->val.andor.rhs = rhs.result;

    return (ParseResult) {res, rhs.position};
}

/* Evaluation */

ExpressionValue EvalExpression(const Expression *expr,
                               NameEvaluator nameevalfn, VarRefEvaluator varrefevalfn, void *param)
{
    switch (expr->op)
    {
    case LOGICAL_OP_OR:
    case LOGICAL_OP_AND:
    {
        ExpressionValue lhs = EXPRESSION_VALUE_ERROR, rhs = EXPRESSION_VALUE_ERROR;

        lhs = EvalExpression(expr->val.andor.lhs, nameevalfn, varrefevalfn, param);
        if (lhs == EXPRESSION_VALUE_ERROR)
        {
            return EXPRESSION_VALUE_ERROR;
        }

        rhs = EvalExpression(expr->val.andor.rhs, nameevalfn, varrefevalfn, param);

        if (rhs == EXPRESSION_VALUE_ERROR)
        {
            return EXPRESSION_VALUE_ERROR;
        }

        if (expr->op == LOGICAL_OP_OR)
        {
            return lhs || rhs;
        }
        else
        {
            return lhs && rhs;
        }
    }

    case LOGICAL_OP_NOT:
    {
        ExpressionValue arg = EvalExpression(expr->val.not.arg,
                                             nameevalfn,
                                             varrefevalfn,
                                             param);

        if (arg == EXPRESSION_VALUE_ERROR)
        {
            return EXPRESSION_VALUE_ERROR;
        }
        else
        {
            return !arg;
        }
    }

    case LOGICAL_OP_EVAL:
    {
        ExpressionValue ret = EXPRESSION_VALUE_ERROR;
        char *name = EvalStringExpression(expr->val.eval.name,
                                          varrefevalfn,
                                          param);

        if (name == NULL)
        {
            return EXPRESSION_VALUE_ERROR;
        }

        ret = (*nameevalfn) (name, param);
        free(name);
        return ret;
    }

    default:
        ProgrammingError("Unexpected class expression type is found: %d", expr->op);
    }
}

/* Freeing results */

void FreeExpression(Expression *e)
{
    if (!e)
    {
        return;
    }

    switch (e->op)
    {
    case LOGICAL_OP_OR:
    case LOGICAL_OP_AND:
        FreeExpression(e->val.andor.lhs);
        FreeExpression(e->val.andor.rhs);
        break;
    case LOGICAL_OP_NOT:
        FreeExpression(e->val.not.arg);
        break;
    case LOGICAL_OP_EVAL:
        FreeStringExpression(e->val.eval.name);
        break;
    default:
        ProgrammingError("Unknown logic expression type encountered in" "FreeExpression: %d", e->op);
    }
    free(e);
}
