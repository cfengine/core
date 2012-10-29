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

#ifdef HAVE_CONFIG_H
# include <conf.h>
#endif

#include "cf3.defs.h"
#include "bool.h"
#include "logic_expressions.h"

#include <stdlib.h>

/* <primary> */

static ParseResult ParsePrimary(const char *expr, int start, int end)
{
    if (start < end && expr[start] == '(')
    {
        ParseResult res = ParseExpression(expr, start + 1, end);

        if (res.result)
        {
            /* Check there is a matching ')' at the end */
            if (res.position < end && expr[res.position] == ')')
            {
                return (ParseResult)
                {
                res.result, res.position + 1};
            }
            else
            {
                /* Didn't find a matching bracket. Give up */
                FreeExpression(res.result);
                return (ParseResult)
                {
                NULL, res.position};
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

            res->op = EVAL;
            res->val.eval.name = strres.result;

            return (ParseResult)
            {
            res, strres.position};
        }
        else
        {
            return (ParseResult)
            {
            NULL, strres.position};
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

            res->op = NOT;
            res->val.not.arg = primres.result;

            return (ParseResult)
            {
            res, primres.position};
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
    res->op = AND;
    res->val.andor.lhs = lhs.result;
    res->val.andor.rhs = rhs.result;

    return (ParseResult)
    {
    res, rhs.position};
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
    res->op = OR;
    res->val.andor.lhs = lhs.result;
    res->val.andor.rhs = rhs.result;

    return (ParseResult)
    {
    res, rhs.position};
}

/* Evaluation */

ExpressionValue EvalExpression(const Expression *expr,
                               NameEvaluator nameevalfn, VarRefEvaluator varrefevalfn, void *param)
{
    switch (expr->op)
    {
    case OR:
    case AND:
    {
        ExpressionValue lhs = EXP_ERROR, rhs = EXP_ERROR;

        lhs = EvalExpression(expr->val.andor.lhs, nameevalfn, varrefevalfn, param);
        if (lhs == EXP_ERROR)
        {
            return EXP_ERROR;
        }

        rhs = EvalExpression(expr->val.andor.rhs, nameevalfn, varrefevalfn, param);

        if (rhs == EXP_ERROR)
        {
            return EXP_ERROR;
        }

        if (expr->op == OR)
        {
            return lhs || rhs;
        }
        else
        {
            return lhs && rhs;
        }
    }

    case NOT:
    {
        ExpressionValue arg = EvalExpression(expr->val.not.arg,
                                             nameevalfn,
                                             varrefevalfn,
                                             param);

        if (arg == EXP_ERROR)
        {
            return EXP_ERROR;
        }
        else
        {
            return !arg;
        }
    }

    case EVAL:
    {
        ExpressionValue ret = EXP_ERROR;
        char *name = EvalStringExpression(expr->val.eval.name,
                                          varrefevalfn,
                                          param);

        if (name == NULL)
        {
            return EXP_ERROR;
        }

        ret = (*nameevalfn) (name, param);
        free(name);
        return ret;
    }

    default:
        FatalError("Unexpected class expression type is found: %d", expr->op);
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
    case OR:
    case AND:
        FreeExpression(e->val.andor.lhs);
        FreeExpression(e->val.andor.rhs);
        break;
    case NOT:
        FreeExpression(e->val.not.arg);
        break;
    case EVAL:
        FreeStringExpression(e->val.eval.name);
        break;
    default:
        FatalError("Unknown logic expression type encountered in" "FreeExpression: %d", e->op);
    }
    free(e);
}
