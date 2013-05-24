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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "syntax.h"

#include "json.h"
#include "files_names.h"
#include "mod_files.h"
#include "item_lib.h"
#include "conversion.h"
#include "expand.h"
#include "matching.h"
#include "scope.h"
#include "fncall.h"
#include "string_lib.h"
#include "misc_lib.h"
#include "rlist.h"
#include "vars.h"
#include "env_context.h"

#include <assert.h>

static SyntaxTypeMatch CheckParseString(const char *lv, const char *s, const char *range);
static SyntaxTypeMatch CheckParseInt(const char *lv, const char *s, const char *range);
static SyntaxTypeMatch CheckParseReal(const char *lv, const char *s, const char *range);
static SyntaxTypeMatch CheckParseRealRange(const char *lval, const char *s, const char *range);
static SyntaxTypeMatch CheckParseIntRange(const char *lval, const char *s, const char *range);
static SyntaxTypeMatch CheckParseOpts(const char *lv, const char *s, const char *range);
static SyntaxTypeMatch CheckFnCallType(const char *lval, const char *s, DataType dtype, const char *range);

/*********************************************************/

static const PromiseTypeSyntax *PromiseTypeSyntaxGetStrict(const char *bundle_type, const char *promise_type)
{
    for (int module_index = 0; module_index < CF3_MODULES; module_index++)
    {
        for (int promise_type_index = 0; CF_ALL_PROMISE_TYPES[module_index][promise_type_index].promise_type; promise_type_index++)
        {
            const PromiseTypeSyntax *promise_type_syntax = &CF_ALL_PROMISE_TYPES[module_index][promise_type_index];

            if (strcmp(bundle_type, promise_type_syntax->bundle_type) == 0
                && strcmp(promise_type, promise_type_syntax->promise_type) == 0)
            {
                return promise_type_syntax;
            }
        }
    }
    return NULL;
}

const PromiseTypeSyntax *PromiseTypeSyntaxGet(const char *bundle_type, const char *promise_type)
{
    const PromiseTypeSyntax *pts = PromiseTypeSyntaxGetStrict(bundle_type, promise_type);
    if (!pts)
    {
        pts = PromiseTypeSyntaxGetStrict("*", promise_type);
    }
    return pts;
}

static const ConstraintSyntax *GetCommonConstraint(const char *lval)
{
    for (int i = 0; CF_COMMON_PROMISE_TYPES[i].promise_type; i++)
    {
        const PromiseTypeSyntax promise_type_syntax = CF_COMMON_PROMISE_TYPES[i];

        for (int j = 0; promise_type_syntax.constraints[j].lval; j++)
        {
            if (strcmp(promise_type_syntax.constraints[j].lval, lval) == 0)
            {
                return &promise_type_syntax.constraints[j];
            }
        }
    }

    return NULL;
}

const ConstraintSyntax *BodySyntaxGetConstraintSyntax(const ConstraintSyntax *body_syntax, const char *lval)
{
    for (int j = 0; body_syntax[j].lval; j++)
    {
        if (strcmp(body_syntax[j].lval, lval) == 0)
        {
            return &body_syntax[j];
        }
    }
    return NULL;
}

const ConstraintSyntax *PromiseTypeSyntaxGetConstraintSyntax(const PromiseTypeSyntax *promise_type_syntax, const char *lval)
{
    for (int i = 0; promise_type_syntax->constraints[i].lval; i++)
    {
        if (strcmp(promise_type_syntax->constraints[i].lval, lval) == 0)
        {
            return &promise_type_syntax->constraints[i];
        }
    }

    const ConstraintSyntax *constraint_syntax = NULL;
    if (strcmp("edit_line", promise_type_syntax->bundle_type) == 0)
    {
        constraint_syntax = BodySyntaxGetConstraintSyntax(CF_COMMON_EDITBODIES, lval);
        if (constraint_syntax)
        {
            return constraint_syntax;
        }
    }
    else if (strcmp("edit_xml", promise_type_syntax->bundle_type) == 0)
    {
        constraint_syntax = BodySyntaxGetConstraintSyntax(CF_COMMON_XMLBODIES, lval);
        if (constraint_syntax)
        {
            return constraint_syntax;
        }
    }

    return GetCommonConstraint(lval);
}

const BodySyntax *BodySyntaxGet(const char *body_type)
{
    for (int i = 0; i < CF3_MODULES; i++)
    {
        const PromiseTypeSyntax *promise_type_syntax = CF_ALL_PROMISE_TYPES[i];

        for (int k = 0; promise_type_syntax[k].bundle_type != NULL; k++)
        {
            for (int z = 0; promise_type_syntax[k].constraints[z].lval != NULL; z++)
            {
                const ConstraintSyntax constraint_syntax = promise_type_syntax[k].constraints[z];

                if (constraint_syntax.dtype == DATA_TYPE_BODY && strcmp(body_type, constraint_syntax.lval) == 0)
                {
                    return constraint_syntax.range.body_type_syntax;
                }
            }
        }
    }

    for (int i = 0; CONTROL_BODIES[i].body_type != NULL; i++)
    {
        const BodySyntax body_syntax = CONTROL_BODIES[i];

        if (strcmp(body_type, body_syntax.body_type) == 0)
        {
            return &CONTROL_BODIES[i];
        }
    }

    return NULL;
}

const char *SyntaxStatusToString(SyntaxStatus status)
{
    static const char *status_strings[] =
    {
        [SYNTAX_STATUS_DEPRECATED] = "deprecated",
        [SYNTAX_STATUS_NORMAL] = "normal",
        [SYNTAX_STATUS_REMOVED] = "removed"
    };
    return status_strings[status];
}

/****************************************************************************/

DataType ExpectedDataType(const char *lvalname)
{
    int i, j, k, l;
    const ConstraintSyntax *bs, *bs2;
    const PromiseTypeSyntax *ss;

    for (i = 0; i < CF3_MODULES; i++)
    {
        if ((ss = CF_ALL_PROMISE_TYPES[i]) == NULL)
        {
            continue;
        }

        for (j = 0; ss[j].promise_type != NULL; j++)
        {
            if ((bs = ss[j].constraints) == NULL)
            {
                continue;
            }

            for (k = 0; bs[k].lval != NULL; k++)
            {
                if (strcmp(lvalname, bs[k].lval) == 0)
                {
                    return bs[k].dtype;
                }
            }

            for (k = 0; bs[k].lval != NULL; k++)
            {
                if (bs[k].dtype == DATA_TYPE_BODY)
                {
                    bs2 = bs[k].range.body_type_syntax->constraints;

                    if (bs2 == NULL || bs2 == (void *) CF_BUNDLE)
                    {
                        continue;
                    }

                    for (l = 0; bs2[l].dtype != DATA_TYPE_NONE; l++)
                    {
                        if (strcmp(lvalname, bs2[l].lval) == 0)
                        {
                            return bs2[l].dtype;
                        }
                    }
                }
            }

        }
    }

    return DATA_TYPE_NONE;
}

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

const char *SyntaxTypeMatchToString(SyntaxTypeMatch result)
{
    assert(result < SYNTAX_TYPE_MATCH_MAX);

    static const char *msgs[SYNTAX_TYPE_MATCH_MAX] =
    {
        [SYNTAX_TYPE_MATCH_OK] = "OK",

        [SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED] = "Cannot check unexpanded value",
        [SYNTAX_TYPE_MATCH_ERROR_RANGE_BRACKETED] = "Real range specification should not be enclosed in brackets - just \"a,b\"",
        [SYNTAX_TYPE_MATCH_ERROR_RANGE_MULTIPLE_ITEMS] = "Range format specifier should be of form \"a,b\" but got multiple items",
        [SYNTAX_TYPE_MATCH_ERROR_GOT_SCALAR] = "Attempted to give a scalar to a non-scalar type",
        [SYNTAX_TYPE_MATCH_ERROR_GOT_LIST] = "Attempted to give a list to a non-list type",

        [SYNTAX_TYPE_MATCH_ERROR_STRING_UNIX_PERMISSION] = "Error parsing Unix permission string",

        [SYNTAX_TYPE_MATCH_ERROR_SCALAR_OUT_OF_RANGE] = "Scalar value is out of range",

        [SYNTAX_TYPE_MATCH_ERROR_INT_PARSE] = "Cannot parse value as integer",
        [SYNTAX_TYPE_MATCH_ERROR_INT_OUT_OF_RANGE] = "Integer is out of range",

        [SYNTAX_TYPE_MATCH_ERROR_REAL_INF] = "Keyword \"inf\" has an integer value, cannot be used as real",
        [SYNTAX_TYPE_MATCH_ERROR_REAL_OUT_OF_RANGE] = "Real value is out of range",

        [SYNTAX_TYPE_MATCH_ERROR_OPTS_OUT_OF_RANGE] = "Selection is out of bounds",

        [SYNTAX_TYPE_MATCH_ERROR_FNCALL_RETURN_TYPE] = "Function does not return the required type",
        [SYNTAX_TYPE_MATCH_ERROR_FNCALL_UNKNOWN] = "Unknown function",

        [SYNTAX_TYPE_MATCH_ERROR_CONTEXT_OUT_OF_RANGE] = "Context string is invalid/out of range"
    };

    return msgs[result];
}

SyntaxTypeMatch CheckConstraintTypeMatch(const char *lval, Rval rval, DataType dt, const char *range, int level)
{
    Rlist *rp;
    Item *checklist;

/* Get type of lval */

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        switch (dt)
        {
        case DATA_TYPE_STRING_LIST:
        case DATA_TYPE_INT_LIST:
        case DATA_TYPE_REAL_LIST:
        case DATA_TYPE_CONTEXT_LIST:
        case DATA_TYPE_OPTION_LIST:
            if (level == 0)
            {
                return SYNTAX_TYPE_MATCH_ERROR_GOT_SCALAR;
            }
            break;
        default:
            /* Only lists are incompatible with scalars */
            break;
        }
        break;

    case RVAL_TYPE_LIST:

        switch (dt)
        {
        case DATA_TYPE_STRING_LIST:
        case DATA_TYPE_INT_LIST:
        case DATA_TYPE_REAL_LIST:
        case DATA_TYPE_CONTEXT_LIST:
        case DATA_TYPE_OPTION_LIST:
            break;
        default:
            return SYNTAX_TYPE_MATCH_ERROR_GOT_LIST;
        }

        for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
        {
            SyntaxTypeMatch err = CheckConstraintTypeMatch(lval, (Rval) {rp->item, rp->type}, dt, range, 1);
            switch (err)
            {
            case SYNTAX_TYPE_MATCH_OK:
            case SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED:
                break;

            default:
                return err;
            }
        }

        return SYNTAX_TYPE_MATCH_OK;

    case RVAL_TYPE_FNCALL:

        /* Fn-like objects are assumed to be parameterized bundles in these... */

        checklist = SplitString("bundlesequence,edit_line,edit_xml,usebundle,service_bundle", ',');

        if (!IsItemIn(checklist, lval))
        {
            SyntaxTypeMatch err = CheckFnCallType(lval, ((FnCall *) rval.item)->name, dt, range);
            DeleteItemList(checklist);
            return err;
        }

        DeleteItemList(checklist);
        return SYNTAX_TYPE_MATCH_OK;

    default:
        break;
    }

/* If we get here, we have a literal scalar type */

    switch (dt)
    {
    case DATA_TYPE_STRING:
    case DATA_TYPE_STRING_LIST:
        return CheckParseString(lval, (const char *) rval.item, range);

    case DATA_TYPE_INT:
    case DATA_TYPE_INT_LIST:
        return CheckParseInt(lval, (const char *) rval.item, range);

    case DATA_TYPE_REAL:
    case DATA_TYPE_REAL_LIST:
        return CheckParseReal(lval, (const char *) rval.item, range);

    case DATA_TYPE_BODY:
    case DATA_TYPE_BUNDLE:
        break;

    case DATA_TYPE_OPTION:
    case DATA_TYPE_OPTION_LIST:
        return CheckParseOpts(lval, (const char *) rval.item, range);

    case DATA_TYPE_CONTEXT:
    case DATA_TYPE_CONTEXT_LIST:
        return CheckParseContext((const char *) rval.item, range);

    case DATA_TYPE_INT_RANGE:
        return CheckParseIntRange(lval, (const char *) rval.item, range);

    case DATA_TYPE_REAL_RANGE:
        return CheckParseRealRange(lval, (char *) rval.item, range);

    default:
        ProgrammingError("Unknown (unhandled) datatype for lval = %s (CheckConstraintTypeMatch)", lval);
        break;
    }

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

DataType StringDataType(EvalContext *ctx, const char *scopeid, const char *string)
{
    DataType dtype;
    Rval rval;
    int islist = false;
    char var[CF_BUFSIZE];

/*-------------------------------------------------------
What happens if we embed vars in a literal string
       "$(list)withending" - a list?
       "$(list1)$(list2)"  - not a simple list
Disallow these manual concatenations as ambiguous.
Demand this syntax to work around

vars:

   "listvar" slist => EmbellishList("prefix$(list)suffix");
---------------------------------------------------------*/

    var[0] = '\0';

    if (*string == '$')
    {
        if (ExtractInnerCf3VarString(string, var))
        {
            if (EvalContextVariableGet(ctx, (VarRef) { NULL, scopeid, var }, &rval, &dtype))
            {
                if (rval.type == RVAL_TYPE_LIST)
                {
                    if (!islist)
                    {
                        islist = true;
                    }
                    else
                    {
                        islist = false;
                    }
                }
            }

            if (strlen(var) == strlen(string))
            {
                /* Can trust variables own datatype  */
                return dtype;
            }
            else
            {
                /* Must force non-pure substitution to be generic type CF_SCALAR.cf_str */
                return DATA_TYPE_STRING;
            }
        }
    }

    return DATA_TYPE_STRING;
}

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

static SyntaxTypeMatch CheckParseString(const char *lval, const char *s, const char *range)
{
    if (s == NULL)
    {
        return SYNTAX_TYPE_MATCH_OK;
    }

    if (strlen(range) == 0)
    {
        return SYNTAX_TYPE_MATCH_OK;
    }

    if (IsNakedVar(s, '@') || IsNakedVar(s, '$'))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

/* Deal with complex strings as special cases */

    if (strcmp(lval, "mode") == 0 || strcmp(lval, "search_mode") == 0)
    {
        mode_t plus, minus;

        if (!ParseModeString(s, &plus, &minus))
        {
            return SYNTAX_TYPE_MATCH_ERROR_STRING_UNIX_PERMISSION;
        }
    }

    if (FullTextMatch(range, s))
    {
        return SYNTAX_TYPE_MATCH_OK;
    }

    if (IsCf3VarString(s))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }
    else
    {
        return SYNTAX_TYPE_MATCH_ERROR_SCALAR_OUT_OF_RANGE;
    }

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

SyntaxTypeMatch CheckParseContext(const char *context, const char *range)
{
    if (strlen(range) == 0)
    {
        return SYNTAX_TYPE_MATCH_OK;
    }

    if (FullTextMatch(range, context))
    {
        return SYNTAX_TYPE_MATCH_OK;
    }

    return SYNTAX_TYPE_MATCH_ERROR_CONTEXT_OUT_OF_RANGE;
}

/****************************************************************************/

static SyntaxTypeMatch CheckParseInt(const char *lval, const char *s, const char *range)
{
    Item *split;
    int n;
    long max = CF_LOWINIT, min = CF_HIGHINIT, val;

    // Numeric types are registered by range separated by comma str "min,max"
    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        ProgrammingError("INTERN: format specifier for int rvalues is not ok for lval %s - got %d items", lval, n);
    }

    sscanf(split->name, "%ld", &min);

    if (strcmp(split->next->name, "inf") == 0)
    {
        max = CF_INFINITY;
    }
    else
    {
        sscanf(split->next->name, "%ld", &max);
    }

    DeleteItemList(split);

    if (min == CF_HIGHINIT || max == CF_LOWINIT)
    {
        ProgrammingError("INTERN: could not parse format specifier for int rvalues for lval %s", lval);
    }

    if (IsCf3VarString(s))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

    val = IntFromString(s);

    if (val == CF_NOINT)
    {
        return SYNTAX_TYPE_MATCH_ERROR_INT_PARSE;
    }

    if (val > max || val < min)
    {
        return SYNTAX_TYPE_MATCH_ERROR_INT_OUT_OF_RANGE;
    }

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

static SyntaxTypeMatch CheckParseIntRange(const char *lval, const char *s, const char *range)
{
    Item *split, *ip, *rangep;
    int n;
    long max = CF_LOWINIT, min = CF_HIGHINIT, val;

    // Numeric types are registered by range separated by comma str "min,max"
    if (*s == '[' || *s == '(')
    {
        return SYNTAX_TYPE_MATCH_ERROR_RANGE_BRACKETED;
    }

    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        ProgrammingError("Format specifier %s for irange rvalues is not ok for lval %s - got %d items", range, lval, n);
    }

    sscanf(split->name, "%ld", &min);

    if (strcmp(split->next->name, "inf") == 0)
    {
        max = CF_INFINITY;
    }
    else
    {
        sscanf(split->next->name, "%ld", &max);
    }

    DeleteItemList(split);

    if (min == CF_HIGHINIT || max == CF_LOWINIT)
    {
        ProgrammingError("Could not parse irange format specifier for int rvalues for lval %s", lval);
    }

    if (IsCf3VarString(s))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

    rangep = SplitString(s, ',');

    if ((n = ListLen(rangep)) != 2)
    {
        return SYNTAX_TYPE_MATCH_ERROR_RANGE_MULTIPLE_ITEMS;
    }

    for (ip = rangep; ip != NULL; ip = ip->next)
    {
        val = IntFromString(ip->name);

        if (val > max || val < min)
        {
            return SYNTAX_TYPE_MATCH_ERROR_INT_OUT_OF_RANGE;
        }
    }

    DeleteItemList(rangep);

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

static SyntaxTypeMatch CheckParseReal(const char *lval, const char *s, const char *range)
{
    Item *split;
    double max = (double) CF_LOWINIT, min = (double) CF_HIGHINIT, val;
    int n;

    if (strcmp(s, "inf") == 0)
    {
        return SYNTAX_TYPE_MATCH_ERROR_REAL_INF;
    }

    if (IsCf3VarString(s))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

/* Numeric types are registered by range separated by comma str "min,max" */

    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        ProgrammingError("Format specifier for real rvalues is not ok for lval %s - %d items", lval, n);
    }

    sscanf(split->name, "%lf", &min);
    sscanf(split->next->name, "%lf", &max);
    DeleteItemList(split);

    if (min == CF_HIGHINIT || max == CF_LOWINIT)
    {
        ProgrammingError("Could not parse format specifier for int rvalues for lval %s", lval);
    }

    if (!DoubleFromString(s, &val))
    {
        return SYNTAX_TYPE_MATCH_ERROR_REAL_OUT_OF_RANGE;
    }

    if (val > max || val < min)
    {
        return SYNTAX_TYPE_MATCH_ERROR_REAL_OUT_OF_RANGE;
    }

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

static SyntaxTypeMatch CheckParseRealRange(const char *lval, const char *s, const char *range)
{
    Item *split, *rangep, *ip;
    double max = (double) CF_LOWINIT, min = (double) CF_HIGHINIT, val;
    int n;

    if (*s == '[' || *s == '(')
    {
        return SYNTAX_TYPE_MATCH_ERROR_RANGE_BRACKETED;
    }

    if (strcmp(s, "inf") == 0)
    {
        return SYNTAX_TYPE_MATCH_ERROR_REAL_INF;
    }

    if (IsCf3VarString(s))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

/* Numeric types are registered by range separated by comma str "min,max" */

    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        ProgrammingError("Format specifier for real rvalues is not ok for lval %s - %d items", lval, n);
    }

    sscanf(split->name, "%lf", &min);
    sscanf(split->next->name, "%lf", &max);
    DeleteItemList(split);

    if (min == CF_HIGHINIT || max == CF_LOWINIT)
    {
        ProgrammingError("Could not parse format specifier for int rvalues for lval %s", lval);
    }

    rangep = SplitString(s, ',');

    if ((n = ListLen(rangep)) != 2)
    {
        return SYNTAX_TYPE_MATCH_ERROR_RANGE_MULTIPLE_ITEMS;
    }

    for (ip = rangep; ip != NULL; ip = ip->next)
    {
        if (!DoubleFromString(ip->name, &val))
        {
            return SYNTAX_TYPE_MATCH_ERROR_REAL_OUT_OF_RANGE;
        }

        if (val > max || val < min)
        {
            return SYNTAX_TYPE_MATCH_ERROR_REAL_OUT_OF_RANGE;
        }
    }

    DeleteItemList(rangep);

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

static SyntaxTypeMatch CheckParseOpts(const char *lval, const char *s, const char *range)
{
    Item *split;

/* List/menu types are separated by comma str "a,b,c,..." */

    if (IsNakedVar(s, '@') || IsNakedVar(s, '$'))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

    split = SplitString(range, ',');

    if (!IsItemIn(split, s))
    {
        DeleteItemList(split);
        return SYNTAX_TYPE_MATCH_ERROR_OPTS_OUT_OF_RANGE;
    }

    DeleteItemList(split);

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

int CheckParseVariableName(const char *name)
{
    const char *reserved[] = { "promiser", "handle", "promise_filename", "promise_linenumber", "this", NULL };
    char scopeid[CF_MAXVARSIZE], vlval[CF_MAXVARSIZE];
    int count = 0, level = 0;

    if (IsStrIn(name, reserved))
    {
        return false;
    }

    scopeid[0] = '\0';

    if (strchr(name, '.'))
    {
        for (const char *sp = name; *sp != '\0'; sp++)
        {
            switch (*sp)
            {
            case '.':
                if (++count > 1 && level != 1)
                {
                    return false;
                }
                break;

            case '[':
                level++;
                break;

            case ']':
                level--;
                break;

            default:
                break;
            }

            if (level > 1)
            {
                yyerror("Too many levels of [] reserved for array use");
                return false;
            }

        }

        if (count == 1)
        {
            sscanf(name, "%[^.].%s", scopeid, vlval);

            if (strlen(scopeid) == 0 || strlen(vlval) == 0)
            {
                return false;
            }
        }
    }

    return true;
}

/****************************************************************************/

bool IsDataType(const char *s)
{
    return strcmp(s, "string") == 0 || strcmp(s, "slist") == 0 ||
        strcmp(s, "int") == 0 || strcmp(s, "ilist") == 0 || strcmp(s, "real") == 0 || strcmp(s, "rlist") == 0;
}

/****************************************************************************/

static SyntaxTypeMatch CheckFnCallType(const char *lval, const char *s, DataType dtype, const char *range)
{
    DataType dt;
    const FnCallType *fn;

    fn = FnCallTypeGet(s);

    if (fn)
    {
        dt = fn->dtype;

        if (dtype != dt)
        {
            /* Ok to allow fn calls of correct element-type in lists */

            if (dt == DATA_TYPE_STRING && dtype == DATA_TYPE_STRING_LIST)
            {
                return SYNTAX_TYPE_MATCH_OK;
            }

            if (dt == DATA_TYPE_INT && dtype == DATA_TYPE_INT_LIST)
            {
                return SYNTAX_TYPE_MATCH_OK;
            }

            if (dt == DATA_TYPE_REAL && dtype == DATA_TYPE_REAL_LIST)
            {
                return SYNTAX_TYPE_MATCH_OK;
            }

            if (dt == DATA_TYPE_OPTION && dtype == DATA_TYPE_OPTION_LIST)
            {
                return SYNTAX_TYPE_MATCH_OK;
            }

            if (dt == DATA_TYPE_CONTEXT && dtype == DATA_TYPE_CONTEXT_LIST)
            {
                return SYNTAX_TYPE_MATCH_OK;
            }

            return SYNTAX_TYPE_MATCH_ERROR_FNCALL_RETURN_TYPE;
        }
        else
        {
            return SYNTAX_TYPE_MATCH_OK;
        }
    }
    else
    {
        return SYNTAX_TYPE_MATCH_ERROR_FNCALL_UNKNOWN;
    }
}


/****************************************************************************/

static JsonElement *ConstraintSyntaxToJson(const ConstraintSyntax *constraint_syntax)
{
    JsonElement *json_constraint = JsonObjectCreate(5);

    JsonObjectAppendString(json_constraint, "attribute", constraint_syntax->lval);
    JsonObjectAppendString(json_constraint, "status", SyntaxStatusToString(constraint_syntax->status));
    JsonObjectAppendString(json_constraint, "type", DataTypeToString(constraint_syntax->dtype));

    if (constraint_syntax->dtype != DATA_TYPE_BODY && constraint_syntax->dtype != DATA_TYPE_BUNDLE)
    {
        JsonObjectAppendString(json_constraint, "range", constraint_syntax->range.validation_string);
    }

    return json_constraint;
}

static JsonElement *BodySyntaxToJson(const BodySyntax *body_syntax)
{
    JsonElement *json_body = JsonObjectCreate(2);

    JsonObjectAppendString(json_body, "status", SyntaxStatusToString(body_syntax->status));
    {
        JsonElement *attributes = JsonObjectCreate(50);

        for (int i = 0; body_syntax->constraints[i].lval; i++)
        {
            const ConstraintSyntax *constraint_syntax = &body_syntax->constraints[i];
            if (constraint_syntax->status != SYNTAX_STATUS_REMOVED)
            {
                JsonElement *json_constraint = ConstraintSyntaxToJson(constraint_syntax);
                JsonObjectAppendString(json_constraint, "visibility", "body");
                JsonObjectAppendObject(attributes, constraint_syntax->lval, json_constraint);
            }
        }

        JsonObjectAppendObject(json_body, "attributes", attributes);
    }

    return json_body;
}

static JsonElement *JsonBundleTypeNew(void)
{
    JsonElement *json_bundle_type = JsonObjectCreate(2);

    JsonObjectAppendString(json_bundle_type, "status", SyntaxStatusToString(SYNTAX_STATUS_NORMAL));
    JsonObjectAppendArray(json_bundle_type, "promiseTypes", JsonArrayCreate(50));

    return json_bundle_type;
}

static JsonElement *BundleTypesToJson(void)
{
    JsonElement *bundle_types = JsonObjectCreate(50);

    Seq *common_promise_types = SeqNew(50, free);

    for (int module_index = 0; module_index < CF3_MODULES; module_index++)
    {
        for (int promise_type_index = 0; CF_ALL_PROMISE_TYPES[module_index][promise_type_index].promise_type; promise_type_index++)
        {
            const PromiseTypeSyntax *promise_type_syntax = &CF_ALL_PROMISE_TYPES[module_index][promise_type_index];

            // skip global constraints
            if (strcmp("*", promise_type_syntax->promise_type) == 0)
            {
                continue;
            }

            // collect common promise types to be appended at the end
            if (strcmp("*", promise_type_syntax->bundle_type) == 0)
            {
                SeqAppend(common_promise_types, xstrdup(promise_type_syntax->promise_type));
                continue;
            }

            if (promise_type_syntax->status == SYNTAX_STATUS_REMOVED)
            {
                continue;
            }

            JsonElement *bundle_type = JsonObjectGet(bundle_types, promise_type_syntax->bundle_type);
            if (!bundle_type)
            {
                bundle_type = JsonBundleTypeNew();
                JsonObjectAppendObject(bundle_types, promise_type_syntax->bundle_type, bundle_type);
            }
            assert(bundle_type);

            JsonElement *promise_types = JsonObjectGet(bundle_type, "promiseTypes");
            assert(promise_types);

            JsonArrayAppendString(promise_types, promise_type_syntax->promise_type);
        }
    }

    // Append the common bundle, which has only common promise types, but is not declared in syntax
    {
        JsonElement *bundle_type = JsonBundleTypeNew();
        JsonObjectAppendObject(bundle_types, "common", bundle_type);
    }

    JsonIterator it = JsonIteratorInit(bundle_types);
    const char *bundle_type = NULL;
    while ((bundle_type = JsonIteratorNextKey(&it)))
    {
        JsonElement *promise_types = JsonObjectGetAsArray(JsonObjectGetAsObject(bundle_types, bundle_type), "promiseTypes");
        for (int i = 0; i < SeqLength(common_promise_types); i++)
        {
            const char *common_promise_type = SeqAt(common_promise_types, i);
            JsonArrayAppendString(promise_types, common_promise_type);
        }
    }

    SeqDestroy(common_promise_types);
    return bundle_types;
}

static JsonElement *JsonPromiseTypeNew(SyntaxStatus status)
{
    JsonElement *promise_type = JsonObjectCreate(2);

    JsonObjectAppendString(promise_type, "status", SyntaxStatusToString(status));
    JsonObjectAppendObject(promise_type, "attributes", JsonObjectCreate(50));

    return promise_type;
}

static JsonElement *PromiseTypesToJson(void)
{
    JsonElement *promise_types = JsonObjectCreate(50);

    const PromiseTypeSyntax *global_promise_type = PromiseTypeSyntaxGet("*", "*");

    for (int module_index = 0; module_index < CF3_MODULES; module_index++)
    {
        for (int promise_type_index = 0; CF_ALL_PROMISE_TYPES[module_index][promise_type_index].promise_type; promise_type_index++)
        {
            const PromiseTypeSyntax *promise_type_syntax = &CF_ALL_PROMISE_TYPES[module_index][promise_type_index];

            // skip global and bundle-local common constraints
            if (strcmp("*", promise_type_syntax->promise_type) == 0)
            {
                continue;
            }

            if (promise_type_syntax->status == SYNTAX_STATUS_REMOVED)
            {
                continue;
            }

            JsonElement *promise_type = JsonObjectGet(promise_types, promise_type_syntax->promise_type);
            if (!promise_type)
            {
                promise_type = JsonPromiseTypeNew(promise_type_syntax->status);
                JsonObjectAppendObject(promise_types, promise_type_syntax->promise_type, promise_type);
            }
            assert(promise_type);

            JsonElement *attributes = JsonObjectGet(promise_type, "attributes");
            assert(attributes);

            for (int i = 0; promise_type_syntax->constraints[i].lval; i++)
            {
                const ConstraintSyntax *constraint_syntax = &promise_type_syntax->constraints[i];
                JsonElement *json_constraint = ConstraintSyntaxToJson(constraint_syntax);
                JsonObjectAppendString(json_constraint, "visibility", "promiseType");
                JsonObjectAppendObject(attributes, constraint_syntax->lval, json_constraint);
            }

            // append bundle common constraints
            const PromiseTypeSyntax *bundle_promise_type = PromiseTypeSyntaxGet(promise_type_syntax->bundle_type, "*");
            if (strcmp("*", bundle_promise_type->bundle_type) != 0)
            {
                for (int i = 0; bundle_promise_type->constraints[i].lval; i++)
                {
                    const ConstraintSyntax *constraint_syntax = &bundle_promise_type->constraints[i];
                    JsonElement *json_constraint = ConstraintSyntaxToJson(constraint_syntax);
                    JsonObjectAppendString(json_constraint, "visibility", "bundle");
                    JsonObjectAppendObject(attributes, constraint_syntax->lval, json_constraint);
                }
            }

            // append global common constraints
            for (int i = 0; global_promise_type->constraints[i].lval; i++)
            {
                const ConstraintSyntax *constraint_syntax = &global_promise_type->constraints[i];
                JsonElement *json_constraint = ConstraintSyntaxToJson(constraint_syntax);
                JsonObjectAppendString(json_constraint, "visibility", "global");
                JsonObjectAppendObject(attributes, constraint_syntax->lval, json_constraint);
            }
        }
    }

    return promise_types;
}

static JsonElement *BodyTypesToJson(void)
{
    JsonElement *body_types = JsonObjectCreate(50);

    for (int module_index = 0; module_index < CF3_MODULES; module_index++)
    {
        for (int promise_type_index = 0; CF_ALL_PROMISE_TYPES[module_index][promise_type_index].promise_type; promise_type_index++)
        {
            const PromiseTypeSyntax *promise_type_syntax = &CF_ALL_PROMISE_TYPES[module_index][promise_type_index];

            for (int constraint_index = 0; promise_type_syntax->constraints[constraint_index].lval; constraint_index++)
            {
                const ConstraintSyntax *constraint_syntax = &promise_type_syntax->constraints[constraint_index];
                if (constraint_syntax->dtype != DATA_TYPE_BODY)
                {
                    continue;
                }

                if (constraint_syntax->status == SYNTAX_STATUS_REMOVED)
                {
                    continue;
                }

                const BodySyntax *body_syntax = constraint_syntax->range.body_type_syntax;
                JsonElement *body_type = JsonObjectGet(body_types, body_syntax->body_type);
                if (!body_type)
                {
                    JsonElement *body_type = BodySyntaxToJson(body_syntax);
                    JsonObjectAppendObject(body_types, body_syntax->body_type, body_type);
                }
            }
        }
    }

    for (int i = 0; CONTROL_BODIES[i].body_type; i++)
    {
        const BodySyntax *body_syntax = &CONTROL_BODIES[i];

        if (body_syntax->status == SYNTAX_STATUS_REMOVED)
        {
            continue;
        }

        JsonElement *body_type = JsonObjectGet(body_types, body_syntax->body_type);
        if (!body_type)
        {
            JsonElement *body_type = BodySyntaxToJson(body_syntax);
            JsonObjectAppendObject(body_types, body_syntax->body_type, body_type);
        }
    }

    return body_types;
}

static const char *FnCallCategoryToString(FnCallCategory category)
{
    static const char *category_str[] =
    {
        [FNCALL_CATEGORY_COMM] = "communication",
        [FNCALL_CATEGORY_DATA] = "data",
        [FNCALL_CATEGORY_FILES] = "files",
        [FNCALL_CATEGORY_IO] = "io",
        [FNCALL_CATEGORY_SYSTEM] = "system",
        [FNCALL_CATEGORY_UTILS] = "utils"
    };

    return category_str[category];
}

static JsonElement *FnCallTypeToJson(const FnCallType *fn_syntax)
{
    JsonElement *json_fn = JsonObjectCreate(10);

    JsonObjectAppendString(json_fn, "status", SyntaxStatusToString(fn_syntax->status));
    JsonObjectAppendString(json_fn, "returnType", DataTypeToString(fn_syntax->dtype));

    {
        JsonElement *params = JsonArrayCreate(10);
        for (int i = 0; fn_syntax->args[i].pattern; i++)
        {
            const FnCallArg *param = &fn_syntax->args[i];

            JsonElement *json_param = JsonObjectCreate(2);
            JsonObjectAppendString(json_param, "type", DataTypeToString(param->dtype));
            JsonObjectAppendString(json_param, "range", param->pattern);
            JsonArrayAppendObject(params, json_param);
        }
        JsonObjectAppendArray(json_fn, "parameters", params);
    }

    JsonObjectAppendBool(json_fn, "variadic", fn_syntax->varargs);
    JsonObjectAppendString(json_fn, "category", FnCallCategoryToString(fn_syntax->category));

    return json_fn;
}

static JsonElement *FunctionsToJson(void)
{
    JsonElement *functions = JsonObjectCreate(500);

    for (int i = 0; CF_FNCALL_TYPES[i].name; i++)
    {
        const FnCallType *fn_syntax = &CF_FNCALL_TYPES[i];

        if (fn_syntax->status == SYNTAX_STATUS_REMOVED)
        {
            continue;
        }

        JsonObjectAppendObject(functions, fn_syntax->name, FnCallTypeToJson(fn_syntax));
    }

    return functions;
}

JsonElement *SyntaxToJson(void)
{
    JsonElement *syntax_tree = JsonObjectCreate(3);

    JsonObjectAppendObject(syntax_tree, "bundleTypes", BundleTypesToJson());
    JsonObjectAppendObject(syntax_tree, "promiseTypes", PromiseTypesToJson());
    JsonObjectAppendObject(syntax_tree, "bodyTypes", BodyTypesToJson());
    JsonObjectAppendObject(syntax_tree, "functions", FunctionsToJson());

    return syntax_tree;
}
