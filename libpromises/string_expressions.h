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

#ifndef CFENGINE_STRING_EXPRESSIONS_H
#define CFENGINE_STRING_EXPRESSIONS_H

/*
   String expressions grammar:

   <name> ::= <term>
              <term> <name>

   <term> ::= <token>
              <var-ref>

   <token> ::= [a-zA-Z0-9_:]+

   <var-ref> ::= $( <qname> )
                 ${ <qname> }

   <qname> ::= <name>
               <name> . <name>

   Subsequent <term>s are concatenated during evaluation.
*/

typedef enum
{
    CONCAT,
    LITERAL,
    VARREF
} StringOp;

typedef enum
{
    VAR_REF_TYPE_SCALAR,
    VAR_REF_TYPE_LIST
} VarRefType;

typedef struct StringExpression_ StringExpression;

struct StringExpression_
{
    StringOp op;
    union StringExpressionValue
    {
        struct
        {
            StringExpression *lhs;
            StringExpression *rhs;
        } concat;

        struct
        {
            char *literal;
        } literal;

        struct
        {
            StringExpression *name;
            VarRefType type;
        } varref;
    } val;
};

/* Parsing and evaluation */

/*
 * Result of parsing.
 *
 * if succeeded, then result is the result of parsing and position is last
 * character consumed.
 *
 * if not succeeded, then result is NULL and position is last character consumed
 * before the error.
 */
typedef struct
{
    StringExpression *result;
    int position;
} StringParseResult;

StringParseResult ParseStringExpression(const char *expr, int start, int end);

/*
 * Evaluator should return either heap-allocated string or NULL.  In later case
 * evaluation will be aborted and NULL will be returned from
 * EvalStringExpression.
 */
typedef char *(*VarRefEvaluator) (const char *varname, VarRefType type, void *param);

/*
 * Result is heap-allocated. In case evalfn() returns NULL whole
 * EvalStringExpression returns NULL as well.
 */
char *EvalStringExpression(const StringExpression *expr, VarRefEvaluator evalfn, void *param);

/*
 * Frees StringExpression produced by ParseStringExpression. NULL-safe.
 */
void FreeStringExpression(StringExpression *expr);

#endif
